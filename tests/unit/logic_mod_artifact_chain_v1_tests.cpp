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
        char payload[5]{};
        if (*p || std::strcmp(present, "1") || std::strcmp(index, "0") ||
            std::strcmp(size, "4") || std::strcmp(codec, "artifact-bytes-v1") ||
            ::read(6, payload, sizeof(payload)) != 4 || std::strcmp(payload, "9/16")) return 82;
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
#include "bruce/mod/logic_mod_artifact_chain_v1.hpp"
#include "bruce/evidence/bundle.hpp"
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
    bruce::LogicModRegistryV1 a, b;
    Fixture(std::string_view first_caps = a_caps, std::string_view second_caps = b_caps, bool escape = false, bool invalid_second = false,
            std::string_view first_override = {}, std::string_view second_override = {}) {
        char pattern[] = "/tmp/bruce-chain-XXXXXX"; check(::mkdtemp(pattern), "mkdtemp failed"); dir = pattern; store = dir / "store";
        for (unsigned i = 0; i != 2; ++i) {
            const auto sub = dir / (i ? "b" : "a"); std::filesystem::create_directory(sub); check(::chmod(sub.c_str(), 0700) == 0, "directory chmod failed");
            const auto override = i ? second_override : first_override;
            const auto bytes = !override.empty() ? std::string(override) : i && invalid_second ? std::string("signed invalid executable") : read(escape ? BRUCE_CHAIN_ESCAPE_WORKER : BRUCE_CHAIN_WORKER);
            write(sub / "worker.bin", bytes); check(::chmod((sub / "worker.bin").c_str(), 0500) == 0, "chmod failed");
            const auto raw = package(bytes, i ? second_caps : first_caps); const auto id = bruce::sha256(raw);
            auto& registry = i ? b : a;
            registry.discover(bruce::unpack_logic_mod_package_v1(raw), sub.string());
            registry.verify(id, bruce::read_runtime_mod_truststore_v1("BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\ntest-rfc8032:d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a\n"));
            registry.test_golden(id, bruce::core_tree_registry_v1()); registry.install(id); registry.activate(id);
        }
    }
    ~Fixture() { std::filesystem::remove_all(dir); }
    bruce::LogicModChainStepV1 first() { return {a, "user.logic", "1.0.0", "math.rational.power.v1", "3/4 hoch 2", {store, "first"}}; }
    bruce::LogicModChainStepV1 second() { return {b, "user.logic", "1.0.0", "math.rational.power.v1", "1/2 hoch 2", {store, "second"}}; }
    auto run() { return bruce::run_logic_mod_artifact_chain_v1(first(), second(), store); }
    void replay(const bruce::LogicModArtifactChainRunV1& r) { bruce::verify_logic_mod_artifact_chain_replay_v1(first(), second(), store, r); }
};
volatile sig_atomic_t child_exits = 0;
void count_child(int) { child_exits = child_exits + 1; }
void malformed_elf_preflight() {
    const auto original = read(BRUCE_CHAIN_WORKER);
    for (unsigned mutation = 0; mutation < 17; ++mutation) {
        auto malformed = original;
        Elf64_Ehdr header; std::memcpy(&header, malformed.data(), sizeof(header));
        if (mutation == 0) { header.e_type = ET_NONE; header.e_machine = EM_NONE; header.e_phnum = 0; malformed.resize(sizeof(header)); }
        if (mutation == 1) header.e_type = ET_REL;
        if (mutation == 2) header.e_machine = EM_NONE;
        if (mutation == 3) header.e_version = EV_NONE;
        if (mutation == 4) header.e_ehsize = 0;
        if (mutation == 5) header.e_phnum = 0;
        if (mutation == 12) header.e_entry = 0;
        if (mutation == 15) header.e_ident[EI_VERSION] = EV_NONE;
        std::memcpy(malformed.data(), &header, sizeof(header));
        for (unsigned i = 0; i < header.e_phnum; ++i) {
            const auto offset = header.e_phoff + i * sizeof(Elf64_Phdr);
            Elf64_Phdr program; std::memcpy(&program, malformed.data() + offset, sizeof(program));
            if (mutation == 6 && program.p_type == PT_LOAD) program.p_type = PT_NOTE;
            if (program.p_type == PT_LOAD && (program.p_flags & PF_X)) {
                if (mutation == 7) program.p_filesz = malformed.size();
                if (mutation == 8) program.p_memsz = program.p_filesz - 1;
                if (mutation == 9) program.p_align = 3;
                if (mutation == 10) ++program.p_offset;
                if (mutation == 11) program.p_vaddr = std::numeric_limits<Elf64_Addr>::max();
                if (mutation == 13) program.p_flags &= ~PF_X;
                if (mutation == 14) program.p_type = PT_INTERP;
                if (mutation == 16) program.p_offset = malformed.size() + 1;
            }
            std::memcpy(malformed.data() + offset, &program, sizeof(program));
        }
        // Both A and B must be rejected before any worker or Store mutation.
        for (const bool first : {false, true}) {
            Fixture bad(a_caps, b_caps, false, false, first ? malformed : std::string_view{}, first ? std::string_view{} : malformed);
            struct sigaction action{}, previous{}; action.sa_handler = count_child; ::sigemptyset(&action.sa_mask);
            child_exits = 0; check(::sigaction(SIGCHLD, &action, &previous) == 0, "sigaction failed");
            rejects([&] { (void)bad.run(); }, "LogicWorkerRequiresStaticELF");
            check(::sigaction(SIGCHLD, &previous, nullptr) == 0 && child_exits == 0, "Malformed ELF started worker");
            check(!std::filesystem::exists(bad.store), "Malformed ELF mutated Store");
        }
    }
}
void preflight() {
    malformed_elf_preflight();
    Fixture f; auto a = f.first(); auto b = f.second(); b.output.manifest_id = "../bad";
    rejects([&] { (void)bruce::run_logic_mod_artifact_chain_v1(a, b, f.store); });
    check(!std::filesystem::exists(f.store), "Invalid B manifest mutated Store");
    auto large = f.second(); large.input = std::string(4097, 'x');
    rejects([&] { (void)bruce::run_logic_mod_artifact_chain_v1(a, large, f.store); });
    check(!std::filesystem::exists(f.store), "Invalid B input mutated Store");
    auto other = f.second(); other.output.store = f.dir / "other";
    rejects([&] { (void)bruce::run_logic_mod_artifact_chain_v1(a, other, f.store); });
    check(!std::filesystem::exists(f.store), "Different Store mutated Store");
    Fixture denied(a_caps, "\"artifact.input.v1\",\"worker.execute\"");
    rejects([&] { (void)denied.run(); }); check(!std::filesystem::exists(denied.store), "Capability denial mutated Store");
    Fixture denied_a("\"worker.execute\"");
    rejects([&] { (void)denied_a.run(); }); check(!std::filesystem::exists(denied_a.store), "A capability denial mutated Store");
    Fixture extra_a(b_caps);
    rejects([&] { (void)extra_a.run(); }); check(!std::filesystem::exists(extra_a.store), "Extra A input capability mutated Store");
    Fixture missing_b_input(a_caps, a_caps);
    rejects([&] { (void)missing_b_input.run(); }); check(!std::filesystem::exists(missing_b_input.store), "Missing B input capability mutated Store");
    Fixture invalid_b(a_caps, b_caps, false, true);
    rejects([&] { (void)invalid_b.run(); });
    check(!std::filesystem::exists(invalid_b.store), "Invalid B executable started A or mutated Store");
    auto same = f.second(); same.output.manifest_id = "first";
    rejects([&] { (void)bruce::run_logic_mod_artifact_chain_v1(a, same, f.store); });
    check(!std::filesystem::exists(f.store), "Duplicate manifest IDs mutated Store");
    auto budget = f.second(); budget.budgets.wall_ms = 0;
    rejects([&] { (void)bruce::run_logic_mod_artifact_chain_v1(a, budget, f.store); });
    check(!std::filesystem::exists(f.store), "Invalid B budget mutated Store");
}
// Construct independently valid accepted transcripts for an exec-escape package.
// Offline replay must accept the mathematics, yet cannot execute this worker:
// the existing worker test proves that its only runtime result is EXEC_DENIED.
bruce::LogicModRunV1 transcript(const bruce::LogicModChainStepV1& step,
    const bruce::LogicModPublishedOutputV1& published, const bruce::LogicModPublishedOutputV1* input) {
    bruce::LogicModRunV1 r; r.accepted = true; r.status = "ACCEPTED"; r.result = step.input == "1/2 hoch 2" ? "1/4" : "9/16";
    r.binding_identity = step.registry.bind(step.mod_id, step.version, step.template_id, "worker.execute").canonical_identity();
    r.normalized_input = step.input; r.budget_decision = "3:2508:671088644:40965:16384";
    std::string payload;
    if (input) {
        r.input_present = true; r.input_manifest_id = input->manifest_id; r.input_sha256 = input->sha256;
        r.input_object_index = 0; r.input_size_bytes = 4; r.input_codec = "artifact-bytes-v1";
        payload = field("1") + field(r.input_manifest_id) + field("0") + field(r.input_sha256) + field("4") + field(r.input_codec);
    }
    r.request_digest = bruce::sha256("BRUCE_LOGIC_REQUEST_DIGEST_V1\n" + field(r.binding_identity) + field(r.normalized_input) + field(r.budget_decision) + payload);
    r.response_frame = field("BRUCE_LOGIC_RESULT_V1\n" + field(r.binding_identity) + field(r.request_digest) + field("candidate") + field("rational-v1") + field(r.result) + field(bruce::sha256(r.result)) + field(""));
    r.response_digest = bruce::sha256(r.response_frame);
    r.output_present = true; r.output_manifest_id = published.manifest_id; r.output_object_index = 0;
    r.output_sha256 = published.sha256; r.output_size_bytes = published.size_bytes; r.output_codec = published.codec; return r;
}
void offline_contracts() {
    Fixture f(a_caps, b_caps, true); const auto a = f.first(); const auto b = f.second();
    auto prepared_a = bruce::prepare_logic_mod_output_v1(a.output);
    auto prepared_b = bruce::prepare_logic_mod_output_v1(b.output);
    bruce::LogicModArtifactChainRunV1 r; r.accepted = true; r.status = "ACCEPTED";
    r.first_output = bruce::publish_logic_mod_result_artifact_v1(prepared_a, f.a.bind("user.logic", "1.0.0", "math.rational.power.v1", "artifact.output.v1"), "9/16");
    r.second_output = bruce::publish_logic_mod_result_artifact_v1(prepared_b, f.b.bind("user.logic", "1.0.0", "math.rational.power.v1", "artifact.output.v1"), "1/4");
    r.first = transcript(a, r.first_output, nullptr); r.second = transcript(b, r.second_output, &r.first_output);
    r.chain_id = bruce::sha256("BRUCE_LOGIC_MOD_ARTIFACT_CHAIN_ID_V1\n" + field(r.first.binding_identity) + field(r.second.binding_identity) + field(r.first.normalized_input) + field(r.second.normalized_input) + field("first") + field("second"));
    struct sigaction action{}, previous{}; action.sa_handler = count_child; ::sigemptyset(&action.sa_mask);
    check(::sigaction(SIGCHLD, &action, &previous) == 0, "sigaction failed");
    f.replay(r);
    check(::sigaction(SIGCHLD, &previous, nullptr) == 0 && !child_exits, "Replay spawned worker");
    const auto bytes = bruce::logic_mod_artifact_chain_record_v1(r);
    check(bruce::logic_mod_artifact_chain_record_v1(bruce::read_logic_mod_artifact_chain_record_v1(bytes)) == bytes, "Offline record roundtrip");
    check(bytes.find(f.dir.string()) == std::string::npos, "Offline record leaked path");
    check(bytes.find(field(bruce::logic_mod_run_record_v1(r.first))) != std::string::npos && bytes.find(field(bruce::logic_mod_run_record_v1(r.second))) != std::string::npos, "Embedded run record changed");
    rejects([&] { (void)bruce::read_logic_mod_artifact_chain_record_v1(bytes + "x"); });
    const auto prefix = std::string("BRUCE_LOGIC_MOD_ARTIFACT_CHAIN_V1\n");
    auto noncanonical = bytes; noncanonical.insert(prefix.size(), "0");
    rejects([&] { (void)bruce::read_logic_mod_artifact_chain_record_v1(noncanonical); });
    for (unsigned mutation = 0; mutation < 7; ++mutation) {
        auto changed = r;
        if (mutation == 0) changed.second.input_manifest_id = "second";
        if (mutation == 1) changed.second.input_object_index = 1;
        if (mutation == 2) changed.second.input_sha256[0] = '0';
        if (mutation == 3) changed.first_output.sha256[0] = '0';
        if (mutation == 4) changed.second_output.codec = "artifact-bytes-v1";
        if (mutation == 5) changed.first.result = "1";
        if (mutation == 6) changed.chain_id[0] = changed.chain_id[0] == '0' ? '1' : '0';
        rejects([&] { f.replay(changed); }, "CHAIN_REPLAY_MISMATCH");
    }
    auto empty_output_a = f.first(); auto empty_output_b = f.second();
    empty_output_a.output = {}; empty_output_b.output = {};
    bruce::verify_logic_mod_artifact_chain_replay_v1(empty_output_a, empty_output_b, f.store, r);
    const auto store = bruce::artifact_v1::ArtifactStoreV1::open_existing(f.store);
    for (const auto* id : {"first", "second"}) {
        const auto manifest = store.read_manifest(id); auto changed = manifest; changed.status = "FAILED";
        const auto target = f.store / "manifests" / (std::string(id) + ".manifest");
        write(target, bruce::artifact_v1::encode_manifest_v1(changed));
        rejects([&] { f.replay(r); }, "CHAIN_REPLAY_MISMATCH");
        write(target, bruce::artifact_v1::encode_manifest_v1(manifest)); f.replay(r);
        std::filesystem::remove(target); rejects([&] { f.replay(r); }, "CHAIN_REPLAY_MISMATCH");
        write(target, bruce::artifact_v1::encode_manifest_v1(manifest));
    }
    for (const auto* output : {&r.first_output, &r.second_output}) {
        const auto object = f.store / "objects" / output->sha256.substr(0, 2) / (output->sha256 + ".blob");
        const auto original = read(object);
        write(object, "bad"); rejects([&] { f.replay(r); }, "CHAIN_REPLAY_MISMATCH");
        write(object, original); f.replay(r);
        std::filesystem::remove(object); rejects([&] { f.replay(r); }, "CHAIN_REPLAY_MISMATCH");
        write(object, original);
    }
    const auto missing = f.dir / "missing-store";
    rejects([&] { bruce::verify_logic_mod_artifact_chain_replay_v1(a, b, missing, r); }, "CHAIN_REPLAY_MISMATCH");
    check(!std::filesystem::exists(missing), "Replay created Store");
}
void worker_contracts() {
    Fixture f; const auto good = f.run();
    check(good.accepted && good.status == "ACCEPTED", "Chain rejected");
    check(good.first.result == "9/16" && good.second.result == "1/4", "Wrong result");
    check(!good.first.input_present && good.second.input_present && good.second.input_manifest_id == "first" &&
        good.second.input_object_index == 0 && good.second.input_sha256 == good.first_output.sha256, "Wrong handoff");
    f.replay(good);
    auto equivalent_a = f.first(); equivalent_a.input = " 3/4  HOCH 2 ";
    const auto equivalent = bruce::run_logic_mod_artifact_chain_v1(equivalent_a, f.second(), f.store);
    check(equivalent.chain_id == good.chain_id, "Normalized equivalent inputs changed chain ID");
    auto other_b = f.second(); other_b.output.manifest_id = "third";
    const auto other = bruce::run_logic_mod_artifact_chain_v1(f.first(), other_b, f.store);
    check(other.chain_id != good.chain_id, "Requested manifest missing from chain ID");
    const auto bytes = bruce::logic_mod_artifact_chain_record_v1(good);
    check(bytes.find(f.dir.string()) == std::string::npos, "Chain record leaked local path");
    check(bruce::logic_mod_artifact_chain_record_v1(bruce::read_logic_mod_artifact_chain_record_v1(bytes)) == bytes, "Record roundtrip");
    rejects([&] { (void)bruce::read_logic_mod_artifact_chain_record_v1(bytes + "x"); });
    rejects([&] { (void)bruce::read_logic_mod_artifact_chain_record_v1(std::string(bruce::logic_mod_artifact_chain_record_limit_v1 + 1, 'x')); });
    auto changed = good; changed.second.input_manifest_id = "second";
    rejects([&] { f.replay(changed); }, "CHAIN_REPLAY_MISMATCH");
    changed = good; changed.second.input_object_index = 1;
    rejects([&] { f.replay(changed); }, "CHAIN_REPLAY_MISMATCH");
    changed = good; changed.second.input_sha256[0] = '0';
    rejects([&] { f.replay(changed); }, "CHAIN_REPLAY_MISMATCH");
    changed = good; changed.chain_id[0] = changed.chain_id[0] == '0' ? '1' : '0';
    rejects([&] { f.replay(changed); }, "CHAIN_REPLAY_MISMATCH");
    Fixture failed_a; auto a = failed_a.first(); a.input = "fail";
    struct sigaction action{}, previous{}; action.sa_handler = count_child; ::sigemptyset(&action.sa_mask);
    child_exits = 0; check(::sigaction(SIGCHLD, &action, &previous) == 0, "sigaction failed");
    const auto no_b = bruce::run_logic_mod_artifact_chain_v1(a, failed_a.second(), failed_a.store);
    check(::sigaction(SIGCHLD, &previous, nullptr) == 0 && child_exits == 1, "A failure spawned B");
    check(!no_b.accepted && no_b.status == "CHAIN_FIRST_FAILED" && no_b.second.binding_identity.empty(), "A failure started B");
    check(!std::filesystem::exists(failed_a.store / "manifests/second.manifest"), "A failure published B");
    rejects([&] { (void)bruce::logic_mod_artifact_chain_record_v1(no_b); });
    Fixture failed_b; auto b = failed_b.second(); b.input = "fail";
    const auto no_record = bruce::run_logic_mod_artifact_chain_v1(failed_b.first(), b, failed_b.store);
    check(!no_record.accepted && no_record.status == "CHAIN_SECOND_FAILED", "B failure accepted");
    check(std::filesystem::exists(failed_b.store / "manifests/first.manifest") && !std::filesystem::exists(failed_b.store / "manifests/second.manifest"), "B failure publication wrong");
    rejects([&] { (void)bruce::logic_mod_artifact_chain_record_v1(no_record); });
}
}
int main(int argc, char**) {
    try { preflight(); offline_contracts(); if (argc == 1) worker_contracts(); std::cout << "chain contracts passed\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
#endif
