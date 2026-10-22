#include "bruce/mod/logic_mod_result_artifact_v1.hpp"
#include "bruce/evidence/bundle.hpp"
#include <openssl/evp.h>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sys/stat.h>
#include <sys/wait.h>
#include <type_traits>
#include <unistd.h>

namespace {
void check(bool value, const char* why) { if (!value) throw bruce::Error(why); }
template<class F> void rejects(F action, std::string_view expected = {}) {
    try { action(); } catch (const bruce::Error& error) {
        if (!expected.empty() && expected != error.what()) throw bruce::Error(std::string("Wrong error: ") + error.what());
        return;
    }
    throw bruce::Error("Expected result artifact rejection");
}
void write(const std::filesystem::path& path, std::string_view bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc); out << bytes; out.close();
    check(bool(out), "fixture write failed");
}
std::string field(std::string_view value) { return std::to_string(value.size()) + ':' + std::string(value); }
std::string member(std::string_view name, std::string_view bytes) { return field(name) + field(bytes); }
std::string sign(std::string_view bytes) {
    // Public RFC 8032 vector; never a production signing key.
    const unsigned char seed[] = {0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed, sizeof(seed)), EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    unsigned char signature[64]; std::size_t size = sizeof(signature);
    check(key && ctx && EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1 &&
          EVP_DigestSign(ctx.get(), signature, &size, reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()) == 1 && size == 64, "sign failed");
    std::string result;
    for (auto b : signature) { result += "0123456789abcdef"[b >> 4]; result += "0123456789abcdef"[b & 15]; }
    return result;
}
const std::string worker = "signed test worker";
std::string package() {
    const auto payload = member("tree.json", R"({"mount":[1,1],"nodes":[{"aliases":["hoch"],"claim_scope":"candidate_only","id":[1,1,5],"key":"power","route_pattern":["<rational>","hoch","<integer>"],"template_id":"math.rational.power.v1"}]})") +
        member("tests/golden-cases.json", R"([{"input":"3/4 hoch 2","status":"MATCHED","template_id":"math.rational.power.v1"}])");
    const auto manifest = "{\"capabilities\":[\"dataset.read\",\"tree.extend\"],\"host_abi_version\":1,\"mod_id\":\"user.logic\",\"owner_scope\":\"user\",\"payload_sha256\":\"" + bruce::sha256(payload) + "\",\"schema_version\":1,\"signing_key_id\":\"test-rfc8032\",\"token_normalization_version\":1,\"tree_schema_version\":1,\"version\":\"1.0.0\"}";
    auto data = "BRUCE_RUNTIME_DATA_MOD_V1\n" + member("manifest.json", manifest) + payload;
    data += member("signature.ed25519", sign(data)) + '\n';
    const auto logic = "{\"capabilities\":[\"artifact.input.v1\",\"artifact.output.v1\",\"worker.execute\"],\"template_ids\":[\"math.rational.power.v1\"],\"worker_path\":\"worker.bin\",\"worker_sha256\":\"" + bruce::sha256(worker) + "\"}";
    const auto raw = "BRUCE_LOGIC_MOD_PLUGIN_V1\n" + member("data.brucemod", data) + member("logic.json", logic) + member("worker.bin", worker);
    return raw + member("signature.ed25519", sign(raw)) + '\n';
}
struct Fixture {
    std::filesystem::path dir, root;
    bruce::LogicModRegistryV1 registry;
    Fixture() {
        char pattern[] = "/tmp/bruce-logic-result-XXXXXX"; check(::mkdtemp(pattern), "mkdtemp failed"); dir = pattern; root = dir / "store";
        write(dir / "worker.bin", worker); check(::chmod((dir / "worker.bin").c_str(), 0500) == 0, "chmod failed");
        const auto raw = package(); const auto id = bruce::sha256(raw);
        registry.discover(bruce::unpack_logic_mod_package_v1(raw), dir.string());
        registry.verify(id, bruce::read_runtime_mod_truststore_v1("BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\ntest-rfc8032:d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a\n"));
        registry.test_golden(id, bruce::core_tree_registry_v1()); registry.install(id); registry.activate(id);
    }
    ~Fixture() { std::filesystem::remove_all(dir); }
    auto binding(std::string_view capability = "artifact.output.v1") { return registry.bind("user.logic", "1.0.0", "math.rational.power.v1", capability); }
    auto config(std::string id = "result") { return bruce::LogicModOutputV1{root, std::move(id)}; }
    auto publish(std::string id = "result", std::string_view bytes = "9/16") {
        auto prepared = bruce::prepare_logic_mod_output_v1(config(std::move(id)));
        return bruce::publish_logic_mod_result_artifact_v1(prepared, binding(), bytes);
    }
    auto object(const bruce::LogicModPublishedOutputV1& value) { return root / "objects" / value.sha256.substr(0, 2) / (value.sha256 + ".blob"); }
    void replace_manifest(const bruce::artifact_v1::ArtifactManifestV1& manifest) {
        write(root / "manifests" / "result.manifest", bruce::artifact_v1::encode_manifest_v1(manifest));
    }
};

