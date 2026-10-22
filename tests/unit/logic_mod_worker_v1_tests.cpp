#include "bruce/mod/logic_mod_worker_v1.hpp"
#include "logic_mod_worker_v1_internal.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include <openssl/evp.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
void check(bool b, const char* s) { if (!b) throw bruce::Error(s); }
template<class F> void rejects(F f) { try { f(); } catch (const bruce::Error&) { return; } throw bruce::Error("Expected rejection"); }
std::string read(const std::string& p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
void write(const std::string& p, std::string_view s) { std::ofstream f(p, std::ios::binary); f << s; f.close(); check(bool(f), "write failed"); }
std::string field(std::string_view s) { return std::to_string(s.size()) + ':' + std::string(s); }
std::string member(std::string_view n, std::string_view s) { return field(n) + field(s); }
const std::string trust_text = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\ntest-rfc8032:d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a\n";
std::string sign(std::string_view s) {
    const unsigned char seed[] = {0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
    auto* key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed, sizeof(seed));
    auto* ctx = EVP_MD_CTX_new(); unsigned char out[64]; std::size_t n = 64;
    check(key && ctx && EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key) == 1 && EVP_DigestSign(ctx, out, &n, reinterpret_cast<const unsigned char*>(s.data()), s.size()) == 1, "sign failed");
    EVP_MD_CTX_free(ctx); EVP_PKEY_free(key);
    std::string r; for (auto b : out) { r += "0123456789abcdef"[b >> 4]; r += "0123456789abcdef"[b & 15]; } return r;
}
std::string package(std::string_view worker) {
    const std::string tree = R"({"mount":[1,1],"nodes":[{"aliases":["hoch"],"claim_scope":"candidate_only","id":[1,1,5],"key":"power","route_pattern":["<rational>","hoch","<integer>"],"template_id":"math.rational.power.v1"}]})";
    auto payload = member("tree.json", tree) + member("tests/golden-cases.json", R"([{"input":"3/4 hoch 2","status":"MATCHED","template_id":"math.rational.power.v1"}])");
    auto manifest = "{\"capabilities\":[\"dataset.read\",\"tree.extend\"],\"host_abi_version\":1,\"mod_id\":\"user.logic\",\"owner_scope\":\"user\",\"payload_sha256\":\"" + bruce::sha256(payload) + "\",\"schema_version\":1,\"signing_key_id\":\"test-rfc8032\",\"token_normalization_version\":1,\"tree_schema_version\":1,\"version\":\"1.0.0\"}";
    auto data = "BRUCE_RUNTIME_DATA_MOD_V1\n" + member("manifest.json", manifest) + payload;
    data += member("signature.ed25519", sign(data)) + '\n';
    auto logic = "{\"capabilities\":[\"worker.execute\"],\"template_ids\":[\"math.rational.power.v1\"],\"worker_path\":\"worker.bin\",\"worker_sha256\":\"" + bruce::sha256(worker) + "\"}";
    auto raw = "BRUCE_LOGIC_MOD_PLUGIN_V1\n" + member("data.brucemod", data) + member("logic.json", logic) + member("worker.bin", worker);
    return raw + member("signature.ed25519", sign(raw)) + '\n';
}
struct Fixture {
    std::string dir, bytes, raw, id;
    bruce::LogicModRegistryV1 registry;
    Fixture(const std::string& worker) : bytes(read(worker)), raw(package(bytes)), id(bruce::sha256(raw)) {
        char p[] = "/tmp/bruce-logic-worker-test-XXXXXX"; check(::mkdtemp(p), "mkdtemp"); dir = p;
        write(dir + "/worker.bin", bytes); ::chmod((dir + "/worker.bin").c_str(), 0500);
        registry.discover(bruce::unpack_logic_mod_package_v1(raw), dir);
        registry.verify(id, bruce::read_runtime_mod_truststore_v1(trust_text));
        registry.test_golden(id, bruce::core_tree_registry_v1()); registry.install(id);
    }
    ~Fixture() { std::filesystem::remove_all(dir); }
    bruce::LogicModRunV1 run(std::string input = "3/4 hoch 2", bruce::LogicModBudgetsV1 budgets = {}) {
        return bruce::run_logic_mod_worker_v1(registry, "user.logic", "1.0.0", "math.rational.power.v1", "worker.execute", input, budgets);
    }
};
void reaped() { int status; errno = 0; check(::waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD, "Child not reaped"); }
void contracts(const std::string& worker) {
    Fixture escape(BRUCE_LOGIC_EXEC_ESCAPE_WORKER); escape.registry.activate(escape.id);
    const auto escaped = escape.run();
    check(!escaped.accepted && escaped.result.empty() && escaped.status == "EXEC_DENIED", "Absolute execveat escaped sealed worker binding");
    reaped();
    Fixture f(worker);
    rejects([&] { (void)f.run(); }); reaped();
    f.registry.activate(f.id);
    const auto good = f.run(" 3/4  HOCH 2 ");
    if (!good.accepted || good.status != "ACCEPTED" || good.result != "9/16") throw bruce::Error("Valid worker rejected: " + good.status + " response=" + good.response_frame);
    check(good.claim_scope == "candidate_only" && good.validator == "rational-power-v1", "Host authority missing");
    check(good.request_digest.size() == 64 && good.response_digest.size() == 64, "Evidence incomplete");
    // The private V6 preparation/finalization seam must be observationally
    // identical to the established synchronous V1 execution.  This pins the
    // exact request/response transcript rather than merely the math result.
    auto prepared = bruce::logic_mod_worker_v1_internal::prepare_logic_mod_execution_v6(
        f.registry, "user.logic", "1.0.0", "math.rational.power.v1", "worker.execute",
        " 3/4  HOCH 2 ", {});
    auto direct_attempt = bruce::logic_mod_worker_v1_internal::start_logic_mod_execution_v6(prepared);
    const auto direct = bruce::logic_mod_worker_v1_internal::finalize_logic_mod_execution_v6(
        std::move(prepared), bruce::logic_mod_worker_v1_internal::finish_logic_mod_worker_attempt_v6(direct_attempt));
    check(bruce::logic_mod_run_record_v1(direct) == bruce::logic_mod_run_record_v1(good),
        "V6 private execution seam changed V1 transcript bytes");
    reaped();
    // V1 preflights the sealed worker before request normalization. Keep this
    // ordering stable when both inputs are invalid.
    const auto non_elf_path = f.dir + "/non-elf-worker.bin";
    write(non_elf_path, "not an elf");
    Fixture non_elf(non_elf_path); non_elf.registry.activate(non_elf.id);
    try {
        (void)non_elf.run(std::string(1, static_cast<char>(0xff)));
        throw bruce::Error("Expected static ELF preflight rejection");
    } catch (const bruce::Error& e) {
        check(std::string_view(e.what()) == "LogicWorkerRequiresStaticELF", "Static ELF preflight lost V1 error precedence");
    }
    bruce::verify_logic_mod_replay_v1(f.registry, "user.logic", "1.0.0", "math.rational.power.v1", "3/4 hoch 2", {}, good);
    auto changed = good; changed.result = "1";
    rejects([&] { bruce::verify_logic_mod_replay_v1(f.registry, "user.logic", "1.0.0", "math.rational.power.v1", "3/4 hoch 2", {}, changed); });
    rejects([&] { bruce::verify_logic_mod_replay_v1(f.registry, "user.logic", "1.0.0", "math.rational.power.v1", "3/4 hoch 3", {}, good); });
    reaped();
    const std::pair<const char*, const char*> cases[] = {
        {"malformed", "LogicFrameInvalid"}, {"oversized", "LogicFrameInvalid"}, {"mismatch", "LogicResponseIdentityMismatch"},
        {"digest", "LogicResultDigestInvalid"}, {"claim", "LogicFrameTrailingBytes"}, {"codec", "LogicHostValidationFailed"},
        {"wrong", "LogicHostValidationFailed"}, {"3/4 hoch 3", "LogicHostValidationFailed"}, {"trailing", "LogicFrameTrailingBytes"},
        {"crash", "WORKER_SIGNAL_31"}, {"timeout", "TIMEOUT"}, {"output", "OUTPUT_LIMIT"}, {"stderr", "OUTPUT_LIMIT"},
        {"memory", "WORKER_EXIT_91"}, {"network", "WORKER_SIGNAL_31"}, {"filesystem", "WORKER_SIGNAL_31"}, {"fork", "WORKER_SIGNAL_31"},
        {"unknown-syscall", "WORKER_SIGNAL_31"}
    };
    for (const auto& [input, expected] : cases) {
        const auto failed = f.run(input);
        if (failed.accepted || !failed.result.empty()) throw bruce::Error(std::string("Accepted hostile case: ") + input);
        if (failed.status != expected) throw bruce::Error(std::string(input) + " expected " + expected + " got " + failed.status);
        reaped();
    }
    auto record = bruce::logic_mod_run_record_v1(good);
    // Independently retain the original V1 wire layout and digest preimage.
    const auto legacy = "BRUCE_LOGIC_RUN_V1\n" + field("1") + field("ACCEPTED") + field("9/16") + field("rational-power-v1") + field("candidate_only") +
        field(good.binding_identity) + field("3/4 hoch 2") + field("3:2508:671088644:40965:16384") + field(good.request_digest) + field(good.response_digest) + field(good.response_frame);
    check(record == legacy, "Inputfree V1 record bytes changed");
    check(good.request_digest == bruce::sha256("BRUCE_LOGIC_REQUEST_DIGEST_V1\n" + field(good.binding_identity) + field("3/4 hoch 2") + field("3:2508:671088644:40965:16384")), "Inputfree request bytes changed");
    check(bruce::logic_mod_run_record_v1(bruce::read_logic_mod_run_record_v1(record)) == record, "Transcript roundtrip failed");
    rejects([&] { (void)bruce::read_logic_mod_run_record_v1(record + "x"); });
    changed = good; changed.claim_scope = "proven";
    rejects([&] { bruce::verify_logic_mod_replay_v1(f.registry, "user.logic", "1.0.0", "math.rational.power.v1", "3/4 hoch 2", {}, changed); });
    bruce::LogicModBudgetsV1 different; different.wall_ms = 500;
    rejects([&] { bruce::verify_logic_mod_replay_v1(f.registry, "user.logic", "1.0.0", "math.rational.power.v1", "3/4 hoch 2", different, good); });
    bruce::LogicModBudgetsV1 budget; budget.wall_ms = 0; rejects([&] { (void)f.run("3/4 hoch 2", budget); });
    budget = {}; budget.input_bytes = 2; rejects([&] { (void)f.run("3/4 hoch 2", budget); });
    rejects([&] { (void)bruce::run_logic_mod_worker_v1(f.registry, "user.logic", "1.0.0", "other", "worker.execute", "3/4 hoch 2", {}); });
    rejects([&] { (void)bruce::run_logic_mod_worker_v1(f.registry, "user.logic", "1.0.0", "math.rational.power.v1", "network", "3/4 hoch 2", {}); });
    f.registry.disable(f.id); rejects([&] { (void)f.run(); });
    f.registry.activate(f.id); f.registry.revoke(f.id); rejects([&] { (void)f.run(); }); reaped();
    Fixture tamper(worker); tamper.registry.activate(tamper.id);
    ::chmod((tamper.dir + "/worker.bin").c_str(), 0700); write(tamper.dir + "/worker.bin", "tampered");
    rejects([&] { (void)tamper.run(); }); reaped();
}
}
int main(int argc, char** argv) {
    try {
        check(argc == 2 || argc == 4, "worker path required");
        if (argc == 4) { write(std::string(argv[2]) + "/demo.brucemod", package(read(argv[1]))); write(std::string(argv[2]) + "/trust.txt", trust_text); return 0; }
        contracts(argv[1]); std::cout << "logic worker contracts passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
