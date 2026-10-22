#ifdef BRUCE_DAG_V3_WORKER_FIXTURE
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdint>
namespace {
char input[65536], output[65536];
void take(char*& p, char* out) {
    char* end{}; const auto n = std::strtoul(p, &end, 10);
    if (*end != ':' || n > 8191) _exit(80);
    std::memcpy(out, end + 1, n); out[n] = 0; p = end + 1 + n;
}
void add(char*& p, const char* value) {
    p += std::sprintf(p, "%zu:", std::strlen(value));
    std::memcpy(p, value, std::strlen(value)); p += std::strlen(value);
}
}
std::uint64_t now() { timespec t{}; ::clock_gettime(CLOCK_MONOTONIC, &t); return static_cast<std::uint64_t>(t.tv_sec)*1000000000 + t.tv_nsec; }
int main() {
    const auto started = now();
    std::size_t count = 0; ssize_t n;
    while ((n = ::read(0, input + count, sizeof(input) - count - 1)) > 0) count += n;
    char* p = std::strchr(input, ':'); if (!p) return 80;
    ++p; if (std::strncmp(p, "BRUCE_LOGIC_REQUEST_V1\n", 23)) return 81; p += 23;
    char identity[8192], digest[8192], text[8192], budget[8192];
    take(p, identity); take(p, digest); take(p, text); take(p, budget);
    if (*p) {
        char present[8192], manifest[8192], index[8192], hash[8192], size[8192], codec[8192];
        take(p, present); take(p, manifest); take(p, index); take(p, hash); take(p, size); take(p, codec);
        char payload[65536]{};
        const auto bytes = ::read(6, payload, sizeof(payload));
        if (*p || std::strcmp(present, "1") || std::strcmp(index, "0") ||
            std::strcmp(codec, "artifact-bytes-v1") || bytes < 30 ||
            std::strncmp(payload, "BRUCE_LOGIC_MOD_DAG_INPUT_V2\n", std::strlen("BRUCE_LOGIC_MOD_DAG_INPUT_V2\n")) ||
            static_cast<unsigned long>(bytes) != std::strtoul(size, nullptr, 10)) return 82;
    }
    const bool second_result = !std::strcmp(text, "1/2 hoch 2");
    const bool fail = !std::strcmp(text, "2/3 hoch 2");
    if (!std::strcmp(text, "malformed")) { ::write(1, "3:bad", 5); return 0; }
    if (!std::strcmp(text, "digestbad")) {
        p = output; std::memcpy(p, "BRUCE_LOGIC_RESULT_V1\n", 22); p += 22;
        add(p, identity); add(p, digest); add(p, "candidate"); add(p, "rational-v1"); add(p, "9/16");
        add(p, "0000000000000000000000000000000000000000000000000000000000000000"); add(p, "");
        char prefix[32]; const auto size = std::sprintf(prefix, "%zu:", static_cast<std::size_t>(p - output));
        ::write(1, prefix, size); ::write(1, output, p - output); return 0;
    }
    if (!second_result && std::strcmp(text, "3/4 hoch 2") && !fail) return 83;
#ifdef BRUCE_DAG_V5_WORKER_FIXTURE
    const auto delay = second_result ? 500000000ULL : 220000000ULL;
#else
    const auto delay = second_result || fail ? 80000000ULL : 220000000ULL;
#endif
    while (now() - started < delay) {}
    char trace[65]; std::sprintf(trace, "%016llx%016llx%032x", static_cast<unsigned long long>(started), static_cast<unsigned long long>(now()), 0);
    p = output; std::memcpy(p, "BRUCE_LOGIC_RESULT_V1\n", 22); p += 22;
    add(p, identity); add(p, digest); add(p, "candidate"); add(p, "rational-v1"); add(p, second_result ? "1/4" : "9/16");
    add(p, second_result ? "f70b94aeb67de2a5eb4bd8c2ea85128f13776cb545a661abe422b378a6cf3099" : "31540b43fa20cb275095247f6784794cecee19c0117b76238fa153964a52af5b"); add(p, trace);
    char prefix[32]; const auto size = std::sprintf(prefix, "%zu:", static_cast<std::size_t>(p - output));
    ::write(1, prefix, size); ::write(1, output, p - output); return 0;
}
#else
#include "bruce/mod/logic_mod_artifact_dag_v3.hpp"
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
volatile sig_atomic_t child_exits = 0;
void count_child(int) { child_exits = child_exits + 1; }

