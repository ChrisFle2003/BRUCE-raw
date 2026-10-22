#include "bruce/mod/logic_mod_artifact_dag_v5.hpp"
#include "bruce/mod/logic_mod_artifact_dag_v6.hpp"
#include "bruce/mod/logic_mod_supervision_log_v6.hpp"
#include <cerrno>
#include <atomic>
#include <array>
#include <chrono>
#include <thread>
#include <set>
#include <map>
#include <mutex>
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

constexpr auto remote = "https://example.org:8443/bruce";
struct Remote {
    std::map<std::string,std::string> files;
    unsigned gets=0;
    std::mutex mutex;
    bruce::artifact_v4::HttpsTransportV4 transport() {
        return [&](const auto& request, const auto& headers, const auto& body) {
            std::lock_guard lock(mutex);
            check(request.endpoint.host=="example.org" && request.endpoint.port==8443,"Wrong remote authority");
            if(request.method=="PUT") {
                files[request.endpoint.path]=request.upload_bytes;
                headers(201,0);
            } else {
                check(request.method=="GET" && request.upload_bytes.empty(),"Restore uploaded data");
                ++gets;
                const auto found=files.find(request.endpoint.path);
                check(found!=files.end(),"Restore requested unexpected identity");
                headers(200,found->second.size()); body(found->second);
            }
        };
    }
};
volatile sig_atomic_t replay_children=0;
void replay_child(int) { replay_children=replay_children+1; }
// A local hit must keep V4 record bytes and remain entirely offline, including replay.
void local_hits_keep_v4_records_and_replay_offline() {
    Fixture f;
    const std::vector<bruce::LogicModDagNodeV2> nodes{f.node("a"),f.node("b",{"a"})};
    const auto baseline=bruce::run_logic_mod_artifact_dag_v4(nodes,f.store,spec_digest);
    const auto bytes=bruce::logic_mod_artifact_dag_record_v4(baseline);
    bruce::LogicModArtifactDagControlV5 control; control.remote_restore=remote;
    unsigned calls=0;
    auto never=[&](const auto&,const auto&,const auto&) { ++calls; throw bruce::Error("Unexpected network"); };
    const auto run=bruce::run_logic_mod_artifact_dag_v5(nodes,f.store,spec_digest,control,never);
    check(run.accepted && calls==0,"Local hit reached network");
    check(bruce::logic_mod_artifact_dag_record_v5(run)==bytes,"Remote option changed V4 record bytes");
    const auto parsed=bruce::read_logic_mod_artifact_dag_record_v5(bytes);
    struct sigaction action{}, previous{}; action.sa_handler=replay_child; ::sigemptyset(&action.sa_mask);
    replay_children=0; check(::sigaction(SIGCHLD,&action,&previous)==0,"Replay sigaction failed");
    try { bruce::verify_logic_mod_artifact_dag_replay_v5(nodes,f.store,spec_digest,parsed); }
    catch(...) { (void)::sigaction(SIGCHLD,&previous,nullptr); throw; }
    check(::sigaction(SIGCHLD,&previous,nullptr)==0 && !replay_children,"V5 replay started worker");
    check(bruce::logic_mod_artifact_dag_record_v5(parsed)==bytes,"V5 record roundtrip changed bytes");
    const auto defaults=bruce::run_logic_mod_artifact_dag_v5(nodes,f.store,spec_digest);
    check(bruce::logic_mod_artifact_dag_record_v5(defaults)==bytes,"V5 default changed V4 bytes");
}
// Root a's child is reaped before a-out is published. Any child observed after
// publication and before b-out therefore proves admission of the independent b
// worker, which follows a's DAG-level verification. Pause b during mutation so
// the successor cannot race the observer after that admission proof.
pid_t next_worker_after_publication(const std::filesystem::path& output, const std::atomic<bool>& done) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(std::chrono::steady_clock::now()<deadline) {
        if(std::filesystem::exists(output)) {
            for(const auto& task:std::filesystem::directory_iterator("/proc/self/task")) {
                std::ifstream children(task.path()/"children");
                pid_t pid=0;
                if(children>>pid) return pid;
            }
        }
        if(done) throw bruce::Error("Independent worker was never admitted");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw bruce::Error("Independent worker admission timed out");
}
struct ResumeWorker {
    pid_t pid;
    ~ResumeWorker() { (void)::kill(pid,SIGCONT); }
};
void await_stopped(pid_t pid) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
    while(std::chrono::steady_clock::now()<deadline) {
        std::ifstream status("/proc/"+std::to_string(pid)+"/status");
        std::string line;
        while(std::getline(status,line)) if(line.starts_with("State:")) {
            if(line.find('T')!=std::string::npos) return;
            check(line.find('Z')==std::string::npos,"Independent worker exited before observer stopped it");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw bruce::Error("Independent worker stop timed out");
}
template<class Mutation, class Run> void with_missing_predecessor(Mutation mutation,Run action) {
    Fixture f(false,true); Remote peer;
    auto b=f.node("b"); b.input="1/2 hoch 2"; b.budgets.wall_ms=2000;
    auto a=f.node("a"); a.budgets.wall_ms=2000;
    auto c=f.node("c",{"a","b"}); c.budgets.wall_ms=2000;
    const std::vector<bruce::LogicModDagNodeV2> nodes{a,b,c};
    std::atomic<bool> done=false; std::exception_ptr observer_error;
    std::thread observer([&] {
        try {
            const auto path=f.store/"manifests/a-out.manifest";
            const auto pid=next_worker_after_publication(path,done);
            check(::kill(pid,SIGSTOP)==0,"Independent worker exited before observer stopped it");
            const ResumeWorker resume{pid};
            await_stopped(pid);
            check(!std::filesystem::exists(f.store/"manifests/b-out.manifest"),"Observer missed independent worker admission");
            bruce::artifact_v4::mirror_to_remote_v4("a-out",f.store,remote,peer.transport());
            std::lock_guard lock(peer.mutex);
            mutation(f,peer,path);
        } catch(...) { observer_error=std::current_exception(); }
    });
    try { action(f,peer,nodes); }
    catch(...) { done=true; observer.join(); if(observer_error) std::rethrow_exception(observer_error); throw; }
    done=true; observer.join(); if(observer_error) std::rethrow_exception(observer_error);
    int status=0; errno=0;
    check(::waitpid(-1,&status,WNOHANG)==-1 && errno==ECHILD,"V5 returned with live or unreaped worker");
}
void missing_expected_predecessor_restored_before_handoff() {
    for(bool remove_object:{false,true}) {
        with_missing_predecessor([&](auto& f,auto&,const auto& path) {
            const auto manifest=bruce::artifact_v1::ArtifactStoreV1::open_existing(f.store).read_manifest("a-out");
            std::filesystem::remove(path);
            if(remove_object) {
                const auto hash=manifest.objects.front().id.sha256;
                std::filesystem::remove(f.store/"objects"/hash.substr(0,2)/(hash+".blob"));
            }
        },[](auto& f,auto& peer,const auto& nodes) {
            bruce::LogicModArtifactDagControlV5 control; control.remote_restore=remote;
            const auto run=bruce::run_logic_mod_artifact_dag_v5(nodes,f.store,spec_digest,control,peer.transport());
            check(run.accepted && run.nodes.size()==3,"Restored predecessor did not reach worker");
            check(peer.gets==2,"Missing predecessor did not use one bounded V4 restore");
            bruce::verify_logic_mod_artifact_dag_replay_v5(nodes,f.store,spec_digest,run);
            check(std::filesystem::exists(f.store/"manifests/c-out.manifest"),"Successor output absent");
        });
    }
}

void corrupt_and_unreadable_local_targets_never_restore() {
    for(unsigned kind=0;kind<10;++kind) {
        with_missing_predecessor([&](auto& f,auto&,const auto& path) {
            if(kind==0) write(path,"corrupt local manifest");
            if(kind==1) check(::chmod(path.c_str(),0000)==0,"chmod failed");
            if(kind==2) { std::filesystem::remove(path); std::filesystem::create_symlink("missing",path); }
            if(kind==3) { std::filesystem::remove(path); std::filesystem::create_directory(path); }
            if(kind>=4) {
                const auto manifest=bruce::artifact_v1::ArtifactStoreV1::open_existing(f.store).read_manifest("a-out");
                const auto hash=manifest.objects.front().id.sha256;
                const auto object=f.store/"objects"/hash.substr(0,2)/(hash+".blob");
                if(kind>=6) std::filesystem::remove(path);
                if(kind==4 || kind==6) write(object,"corrupt local object");
                else if(kind==8) check(::chmod(object.c_str(),0000)==0,"Object chmod failed");
                else if(kind==9) {
                    std::filesystem::rename(object.parent_path(),f.dir/"saved-prefix");
                    std::filesystem::create_directory_symlink("missing",object.parent_path());
                } else { std::filesystem::remove(object); std::filesystem::create_symlink("missing",object); }
            }
        },[](auto& f,auto& peer,const auto& nodes) {
            bruce::LogicModArtifactDagControlV5 control; control.remote_restore=remote;
            rejects([&] { (void)bruce::run_logic_mod_artifact_dag_v5(nodes,f.store,spec_digest,control,peer.transport()); });
            check(peer.gets==0,"Unsafe existing predecessor reached network");
            check(!std::filesystem::exists(f.store/"manifests/c-input.manifest") &&
                !std::filesystem::exists(f.store/"manifests/c-out.manifest"),"Invalid predecessor started successor");
        });
    }
}
void remote_failure_and_wrong_expected_identity_never_start_successor() {
    for(unsigned kind=0;kind<5;++kind) {
        with_missing_predecessor([&](auto&,auto& peer,const auto& path) {
            std::filesystem::remove(path);
            auto& envelope=peer.files.at("/bruce/manifests/a-out.mirror");
            if(kind==1) envelope.back()^=1;
            if(kind==2) for(auto& [name,bytes]:peer.files) if(name.ends_with(".blob")) bytes="wrong digest";
            if(kind==3) {
                // Rewrap a fully canonical, digest-valid V4 manifest whose codec
                // disagrees with the already completed predecessor's identity.
                const auto start=envelope.find("BRUCE_ARTIFACT_MANIFEST_V1\n");
                check(start!=std::string::npos,"Manifest envelope marker missing");
                auto manifest=bruce::artifact_v1::decode_manifest_v1(std::string_view(envelope).substr(start));
                manifest.objects.front().media_type="different-codec";
                const auto bytes=bruce::artifact_v1::encode_manifest_v1(manifest);
                envelope="BRUCE_ARTIFACT_MIRROR_V4\nmanifest_id=a-out\nmanifest_sha256="+bruce::sha256(bytes)+
                    "\nmanifest_size_bytes="+std::to_string(bytes.size())+"\n"+bytes;
            }
            if(kind==4) envelope.replace(envelope.find("manifest_id=a-out"),17,"manifest_id=x-out");
        },[&](auto& f,auto& peer,const auto& nodes) {
            bruce::LogicModArtifactDagControlV5 control; control.remote_restore=remote;
            auto transport=peer.transport();
            bruce::artifact_v4::HttpsTransportV4 failing=[&](const auto& request,const auto& headers,const auto& body) {
                if(kind==0) throw bruce::Error("TLS certificate failed");
                transport(request,headers,body);
            };
            rejects([&] { (void)bruce::run_logic_mod_artifact_dag_v5(nodes,f.store,spec_digest,control,failing); });
            check(!std::filesystem::exists(f.store/"manifests/c-input.manifest") &&
                !std::filesystem::exists(f.store/"manifests/c-out.manifest"),"Bad remote started successor");
        });
    }
}
void disabled_remote_does_not_repair_missing_predecessor() {
    with_missing_predecessor([](auto&,auto&,const auto& path) { std::filesystem::remove(path); },
        [](auto& f,auto& peer,const auto& nodes) {
            rejects([&] { (void)bruce::run_logic_mod_artifact_dag_v5(nodes,f.store,spec_digest,{},peer.transport()); });
            check(peer.gets==0 && !std::filesystem::exists(f.store/"manifests/c-input.manifest"),"Disabled restore changed behavior");
        });
}
void invalid_remote_is_rejected_before_any_worker_or_store() {
    for(const auto* url:{"http://example.org","https://user:secret@example.org","https://example.org/?token=x",""}) {
        Fixture f; bruce::LogicModArtifactDagControlV5 control; control.remote_restore=url;
        unsigned calls=0;
        rejects([&] { (void)bruce::run_logic_mod_artifact_dag_v5({f.node("a")},f.store,spec_digest,control,
            [&](const auto&,const auto&,const auto&) { ++calls; }); });
        check(calls==0 && !std::filesystem::exists(f.store),"Invalid remote started root or accessed network");
    }
}
void v6_attempts_sidecar_and_replay_remain_record_compatible() {
    Fixture f;
    const std::vector<bruce::LogicModDagNodeV2> nodes{f.node("a"), f.node("b", {"a"}), f.node("c", {"a"})};
    bruce::LogicModArtifactDagControlV6 control;
    control.max_parallel_nodes = 2;
    control.report_root = f.dir / "report";
    const auto run = bruce::run_logic_mod_artifact_dag_v6(nodes, f.store, spec_digest, control);
    check(run.accepted && run.status == "ACCEPTED" && run.nodes.size() == 3, "V6 direct attempts did not accept DAG");
    const auto events = bruce::mod::validate_logic_mod_supervision_log_v6(control.report_root, "logic-mod-dag-v6");
    check(events.size() == 9, "V6 did not write exactly start/reap/finish sidecar lifecycle");
    for (const auto& node : {"a", "b", "c"}) {
        std::vector<std::string> lifecycle;
        for (const auto& sidecar : events) if (sidecar.node_id == node) {
            lifecycle.push_back(sidecar.event);
            if (sidecar.event != "ATTEMPT_STARTED") check(sidecar.terminal_reason == "READY_ACCEPTED",
                "V6 sidecar terminal reason incorrect");
        }
        check(lifecycle == std::vector<std::string>{"ATTEMPT_STARTED", "REAPED", "ATTEMPT_FINISHED"},
            "V6 sidecar lifecycle incorrect");
    }
    const auto bytes = bruce::logic_mod_artifact_dag_record_v6(run);
    check(bruce::logic_mod_artifact_dag_record_v5(bruce::read_logic_mod_artifact_dag_record_v5(bytes)) == bytes,
        "V6 changed canonical V2-V5 record bytes");
    bruce::verify_logic_mod_artifact_dag_replay_v6(nodes, f.store, spec_digest,
        bruce::read_logic_mod_artifact_dag_record_v6(bytes));
}
void v6_observed_cancel_never_publishes_unaccepted_ready() {
    Fixture f(false, true);
    bruce::LogicModArtifactDagControlV6 control;
    control.cancel_after_ms = 1;
    control.report_root = f.dir / "cancel-report";
    const auto run = bruce::run_logic_mod_artifact_dag_v6({f.node("a")}, f.store, spec_digest, control);
    check(!run.accepted && run.status == "DAG_HOST_CANCELLED" &&
        (run.nodes.empty() || (run.nodes.size() == 1 && !run.nodes.front().run.accepted)),
        "Observed V6 cancel accepted a node");
    check(!std::filesystem::exists(f.store / "manifests/a-out.manifest"),
        "Observed V6 cancel published an unaccepted result");
    const auto events = bruce::mod::validate_logic_mod_supervision_log_v6(control.report_root, "logic-mod-dag-v6");
    if (!events.empty()) {
        check(std::any_of(events.begin(), events.end(), [](const auto& sidecar) { return sidecar.event == "CANCEL_SENT"; }) &&
            events.back().terminal_reason == "HOST_CANCELLED", "V6 cancel sidecar reason incorrect");
        for (const auto& sidecar : events) if (sidecar.event == "CANCEL_SENT" || sidecar.event == "TERM_SENT" || sidecar.event == "KILL_SENT")
            check(sidecar.peak_memory_availability == bruce::mod::LogicModPeakMemoryAvailabilityV6::UNAVAILABLE,
                "V6 post-spawn action fabricated not-applicable peak");
    }
}
void v6_external_latch_before_admission_has_no_side_effects() {
    Fixture f;
    auto latch = std::make_shared<std::atomic_bool>(true);
    bruce::LogicModArtifactDagControlV6 control;
    control.report_root = f.dir / "latched-before-report";
    control.host_cancel_latch = latch;
    const auto run = bruce::run_logic_mod_artifact_dag_v6({f.node("a")}, f.store, spec_digest, control);
    check(!run.accepted && run.status == "DAG_HOST_CANCELLED" && run.nodes.empty(),
        "Pre-latched V6 run was admitted");
    check(!std::filesystem::exists(f.store) &&
        !std::filesystem::exists(std::filesystem::path(control.report_root.string() + ".run")),
        "Pre-latched V6 run created store or sidecar");
}
void v6_external_latch_drains_running_attempt_without_publication() {
    Fixture f(false, true);
    auto latch = std::make_shared<std::atomic_bool>(false);
    bruce::LogicModArtifactDagControlV6 control;
    control.report_root = f.dir / "latched-running-report";
    control.host_cancel_latch = latch;
    bruce::LogicModArtifactDagRunV6 run;
    std::thread caller([&] { run = bruce::run_logic_mod_artifact_dag_v6({f.node("a"), f.node("b", {"a"})}, f.store, spec_digest, control); });
    const auto sidecar = std::filesystem::path(control.report_root.string() + ".run") / "control" / "supervision-v6.jsonl";
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while ((!std::filesystem::exists(sidecar) || read(sidecar).find("ATTEMPT_STARTED") == std::string::npos) &&
        std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(std::filesystem::exists(sidecar) && read(sidecar).find("ATTEMPT_STARTED") != std::string::npos,
        "Timed V6 attempt did not start");
    latch->store(true, std::memory_order_release);
    caller.join();
    check(!run.accepted && run.status == "DAG_HOST_CANCELLED" && run.nodes.size() == 1 && !run.nodes.front().run.accepted,
        "External V6 latch did not drain active attempt as cancelled");
    check(!std::filesystem::exists(f.store / "manifests/a-out.manifest"),
        "External latch published an unaccepted result");
    check(!std::filesystem::exists(f.store / "manifests/b-out.manifest"),
        "External latch admitted a later wave");
    const auto events = bruce::mod::validate_logic_mod_supervision_log_v6(control.report_root, "logic-mod-dag-v6");
    check(std::any_of(events.begin(), events.end(), [](const auto& value) { return value.event == "CANCEL_SENT"; }) &&
        events.back().event == "ATTEMPT_FINISHED" && events.back().terminal_reason == "HOST_CANCELLED",
        "External latch sidecar did not record V6 cancellation drain");
    int status = 0; errno = 0;
    check(::waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD,
        "External latch returned before V6 reaped worker");
}
void v6_preflights_every_node_before_sidecar_or_worker() {
    Fixture f(true);
    bruce::LogicModArtifactDagControlV6 control;
    control.report_root = f.dir / "preflight-report";
    rejects([&] { (void)bruce::run_logic_mod_artifact_dag_v6({f.node("a"), f.node("b", {"a"})}, f.store, spec_digest, control); },
        "LogicWorkerRequiresStaticELF");
    check(!std::filesystem::exists(f.store) && !std::filesystem::exists(std::filesystem::path(control.report_root.string() + ".run")),
        "V6 partial preflight created Store, sidecar or worker state");
}
void v6_classifies_host_response_rejections() {
    for (const auto& [input, expected] : std::array<std::pair<const char*, const char*>, 2>{{{"malformed", "PROTOCOL_ERROR"}, {"digestbad", "DIGEST_MISMATCH"}}}) {
        Fixture f(false, true); auto node = f.node("a"); node.input = input;
        bruce::LogicModArtifactDagControlV6 control; control.report_root = f.dir / "reason-report";
        const auto run = bruce::run_logic_mod_artifact_dag_v6({node}, f.store, spec_digest, control);
        check(!run.accepted && run.nodes.size() == 1 && !run.nodes.front().run.accepted, "V6 accepted host-rejected response");
        const auto events = bruce::mod::validate_logic_mod_supervision_log_v6(control.report_root, "logic-mod-dag-v6");
        check(events.front().peak_memory_availability == bruce::mod::LogicModPeakMemoryAvailabilityV6::UNAVAILABLE &&
            events.back().terminal_reason == expected, "V6 host rejection reason/started peak incorrect");
    }
}
void v6_restores_expected_predecessor_before_fd6_handoff() {
    with_missing_predecessor([](auto&, auto&, const auto& manifest) { std::filesystem::remove(manifest); },
        [](auto& f, auto& peer, const auto& nodes) {
            bruce::LogicModArtifactDagControlV6 control; control.remote_restore = remote; control.report_root = f.dir / "restore-report";
            const auto run = bruce::run_logic_mod_artifact_dag_v6(nodes, f.store, spec_digest, control, peer.transport());
            check(run.accepted && peer.gets > 0 && std::filesystem::exists(f.store / "manifests/c-out.manifest"),
                "V6 did not restore expected predecessor before FD6 handoff");
        });
}
}
int main() {
    const std::pair<const char*,void(*)()> cases[]{
        {"local_hits_keep_v4_records_and_replay_offline",local_hits_keep_v4_records_and_replay_offline},
        {"missing_expected_predecessor_restored_before_handoff",missing_expected_predecessor_restored_before_handoff},
        {"corrupt_and_unreadable_local_targets_never_restore",corrupt_and_unreadable_local_targets_never_restore},
        {"remote_failure_and_wrong_expected_identity_never_start_successor",remote_failure_and_wrong_expected_identity_never_start_successor},
        {"disabled_remote_does_not_repair_missing_predecessor",disabled_remote_does_not_repair_missing_predecessor},
        {"invalid_remote_is_rejected_before_any_worker_or_store",invalid_remote_is_rejected_before_any_worker_or_store},
        {"v6_attempts_sidecar_and_replay_remain_record_compatible",v6_attempts_sidecar_and_replay_remain_record_compatible},
        {"v6_observed_cancel_never_publishes_unaccepted_ready",v6_observed_cancel_never_publishes_unaccepted_ready},
        {"v6_external_latch_before_admission_has_no_side_effects",v6_external_latch_before_admission_has_no_side_effects},
        {"v6_external_latch_drains_running_attempt_without_publication",v6_external_latch_drains_running_attempt_without_publication},
        {"v6_preflights_every_node_before_sidecar_or_worker",v6_preflights_every_node_before_sidecar_or_worker},
        {"v6_classifies_host_response_rejections",v6_classifies_host_response_rejections},
        {"v6_restores_expected_predecessor_before_fd6_handoff",v6_restores_expected_predecessor_before_fd6_handoff},
    };
    bool failed=false;
    for(const auto& [name,test]:cases) {
        try { test(); std::cout<<"PASS "<<name<<'\n'; }
        catch(const std::exception& e) { failed=true; std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n'; }
    }
    return failed ? 1:0;
}
