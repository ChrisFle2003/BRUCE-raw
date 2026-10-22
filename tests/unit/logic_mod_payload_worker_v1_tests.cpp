#include "bruce/mod/logic_mod_worker_v1.hpp"
#include "bruce/mod/logic_mod_result_artifact_v1.hpp"
#include "bruce/artifact/artifact_store_v1.hpp"
#include "bruce/evidence/bundle.hpp"
#include <openssl/evp.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
void check(bool b, const char* why) { if (!b) throw bruce::Error(why); }
template<class F> void rejects(F fn) { try { fn(); } catch (const bruce::Error&) { return; } throw bruce::Error("Expected rejection before worker start"); }
void write(const std::filesystem::path& p, std::string_view s) { std::ofstream f(p, std::ios::binary); f << s; f.close(); check(bool(f), "write"); }
std::string read(const std::filesystem::path& p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
std::string field(std::string_view s) { return std::to_string(s.size()) + ':' + std::string(s); }
std::string member(std::string_view n, std::string_view s) { return field(n) + field(s); }
const std::string trust = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\ntest-rfc8032:d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a\n";
std::string sign(std::string_view bytes) {
    const unsigned char seed[] = {0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
    auto* key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed, sizeof(seed));
    auto* ctx = EVP_MD_CTX_new(); unsigned char out[64]; std::size_t n = 64;
    check(key && ctx && EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key) == 1 && EVP_DigestSign(ctx, out, &n, reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()) == 1, "sign");
    EVP_MD_CTX_free(ctx); EVP_PKEY_free(key);
    std::string result; for (auto b : out) { result += "0123456789abcdef"[b >> 4]; result += "0123456789abcdef"[b & 15]; } return result;
}
std::string package(std::string_view worker, bool capable = true, bool output = false) {
    const auto payload = member("tree.json", R"({"mount":[1,1],"nodes":[{"aliases":["hoch"],"claim_scope":"candidate_only","id":[1,1,5],"key":"power","route_pattern":["<rational>","hoch","<integer>"],"template_id":"math.rational.power.v1"}]})") + member("tests/golden-cases.json", R"([{"input":"3/4 hoch 2","status":"MATCHED","template_id":"math.rational.power.v1"}])");
    const auto manifest = "{\"capabilities\":[\"dataset.read\",\"tree.extend\"],\"host_abi_version\":1,\"mod_id\":\"user.logic\",\"owner_scope\":\"user\",\"payload_sha256\":\"" + bruce::sha256(payload) + "\",\"schema_version\":1,\"signing_key_id\":\"test-rfc8032\",\"token_normalization_version\":1,\"tree_schema_version\":1,\"version\":\"1.0.0\"}";
    auto data = "BRUCE_RUNTIME_DATA_MOD_V1\n" + member("manifest.json", manifest) + payload;
    data += member("signature.ed25519", sign(data)) + '\n';
    const auto logic = std::string("{\"capabilities\":[") + (capable ? "\"artifact.input.v1\"," : "") + (output ? "\"artifact.output.v1\"," : "") + "\"worker.execute\"],\"template_ids\":[\"math.rational.power.v1\"],\"worker_path\":\"worker.bin\",\"worker_sha256\":\"" + bruce::sha256(worker) + "\"}";
    auto raw = "BRUCE_LOGIC_MOD_PLUGIN_V1\n" + member("data.brucemod", data) + member("logic.json", logic) + member("worker.bin", worker);
    return raw + member("signature.ed25519", sign(raw)) + '\n';
}
struct Fixture {
    std::filesystem::path dir, root;
    std::string id;
    bruce::LogicModRegistryV1 registry;
    Fixture(const std::string& worker, bool capable = true, bool output = false) {
        char p[] = "/tmp/bruce-payload-worker-XXXXXX"; check(::mkdtemp(p), "mkdtemp"); dir = p; root = dir / "store";
        const auto bytes = read(worker); const auto raw = package(bytes, capable, output); id = bruce::sha256(raw);
        write(dir / "worker.bin", bytes); ::chmod((dir / "worker.bin").c_str(), 0500);
        registry.discover(bruce::unpack_logic_mod_package_v1(raw), dir.string());
        registry.verify(id, bruce::read_runtime_mod_truststore_v1(trust));
        registry.test_golden(id, bruce::core_tree_registry_v1()); registry.install(id); registry.activate(id);
    }
    ~Fixture() { std::filesystem::remove_all(dir); }
    auto put(std::string_view name, std::string_view bytes, std::string status = "READY") {
        auto store = bruce::artifact_v1::ArtifactStoreV1::open(root); auto id = store.put_bytes(bytes);
        store.publish_manifest(name, {"run", "node", "attempt", status, "OK", {{id, "application/octet-stream", "opaque"}}}); return id;
    }
    auto ref(std::string name = "good") { return bruce::LogicModInputV1{root, name, 0}; }
    auto run(std::string name = "good", std::string input = "3/4 hoch 2") {
        return bruce::run_logic_mod_worker_v1(registry, "user.logic", "1.0.0", "math.rational.power.v1", "worker.execute", input, {}, ref(name));
    }
    void replay(const bruce::LogicModRunV1& record, std::string name = "good") {
        bruce::verify_logic_mod_replay_v1(registry, "user.logic", "1.0.0", "math.rational.power.v1", "3/4 hoch 2", {}, record, ref(name));
    }
};
// These run the real sealed workers: any leaked descriptor or environment
// makes their startup probe fail, and each forbidden syscall must still die.
void output_contracts(const std::string& worker, bool input) {
    Fixture f(worker, input, true);
    if (input) f.put("good", "abc");
    const std::optional<bruce::LogicModInputV1> selected = input ? std::optional{f.ref()} : std::nullopt;
    const bruce::LogicModOutputV1 output{f.root, "result"};
    const auto run = [&](std::string text = "3/4 hoch 2", std::string name = "result") {
        return bruce::run_logic_mod_worker_v1(f.registry, "user.logic", "1.0.0", "math.rational.power.v1", "worker.execute",
            text, {}, selected, bruce::LogicModOutputV1{f.root, name});
    };
    const auto replay = [&](const bruce::LogicModRunV1& record) {
        bruce::verify_logic_mod_replay_v1(f.registry, "user.logic", "1.0.0", "math.rational.power.v1", "3/4 hoch 2", {}, record, selected, output);
    };
    const auto good = run();
    check(good.accepted && good.output_present && good.output_manifest_id == "result" && good.output_object_index == 0 &&
        good.output_sha256 == "31540b43fa20cb275095247f6784794cecee19c0117b76238fa153964a52af5b" &&
        good.output_size_bytes == 4 && good.output_codec == "logic-mod-result-text-v1", "output publication evidence missing");
    auto store = bruce::artifact_v1::ArtifactStoreV1::open_existing(f.root);
    check(store.read_manifest("result").objects.size() == 1, "not a single result object");
    const auto object_path = f.root / "objects/31" / (good.output_sha256 + ".blob");
    check(read(object_path) == "9/16", "published private worker frame");
    const auto record = bruce::logic_mod_run_record_v1(good);
    check(record.ends_with("9:output.v11:16:result1:064:31540b43fa20cb275095247f6784794cecee19c0117b76238fa153964a52af5b1:424:logic-mod-result-text-v1"), "noncanonical output suffix");
    const auto without_output = bruce::run_logic_mod_worker_v1(f.registry, "user.logic", "1.0.0", "math.rational.power.v1", "worker.execute",
        "3/4 hoch 2", {}, selected);
    check(without_output.accepted && record == bruce::logic_mod_run_record_v1(without_output) +
        "9:output.v11:16:result1:064:31540b43fa20cb275095247f6784794cecee19c0117b76238fa153964a52af5b1:424:logic-mod-result-text-v1", "output changed worker request or old record bytes");
    check(record.find(f.root.string()) == std::string::npos, "output path leaked");
    replay(bruce::read_logic_mod_run_record_v1(record));
    check(bruce::logic_mod_run_record_v1(run()) == record, "idempotent run changed bytes");
    rejects([&] { bruce::verify_logic_mod_replay_v1(f.registry, "user.logic", "1.0.0", "math.rational.power.v1", "3/4 hoch 2", {}, good, selected); });
    for (int i = 0; i != 7; ++i) {
        auto changed = good;
        switch (i) {
        case 0: changed.output_present = false; break;
        case 1: changed.output_manifest_id = "alias"; break;
        case 2: changed.output_object_index = 1; break;
        case 3: changed.output_sha256[0] = '0'; break;
        case 4: changed.output_size_bytes = 5; break;
        case 5: changed.output_codec = "other"; break;
        case 6: changed.accepted = false; break;
        }
        rejects([&] { replay(changed); });
    }
    rejects([&] { (void)bruce::read_logic_mod_run_record_v1(record + "1:0"); });
    const auto manifest_path = f.root / "manifests/result.manifest";
    const auto manifest = read(manifest_path);
    for (const auto* text : {"wrong", "malformed", "timeout", "crash", "output", "filesystem", "network", "fork"}) {
        const auto failed = run(text, text);
        check(!failed.accepted && !failed.output_present && failed.output_manifest_id.empty() && failed.output_sha256.empty() &&
            failed.output_codec.empty() && failed.output_object_index == 0 && failed.output_size_bytes == 0, "failed run retained output evidence");
        check(!std::filesystem::exists(f.root / "manifests" / (std::string(text) + ".manifest")), "failed run published output");
        if (std::string_view(text) == "filesystem" || std::string_view(text) == "network" || std::string_view(text) == "fork")
            check(failed.status == "WORKER_SIGNAL_31", "output capability widened Seccomp");
        if (std::string_view(text) == "timeout") check(failed.status == "TIMEOUT", "timeout fixture failed");
        if (std::string_view(text) == "output") check(failed.status == "OUTPUT_LIMIT", "output limit fixture failed");
    }
    if (input) for (const auto* text : {"payload-write", "payload-mmap"}) {
        const auto failed = run(text, text);
        check(!failed.accepted && !failed.output_present && failed.status == "WORKER_SIGNAL_31", "output widened input-FD rights");
        check(!std::filesystem::exists(f.root / "manifests" / (std::string(text) + ".manifest")), "FD violation published output");
    }
    rejects([&] { (void)run("3/4 hoch 2", "../bad"); });
    f.put("conflict", "abc"); const auto conflict = read(f.root / "manifests/conflict.manifest");
    rejects([&] { (void)run("3/4 hoch 2", "conflict"); });
    check(read(f.root / "manifests/conflict.manifest") == conflict, "output overwrote manifest");
    write(object_path, "1/16"); rejects([&] { replay(good); });
    std::filesystem::remove(object_path); rejects([&] { replay(good); }); write(object_path, "9/16");
    check(::chmod(object_path.c_str(), 0600) == 0, "restore object permissions");
    std::filesystem::remove(manifest_path); rejects([&] { replay(good); }); write(manifest_path, manifest);
    check(::chmod(manifest_path.c_str(), 0600) == 0, "restore manifest permissions");
    // Exact metadata matters even when digest/size and canonical result match.
    for (int i = 0; i != 2; ++i) {
        auto changed = store.read_manifest("result");
        if (i == 0) changed.status = "FAILED"; else changed.objects[0].media_type = "other";
        write(manifest_path, bruce::artifact_v1::encode_manifest_v1(changed));
        rejects([&] { replay(good); }); write(manifest_path, manifest);
    }
    // A child cannot fork under this installed filter. Successful replay under
    // it is a positive proof that no worker is spawned (not merely no zombie).
    const auto child = ::fork(); check(child >= 0, "fork replay probe");
    if (child == 0) {
        const sock_filter filter[] = {
            BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, nr)),
            BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_clone, 0, 1), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#ifdef SYS_clone3
            BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_clone3, 0, 1), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