// Catch changed publication metadata, content, identity encoding, or overwrite.
void publication_contracts() {
    static_assert(!std::is_copy_constructible_v<bruce::PreparedLogicModOutputV1>);
    static_assert(!std::is_copy_assignable_v<bruce::PreparedLogicModOutputV1>);
    static_assert(std::is_nothrow_move_constructible_v<bruce::PreparedLogicModOutputV1>);
    static_assert(std::is_nothrow_move_assignable_v<bruce::PreparedLogicModOutputV1>);
    Fixture f;
    const auto output = f.publish();
    check(output.manifest_id == "result" && output.object_index == 0 && output.size_bytes == 4 && output.codec == "logic-mod-result-text-v1", "Published identity wrong");
    check(output.sha256 == "31540b43fa20cb275095247f6784794cecee19c0117b76238fa153964a52af5b", "Published wrong result bytes");
    check(output.canonical_identity() == "BRUCE_LOGIC_RESULT_ARTIFACT_V1\n6:result1:064:31540b43fa20cb275095247f6784794cecee19c0117b76238fa153964a52af5b1:424:logic-mod-result-text-v1\n", "Published identity not canonical");
    check(output.canonical_identity().find(f.root.string()) == std::string::npos, "Store path leaked into identity");
    auto store = bruce::artifact_v1::ArtifactStoreV1::open_existing(f.root);
    const auto manifest = store.read_manifest("result");
    check(manifest.run_id == "result" && manifest.node_id == "result" && manifest.attempt_id == "result" && manifest.status == "READY" && manifest.result_code == "ACCEPTED" && manifest.objects.size() == 1, "Result manifest wrong");
    check(manifest.objects[0].media_type == "logic-mod-result-text-v1" && manifest.objects[0].origin == "logic-mod-result-v1" && manifest.objects[0].id.sha256 == output.sha256 && manifest.objects[0].id.size_bytes == 4, "Result manifest object wrong");
    store.verify(manifest.objects[0].id);
    bruce::verify_logic_mod_result_artifact_v1(f.config(), output, "9/16");
    check(f.publish().canonical_identity() == output.canonical_identity(), "Identical publication not idempotent");
    rejects([&] { (void)f.publish("result", "1/2"); }, "ArtifactManifestConflict");
    check(store.read_manifest("result") == manifest, "Conflicting publication overwrote manifest");
    auto prepared = bruce::prepare_logic_mod_output_v1(f.config("moved"));
    auto moved = std::move(prepared);
    const auto moved_output = bruce::publish_logic_mod_result_artifact_v1(moved, f.binding(), "9/16");
    bruce::verify_logic_mod_result_artifact_v1(f.config("moved"), moved_output, "9/16");
    const auto limit = f.publish("limit", std::string(16384, 'x'));
    check(limit.size_bytes == 16384, "Exact output size limit rejected");
    bruce::verify_logic_mod_result_artifact_v1(f.config("limit"), limit, std::string(16384, 'x'));
    const auto empty = f.publish("empty", "");
    bruce::verify_logic_mod_result_artifact_v1(f.config("empty"), empty, "");
}