void limits_and_preflight() {
    Fixture f;
    for (const auto limit : {0U, 5U, std::numeric_limits<std::uint32_t>::max()}) {
        rejects([&] { (void)bruce::run_logic_mod_artifact_dag_v3({f.node("a")},f.store,spec_digest,{limit}); }, "LogicModDagParallelLimitInvalid");
        check(!std::filesystem::exists(f.store),"Invalid parallel limit mutated Store");
    }
    Fixture late(true);
    rejects([&] { (void)bruce::run_logic_mod_artifact_dag_v3({late.node("a"),late.node("b",{"a"})},late.store,spec_digest,{4}); }, "LogicWorkerRequiresStaticELF");
    check(!std::filesystem::exists(late.store),"Late invalid ELF mutated Store");
    auto reject_graph = [&](std::vector<bruce::LogicModDagNodeV2> nodes) {
        rejects([&] { (void)bruce::run_logic_mod_artifact_dag_v3(nodes,f.store,spec_digest,{4}); });
        check(!std::filesystem::exists(f.store),"Invalid graph mutated Store");
    };
    auto a=f.node("a"), b=f.node("b",{"a"});
    reject_graph({}); reject_graph({a,a}); reject_graph({f.node("a",{"b"}),b});
    reject_graph({a,f.node("b",{"missing"})});
    auto alias=b; alias.input_artifact->manifest_id="a-out"; reject_graph({a,alias});
    auto absent=b; absent.input_artifact.reset(); reject_graph({a,absent});
    auto extra=a; extra.input_artifact=bruce::LogicModOutputV1{f.store,"extra"}; reject_graph({extra});
    auto budget=b; budget.budgets.wall_ms=0; reject_graph({a,budget});
}
void deterministic_records_and_replay() {
    Fixture f;
    // Canonical order is a,b,c,d,z; physical waves are a,z / b,c / d.
    const std::vector<bruce::LogicModDagNodeV2> nodes{f.node("z"),f.node("d",{"b","c"}),f.node("c",{"a"}),f.node("a"),f.node("b",{"a"})};
    const auto serial=bruce::run_logic_mod_artifact_dag_v2(nodes,f.store,spec_digest);
    const auto expected=bruce::logic_mod_artifact_dag_record_v2(serial);
    for (unsigned limit=1;limit<=4;++limit) {
        const auto run=bruce::run_logic_mod_artifact_dag_v3(nodes,f.store,spec_digest,{limit});
        check(run.accepted && run.node_order==std::vector<std::string>{"a","b","c","d","z"},"V3 canonical order wrong");
        check(bruce::logic_mod_artifact_dag_record_v3(run)==expected,"Parallel limit changed V2 record bytes");
        check(bruce::logic_mod_artifact_dag_record_v3(bruce::read_logic_mod_artifact_dag_record_v3(expected))==expected,"V3 record roundtrip changed bytes");
    }
    const auto defaults=bruce::run_logic_mod_artifact_dag_v3(nodes,f.store,spec_digest);
    check(bruce::logic_mod_artifact_dag_record_v3(defaults)==expected,"Default execution changed V2 record");
    struct sigaction action{}, previous{}; action.sa_handler=count_child; ::sigemptyset(&action.sa_mask);
    child_exits=0; check(::sigaction(SIGCHLD,&action,&previous)==0,"sigaction failed");
    bruce::verify_logic_mod_artifact_dag_replay_v3(nodes,f.store,spec_digest,serial);
    auto changed=serial; changed.nodes.back().run.result="bad";
    rejects([&] { bruce::verify_logic_mod_artifact_dag_replay_v3(nodes,f.store,spec_digest,changed); },"DAG_REPLAY_MISMATCH");
    const auto target=f.store/"manifests/a-out.manifest"; const auto bytes=read(target); write(target,"bad");
    rejects([&] { bruce::verify_logic_mod_artifact_dag_replay_v3(nodes,f.store,spec_digest,serial); },"DAG_REPLAY_MISMATCH");
    write(target,bytes);
    check(::sigaction(SIGCHLD,&previous,nullptr)==0 && !child_exits,"V3 replay spawned worker");
}
struct Event { std::uint64_t start,end; };
Event event(const bruce::LogicModArtifactDagRunV3& run,std::string_view id) {
    for(const auto& n:run.nodes) if(n.id==id) {
        const auto& response=n.run.response_frame;
        check(response.size()>=67 && response.substr(response.size()-67,3)=="64:","Missing worker clock events");
        return {std::stoull(response.substr(response.size()-64,16),nullptr,16),std::stoull(response.substr(response.size()-48,16),nullptr,16)};
    }
    throw bruce::Error("Missing event node");
}
void reaped() {
    int status=0; errno=0;
    check(::waitpid(-1,&status,WNOHANG)==-1 && errno==ECHILD,"DAG returned with unreaped worker");
}
void overlap_and_barriers() {
    Fixture f(false,true);
    auto a=f.node("a"); a.input="1/2 hoch 2";
    // b becomes ready as soon as short a ends; long z must still finish first.
    const auto run=bruce::run_logic_mod_artifact_dag_v3({f.node("z"),f.node("b",{"a"}),a},f.store,spec_digest,{2});
    check(run.accepted,"Timed V3 workers rejected");
    const auto ae=event(run,"a"), ze=event(run,"z"), be=event(run,"b");
    check(ae.start<ze.end && ze.start<ae.end,"Independent workers did not overlap");
    check(be.start>=ae.end && be.start>=ze.end,"Successor crossed whole-wave barrier");
    reaped();
    Fixture roots(false,true);
    const auto bounded=bruce::run_logic_mod_artifact_dag_v3({roots.node("d"),roots.node("c"),roots.node("b"),roots.node("a")},roots.store,spec_digest,{2});
    check(bounded.accepted,"Bounded roots failed");
    const auto aa=event(bounded,"a"), bb=event(bounded,"b"), cc=event(bounded,"c"), dd=event(bounded,"d");
    check(aa.start<bb.end && bb.start<aa.end,"First wave was serialized");
    check(cc.start>=aa.end && cc.start>=bb.end && dd.start>=aa.end && dd.start>=bb.end,"Parallel limit exceeded");
    reaped();
}
void failure_stops_and_reaps() {
    Fixture f(false,true);
    auto fail=f.node("a"); fail.input="2/3 hoch 2";
    const auto run=bruce::run_logic_mod_artifact_dag_v3({fail,f.node("b"),f.node("c"),f.node("d",{"a"})},f.store,spec_digest,{2});
    check(!run.accepted && run.status=="DAG_NODE_FAILED" && run.nodes.size()==2,"Failed wave did not stop graph");
    check(std::filesystem::exists(f.store/"manifests/b-out.manifest"),"Failure did not await successful peer");
    check(!std::filesystem::exists(f.store/"manifests/c-out.manifest") && !std::filesystem::exists(f.store/"manifests/d-input.manifest"),"Failure started future node");
    rejects([&] { (void)bruce::logic_mod_artifact_dag_record_v3(run); });
    check(event(run,"a").end<event(run,"b").end,"Failure fixture did not fail before its peer");
    reaped();
}
void exception_drains_started_workers() {
    Fixture f(false,true);
    auto a=f.node("a"); a.input="1/2 hoch 2";
    std::atomic<bool> attacked=false;
    // Observe real forked workers through every host thread's child list.
    // Both workers prepare their publication targets before fork, so this
    // introduces a publication exception only after the wave has started.
    std::jthread attacker([&](std::stop_token stop) {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while (!stop.stop_requested() && std::chrono::steady_clock::now()<deadline) {
            std::set<long> children;
            std::error_code ec;
            for (const auto& task:std::filesystem::directory_iterator("/proc/self/task",ec)) {
                std::ifstream input(task.path()/"children"); long pid;
                while (input>>pid) children.insert(pid);
            }
            if (children.size()==2) {
                std::filesystem::create_symlink("missing",f.store/"manifests/a-out.manifest",ec);
                attacked=!ec; return;
            }
            std::this_thread::yield();
        }
    });
    bool threw=false;
    try { (void)bruce::run_logic_mod_artifact_dag_v3({a,f.node("b"),f.node("c")},f.store,spec_digest,{2}); }
    catch (const bruce::Error&) { threw=true; }
    attacker.request_stop(); attacker.join();
    check(attacked && threw,"Failed to exercise running-wave publication exception");
    check(std::filesystem::exists(f.store/"manifests/b-out.manifest"),"Exception returned before successful peer completed");
    check(!std::filesystem::exists(f.store/"manifests/c-out.manifest"),"Exception started a future wave");
    reaped();
}
}
int main() {
    try { limits_and_preflight(); deterministic_records_and_replay(); overlap_and_barriers(); failure_stops_and_reaps(); exception_drains_started_workers(); std::cout<<"DAG V3 contracts passed\n"; return 0; }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
#endif
