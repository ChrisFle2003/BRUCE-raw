#ifdef BRUCE_CHAIN_WORKER_FIXTURE
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
int main() {
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
    if (!second_result && std::strcmp(text, "3/4 hoch 2")) return 83;
    p = output; std::memcpy(p, "BRUCE_LOGIC_RESULT_V1\n", 22); p += 22;
    add(p, identity); add(p, digest); add(p, "candidate"); add(p, "rational-v1"); add(p, second_result ? "1/4" : "9/16");
    add(p, second_result ? "f70b94aeb67de2a5eb4bd8c2ea85128f13776cb545a661abe422b378a6cf3099" : "31540b43fa20cb275095247f6784794cecee19c0117b76238fa153964a52af5b"); add(p, "");
    char prefix[32]; const auto size = std::sprintf(prefix, "%zu:", static_cast<std::size_t>(p - output));
    ::write(1, prefix, size); ::write(1, output, p - output); return 0;
}
#else
#include "bruce/mod/logic_mod_artifact_dag_v2.hpp"
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
    Fixture(bool invalid_child = false) {
        char pattern[] = "/tmp/bruce-dag-XXXXXX"; check(::mkdtemp(pattern), "mkdtemp failed"); dir = pattern; store = dir / "store";
        for (unsigned i = 0; i != 2; ++i) {
            const auto sub = dir / (i ? "child" : "root"); std::filesystem::create_directory(sub); ::chmod(sub.c_str(), 0700);
            const auto bytes = i && invalid_child ? std::string("signed invalid executable") : read(BRUCE_DAG_WORKER);
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
void preflight() {
    Fixture f;
    auto a = f.node("a"); auto b = f.node("b", {"a"});
    auto reject_graph = [&](std::vector<bruce::LogicModDagNodeV2> nodes) {
        rejects([&] { (void)bruce::run_logic_mod_artifact_dag_v2(nodes, f.store, spec_digest); });
        check(!std::filesystem::exists(f.store), "Invalid graph mutated Store");
    };
    reject_graph({}); reject_graph({a,a});
    auto dangling = b; dangling.predecessors = {"missing"}; reject_graph({a,dangling});
    auto cycle = f.node("a", {"b"}); reject_graph({cycle,b});
    auto alias = b; alias.input_artifact->manifest_id = "a-out"; reject_graph({a,alias});
    auto absent = b; absent.input_artifact.reset(); reject_graph({a,absent});
    auto extra = a; extra.input_artifact = bruce::LogicModOutputV1{f.store,"extra"}; reject_graph({extra});
    auto invalid = a; invalid.id = "../bad"; reject_graph({invalid});
    auto unsorted = f.node("c", {"b","a"}); reject_graph({a,f.node("b"),unsorted});
    auto duplicate = f.node("c", {"a","a"}); reject_graph({a,duplicate});
    auto budget = b; budget.budgets.wall_ms = 0; reject_graph({a,budget});
    auto capabilities = b; capabilities.predecessors.clear(); capabilities.input_artifact.reset(); reject_graph({capabilities});
    Fixture late(true);
    struct sigaction action{}, previous{}; action.sa_handler = count_child; ::sigemptyset(&action.sa_mask);
    child_exits = 0; check(::sigaction(SIGCHLD,&action,&previous)==0,"sigaction failed");
    rejects([&] { (void)bruce::run_logic_mod_artifact_dag_v2({late.node("a"),late.node("b",{"a"})},late.store,spec_digest); },"LogicWorkerRequiresStaticELF");
    check(::sigaction(SIGCHLD,&previous,nullptr)==0 && !child_exits,"Late ELF started worker");
    check(!std::filesystem::exists(late.store),"Late ELF mutated Store");
}
std::string expected_input(const bruce::LogicModArtifactDagRunV2& run, const std::vector<std::string>& ids) {
    std::string out = "BRUCE_LOGIC_MOD_DAG_INPUT_V2\n" + field("2") + field(std::to_string(ids.size()));
    for (const auto& id : ids) for (const auto& n : run.nodes) if (n.id == id) {
        const auto& p = n.result_artifact;
        out += field(id)+field(p.manifest_id)+field(p.sha256)+field(std::to_string(p.size_bytes))+field(p.codec)+field(n.run.result);
    }
    return out + '\n';
}
void contracts() {
    Fixture f;
    auto nodes = std::vector<bruce::LogicModDagNodeV2>{f.node("d",{"b","c"}), f.node("c",{"a"}), f.node("b",{"a"}), f.node("a")};
    auto r = bruce::run_logic_mod_artifact_dag_v2(nodes,f.store,spec_digest);
    check(r.accepted && r.node_order == std::vector<std::string>{"a","b","c","d"},"DAG ordering/success wrong");
    for (const auto& n : r.nodes) {
        check(n.run.accepted && n.run.result=="9/16", "Wrong node result");
        if (!n.internal_input_artifact) continue;
        auto payload = bruce::resolve_logic_mod_payload_v1(f.child.bind("user.logic","1.0.0","math.rational.power.v1","artifact.input.v1"), f.store,n.internal_input_artifact->manifest_id,0);
        std::string bytes(payload.size_bytes(),' '); check(::read(payload.fd(),bytes.data(),bytes.size())==static_cast<ssize_t>(bytes.size()),"Payload read failed");
        check(bytes == expected_input(r,n.predecessors),"Predecessor bytes/order/ancestor wrong");
    }
    auto tri = std::vector<bruce::LogicModDagNodeV2>{f.node("z",{"x","y"}), f.node("y"),f.node("x")};
    check(bruce::run_logic_mod_artifact_dag_v2(tri,f.store,spec_digest).accepted,"Three node diamond failed");
    const auto record = bruce::logic_mod_artifact_dag_record_v2(r);
    check(record.find(f.dir.string())==std::string::npos,"Record leaked path");
    check(bruce::logic_mod_artifact_dag_record_v2(bruce::read_logic_mod_artifact_dag_record_v2(record))==record,"Record roundtrip failed");
    rejects([&] { (void)bruce::read_logic_mod_artifact_dag_record_v2(record+"x"); });
    rejects([&] { (void)bruce::read_logic_mod_artifact_dag_record_v2(std::string(bruce::logic_mod_artifact_dag_record_limit_v2+1,'x')); });
    struct sigaction action{}, previous{}; action.sa_handler=count_child; ::sigemptyset(&action.sa_mask);
    child_exits=0; check(::sigaction(SIGCHLD,&action,&previous)==0,"sigaction failed");
    auto replay = [&](const auto& run) { bruce::verify_logic_mod_artifact_dag_replay_v2(nodes,f.store,spec_digest,run); };
    replay(r);
    auto changed = r; changed.spec_digest[0]='b'; rejects([&] { replay(changed); },"DAG_REPLAY_MISMATCH");
    changed = r; changed.nodes.back().run.result="1"; rejects([&] { replay(changed); },"DAG_REPLAY_MISMATCH");
    changed = r; changed.nodes.back().predecessors={"a"}; rejects([&] { replay(changed); },"DAG_REPLAY_MISMATCH");
    for (const auto& n : r.nodes) {
        std::vector<bruce::LogicModPublishedOutputV1> outputs{n.result_artifact};
        if (n.internal_input_artifact) outputs.push_back(*n.internal_input_artifact);
        for (const auto& p : outputs) {
            const auto target=f.store/"manifests"/(p.manifest_id+".manifest"); const auto original=read(target);
            write(target,"bad"); rejects([&] { replay(r); },"DAG_REPLAY_MISMATCH"); write(target,original);
            const auto object=f.store/"objects"/p.sha256.substr(0,2)/(p.sha256+".blob"); const auto bytes=read(object);
            write(object,"bad"); rejects([&] { replay(r); },"DAG_REPLAY_MISMATCH"); write(object,bytes);
        }
    }
    replay(r); check(::sigaction(SIGCHLD,&previous,nullptr)==0 && !child_exits,"Replay spawned worker");
    Fixture fail; auto failing=fail.node("a"); failing.input="fail";
    const auto failed=bruce::run_logic_mod_artifact_dag_v2({failing,fail.node("b",{"a"})},fail.store,spec_digest);
    check(!failed.accepted && failed.nodes.size()==1,"Failure did not stop graph");
    check(!std::filesystem::exists(fail.store/"manifests/b-input.manifest"),"Failure published successor input");
    rejects([&] { (void)bruce::logic_mod_artifact_dag_record_v2(failed); });
}
}
int main(int argc,char**) {
    try { preflight(); if(argc==1) contracts(); std::cout<<"DAG contracts passed\n"; return 0; }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
#endif