// Catch preflight writes before validation and importing unauthorized/large bytes.
void preflight_contracts() {
    Fixture f;
    for (const auto* id : {"", "../escape", "/absolute", "bad/id", "a:b"}) {
        rejects([&] { (void)bruce::prepare_logic_mod_output_v1(f.config(id)); }, "ArtifactManifestIdInvalid");
        check(!std::filesystem::exists(f.root), "Invalid ID created Store");
    }
    rejects([&] { (void)bruce::prepare_logic_mod_output_v1(f.config(std::string(129, 'x'))); }, "ArtifactManifestIdInvalid");
    auto prepared = bruce::prepare_logic_mod_output_v1(f.config());
    for (const auto* capability : {"worker.execute", "artifact.input.v1"})
        rejects([&] { (void)bruce::publish_logic_mod_result_artifact_v1(prepared, f.binding(capability), "9/16"); }, "LogicModOutputCapabilityDenied");
    rejects([&] { (void)bruce::publish_logic_mod_result_artifact_v1(prepared, f.binding(), std::string(16385, 'x')); }, "LogicModOutputLimitExceeded");
    check(std::filesystem::is_empty(f.root / "objects") && std::filesystem::is_empty(f.root / "manifests"), "Rejected output published content");
}

void preflight_entry_contracts() {
    Fixture f;
    auto store = bruce::artifact_v1::ArtifactStoreV1::open(f.root);
    const auto target = f.root / "manifests/result.manifest";
    const auto other = f.dir / "other";
    write(other, "untouched"); check(::chmod(other.c_str(), 0600) == 0, "fixture chmod failed");
    std::filesystem::create_symlink(other, target);
    rejects([&] { (void)bruce::prepare_logic_mod_output_v1(f.config()); }, "ArtifactManifestConflict");
    check(std::filesystem::is_symlink(std::filesystem::symlink_status(target)), "Preparation followed symlink");
    std::filesystem::remove(target);
    std::filesystem::create_directory(target);
    rejects([&] { (void)bruce::prepare_logic_mod_output_v1(f.config()); }, "ArtifactManifestConflict");
    std::filesystem::remove(target);
    std::filesystem::create_hard_link(other, target);
    rejects([&] { (void)bruce::prepare_logic_mod_output_v1(f.config()); }, "ArtifactManifestConflict");
    std::filesystem::remove(target);
    write(target, "noncanonical existing bytes"); check(::chmod(target.c_str(), 0644) == 0, "fixture chmod failed");
    rejects([&] { (void)bruce::prepare_logic_mod_output_v1(f.config()); }, "ArtifactManifestConflict");
    check(::chmod(target.c_str(), 0600) == 0, "fixture chmod failed");
    // Preparation checks entry safety without changing ordinary immutable
    // conflicts into a manifest-decoding error or interpreting existing bytes.
    auto prepared = bruce::prepare_logic_mod_output_v1(f.config());
    rejects([&] { (void)bruce::publish_logic_mod_result_artifact_v1(prepared, f.binding(), "9/16"); }, "ArtifactManifestConflict");
}

