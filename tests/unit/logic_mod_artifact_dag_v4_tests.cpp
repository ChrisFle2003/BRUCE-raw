#include "bruce/mod/logic_mod_artifact_dag_v4.hpp"
#include <cerrno>
#include <atomic>
#include <chrono>
#include <thread>
#include <set>
#include "bruce/evidence/bundle.hpp"
#include "bruce/mod/logic_mod_payload_v1.hpp"
#include <openssl/evp.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <elf.h>
#include <cstring>
#include <limits>
#include <unistd.h>
namespace {
void check(bool value, const char* why) { if (!value) throw bruce::Error(why); }
template<class F> void rejects(F action, std::string_view expected = {}) {
    try { action(); } catch (const bruce::Error& e) {
        if (!expected.empty() && e.what() != expected) throw bruce::Error(std::string("Wrong error: ") + e.what());
        return;
    }
    throw bruce::Error("Expected chain rejection");
}
std::string field(std::string_view value) { return std::to_string(value.size()) + ':' + std::string(value); }
std::string member(std::string_view name, std::string_view value) { return field(name) + field(value); }
std::string read(const std::filesystem::path& path) { std::ifstream in(path, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}}; }
void write(const std::filesystem::path& path, std::string_view bytes) { std::ofstream out(path, std::ios::binary); out << bytes; out.close(); check(bool(out), "fixture write failed"); check(::chmod(path.c_str(), 0600) == 0, "fixture write chmod failed"); }
std::string sign(std::string_view bytes) {
    const unsigned char seed[] = {0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
    auto* key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed, sizeof(seed));
    auto* ctx = EVP_MD_CTX_new(); unsigned char bytes_out[64]; std::size_t count = 64;
    check(key && ctx && EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key) == 1 && EVP_DigestSign(ctx, bytes_out, &count, reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()) == 1, "sign failed");
    EVP_MD_CTX_free(ctx); EVP_PKEY_free(key);
    std::string out; for (auto byte : bytes_out) { out += "0123456789abcdef"[byte >> 4]; out += "0123456789abcdef"[byte & 15]; } return out;
}
std::string package(std::string_view bytes, std::string_view capabilities) {
    const auto payload = member("tree.json", R"({"mount":[1,1],"nodes":[{"aliases":["hoch"],"claim_scope":"candidate_only","id":[1,1,5],"key":"power","route_pattern":["<rational>","hoch","<integer>"],"template_id":"math.rational.power.v1"}]})") + member("tests/golden-cases.json", R"([{"input":"3/4 hoch 2","status":"MATCHED","template_id":"math.rational.power.v1"}])");
    const auto manifest = "{\"capabilities\":[\"dataset.read\",\"tree.extend\"],\"host_abi_version\":1,\"mod_id\":\"user.logic\",\"owner_scope\":\"user\",\"payload_sha256\":\"" + bruce::sha256(payload) + "\",\"schema_version\":1,\"signing_key_id\":\"test-rfc8032\",\"token_normalization_version\":1,\"tree_schema_version\":1,\"version\":\"1.0.0\"}";
    auto data = "BRUCE_RUNTIME_DATA_MOD_V1\n" + member("manifest.json", manifest) + payload;
    data += member("signature.ed25519", sign(data)) + '\n';
    const auto logic = "{\"capabilities\":[" + std::string(capabilities) + "],\"template_ids\":[\"math.rational.power.v1\"],\"worker_path\":\"worker.bin\",\"worker_sha256\":\"" + bruce::sha256(bytes) + "\"}";
    const auto raw = "BRUCE_LOGIC_MOD_PLUGIN_V1\n" + member("data.brucemod", data) + member("logic.json", logic) + member("worker.bin", bytes);
    return raw + member("signature.ed25519", sign(raw)) + '\n';
}
constexpr auto a_caps = "\"artifact.output.v1\",\"worker.execute\"";
constexpr auto b_caps = "\"artifact.input.v1\",\"artifact.output.v1\",\"worker.execute\"";
struct Fixture {
    std::filesystem::path dir, store;
    bruce::LogicModRegistryV1 root, child;
    Fixture(bool invalid_child = false, bool timed = false) {
        char pattern[] = "/tmp/bruce-dag-XXXXXX"; check(::mkdtemp(pattern), "mkdtemp failed"); dir = pattern; store = dir / "store";
        for (unsigned i = 0; i != 2; ++i) {
            const auto sub = dir / (i ? "child" : "root"); std::filesystem::create_directory(sub); ::chmod(sub.c_str(), 0700);
            const auto bytes = i && invalid_child ? std::string("signed invalid executable") : read(timed ? BRUCE_DAG_V3_WORKER : BRUCE_DAG_WORKER);
            write(sub / "worker.bin", bytes); ::chmod((sub / "worker.bin").c_str(), 0500);
            const auto raw = package(bytes, i ? b_caps : a_caps); const auto id = bruce::sha256(raw);
            auto& registry = i ? child : root;
            registry.discover(bruce::unpack_logic_mod_package_v1(raw), sub.string());
            registry.verify(id, bruce::read_runtime_mod_truststore_v1("BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\ntest-rfc8032:d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a\n"));
            registry.test_golden(id, bruce::core_tree_registry_v1()); registry.install(id); registry.activate(id);
        }
    }
    ~Fixture() { std::filesystem::remove_all(dir); }
    bruce::LogicModDagNodeV2 node(std::string id, std::vector<std::string> predecessors = {}) {
        const bool nonroot = !predecessors.empty();
        return {id, nonroot ? child : root, "user.logic", "1.0.0", "math.rational.power.v1", "3/4 hoch 2", predecessors,
            nonroot ? std::optional<bruce::LogicModOutputV1>{{store, id + "-input"}} : std::nullopt, {store, id + "-out"}};
    }
};
constexpr auto spec_digest = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