#ifdef SYS_fork
            BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_fork, 0, 1), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
#ifdef SYS_vfork
            BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_vfork, 0, 1), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
            BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)
        };
        const sock_fprog program{static_cast<unsigned short>(std::size(filter)), const_cast<sock_filter*>(filter)};
        if (::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) || ::prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program)) _exit(90);
        try { replay(good); _exit(0); } catch (...) { _exit(91); }
    }
    int status; check(::waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0, "replay spawned worker");
    f.registry.disable(f.id); rejects([&] { (void)run(); }); rejects([&] { replay(good); });
    f.registry.activate(f.id); f.registry.revoke(f.id); rejects([&] { (void)run(); }); rejects([&] { replay(good); });
    Fixture denied(worker, input);
    if (input) denied.put("good", "abc");
    const auto absent = denied.dir / "absent-store";
    rejects([&] { (void)bruce::run_logic_mod_worker_v1(denied.registry, "user.logic", "1.0.0", "math.rational.power.v1", "worker.execute",
        "3/4 hoch 2", {}, input ? std::optional{denied.ref()} : std::nullopt, bruce::LogicModOutputV1{absent, "denied"}); });
    check(!std::filesystem::exists(absent), "missing output right touched Store");
    Fixture escape(BRUCE_LOGIC_EXEC_ESCAPE_WORKER, false, true);
    const auto escaped = bruce::run_logic_mod_worker_v1(escape.registry, "user.logic", "1.0.0", "math.rational.power.v1", "worker.execute",
        "3/4 hoch 2", {}, std::nullopt, bruce::LogicModOutputV1{escape.root, "escape"});
    check(escaped.status == "EXEC_DENIED" && !escaped.accepted && !escaped.output_present &&
        !std::filesystem::exists(escape.root / "manifests/escape.manifest"), "output widened exec gate");
}
void contracts(const std::string& worker) {
    // Missing output-aware parsing must reject this previously unknown suffix.
    // Keep the accepted legacy record prefix independently constructed.
    const auto output_record = "BRUCE_LOGIC_RUN_V1\n" + field("1") + field("ACCEPTED") + field("9/16") +
        field("rational-power-v1") + field("candidate_only") + field("binding") + field("3/4 hoch 2") +
        field("budget") + field("request") + field("response") + field("frame") +
        field("output.v1") + field("1") + field("result") + field("0") +
        field("31540b43fa20cb275095247f6784794cecee19c0117b76238fa153964a52af5b") + field("4") + field("logic-mod-result-text-v1");
    check(bruce::logic_mod_run_record_v1(bruce::read_logic_mod_run_record_v1(output_record)) == output_record, "output record unsupported");
    Fixture f(worker); const auto id = f.put("good", "abc");
    const auto good = f.run();
    if (!good.accepted) throw bruce::Error("Payload worker rejected: " + good.status);
    check(good.result == "9/16" && good.input_present && good.input_manifest_id == "good" && good.input_object_index == 0 && good.input_size_bytes == 3 && good.input_codec == "artifact-bytes-v1" && good.input_sha256 == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "payload evidence missing");
    const auto record = bruce::logic_mod_run_record_v1(good);
    check(record.ends_with("1:14:good1:064:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad1:317:artifact-bytes-v1"), "canonical payload record changed");
    const auto legacy_input = "BRUCE_LOGIC_RUN_V1\n" + field("1") + field("ACCEPTED") + field("9/16") + field("rational-power-v1") +
        field("candidate_only") + field(good.binding_identity) + field("3/4 hoch 2") + field("3:2508:671088644:40965:16384") +
        field(good.request_digest) + field(good.response_digest) + field(good.response_frame) +
        "1:14:good1:064:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad1:317:artifact-bytes-v1";
    check(record == legacy_input, "input-only V1 record bytes changed");
    check(record.find(f.root.string()) == std::string::npos, "store path leaked");
    f.replay(bruce::read_logic_mod_run_record_v1(record));
    rejects([&] { (void)bruce::read_logic_mod_run_record_v1(record + "1:0"); });
    rejects([&] { (void)bruce::read_logic_mod_run_record_v1(record + "0:"); });
    auto noncanonical = record; noncanonical.replace(noncanonical.rfind("1:317:"), 3, "2:03");
    rejects([&] { (void)bruce::read_logic_mod_run_record_v1(noncanonical); });
    rejects([&] { bruce::verify_logic_mod_replay_v1(f.registry, "user.logic", "1.0.0", "math.rational.power.v1", "3/4 hoch 2", {}, good); });
    f.put("alias", "abc"); rejects([&] { f.replay(good, "alias"); });
    for (const auto* name : {"input_manifest_id", "input_sha256", "input_codec"}) {
        auto changed = good;
        if (std::string_view(name) == "input_manifest_id") changed.input_manifest_id = "alias";
        else if (std::string_view(name) == "input_sha256") changed.input_sha256[0] = '0';
        else changed.input_codec = "other";
        rejects([&] { f.replay(changed); });
    }
    auto changed = good; changed.input_size_bytes++; rejects([&] { f.replay(changed); });
    changed = good; changed.input_object_index++; rejects([&] { f.replay(changed); });
    changed = good; changed.input_present = false; rejects([&] { f.replay(changed); });
    for (const auto* input : {"payload-write", "payload-mmap", "filesystem", "network", "fork"}) {
        const auto bad = f.run("good", input);
        check(!bad.accepted && bad.result.empty() && bad.status == "WORKER_SIGNAL_31", "payload sandbox operation allowed");
    }
    f.put("empty", ""); check(f.run("empty").accepted, "empty payload rejected");
    f.put("limit", std::string(1024*1024, 'x')); check(f.run("limit").accepted, "1 MiB input rejected");
    f.put("oversize", std::string(1024*1024+1, 'x')); rejects([&] { (void)f.run("oversize"); });
    f.put("failed", "abc", "FAILED"); rejects([&] { (void)f.run("failed"); });
    const auto manifest_path = f.root / "manifests" / "good.manifest";
    const auto manifest_bytes = read(manifest_path);
    write(manifest_path, read(f.root / "manifests" / "failed.manifest"));
    rejects([&] { f.replay(good); });
    write(manifest_path, read(f.root / "manifests" / "limit.manifest"));
    rejects([&] { f.replay(good); });
    write(manifest_path, manifest_bytes);
    rejects([&] { (void)f.run("missing"); });
    const auto path = f.root / "objects" / id.sha256.substr(0, 2) / (id.sha256 + ".blob");
    write(path, "xyz"); rejects([&] { f.replay(good); }); rejects([&] { (void)f.run(); });
    std::filesystem::remove(path); rejects([&] { f.replay(good); });
    f.registry.disable(f.id); rejects([&] { f.replay(good); }); rejects([&] { (void)f.run(); });
    Fixture denied(worker, false); rejects([&] { (void)denied.run(); }); check(!std::filesystem::exists(denied.root), "denied run touched missing store");
    Fixture escape(BRUCE_LOGIC_EXEC_ESCAPE_WORKER); escape.put("good", "abc");
    const auto escaped = escape.run();
    check(!escaped.accepted && escaped.result.empty() && escaped.status == "EXEC_DENIED", "payload bypassed single-exec authorization");
    int status; check(::waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD, "unreaped worker");
}
}
int main(int argc, char** argv) {
    try {
        check(argc == 2 || argc == 4, "worker path required");
        if (argc == 4) {
            const std::filesystem::path root(argv[2]); write(root / "payload.brucemod", package(read(argv[1]))); write(root / "trust.txt", trust);
            write(root / "combined.brucemod", package(read(argv[1]), true, true));
            write(root / "output.brucemod", package(read(BRUCE_LOGIC_PLAIN_WORKER), false, true));
            write(root / "legacy.brucemod", package(read(BRUCE_LOGIC_PLAIN_WORKER), false));
            auto store = bruce::artifact_v1::ArtifactStoreV1::open(root / "store");
            for (const auto& [name, bytes] : {std::pair{"good", std::string("abc")}, {"limit", std::string(1024*1024, 'x')}, {"oversize", std::string(1024*1024+1, 'x')}}) {
                const auto id = store.put_bytes(bytes); store.publish_manifest(name, {"run", "node", "attempt", "READY", "OK", {{id, "application/octet-stream", "opaque"}}});
            }
            return 0;
        }
        contracts(argv[1]); output_contracts(argv[1], true); output_contracts(BRUCE_LOGIC_PLAIN_WORKER, false);
        std::cout << "payload/output worker contracts passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