// Catch replay trusting the record, cached manifest, or unverified object bytes.
void replay_contracts() {
    Fixture f;
    const bruce::LogicModPublishedOutputV1 recorded{"result", 0, "31540b43fa20cb275095247f6784794cecee19c0117b76238fa153964a52af5b", 4, "logic-mod-result-text-v1"};
    rejects([&] { bruce::verify_logic_mod_result_artifact_v1(f.config(), recorded, "9/16"); });
    check(!std::filesystem::exists(f.root), "Replay created missing Store");
    const auto output = f.publish();
    for (unsigned mutation = 0; mutation < 5; ++mutation) {
        auto wrong = output;
        if (mutation == 0) wrong.manifest_id = "other";
        if (mutation == 1) wrong.object_index = 1;
        if (mutation == 2) wrong.sha256[0] = '0';
        if (mutation == 3) wrong.size_bytes = 3;
        if (mutation == 4) wrong.codec = "artifact-bytes-v1";
        rejects([&] { bruce::verify_logic_mod_result_artifact_v1(f.config(), wrong, "9/16"); });
    }
    rejects([&] { bruce::verify_logic_mod_result_artifact_v1(f.config("other"), output, "9/16"); });
    rejects([&] { bruce::verify_logic_mod_result_artifact_v1(f.config(), output, "8/16"); });
    rejects([&] { bruce::verify_logic_mod_result_artifact_v1(f.config(), output, "9/160"); });
    rejects([&] { bruce::verify_logic_mod_result_artifact_v1(f.config(), output, std::string(16385, 'x')); });
    auto store = bruce::artifact_v1::ArtifactStoreV1::open_existing(f.root);
    const auto original = store.read_manifest("result");
    for (unsigned mutation = 0; mutation < 9; ++mutation) {
        auto manifest = original;
        if (mutation == 0) manifest.status = "FAILED";
        if (mutation == 1) manifest.result_code = "REJECTED";
        if (mutation == 2) manifest.run_id = "other";
        if (mutation == 3) manifest.node_id = "other";
        if (mutation == 4) manifest.attempt_id = "other";
        if (mutation == 5) manifest.objects[0].media_type = "artifact-bytes-v1";
        if (mutation == 6) manifest.objects[0].origin = "other";
        if (mutation == 7) manifest.objects.clear();
        if (mutation == 8) manifest.objects.push_back(original.objects[0]);
        f.replace_manifest(manifest);
        rejects([&] { bruce::verify_logic_mod_result_artifact_v1(f.config(), output, "9/16"); });
    }
    f.replace_manifest(original);
    write(f.object(output), "8/16");
    rejects([&] { bruce::verify_logic_mod_result_artifact_v1(f.config(), output, "9/16"); });
    write(f.object(output), "9/16");
    bruce::verify_logic_mod_result_artifact_v1(f.config(), output, "9/16");
    std::filesystem::remove(f.object(output));
    rejects([&] { bruce::verify_logic_mod_result_artifact_v1(f.config(), output, "9/16"); });
    std::filesystem::remove(f.root / "manifests" / "result.manifest");
    rejects([&] { bruce::verify_logic_mod_result_artifact_v1(f.config(), output, "9/16"); });
}

// A FIFO without a writer must be rejected before opening it can block.
// Swap after preparation as well: preflight alone cannot authorize a later entry.
void fifo_contract(bool swap_after_preparation) {
    Fixture f;
    auto store = bruce::artifact_v1::ArtifactStoreV1::open(f.root);
    std::optional<bruce::PreparedLogicModOutputV1> prepared;
    if (swap_after_preparation) prepared.emplace(bruce::prepare_logic_mod_output_v1(f.config()));
    const auto target = f.root / "manifests" / "result.manifest";
    check(::mkfifo(target.c_str(), 0600) == 0, "FIFO setup failed");
    const auto pid = ::fork(); check(pid >= 0, "test fork failed");
    if (pid == 0) {
        ::alarm(2);
        try {
            if (swap_after_preparation)
                (void)bruce::publish_logic_mod_result_artifact_v1(*prepared, f.binding(), "9/16");
            else
                (void)bruce::prepare_logic_mod_output_v1(f.config());
            ::_exit(2);
        } catch (const bruce::Error& error) {
            ::_exit(std::string_view(error.what()) == "ArtifactManifestConflict" ? 0 : 3);
        } catch (...) { ::_exit(4); }
    }
    int status{}; pid_t waited;
    do { waited = ::waitpid(pid, &status, 0); } while (waited < 0 && errno == EINTR);
    check(waited == pid, "test child not reaped");
    check(WIFEXITED(status) && WEXITSTATUS(status) == 0,
        swap_after_preparation ? "FIFO replacement blocked or publication was not rejected"
                               : "FIFO output target passed preparation or blocked");
    check(std::filesystem::is_fifo(std::filesystem::symlink_status(target)), "FIFO target was replaced");
    check(std::distance(std::filesystem::directory_iterator(f.root / "manifests"), std::filesystem::directory_iterator{}) == 1,
        "Failed publication left temporary or success manifest");
    if (!swap_after_preparation) check(std::filesystem::is_empty(f.root / "objects"), "Preflight imported result bytes");
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 1) { publication_contracts(); preflight_contracts(); preflight_entry_contracts(); replay_contracts(); }
        else if (argc == 2 && std::string_view(argv[1]) == "--fifo-preflight") fifo_contract(false);
        else if (argc == 2 && std::string_view(argv[1]) == "--fifo-swap") fifo_contract(true);
        else throw bruce::Error("Unknown test selection");
        std::cout << "logic result artifact contracts passed\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