void reaped() {
    int status=0; errno=0;
    check(::waitpid(-1,&status,WNOHANG)==-1 && errno==ECHILD,"V4 returned with unreaped worker");
}
void refused(const bruce::LogicModArtifactDagRunV4& run, std::string_view status, std::size_t completed) {
    check(!run.accepted && run.status==status && run.nodes.size()==completed && run.dag_id.empty(),"V4 controlled stop mismatch");
    rejects([&] { (void)bruce::logic_mod_artifact_dag_record_v4(run); });
    reaped();
}
void limits_are_early_and_strict() {
    Fixture f;
    using Control=bruce::LogicModArtifactDagControlV4;
    std::vector<Control> bad;
    for(auto value:{0U,5U}) { Control c; c.max_parallel_nodes=value; bad.push_back(c); }
    for(auto value:{0U,17U}) { Control c; c.max_started_workers=value; bad.push_back(c); }
    for(auto value:{0U,60001U}) {
        Control c; c.max_total_wall_ms=value; bad.push_back(c);
        c={}; c.cancel_after_ms=value; bad.push_back(c);
    }
    Control c; c.max_total_payload_bytes=8388609; bad.push_back(c);
    for(const auto& control:bad) {
        rejects([&] { (void)bruce::run_logic_mod_artifact_dag_v4({f.node("a")},f.store,spec_digest,control); });
        check(!std::filesystem::exists(f.store),"Invalid V4 control mutated store");
    }
}
void worker_budget_refuses_whole_wave() {
    Fixture f;
    bruce::LogicModArtifactDagControlV4 c; c.max_parallel_nodes=2; c.max_started_workers=1;
    const auto run=bruce::run_logic_mod_artifact_dag_v4({f.node("a"),f.node("b")},f.store,spec_digest,c);
    refused(run,"DAG_GLOBAL_BUDGET_EXHAUSTED",0);
    check(!std::filesystem::exists(f.store),"Worker budget started partial wave");
}
void worker_budget_preserves_completed_wave() {
    Fixture f(false,true);
    auto a=f.node("a"); a.input="1/2 hoch 2";
    bruce::LogicModArtifactDagControlV4 c; c.max_parallel_nodes=2; c.max_started_workers=2;
    const auto run=bruce::run_logic_mod_artifact_dag_v4({a,f.node("b",{"a"}),f.node("z")},f.store,spec_digest,c);
    refused(run,"DAG_GLOBAL_BUDGET_EXHAUSTED",2);
    check(std::filesystem::exists(f.store/"manifests/a-out.manifest") && std::filesystem::exists(f.store/"manifests/z-out.manifest"),"Worker stop lost prior outputs or failed to drain peer");
    check(!std::filesystem::exists(f.store/"manifests/b-input.manifest") && !std::filesystem::exists(f.store/"manifests/b-out.manifest"),"Worker stop prepared future wave");
}
void payload_budget_counts_internal_inputs_and_is_cumulative() {
    Fixture f;
    bruce::LogicModArtifactDagControlV4 c; c.max_total_payload_bytes=0;
    const auto stopped=bruce::run_logic_mod_artifact_dag_v4({f.node("a"),f.node("b",{"a"})},f.store,spec_digest,c);
    refused(stopped,"DAG_GLOBAL_BUDGET_EXHAUSTED",1);
    check(std::filesystem::exists(f.store/"manifests/a-out.manifest") && !std::filesystem::exists(f.store/"manifests/b-input.manifest"),"Payload stop published future input or lost root");
    // Single predecessor a/out: header 29, version 3, count 3, id 3,
    // manifest a-out 7, SHA256 67, size 3, codec logic-mod-result-text-v1 27,
    // result 9/16 6, trailing newline 1 = 149 bytes.
    c.max_total_payload_bytes=149;
    const auto exact=bruce::run_logic_mod_artifact_dag_v4({f.node("a"),f.node("b",{"a"})},f.store,spec_digest,c);
    check(exact.accepted && exact.nodes.back().internal_input_artifact->size_bytes==149,"Exact payload boundary denied");
    const auto cumulative=bruce::run_logic_mod_artifact_dag_v4({f.node("a"),f.node("b",{"a"}),f.node("c",{"b"})},f.store,spec_digest,c);
    refused(cumulative,"DAG_GLOBAL_BUDGET_EXHAUSTED",2);
    check(!std::filesystem::exists(f.store/"manifests/c-input.manifest"),"Payload accounting reset between waves");
    Fixture whole;
    c.max_parallel_nodes=2; c.max_total_payload_bytes=149;
    const auto wave=bruce::run_logic_mod_artifact_dag_v4({whole.node("a"),whole.node("b",{"a"}),whole.node("c",{"a"})},whole.store,spec_digest,c);
    refused(wave,"DAG_GLOBAL_BUDGET_EXHAUSTED",1);
    check(!std::filesystem::exists(whole.store/"manifests/b-input.manifest") && !std::filesystem::exists(whole.store/"manifests/c-input.manifest"),"Payload budget partially published refused wave");
}
void timers_stop_only_before_next_wave() {
    for(bool cancellation:{false,true}) {
        Fixture f(false,true);
        bruce::LogicModArtifactDagControlV4 c; c.max_parallel_nodes=2;
        if(cancellation) c.cancel_after_ms=50; else c.max_total_wall_ms=50;
        const auto run=bruce::run_logic_mod_artifact_dag_v4({f.node("a"),f.node("b",{"a"}),f.node("z")},f.store,spec_digest,c);
        refused(run,cancellation ? "DAG_HOST_CANCELLED":"DAG_GLOBAL_BUDGET_EXHAUSTED",2);
        check(std::filesystem::exists(f.store/"manifests/a-out.manifest") && std::filesystem::exists(f.store/"manifests/z-out.manifest"),"Timer killed in-flight worker");
        check(!std::filesystem::exists(f.store/"manifests/b-input.manifest"),"Timer started future wave");
        Fixture terminal(false,true);
        const auto completed=bruce::run_logic_mod_artifact_dag_v4({terminal.node("a")},terminal.store,spec_digest,c);
        check(completed.accepted,"Timer rejected completed terminal wave");
        reaped();
    }
}
volatile sig_atomic_t replay_children=0;
void replay_child(int) { replay_children=replay_children+1; }
void failed_wave_drains_and_preserves_v3_status() {
    Fixture f(false,true);
    auto a=f.node("a"); a.input="2/3 hoch 2";
    bruce::LogicModArtifactDagControlV4 c; c.max_parallel_nodes=2;
    const auto run=bruce::run_logic_mod_artifact_dag_v4({a,f.node("b"),f.node("c",{"a"})},f.store,spec_digest,c);
    refused(run,"DAG_NODE_FAILED",2);
    check(std::filesystem::exists(f.store/"manifests/b-out.manifest") &&
        !std::filesystem::exists(f.store/"manifests/c-input.manifest"),"Failed V4 wave did not drain peer and stop successor");
}
void default_and_controls_keep_v3_records() {
    Fixture f;
    const std::vector<bruce::LogicModDagNodeV2> nodes{f.node("z"),f.node("b",{"a"}),f.node("a")};
    const auto baseline=bruce::run_logic_mod_artifact_dag_v3(nodes,f.store,spec_digest);
    const auto bytes=bruce::logic_mod_artifact_dag_record_v3(baseline);
    for(unsigned parallel:{1U,2U,4U}) {
        bruce::LogicModArtifactDagControlV4 c; c.max_parallel_nodes=parallel;
        c.max_started_workers=3; c.max_total_payload_bytes=8388608; c.max_total_wall_ms=60000; c.cancel_after_ms=60000;
        const auto run=bruce::run_logic_mod_artifact_dag_v4(nodes,f.store,spec_digest,c);
        check(run.accepted && bruce::logic_mod_artifact_dag_record_v4(run)==bytes,"Host budget changed V3 record bytes");
        const auto parsed=bruce::read_logic_mod_artifact_dag_record_v4(bytes);
        struct sigaction action{}, previous{}; action.sa_handler=replay_child; ::sigemptyset(&action.sa_mask);
        replay_children=0; check(::sigaction(SIGCHLD,&action,&previous)==0,"Replay sigaction failed");
        bruce::verify_logic_mod_artifact_dag_replay_v4(nodes,f.store,spec_digest,parsed);
        auto altered=parsed; altered.nodes.front().run.result="bad";
        rejects([&] { bruce::verify_logic_mod_artifact_dag_replay_v4(nodes,f.store,spec_digest,altered); },"DAG_REPLAY_MISMATCH");
        check(::sigaction(SIGCHLD,&previous,nullptr)==0 && !replay_children,"V4 replay started worker");
        check(bruce::logic_mod_artifact_dag_record_v4(parsed)==bytes,"V4 record roundtrip changed bytes");
        reaped();
    }
    const auto defaults=bruce::run_logic_mod_artifact_dag_v4(nodes,f.store,spec_digest);
    check(bruce::logic_mod_artifact_dag_record_v4(defaults)==bytes,"V4 defaults changed V3 record bytes");
    Fixture roots;
    bruce::LogicModArtifactDagControlV4 c; c.max_total_payload_bytes=0;
    check(bruce::run_logic_mod_artifact_dag_v4({roots.node("a")},roots.store,spec_digest,c).accepted,"Zero payload denied root without internal input");
}
}
int main() {
    const std::pair<const char*,void(*)()> cases[]{
        {"limits_are_early_and_strict",limits_are_early_and_strict},
        {"worker_budget_refuses_whole_wave",worker_budget_refuses_whole_wave},
        {"worker_budget_preserves_completed_wave",worker_budget_preserves_completed_wave},
        {"payload_budget_counts_internal_inputs_and_is_cumulative",payload_budget_counts_internal_inputs_and_is_cumulative},
        {"timers_stop_only_before_next_wave",timers_stop_only_before_next_wave},
        {"default_and_controls_keep_v3_records",default_and_controls_keep_v3_records},
        {"failed_wave_drains_and_preserves_v3_status",failed_wave_drains_and_preserves_v3_status},
    };
    bool failed=false;
    for(const auto& [name,test]:cases) {
        try { test(); std::cout<<"PASS "<<name<<'\n'; }
        catch(const std::exception& e) { failed=true; std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n'; }
    }
    return failed ? 1:0;
}
