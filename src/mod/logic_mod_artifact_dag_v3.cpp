#include "bruce/mod/logic_mod_artifact_dag_v3.hpp"
#include "bruce/mod/logic_mod_payload_v1.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/tree/normalizer.hpp"
#include <algorithm>
#include <cstring>
#include <elf.h>
#include <exception>
#include <future>
#include <map>
#include <set>
#include <unistd.h>

namespace bruce {
namespace {
// Admission and payload derivation deliberately retain the V2 node contract.
std::string field(std::string_view value) { return std::to_string(value.size()) + ':' + std::string(value); }
[[noreturn]] void invalid() { throw Error("LogicModDagRecordInvalid"); }
bool digest(std::string_view value) {
    return value.size() == 64 && value.find_first_not_of("0123456789abcdef") == std::string_view::npos;
}
bool node_id(std::string_view id) {
    return !id.empty() && id.size() <= 64 && id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == std::string_view::npos;
}
LogicModPublishedOutputV1 output_of(const LogicModRunV1& r) {
    return {r.output_manifest_id,r.output_object_index,r.output_sha256,r.output_size_bytes,r.output_codec};
}
std::vector<std::string> order_graph(const std::map<std::string,std::vector<std::string>>& graph) {
    if(graph.empty() || graph.size()>16) throw Error("LogicModDagInvalid");
    for(const auto& [id,parents]:graph) {
        if(!node_id(id) || parents.size()>15 || !std::is_sorted(parents.begin(),parents.end()) || std::adjacent_find(parents.begin(),parents.end())!=parents.end()) throw Error("LogicModDagInvalid");
        for(const auto& p:parents) if(p==id || !graph.contains(p)) throw Error("LogicModDagInvalid");
    }
    std::set<std::string> done; std::vector<std::string> order;
    while(order.size()<graph.size()) {
        bool found=false;
        for(const auto& [id,parents]:graph) if(!done.contains(id) && std::all_of(parents.begin(),parents.end(),[&](const auto& p){return done.contains(p);})) {
            done.insert(id); order.push_back(id); found=true; break;
        }
        if(!found) throw Error("LogicModDagInvalid");
    }
    return order;
}
std::vector<std::string> validate_nodes(const std::vector<LogicModDagNodeV2>& nodes,const std::filesystem::path& store,std::string_view spec) {
    if(!digest(spec)) throw Error("LogicModDagInvalid");
    std::map<std::string,std::vector<std::string>> graph; std::set<std::string> manifests;
    for(const auto& n:nodes) {
        if(!graph.emplace(n.id,n.predecessors).second || n.input_artifact.has_value()!=!n.predecessors.empty()) throw Error("LogicModDagInvalid");
        for(const auto* output:{&n.output_artifact,n.input_artifact?&*n.input_artifact:nullptr}) if(output) {
            if(output->store!=store || !artifact_v1::is_valid_manifest_id_v1(output->manifest_id) || !manifests.insert(output->manifest_id).second) throw Error("LogicModDagInvalid");
        }
    }
    return order_graph(graph);
}
void static_elf(std::string_view bytes) {
#if defined(__x86_64__)
    constexpr auto machine = EM_X86_64;
    constexpr std::uint64_t user_ceiling = std::uint64_t{1} << 47;
#elif defined(__aarch64__)
    constexpr auto machine = EM_AARCH64;
    constexpr std::uint64_t user_ceiling = std::uint64_t{1} << 39;
#else
    throw Error("LogicWorkerRequiresStaticELF");
    constexpr auto machine = EM_NONE;
    constexpr std::uint64_t user_ceiling = 0;
#endif
    // Conservative native user ranges work on the supported Linux platforms,
    // including hosts with fewer virtual address bits. Higher-address layouts
    // are outside this bounded V1 admission policy.
    const auto page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size <= 0 || (page_size & (page_size - 1)) || bytes.size() < sizeof(Elf64_Ehdr))
        throw Error("LogicWorkerRequiresStaticELF");
    const auto page = static_cast<std::uint64_t>(page_size);
    Elf64_Ehdr header; std::memcpy(&header, bytes.data(), sizeof(header));
    if (std::memcmp(header.e_ident, ELFMAG, SELFMAG) || header.e_ident[EI_CLASS] != ELFCLASS64 ||
        header.e_ident[EI_DATA] != ELFDATA2LSB || header.e_ident[EI_VERSION] != EV_CURRENT ||
        header.e_version != EV_CURRENT || (header.e_type != ET_EXEC && header.e_type != ET_DYN) ||
        header.e_machine != machine || header.e_ehsize != sizeof(Elf64_Ehdr) ||
        header.e_phentsize != sizeof(Elf64_Phdr) || !header.e_phnum || header.e_phnum > 128 ||
        header.e_phoff < sizeof(Elf64_Ehdr) || header.e_phoff > bytes.size() ||
        header.e_phnum > (bytes.size() - header.e_phoff) / sizeof(Elf64_Phdr)) throw Error("LogicWorkerRequiresStaticELF");
    bool load_present = false, executable_entry = false;
    std::uint64_t previous_address = 0;
    for (unsigned i = 0; i < header.e_phnum; ++i) {
        Elf64_Phdr program; std::memcpy(&program, bytes.data() + header.e_phoff + i * sizeof(program), sizeof(program));
        if (program.p_type == PT_INTERP || program.p_offset > bytes.size() ||
            program.p_filesz > bytes.size() - program.p_offset) throw Error("LogicWorkerRequiresStaticELF");
        if (program.p_type != PT_LOAD) continue;
        if (program.p_filesz > program.p_memsz || program.p_vaddr >= user_ceiling ||
            program.p_memsz > user_ceiling - program.p_vaddr ||
            (load_present && program.p_vaddr < previous_address) ||
            program.p_vaddr % page != program.p_offset % page ||
            (program.p_align > 1 && ((program.p_align & (program.p_align - 1)) ||
                program.p_vaddr % program.p_align != program.p_offset % program.p_align)))
            throw Error("LogicWorkerRequiresStaticELF");
        load_present = true; previous_address = program.p_vaddr;
        // Entry must name actual executable file bytes, not zero-filled BSS or
        // an unmapped address that would fail immediately after exec.
        if ((program.p_flags & PF_X) && header.e_entry >= program.p_vaddr &&
            header.e_entry - program.p_vaddr < program.p_filesz) executable_entry = true;
    }
    if (!load_present || !executable_entry) throw Error("LogicWorkerRequiresStaticELF");
}
void preflight_node(const LogicModDagNodeV2& step) {
    const auto b = step.budgets;
    if (!b.wall_ms || b.wall_ms > 5000 || b.memory_bytes < 8*1024*1024 || b.memory_bytes > 256*1024*1024 ||
        !b.input_bytes || b.input_bytes > 4096 || !b.output_bytes || b.output_bytes > 65536) throw Error("LogicBudgetDenied");
    if (step.input.size() > b.input_bytes) throw Error("LogicInputBudgetExceeded");
    (void)normalize_route_text_v1(step.input);
    const auto execution = step.registry.bind(step.mod_id, step.version, step.template_id, "worker.execute");
    static_elf(execution.worker_bytes());
    (void)step.registry.bind(step.mod_id, step.version, step.template_id, "artifact.output.v1");
    if (!step.predecessors.empty()) (void)step.registry.bind(step.mod_id, step.version, step.template_id, "artifact.input.v1");
    else {
        bool extra_input = false;
        try { (void)step.registry.bind(step.mod_id, step.version, step.template_id, "artifact.input.v1"); extra_input = true; }
        catch (const Error& error) {
            if (std::string_view(error.what()) != "LogicModCapabilityDenied") throw;
        }
        if (extra_input) throw Error("LogicModDagCapabilityDenied");
    }
}
std::string input_document(const std::vector<LogicModDagNodeRunV2>& completed, const std::vector<std::string>& predecessors) {
    std::string bytes="BRUCE_LOGIC_MOD_DAG_INPUT_V2\n"+field("2")+field(std::to_string(predecessors.size()));
    for(const auto& id:predecessors) {
        const auto found=std::find_if(completed.begin(),completed.end(),[&](const auto& n){return n.id==id;});
        if(found==completed.end()) invalid();
        const auto& p=found->result_artifact;
        bytes+=field(id)+field(p.manifest_id)+field(p.sha256)+field(std::to_string(p.size_bytes))+field(p.codec)+field(found->run.result);
    }
    bytes+='\n';
    if(bytes.size()>logic_mod_payload_limit_v1) throw Error("LogicModDagInputTooLarge");
    return bytes;
}
artifact_v1::ArtifactManifestV1 input_manifest(const LogicModPublishedOutputV1& p) {
    return {p.manifest_id,p.manifest_id,p.manifest_id,"READY","ACCEPTED",{{{p.sha256,p.size_bytes},p.codec,"logic-mod-dag-input-v2"}}};
}
LogicModPublishedOutputV1 publish_input(const LogicModOutputV1& target,std::string_view bytes) {
    auto store=artifact_v1::ArtifactStoreV1::open(target.store);
    const auto object=store.put_bytes(bytes);
    LogicModPublishedOutputV1 out{target.manifest_id,0,object.sha256,object.size_bytes,"logic-mod-dag-input-v2"};
    store.publish_manifest(out.manifest_id,input_manifest(out)); return out;
}
void verify_input(const std::filesystem::path& root,const LogicModPublishedOutputV1& p,std::string_view bytes) {
    if(p.object_index || p.sha256!=sha256(bytes) || p.size_bytes!=bytes.size() || p.codec!="logic-mod-dag-input-v2") invalid();
    auto store=artifact_v1::ArtifactStoreV1::open_existing(root);
    if(store.read_manifest(p.manifest_id)!=input_manifest(p)) invalid();
    store.verify({p.sha256,p.size_bytes});
}
std::string record_body(const LogicModArtifactDagRunV2& r) {
    std::string bytes=field(r.spec_digest)+field(std::to_string(r.nodes.size()));
    for(const auto& n:r.nodes) {
        bytes+=field(n.id)+field(std::to_string(n.predecessors.size()));
        for(const auto& p:n.predecessors) bytes+=field(p);
        bytes+=field(logic_mod_run_record_v1(n.run))+field(n.result_artifact.canonical_identity())+field(n.internal_input_artifact ? "1":"0");
        if(n.internal_input_artifact) bytes+=field(n.internal_input_artifact->canonical_identity());
        if(bytes.size()>logic_mod_artifact_dag_record_limit_v2) throw Error("LogicModDagRecordTooLarge");
    }
    return bytes;
}
std::string dag_id(const LogicModArtifactDagRunV2& r) { return sha256("BRUCE_LOGIC_MOD_ARTIFACT_DAG_ID_V2\n"+record_body(r)); }
const LogicModDagNodeV2& node_for(const std::vector<LogicModDagNodeV2>& nodes,std::string_view id) {
    const auto found=std::find_if(nodes.begin(),nodes.end(),[&](const auto& n){return n.id==id;});
    if(found==nodes.end()) invalid();
    return *found;
}
void validate_targets(const std::vector<LogicModDagNodeV2>& nodes,const std::filesystem::path& root) {
    // Absent stores cannot contain unsafe publication targets. Existing stores
    // and every selected target are inspected read-only before any mutation.
    if(!std::filesystem::exists(std::filesystem::symlink_status(root))) return;
    auto store=artifact_v1::ArtifactStoreV1::open_existing(root);
    for(const auto& n:nodes) {
        store.validate_manifest_target(n.output_artifact.manifest_id);
        if(n.input_artifact) store.validate_manifest_target(n.input_artifact->manifest_id);
    }
}
LogicModDagNodeRunV2 prepare_node(const LogicModDagNodeV2& node,
    const std::filesystem::path& store, const std::vector<LogicModDagNodeRunV2>& completed) {
    LogicModDagNodeRunV2 entry; entry.id=node.id; entry.predecessors=node.predecessors;
    if (node.input_artifact) {
        for (const auto& id:node.predecessors) {
            const auto found=std::find_if(completed.begin(),completed.end(),[&](const auto& n){ return n.id==id; });
            if (found==completed.end()) invalid();
            verify_logic_mod_result_artifact_v1({store,found->result_artifact.manifest_id},found->result_artifact,found->run.result);
        }
        const auto bytes=input_document(completed,node.predecessors);
        entry.internal_input_artifact=publish_input(*node.input_artifact,bytes);
        verify_input(store,*entry.internal_input_artifact,bytes);
    }
    return entry;
}
}
LogicModArtifactDagRunV3 run_logic_mod_artifact_dag_v3(const std::vector<LogicModDagNodeV2>& nodes,
    const std::filesystem::path& store, std::string_view spec_digest, LogicModArtifactDagControlV3 control) {
    if (!control.max_parallel_nodes || control.max_parallel_nodes>4) throw Error("LogicModDagParallelLimitInvalid");
    if (control.max_parallel_nodes==1) return run_logic_mod_artifact_dag_v2(nodes,store,spec_digest);
    const auto order=validate_nodes(nodes,store,spec_digest);
    for (const auto& node:nodes) preflight_node(node);
    validate_targets(nodes,store);
    LogicModArtifactDagRunV3 result; result.spec_digest=spec_digest; result.node_order=order;
    std::set<std::string> completed;
    while (completed.size()<nodes.size()) {
        std::vector<const LogicModDagNodeV2*> wave;
        for (const auto& id:order) {
            const auto& node=node_for(nodes,id);
            if (!completed.contains(id) && std::all_of(node.predecessors.begin(),node.predecessors.end(),[&](const auto& parent){ return completed.contains(parent); })) {
                wave.push_back(&node);
                if (wave.size()==control.max_parallel_nodes) break;
            }
        }
        // Freeze readiness and prepare every input before starting this wave.
        std::vector<LogicModDagNodeRunV2> entries;
        entries.reserve(wave.size());
        for (const auto* node:wave) entries.push_back(prepare_node(*node,store,result.nodes));
        std::vector<std::future<LogicModRunV1>> running;
        running.reserve(wave.size());
        std::exception_ptr error;
        try {
            for (const auto* node:wave) {
                const auto input=node->input_artifact ? std::optional<LogicModInputV1>{{store,node->input_artifact->manifest_id,0}} : std::nullopt;
                running.push_back(std::async(std::launch::async,[node,input] {
                    return run_logic_mod_worker_v1(node->registry,node->mod_id,node->version,node->template_id,
                        "worker.execute",node->input,node->budgets,input,node->output_artifact);
                }));
            }
        } catch (...) { error=std::current_exception(); }
        // Even launch, authorization, or publication exceptions drain all starts.
        for (std::size_t i=0;i<running.size();++i) {
            try { entries[i].run=running[i].get(); }
            catch (...) { if (!error) error=std::current_exception(); }
        }
        if (error) std::rethrow_exception(error);
        bool failed=false;
        for (std::size_t i=0;i<entries.size();++i) {
            auto& entry=entries[i];
            if (entry.run.accepted) {
                entry.result_artifact=output_of(entry.run);
                verify_logic_mod_result_artifact_v1(wave[i]->output_artifact,entry.result_artifact,entry.run.result);
            } else failed=true;
            completed.insert(entry.id);
            result.nodes.push_back(std::move(entry));
        }
        // Physical readiness can differ from V2's logical topological order.
        std::sort(result.nodes.begin(),result.nodes.end(),[&](const auto& a,const auto& b) {
            return std::find(order.begin(),order.end(),a.id)<std::find(order.begin(),order.end(),b.id);
        });
        if (failed) { result.status="DAG_NODE_FAILED"; return result; }
    }
    result.accepted=true; result.status="ACCEPTED"; result.dag_id=dag_id(result);
    (void)logic_mod_artifact_dag_record_v2(result);
    return result;
}
std::string logic_mod_artifact_dag_record_v3(const LogicModArtifactDagRunV3& result) {
    return logic_mod_artifact_dag_record_v2(result);
}
LogicModArtifactDagRunV3 read_logic_mod_artifact_dag_record_v3(std::string_view bytes) {
    return read_logic_mod_artifact_dag_record_v2(bytes);
}
void verify_logic_mod_artifact_dag_replay_v3(const std::vector<LogicModDagNodeV2>& nodes,
    const std::filesystem::path& store, std::string_view spec_digest, const LogicModArtifactDagRunV3& result) {
    verify_logic_mod_artifact_dag_replay_v2(nodes,store,spec_digest,result);
}
}
