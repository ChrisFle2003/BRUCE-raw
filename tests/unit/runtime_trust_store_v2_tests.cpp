#include "bruce/mod/runtime_trust_store_v2.hpp"
#include "bruce/core/types.hpp"
#include "bruce/mod/runtime_registry.hpp"
#include "bruce/mod/logic_mod_registry_v1.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/mod/local_builder.hpp"
#include "bruce/evidence/bundle.hpp"
#include "../../src/mod/runtime_mod_store_internal.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) { if (!condition) throw bruce::Error(message); }
template<class F> void rejected(F&& action) {
    try { action(); } catch (const bruce::Error&) { return; }
    throw bruce::Error("Invalid V2 trust contract accepted");
}
struct FixedClock final : bruce::RuntimeTrustClockV2 {
    std::string value = "2026-10-19T12:34:56Z";
    std::string now_utc() const override { return value; }
};
const std::string trust = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n"
    "alpha:0000000000000000000000000000000000000000000000000000000000000000\n"
    "beta:1111111111111111111111111111111111111111111111111111111111111111\n";
const std::string digest = "d08dc40c2c866291521b72266e47163ff6d81b917be5433d188cda333e21d089";
const std::string metadata = "{\"created_at\":\"2026-10-19T12:34:56Z\",\"generation\":\"00000000000000000001\",\"trust_sha256\":\"" + digest + "\"}\n";
void generation_and_metadata() {
    FixedClock clock;
    const auto imported = bruce::make_runtime_trust_generation_v2("00000000000000000001", trust, clock);
    require(imported.trust_bytes == trust, "V1 truststore bytes changed");
    require(imported.trust_sha256 == digest, "trust digest changed");
    require(bruce::render_runtime_trust_metadata_v2(imported) == metadata, "generation-one metadata is not canonical");
    require(bruce::parse_runtime_trust_metadata_v2(metadata, trust) == imported, "generation-one metadata round trip failed");
    require(bruce::render_runtime_trust_metadata_v2(imported) == bruce::render_runtime_trust_metadata_v2(imported), "metadata bytes vary");
    auto second = bruce::make_runtime_trust_generation_v2("00000000000000000002", trust, clock, "rotation");
    require(bruce::render_runtime_trust_metadata_v2(second).find("\"comment\":\"rotation\"") != std::string::npos, "comment missing");
    require(bruce::same_runtime_trust_identity_v2(imported, second), "generation should not affect trust identity");
    second.comment.reset();
    second.created_at = "2026-10-20T00:00:00Z";
    require(bruce::same_runtime_trust_identity_v2(imported, second), "time/comment affected trust identity");
    for (const auto& bad : {"1", "00000000000000000000", "000000000000000000001", "0000000000000000000x"})
        rejected([&] { (void)bruce::make_runtime_trust_generation_v2(bad, trust, clock); });
    rejected([&] { (void)bruce::make_runtime_trust_generation_v2("00000000000000000001", trust + "\n", clock); });
    clock.value = "2026-10-19T12:34:56+00:00";
    rejected([&] { (void)bruce::make_runtime_trust_generation_v2("00000000000000000001", trust, clock); });
    clock.value = "2026-02-30T12:34:56Z";
    rejected([&] { (void)bruce::make_runtime_trust_generation_v2("00000000000000000001", trust, clock); });
}
void invalid_metadata() {
    rejected([&] { (void)bruce::parse_runtime_trust_metadata_v2(metadata, trust + "x"); });
    rejected([&] { (void)bruce::parse_runtime_trust_metadata_v2(metadata.substr(0, metadata.size() - 1), trust); });
    const auto replace_once = [](std::string input, std::string_view from, std::string_view to) {
        const auto at = input.find(from); require(at != std::string::npos, "fixture replacement missing");
        input.replace(at, from.size(), to); return input;
    };
    rejected([&] { (void)bruce::parse_runtime_trust_metadata_v2(replace_once(metadata, "\"generation\":", "\"generation\":\"00000000000000000001\",\"generation\":"), trust); });
    rejected([&] { (void)bruce::parse_runtime_trust_metadata_v2(replace_once(metadata, "\"created_at\":\"2026-10-19T12:34:56Z\",", ""), trust); });
    rejected([&] { (void)bruce::parse_runtime_trust_metadata_v2(replace_once(metadata, "\"generation\":", "\"unknown\":\"x\",\"generation\":"), trust); });
    rejected([&] { (void)bruce::parse_runtime_trust_metadata_v2(replace_once(metadata, digest, std::string(64, '0')), trust); });
    rejected([&] { (void)bruce::parse_runtime_trust_metadata_v2(std::string(65537, 'x'), trust); });
    rejected([&] { (void)bruce::parse_runtime_trust_metadata_v2(metadata, "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\nalpha:xyz\n"); });
    rejected([&] { (void)bruce::parse_runtime_trust_metadata_v2(metadata, "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\nalpha:0000000000000000000000000000000000000000000000000000000000000000\nalpha:1111111111111111111111111111111111111111111111111111111111111111\n"); });
}
void comment_encoding_contract() {
    FixedClock clock;
    const std::string unicode = "Gr\xC3\xBC\xC3\x9F" "e \xE2\x82\xAC \xF0\x9F\x98\x80";
    const auto with_unicode = bruce::make_runtime_trust_generation_v2("00000000000000000001", trust, clock, unicode);
    const auto unicode_metadata = bruce::render_runtime_trust_metadata_v2(with_unicode);
    require(unicode_metadata.find(unicode) != std::string::npos, "valid UTF-8 comment bytes changed");
    require(bruce::parse_runtime_trust_metadata_v2(unicode_metadata, trust) == with_unicode, "Unicode comment did not round trip");
    require(bruce::render_runtime_trust_metadata_v2(bruce::parse_runtime_trust_metadata_v2(unicode_metadata, trust)) == unicode_metadata,
            "Unicode metadata was not byte-identical after parse/render");
    const std::string escaped = "line\n\"quoted\"\\path\t";
    const auto with_escapes = bruce::make_runtime_trust_generation_v2("00000000000000000001", trust, clock, escaped);
    const auto escaped_metadata = bruce::render_runtime_trust_metadata_v2(with_escapes);
    require(escaped_metadata.find("\"comment\":\"line\\u000a\\\"quoted\\\"\\\\path\\u0009\"") != std::string::npos,
            "comment JSON escapes are not canonical");
    require(bruce::parse_runtime_trust_metadata_v2(escaped_metadata, trust) == with_escapes, "escaped comment did not round trip");
    for (const auto& malformed : {std::string("\x80", 1), std::string("\xC3", 1),
                                  std::string("\xC0\xAF", 2), std::string("\xED\xA0\x80", 3),
                                  std::string("\xF4\x90\x80\x80", 4)}) {
        rejected([&] { (void)bruce::make_runtime_trust_generation_v2("00000000000000000001", trust, clock, malformed); });
        auto generation = with_unicode;
        generation.comment = malformed;
        rejected([&] { (void)bruce::render_runtime_trust_metadata_v2(generation); });
    }
}
void state_contract() {
    const bruce::RuntimeTrustReferenceV2 one{"00000000000000000001", digest};
    const bruce::RuntimeTrustReferenceV2 two{"00000000000000000002", std::string(64, 'a')};
    const bruce::RuntimeTrustStateV2 state{two, one, std::nullopt};
    const std::string bytes = "{\"active\":{\"generation\":\"00000000000000000001\",\"trust_sha256\":\"" + digest + "\"},\"prepared\":{\"generation\":\"00000000000000000002\",\"trust_sha256\":\"" + std::string(64, 'a') + "\"},\"previous\":null}\n";
    require(bruce::render_runtime_trust_state_v2(state) == bytes, "state order/width changed");
    require(bruce::parse_runtime_trust_state_v2(bytes) == state, "state round trip failed");
    require(bruce::render_runtime_trust_state_v2(state) == bruce::render_runtime_trust_state_v2(state), "state bytes vary");
    rejected([&] { (void)bruce::render_runtime_trust_state_v2({one, one, std::nullopt}); });
    rejected([&] { (void)bruce::render_runtime_trust_state_v2({std::nullopt, one, one}); });
    rejected([&] { (void)bruce::render_runtime_trust_state_v2({bruce::RuntimeTrustReferenceV2{one.generation, std::string(64, 'b')}, one, std::nullopt}); });
    rejected([&] { (void)bruce::parse_runtime_trust_state_v2(bytes.substr(0, bytes.size() - 1)); });
    rejected([&] { (void)bruce::parse_runtime_trust_state_v2(std::string(65537, 'x')); });
    rejected([&] { (void)bruce::parse_runtime_trust_state_v2("{\"active\":null,\"active\":null,\"prepared\":null,\"previous\":null}\n"); });
    rejected([&] { (void)bruce::parse_runtime_trust_state_v2("{\"active\":null,\"prepared\":null}\n"); });
    rejected([&] { (void)bruce::parse_runtime_trust_state_v2("{\"active\":null,\"prepared\":null,\"previous\":null,\"unknown\":null}\n"); });
    rejected([&] { (void)bruce::parse_runtime_trust_state_v2("{\"active\":{\"generation\":\"00000000000000000001\"},\"prepared\":null,\"previous\":null}\n"); });
}
void prospective_trust() {
    FixedClock clock;
    const auto old = bruce::make_runtime_trust_generation_v2("00000000000000000001", trust, clock);
    const std::string next_bytes = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\nalpha:0000000000000000000000000000000000000000000000000000000000000000\ngamma:" + std::string(64, '2') + "\n";
    const auto next = bruce::make_runtime_trust_generation_v2("00000000000000000002", next_bytes, clock);
    using K = bruce::RuntimeInstalledModKindV2;
    const std::vector<bruce::RuntimeActiveInstalledModV2> records{
        {K::logic, "z.logic", "2", "beta", true},
        {K::data, "b.data", "1", "alpha", true},
        {K::data, "a.data", "2", "beta", true},
        {K::logic, "inactive.logic", "1", "beta", false},
        {K::data, "new-key", "1", "gamma", false}
    };
    const std::vector<bruce::RuntimeAffectedModV2> expected{{K::data, "a.data", "2", "beta"}, {K::logic, "z.logic", "2", "beta"}};
    require(bruce::runtime_mods_losing_trust_v2(old, next, records) == expected, "affected Data/Logic set or order wrong");
    auto reversed = records; std::reverse(reversed.begin(), reversed.end());
    require(bruce::runtime_mods_losing_trust_v2(old, next, reversed) == expected, "affected order depends on input order");
    require(bruce::runtime_mods_losing_trust_v2(old, old, records).empty(), "unchanged trust deactivates mods");
    const std::vector<bruce::RuntimeActiveInstalledModV2> invalid_active{{K::data, "x", "1", "gamma", true}};
    rejected([&] { (void)bruce::runtime_mods_losing_trust_v2(old, next, invalid_active); });
}
void changed_key_ids_affect_both_mod_kinds() {
    FixedClock clock;
    const std::string old_bytes = trust + "delta:" + std::string(64, '3') + "\n";
    const std::string candidate_bytes =
        "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\nalpha:" + std::string(64, '0') +
        "\ndelta:" + std::string(64, '4') + "\ngamma:" + std::string(64, '2') + "\n";
    const auto old = bruce::make_runtime_trust_generation_v2("00000000000000000001", old_bytes, clock);
    const auto candidate = bruce::make_runtime_trust_generation_v2("00000000000000000002", candidate_bytes, clock);
    using K = bruce::RuntimeInstalledModKindV2;
    const std::vector<bruce::RuntimeActiveInstalledModV2> records{
        {K::logic, "z.changed", "1", "delta", true},
        {K::data, "b.removed", "1", "beta", true},
        {K::logic, "a.retained", "1", "alpha", true},
        {K::data, "a.changed", "2", "delta", true},
        {K::logic, "b.removed", "2", "beta", true},
        {K::data, "inactive.new", "1", "gamma", false},
        {K::logic, "inactive.new", "1", "gamma", false}
    };
    const std::vector<bruce::RuntimeAffectedModV2> expected{
        {K::data, "a.changed", "2", "delta"}, {K::data, "b.removed", "1", "beta"},
        {K::logic, "b.removed", "2", "beta"}, {K::logic, "z.changed", "1", "delta"}
    };
    require(bruce::runtime_mods_losing_trust_v2(old, candidate, records) == expected,
            "changed public key bytes under the same ID did not affect active Data and Logic mods");
    auto reversed = records; std::reverse(reversed.begin(), reversed.end());
    require(bruce::runtime_mods_losing_trust_v2(old, candidate, reversed) == expected,
            "changed/removed key result depends on installed-record order");
}

// Each persistence test catches an observable incorrect publication or recovery,
// never a private helper's implementation. Fixtures contain real V1 registries.
struct DiskRoot {
    std::string base, root, next;
    DiskRoot() {
        char name[] = "/tmp/bruce-trust-v2-XXXXXX";
        require(::mkdtemp(name), "mkdtemp failed"); base = name;
        root = base + "/mods"; next = base + "/next.pub";
        const auto keys = bruce::read_runtime_mod_truststore_v1(trust);
        (void)bruce::RuntimeModRegistryV1::open(root, keys);
        (void)bruce::DurableLogicModRegistryV1::open(root, keys);
        bruce::LocalFiles::publish_new_atomically(next,
            "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\ngamma:" + std::string(64, '2') + "\n");
    }
    ~DiskRoot() { bruce::runtime_mod_store_detail::clear_test_failpoint(); std::filesystem::remove_all(base); }
};
using Store = bruce::RuntimeTrustStoreV2;
int old_reader(const DiskRoot& f, const char* kind, const char* action) {
    const auto pid = ::fork(); require(pid >= 0, "legacy reader fork failed");
    if (pid == 0) {
        const int quiet = ::open("/dev/null", O_WRONLY);
        if (quiet >= 0) { ::dup2(quiet, STDERR_FILENO); ::close(quiet); }
        ::execl(BRUCE_TRUST_V1_READER_FIXTURE, BRUCE_TRUST_V1_READER_FIXTURE, kind, f.root.c_str(), action, nullptr);
        ::_exit(127);
    }
    int status; require(::waitpid(pid, &status, 0) == pid && WIFEXITED(status), "legacy reader crashed");
    return WEXITSTATUS(status);
}
void disk_lifecycle() {
    FixedClock clock; DiskRoot f;
    rejected([&] { (void)Store::open_existing(f.base + "/missing"); });
    require(!std::filesystem::exists(f.base + "/missing"), "status created a root");
    rejected([&] { (void)Store::open_existing(f.root); });
    auto store = Store::import_v1(f.root, clock);
    require(store.state().active->generation == "00000000000000000001", "import did not activate generation 1");
    require(bruce::LocalFiles::read(f.root + "/truststore.pub") == trust, "import changed trust");
    rejected([&] { (void)Store::import_v1(f.root, clock); });
    store.prepare(f.next, clock);
    require(store.state().prepared->generation == "00000000000000000002", "prepare did not persist generation 2");
    require(bruce::LocalFiles::read(f.root + "/truststore.pub") == trust, "prepare activated trust");
    const auto before = bruce::LocalFiles::read(f.root + "/trust/journal.v1");
    require(store.verify().active_incompatible.empty(), "empty registry incompatible");
    (void)store.verify();
    require(bruce::LocalFiles::read(f.root + "/trust/journal.v1") == before, "verify modified journal");
    store.activate();
    require(store.state().active->generation == "00000000000000000002" &&
        store.state().previous->generation == "00000000000000000001" && !store.state().prepared, "activation state wrong");
    require(bruce::LocalFiles::read(f.root + "/truststore.pub") == bruce::LocalFiles::read(f.next), "active projection stale");
    store.rollback();
    require(Store::open_existing(f.root).state().active->generation == "00000000000000000001", "rollback did not survive reopen");
    (void)bruce::RuntimeModRegistryV1::open_existing(f.root).snapshot();
    (void)bruce::DurableLogicModRegistryV1::open_existing(f.root).snapshot();
}
void disk_crash_recovery() {
    FixedClock clock;
    for (const auto& point : {"trust.after_generation", "trust.after_prepare", "trust.after_snapshot", "trust.after_commit", "trust.after_projection"}) {
        DiskRoot f; auto store = Store::import_v1(f.root, clock); store.prepare(f.next, clock);
        bruce::runtime_mod_store_detail::set_test_failpoint(point);
        // Generation boundary belongs to prepare; all other boundaries belong to activate.
        rejected([&] { if (std::string(point) == "trust.after_generation") store.prepare(f.next, clock); else store.activate(); });
        bruce::runtime_mod_store_detail::clear_test_failpoint();
        auto recovered = Store::open_existing(f.root);
        const bool committed = std::string(point) == "trust.after_commit" || std::string(point) == "trust.after_projection";
        require(recovered.state().active->generation == (committed ? "00000000000000000002" : "00000000000000000001"), "crash crossed commit boundary");
        require(bruce::LocalFiles::read(f.root + "/truststore.pub") == (committed ? bruce::LocalFiles::read(f.next) : trust), "mixed trust projection after recovery");
    }
    for (const auto& point : {"trust.after_fence", "trust.after_data_barrier", "trust.after_logic_barrier", "trust.after_commit", "trust.after_projection"}) {
        DiskRoot f;
        require(old_reader(f, "data", "open") == 0 && old_reader(f, "logic", "open") == 0, "frozen legacy readers could not open original V1 root");
        bruce::runtime_mod_store_detail::set_test_failpoint(point);
        rejected([&] { (void)Store::import_v1(f.root, clock); });
        bruce::runtime_mod_store_detail::clear_test_failpoint();
        const auto data_before = bruce::LocalFiles::read(f.root + "/journal.v1");
        const auto logic_before = bruce::LocalFiles::read(f.root + "/logic-journal.v1");
        for (const auto* kind : {"data", "logic"})
            for (const auto* action : {"open", "mutate"})
                require(old_reader(f, kind, action) == 1, "legacy reader bypassed irreversible migration fence");
        require(bruce::LocalFiles::read(f.root + "/journal.v1") == data_before &&
            bruce::LocalFiles::read(f.root + "/logic-journal.v1") == logic_before, "legacy reader changed fenced journals");
        // Both current registry entry points must recover an irreversible import.
        (void)bruce::RuntimeModRegistryV1::open_existing(f.root).snapshot();
        (void)bruce::DurableLogicModRegistryV1::open_existing(f.root).snapshot();
        require(Store::open_existing(f.root).state().active->generation == "00000000000000000001", "fenced import was abandoned");
    }
}

void import_before_fence_is_legacy() {
    FixedClock clock;
    for (const auto* point : {"trust.after_generation", "trust.after_prepare", "trust.after_snapshot"}) {
        DiskRoot f;
        const auto data_before = bruce::LocalFiles::read(f.root + "/journal.v1");
        const auto logic_before = bruce::LocalFiles::read(f.root + "/logic-journal.v1");
        bruce::runtime_mod_store_detail::set_test_failpoint(point);
        rejected([&] { (void)Store::import_v1(f.root, clock); });
        bruce::runtime_mod_store_detail::clear_test_failpoint();
        require(bruce::LocalFiles::read(f.root + "/truststore.pub") == trust &&
            bruce::LocalFiles::read(f.root + "/journal.v1") == data_before &&
            bruce::LocalFiles::read(f.root + "/logic-journal.v1") == logic_before, "pre-fence crash changed V1 authority");
        require(old_reader(f, "data", "open") == 0 && old_reader(f, "logic", "open") == 0, "pre-fence crash fenced legacy readers");
        (void)bruce::RuntimeModRegistryV1::open_existing(f.root).snapshot();
        (void)bruce::DurableLogicModRegistryV1::open_existing(f.root).snapshot();
    }
}

void torn_import_barriers_recover() {
    FixedClock clock;
    const std::string barrier = "BRUCE_TRUST_COORDINATOR_V2\n";
    for (const auto* journal : {"journal.v1", "logic-journal.v1"}) {
        for (std::size_t count = 1; count < barrier.size(); ++count) {
            DiskRoot f;
            bruce::runtime_mod_store_detail::set_test_failpoint("trust.after_fence");
            rejected([&] { (void)Store::import_v1(f.root, clock); });
            bruce::runtime_mod_store_detail::clear_test_failpoint();
            std::ofstream append(f.root + '/' + journal, std::ios::binary | std::ios::app);
            append << barrier.substr(0, count); append.close();
            require(Store::open_existing(f.root).state().active->generation == "00000000000000000001", "torn import barrier prevented irreversible recovery");
            require(old_reader(f, "data", "open") == 1 && old_reader(f, "logic", "open") == 1, "barrier repair reopened legacy root");
        }
    }
}
void single_registry_legacy_import() {
    FixedClock clock;
    for (const bool data_only : {true, false}) {
        DiskRoot f;
        if (data_only) {
            for (const auto* name : {"logic-journal.v1", "logic-registry.v1", "logic-packages", "logic-workers"}) std::filesystem::remove(f.root + '/' + name);
        } else {
            for (const auto* name : {"journal.v1", "registry.v1", "packages"}) std::filesystem::remove(f.root + '/' + name);
        }
        auto store = Store::import_v1(f.root, clock);
        require(store.state().active->generation == "00000000000000000001", "single-kind V1 import failed");
        require(old_reader(f, "data", "open") == 1 && old_reader(f, "logic", "open") == 1, "single-kind migration did not fence both types");
    }
}

// Mutation caught: treating unknown historical keys as corruption, accepting
// changed key bytes, or authorizing a default transition that drops an active key.
void signed_package_files(const DiskRoot& f) {
    const auto write = [](const std::string& path, std::string_view bytes, unsigned mode = 0600) {
        std::ofstream stream(path, std::ios::binary); stream << bytes; stream.close();
        require(!stream.fail() && ::chmod(path.c_str(), mode) == 0, "signed fixture write failed");
    };
    const auto signing = f.base + "/signing.env";
    write(signing, "BRUCE_MOD_ED25519_SEED_HEX=9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60\n");
    const auto data = f.base + "/data"; require(::mkdir(data.c_str(), 0700) == 0, "data source mkdir failed");
    write(data + "/manifest.json", "{\"mod_id\":\"user.power\",\"signing_key_id\":\"test-rfc8032\",\"version\":\"1.0.0\"}");
    write(data + "/tree.json", "{\"mount\":[1,1],\"nodes\":[{\"aliases\":[],\"claim_scope\":\"candidate_only\",\"id\":[1,1,5],\"key\":\"power\",\"route_pattern\":[\"<rational>\",\"hoch\",\"<integer>\"],\"template_id\":\"user.power.v1\"}]}");
    write(data + "/golden.json", "[{\"input\":\"3/4 hoch 2\",\"status\":\"MATCHED\",\"template_id\":\"user.power.v1\"}]");
    bruce::build_local_data_mod_v1({data, data + "/manifest.json", data + "/tree.json", data + "/golden.json", f.base + "/data.brucemod", f.base + "/signed.pub", signing});
    const auto logic = f.base + "/logic"; require(::mkdir(logic.c_str(), 0700) == 0, "logic source mkdir failed");
    std::filesystem::copy_file(f.base + "/data.brucemod", logic + "/data.brucemod");
    write(logic + "/worker.bin", "worker bytes\n", 0700);
    write(logic + "/logic.json", "{\"capabilities\":[\"worker.execute\"],\"template_ids\":[\"user.power.v1\"],\"worker_path\":\"worker.bin\",\"worker_sha256\":\"" + bruce::sha256("worker bytes\n") + "\"}");
    bruce::build_local_logic_mod_v1({logic, f.base + "/logic.brucemod", f.base + "/logic.pub", signing});
}
void installed_package_compatibility() {
    FixedClock clock; DiskRoot f; signed_package_files(f);
    const auto write = [](const std::string& path, std::string_view bytes, unsigned mode = 0600) {
        std::ofstream stream(path, std::ios::binary); stream << bytes; stream.close();
        require(!stream.fail() && ::chmod(path.c_str(), mode) == 0, "signed fixture write failed");
    };
    const auto signed_trust = bruce::LocalFiles::read(f.base + "/signed.pub");
    bruce::LocalFiles::replace_atomically(f.root + "/truststore.pub", signed_trust);
    auto data_registry = bruce::RuntimeModRegistryV1::open_existing(f.root);
    auto logic_registry = bruce::DurableLogicModRegistryV1::open_existing(f.root);
    const auto package = bruce::unpack_runtime_data_mod_package_v1(bruce::LocalFiles::read(f.base + "/data.brucemod"));
    data_registry.install(package);
    logic_registry.install(bruce::unpack_logic_mod_package_v1(bruce::LocalFiles::read(f.base + "/logic.brucemod")));
    data_registry.activate("user.power", "1.0.0"); logic_registry.activate("user.power", "1.0.0");
    auto store = Store::import_v1(f.root, clock); store.prepare(f.next, clock);
    const auto report = store.verify();
    require(report.active_incompatible.empty() && report.prepared_incompatible.size() == 2, "active/prepared compatibility not separate");
    const auto state_before = bruce::LocalFiles::read(f.root + "/trust/state.json");
    rejected([&] { store.activate(); });
    require(bruce::LocalFiles::read(f.root + "/trust/state.json") == state_before, "incompatible activation changed trust");
    data_registry.disable("user.power"); logic_registry.disable("user.power");
    store.activate();
    require(store.verify().active_incompatible.size() == 2, "inactive historical package treated as structural failure");
    store.rollback();
    const auto changed = f.base + "/changed.pub";
    write(changed, "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\ntest-rfc8032:" + std::string(64, '3') + "\n");
    store.prepare(changed, clock);
    require(store.verify().prepared_incompatible.size() == 2, "changed key bytes inherited trust from key ID");
    const auto worker = f.root + "/logic-workers/" + bruce::unpack_logic_mod_package_v1(bruce::LocalFiles::read(f.base + "/logic.brucemod")).package_digest() + "/worker.bin";
    const auto worker_dir = std::filesystem::path(worker).parent_path().string();
    std::filesystem::rename(worker_dir, f.base + "/moved-worker");
    require(::symlink((f.base + "/moved-worker").c_str(), worker_dir.c_str()) == 0, "worker directory alias failed");
    rejected([&] { (void)store.verify(); });
    std::filesystem::remove(worker_dir);
    std::filesystem::rename(f.base + "/moved-worker", worker_dir);
    require(::chmod(worker.c_str(), 0600) == 0, "tamper fixture chmod failed");
    write(worker, "tampered worker\n", 0500);
    rejected([&] { (void)store.verify(); });
}
void disk_integrity_and_paths() {
    FixedClock clock;
    for (const auto& file : {"trust/journal.v1", "trust/state.json", "trust/generations/00000000000000000001/truststore.pub", "trust/generations/00000000000000000001/metadata.json"}) {
        DiskRoot f; auto store = Store::import_v1(f.root, clock);
        bruce::LocalFiles::replace_atomically(f.root + '/' + file, "corrupt\n");
        rejected([&] { (void)store.verify(); });
        rejected([&] { (void)Store::open_existing(f.root); });
    }
    for (int attack = 0; attack != 5; ++attack) {
        DiskRoot f; auto store = Store::import_v1(f.root, clock);
        const auto path = f.root + "/trust/state.json";
        if (attack == 0) { std::filesystem::rename(path, path + ".saved"); require(::symlink((path + ".saved").c_str(), path.c_str()) == 0, "symlink failed"); }
        if (attack == 1) { std::filesystem::rename(path, path + ".saved"); require(::mkfifo(path.c_str(), 0600) == 0, "mkfifo failed"); }
        if (attack == 2) require(::link(path.c_str(), (path + ".alias").c_str()) == 0, "hardlink failed");
        if (attack == 3) require(::chmod(path.c_str(), 0640) == 0, "chmod failed");
        if (attack == 4) {
            require(::symlink(f.root.c_str(), (f.base + "/alias").c_str()) == 0, "root alias failed");
            rejected([&] { (void)Store::open_existing(f.base + "/alias"); }); continue;
        }
        rejected([&] { (void)store.verify(); });
        rejected([&] { (void)Store::open_existing(f.root); });
    }
    {
        DiskRoot f; auto store = Store::import_v1(f.root, clock);
        std::filesystem::rename(f.root + "/trust/generations/00000000000000000001", f.base + "/removed");
        rejected([&] { (void)store.verify(); });
    }
    {
        DiskRoot f; auto store = Store::import_v1(f.root, clock);
        const auto path = f.root + "/trust/generations/00000000000000000002";
        require(::mkdir(path.c_str(), 0700) == 0, "collision mkdir failed");
        bruce::LocalFiles::publish_new_atomically(path + "/truststore.pub", trust);
        const auto original_state = bruce::LocalFiles::read(f.root + "/trust/state.json");
        rejected([&] { store.prepare(f.next, clock); });
        require(bruce::LocalFiles::read(f.root + "/trust/state.json") == original_state, "generation collision mutated state");
    }
}
void external_trust_paths_are_safe() {
    FixedClock clock;
    for (int attack = 0; attack != 5; ++attack) {
        DiskRoot f; auto store = Store::import_v1(f.root, clock);
        const auto saved = f.next + ".saved";
        if (attack == 0) { std::filesystem::rename(f.next, saved); require(::symlink(saved.c_str(), f.next.c_str()) == 0, "external symlink failed"); }
        if (attack == 1) { std::filesystem::rename(f.next, saved); require(::mkfifo(f.next.c_str(), 0600) == 0, "external fifo failed"); }
        if (attack == 2) require(::link(f.next.c_str(), saved.c_str()) == 0, "external hardlink failed");
        if (attack == 3) require(::chmod(f.next.c_str(), 0640) == 0, "external chmod failed");
        if (attack == 4) {
            require(::mkdir((f.base + "/private").c_str(), 0700) == 0, "external parent mkdir failed");
            std::filesystem::rename(f.next, f.base + "/private/next.pub");
            require(::symlink((f.base + "/private").c_str(), (f.base + "/alias").c_str()) == 0, "external parent symlink failed");
            f.next = f.base + "/alias/next.pub";
        }
        rejected([&] { store.prepare(f.next, clock); });
        require(!store.state().prepared && bruce::LocalFiles::read(f.root + "/truststore.pub") == trust, "unsafe external trust changed authority");
    }
}
void coordinator_detects_hash_and_projection_tampering() {
    FixedClock clock;
    for (bool hash : {true, false}) {
        DiskRoot f; auto store = Store::import_v1(f.root, clock);
        const auto file = f.root + (hash ? "/trust/journal.v1" : "/truststore.pub");
        auto bytes = bruce::LocalFiles::read(file);
        if (hash) bytes[bytes.size() - 3] = bytes[bytes.size() - 3] == 'a' ? 'b' : 'a';
        else bytes = bruce::LocalFiles::read(f.next);
        bruce::LocalFiles::replace_atomically(file, bytes);
        rejected([&] { (void)store.verify(); });
        rejected([&] { (void)Store::open_existing(f.root); });
        rejected([&] { (void)bruce::RuntimeModRegistryV1::open_existing(f.root); });
        rejected([&] { (void)bruce::DurableLogicModRegistryV1::open_existing(f.root); });
    }
}
void migration_barriers_require_the_coordinator() {
    FixedClock clock; DiskRoot f; (void)Store::import_v1(f.root, clock);
    std::filesystem::rename(f.root + "/trust", f.base + "/removed-coordinator");
    rejected([&] { (void)bruce::RuntimeModRegistryV1::open_existing(f.root); });
    rejected([&] { (void)bruce::DurableLogicModRegistryV1::open_existing(f.root); });
}
void incomplete_coordinator_append_is_not_commit() {
    FixedClock clock;
    for (const auto& suffix : {std::string("6:"), std::string("6:Commit64:"), std::string("6:Commit64:") + std::string(64, 'a') + "1:x64:"}) {
        DiskRoot f; auto store = Store::import_v1(f.root, clock); store.prepare(f.next, clock);
        std::ofstream append(f.root + "/trust/journal.v1", std::ios::binary | std::ios::app); append << suffix; append.close();
        rejected([&] { (void)store.verify(); });
        require(Store::open_existing(f.root).state().active->generation == "00000000000000000001", "torn append became commit");
    }
}
void ownership_errors_fail_closed_when_privileged() {
    if (::geteuid() != 0) return; // Setting foreign ownership requires privilege.
    FixedClock clock; DiskRoot f; auto store = Store::import_v1(f.root, clock);
    const auto file = f.root + "/trust/state.json";
    require(::chown(file.c_str(), 12345, static_cast<gid_t>(-1)) == 0, "ownership fixture chown failed");
    rejected([&] { (void)store.verify(); });
    rejected([&] { (void)Store::open_existing(f.root); });
}
void recovery_guard_is_scoped_to_the_root() {
    FixedClock clock; DiskRoot first, second;
    auto first_store = Store::import_v1(first.root, clock);
    auto second_store = Store::import_v1(second.root, clock); second_store.prepare(second.next, clock);
    bruce::runtime_mod_store_detail::set_test_failpoint("trust.after_commit");
    rejected([&] { second_store.activate(); });
    bruce::runtime_mod_store_detail::clear_test_failpoint();
    struct InspectingClock final : bruce::RuntimeTrustClockV2 {
        const DiskRoot& other;
        explicit InspectingClock(const DiskRoot& root) : other(root) {}
        std::string now_utc() const override {
            (void)bruce::RuntimeModRegistryV1::open_existing(other.root).snapshot();
            require(bruce::LocalFiles::read(other.root + "/truststore.pub") == bruce::LocalFiles::read(other.next), "another root bypassed committed recovery");
            return "2026-10-19T12:34:56Z";
        }
    } inspecting(second);
    first_store.prepare(first.next, inspecting);
}
void committed_recovery_binds_exact_registry_snapshots() {
    FixedClock clock;
    for (const bool data : {true, false}) {
        DiskRoot f;
        bruce::runtime_mod_store_detail::set_test_failpoint("trust.after_commit");
        rejected([&] { (void)Store::import_v1(f.root, clock); });
        bruce::runtime_mod_store_detail::clear_test_failpoint();
        // This is a valid V1 append with an empty generation-1 snapshot, but it
        // is not either exact snapshot bound by the committed coordinator.
        const std::string header = data ? "BRUCE_RUNTIME_MOD_JOURNAL_V1\n" : "BRUCE_LOGIC_MOD_JOURNAL_V1\n";
        const std::string snapshot = (data ? "BRUCE_RUNTIME_MOD_REGISTRY_V1\n" : "BRUCE_LOGIC_MOD_REGISTRY_V1\n") + std::string("1:11:0\n");
        const auto field = [](std::string_view bytes) { return std::to_string(bytes.size()) + ':' + std::string(bytes); };
        const auto unsigned_record = field(bruce::sha256(header)) + field(snapshot);
        std::ofstream append(f.root + (data ? "/journal.v1" : "/logic-journal.v1"), std::ios::binary | std::ios::app);
        append << unsigned_record << field(bruce::sha256(unsigned_record)) << '\n'; append.close();
        rejected([&] { (void)Store::open_existing(f.root); });
        require(old_reader(f, "data", "open") == 1 && old_reader(f, "logic", "open") == 1, "snapshot mismatch reopened fenced root");
    }
}
void verification_never_performs_committed_recovery() {
    FixedClock clock; DiskRoot f; auto store = Store::import_v1(f.root, clock); store.prepare(f.next, clock);
    bruce::runtime_mod_store_detail::set_test_failpoint("trust.after_commit");
    rejected([&] { store.activate(); });
    bruce::runtime_mod_store_detail::clear_test_failpoint();
    const auto journal = bruce::LocalFiles::read(f.root + "/trust/journal.v1");
    const auto state = bruce::LocalFiles::read(f.root + "/trust/state.json");
    rejected([&] { (void)store.verify(); });
    require(bruce::LocalFiles::read(f.root + "/trust/journal.v1") == journal &&
        bruce::LocalFiles::read(f.root + "/trust/state.json") == state &&
        bruce::LocalFiles::read(f.root + "/truststore.pub") == trust, "read-only verify published committed projections");
    require(Store::open_existing(f.root).state().active->generation == "00000000000000000002", "mutating open failed to complete projections");
}
void disk_concurrent_prepare() {
    FixedClock clock; DiskRoot f; (void)Store::import_v1(f.root, clock);
    std::vector<pid_t> children;
    for (unsigned i = 0; i != 3; ++i) {
        const auto pid = ::fork(); require(pid >= 0, "fork failed");
        if (pid == 0) {
            try { Store::open_existing(f.root).prepare(f.next, clock); ::_exit(0); }
            catch (...) { ::_exit(1); }
        }
        children.push_back(pid);
    }
    for (auto pid : children) { int status; require(::waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0, "concurrent writer failed"); }
    auto store = Store::open_existing(f.root);
    require(store.state().prepared->generation == "00000000000000000004", "concurrent prepare lost an update");
    (void)store.verify();
}
void noncommitted_coordinator_cannot_downgrade_migrated_root() {
    FixedClock clock;
    for (const bool torn : {false, true}) for (int evidence = 0; evidence != 5; ++evidence) {
        DiskRoot f; signed_package_files(f);
        const auto attacker_trust = bruce::LocalFiles::read(f.base + "/signed.pub");
        bruce::LocalFiles::replace_atomically(f.root + "/truststore.pub", attacker_trust);
        auto data = bruce::RuntimeModRegistryV1::open_existing(f.root);
        auto logic = bruce::DurableLogicModRegistryV1::open_existing(f.root);
        bruce::LocalFiles::replace_atomically(f.root + "/truststore.pub", trust);
        auto store = Store::import_v1(f.root, clock);
        if (evidence == 4) store.prepare(f.next, clock);
        const auto original_state = bruce::LocalFiles::read(f.root + "/trust/state.json");
        bruce::LocalFiles::replace_atomically(f.root + "/truststore.pub", attacker_trust);
        bruce::LocalFiles::replace_atomically(f.root + "/trust/journal.v1",
            torn ? "BRUCE_RUNTIME_TRUST_JOURNAL_V2\n7:Pre" : "BRUCE_RUNTIME_TRUST_JOURNAL_V2\n");
        // Evidence separately exercises the live state and each rootwide barrier.
        if (evidence != 0) std::filesystem::remove(f.root + "/trust/state.json");
        if (evidence == 1 || evidence == 3 || evidence == 4) bruce::LocalFiles::replace_atomically(f.root + "/journal.v1", "BRUCE_RUNTIME_MOD_JOURNAL_V1\n");
        if (evidence == 2 || evidence == 3 || evidence == 4) bruce::LocalFiles::replace_atomically(f.root + "/logic-journal.v1", "BRUCE_LOGIC_MOD_JOURNAL_V1\n");
        if (evidence == 3) bruce::LocalFiles::publish_new_atomically(f.root + "/trust/state.json", original_state);
        const auto data_before = bruce::LocalFiles::read(f.root + "/journal.v1");
        const auto logic_before = bruce::LocalFiles::read(f.root + "/logic-journal.v1");
        const auto package = bruce::unpack_runtime_data_mod_package_v1(bruce::LocalFiles::read(f.base + "/data.brucemod"));
        const auto logic_package = bruce::unpack_logic_mod_package_v1(bruce::LocalFiles::read(f.base + "/logic.brucemod"));
        rejected([&] { data.install(package); });
        rejected([&] { logic.install(logic_package); });
        rejected([&] { data.activate("user.power", "1.0.0"); });
        rejected([&] { logic.activate("user.power", "1.0.0"); });
        rejected([&] { (void)bruce::RuntimeModRegistryV1::open_existing(f.root); });
        rejected([&] { (void)bruce::DurableLogicModRegistryV1::open_existing(f.root); });
        rejected([&] { (void)Store::open_existing(f.root); });
        require(bruce::LocalFiles::read(f.root + "/journal.v1") == data_before &&
            bruce::LocalFiles::read(f.root + "/logic-journal.v1") == logic_before, "downgrade admitted registry mutation");
        require(bruce::LocalFiles::read(f.root + "/trust/generations/00000000000000000001/truststore.pub") == trust, "downgrade changed bound trust generation");
    }
}
constexpr std::size_t journal_capacity = 32 * 1024 * 1024;
std::string disk_field(std::string_view bytes) { return std::to_string(bytes.size()) + ':' + std::string(bytes); }
void fixture_replace(const std::string& path, std::string_view bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc); output << bytes; output.close();
    require(!output.fail() && ::chmod(path.c_str(), 0600) == 0, "capacity fixture write failed");
}
std::string registry_fixture_snapshot(bool data, std::uint64_t generation, unsigned records = 0, unsigned pad = 0) {
    std::string bytes = data ? "BRUCE_RUNTIME_MOD_REGISTRY_V1\n" : "BRUCE_LOGIC_MOD_REGISTRY_V1\n";
    bytes += disk_field(std::to_string(generation)) + disk_field(std::to_string(records));
    for (unsigned i = 0; i < records; ++i) {
        const auto a = std::min(pad, 127u); pad -= a;
        const auto b = std::min(pad, 127u); pad -= b;
        bytes += disk_field(std::string(1, static_cast<char>('a' + i)) + std::string(a, 'x'));
        bytes += disk_field("1" + std::string(b, '0')) + disk_field(std::string(64, '0'));
        if (!data) bytes += disk_field(std::string(64, '0'));
        bytes += disk_field(data ? "0" : "3");
    }
    return bytes + '\n';
}
std::string registry_fixture_record(std::string& chain, std::string_view snapshot) {
    const auto bytes = disk_field(chain) + disk_field(snapshot);
    chain = bruce::sha256(bytes); return bytes + disk_field(chain) + '\n';
}
void fill_registry_journal(const DiskRoot& f, bool data, std::size_t target) {
    std::string bytes = data ? "BRUCE_RUNTIME_MOD_JOURNAL_V1\n" : "BRUCE_LOGIC_MOD_JOURNAL_V1\n";
    auto chain = bruce::sha256(bytes); std::uint64_t generation = 0;
    while (bytes.size() < target - 2000) bytes += registry_fixture_record(chain, registry_fixture_snapshot(data, ++generation));
    bool fitted = false; std::string final;
    for (unsigned count = 1; count <= 8 && !fitted; ++count) for (unsigned pad = 0; pad <= 254 * count && !fitted; ++pad) {
        auto trial = chain;
        const auto middle = registry_fixture_record(trial, registry_fixture_snapshot(data, generation + 1, count, pad));
        final = registry_fixture_snapshot(data, generation + 2);
        const auto last = registry_fixture_record(trial, final);
        if (bytes.size() + middle.size() + last.size() == target) { bytes += middle + last; fitted = true; }
    }
    require(fitted, "capacity fixture did not fit requested byte boundary");
    fixture_replace(f.root + (data ? "/journal.v1" : "/logic-journal.v1"), bytes);
    fixture_replace(f.root + (data ? "/registry.v1" : "/logic-registry.v1"), final);
}
void migration_barrier_capacity_is_reserved_before_fence() {
    FixedClock clock;
    for (const bool data : {true, false}) for (const int extra : {26, 1, 0}) {
        DiskRoot f;
        // The hand-counted literal marker is 27 bytes. extra==0 is an exact fit.
        fill_registry_journal(f, data, journal_capacity - 27 + extra);
        const auto journal_path = f.root + (data ? "/journal.v1" : "/logic-journal.v1");
        const auto original_journal = bruce::LocalFiles::read(journal_path);
        if (extra != 0) {
            rejected([&] { (void)Store::import_v1(f.root, clock); });
            require(bruce::LocalFiles::read(f.root + "/truststore.pub") == trust, "capacity failure published irreversible fence");
            require(bruce::LocalFiles::read(journal_path) == original_journal, "capacity failure changed old registry journal");
            (void)bruce::RuntimeModRegistryV1::open_existing(f.root).snapshot();
            (void)bruce::DurableLogicModRegistryV1::open_existing(f.root).snapshot();
        } else {
            // Import first recovers the ordinary torn V1 tail, then reserves
            // the exact 27 available bytes. A torn barrier is also net-counted.
            { std::ofstream append(journal_path, std::ios::binary | std::ios::app); append << "6:abcd"; }
            bruce::runtime_mod_store_detail::set_test_failpoint("trust.after_fence");
            rejected([&] { (void)Store::import_v1(f.root, clock); });
            bruce::runtime_mod_store_detail::clear_test_failpoint();
            { std::ofstream append(journal_path, std::ios::binary | std::ios::app); append << "BRUCE_TRUST_COORDINATOR_V"; }
            require(Store::open_existing(f.root).state().active->generation == "00000000000000000001", "exact barrier capacity could not recover torn suffix");
            require(std::filesystem::file_size(journal_path) == journal_capacity, "exact barrier fit used wrong byte count");
        }
    }
}
std::string coordinator_fixture_transaction(const bruce::RuntimeTrustStateV2& old,
    const bruce::RuntimeTrustStateV2& next, const std::string& old_trust, const std::string& next_trust, bool importing = false) {
    const std::string data = "BRUCE_RUNTIME_MOD_REGISTRY_V1\n1:01:0\n", logic = "BRUCE_LOGIC_MOD_REGISTRY_V1\n1:01:0\n";
    return disk_field(bruce::render_runtime_trust_state_v2(old)) + disk_field(bruce::render_runtime_trust_state_v2(next)) +
        disk_field(old_trust) + disk_field(next_trust) + disk_field(bruce::sha256(old_trust)) + disk_field(bruce::sha256(next_trust)) +
        disk_field(data) + disk_field(data) + disk_field(logic) + disk_field(logic) + disk_field(importing ? "import-v1" : "transition");
}
std::string coordinator_fixture_record(std::string& chain, std::string_view type, std::string_view payload) {
    const auto bytes = disk_field(type) + disk_field(chain) + disk_field(payload);
    chain = bruce::sha256(bytes); return bytes + disk_field(chain) + '\n';
}
std::string coordinator_fixture_completed(std::string& chain, const std::string& tx) {
    auto bytes = coordinator_fixture_record(chain, "Prepare", tx);
    bytes += coordinator_fixture_record(chain, "Commit", bruce::sha256(tx));
    bytes += coordinator_fixture_record(chain, "Projected", bruce::sha256(tx)); return bytes;
}
void complete_coordinator_capacity_is_reserved() {
    FixedClock clock;
    for (const bool exact_fit : {false, true}) {
        DiskRoot f;
        const std::string t1 = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\nkeyx:" + std::string(64, '1') + '\n';
        const std::string t2 = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\nkeyx:" + std::string(64, '2') + '\n';
        bruce::LocalFiles::replace_atomically(f.root + "/truststore.pub", t1);
        bruce::LocalFiles::replace_atomically(f.next, t2);
        const bruce::RuntimeTrustReferenceV2 r1{"00000000000000000001", bruce::sha256(t1)}, r2{"00000000000000000002", bruce::sha256(t2)};
        const bruce::RuntimeTrustStateV2 one{std::nullopt, r1, std::nullopt}, prepared{r2, r1, std::nullopt};
        const bruce::RuntimeTrustStateV2 two{std::nullopt, r2, r1}, back{std::nullopt, r1, r2};
        std::string journal = "BRUCE_RUNTIME_TRUST_JOURNAL_V2\n", chain = bruce::sha256(journal);
        journal += coordinator_fixture_completed(chain, coordinator_fixture_transaction({}, one, t1, t1, true));
        journal += coordinator_fixture_completed(chain, coordinator_fixture_transaction(one, prepared, t1, t1));
        journal += coordinator_fixture_completed(chain, coordinator_fixture_transaction(prepared, two, t1, t2));
        auto store = Store::import_v1(f.root, clock); store.prepare(f.next, clock); store.activate();
        require(bruce::LocalFiles::read(f.root + "/trust/journal.v1") == journal, "capacity fixture disagrees with real API journal");
        const auto tx_back = coordinator_fixture_transaction(two, back, t2, t1), tx_forward = coordinator_fixture_transaction(back, two, t1, t2);
        auto trial = chain; const auto full = coordinator_fixture_completed(trial, tx_back).size();
        trial = chain; const auto prepare_bytes = coordinator_fixture_record(trial, "Prepare", tx_back).size();
        const auto commit_bytes = coordinator_fixture_record(trial, "Commit", bruce::sha256(tx_back)).size();
        require(full == 1651 && prepare_bytes + commit_bytes == 1438, "hand-checked capacity sizes changed");
        if (exact_fit) {
            // Ignored uncommitted Prepare records adjust exact available space
            // without adding a generation or changing authoritative state.
            unsigned count = 0;
            while ((journal_capacity - journal.size()) % full != 0 && count++ < full)
                journal += coordinator_fixture_record(chain, "Prepare", tx_back);
            require((journal_capacity - journal.size()) % full == 0, "exact capacity fixture not aligned");
        }
        bool at_two = true;
        while (journal.size() + (exact_fit ? 2 : 1) * full <= journal_capacity) {
            journal += coordinator_fixture_completed(chain, at_two ? tx_back : tx_forward); at_two = !at_two;
        }
        require(journal_capacity - journal.size() == (exact_fit ? 1651 : 1593), "capacity fixture remainder changed");
        fixture_replace(f.root + "/trust/journal.v1", journal);
        bruce::LocalFiles::replace_atomically(f.root + "/trust/state.json", bruce::render_runtime_trust_state_v2(at_two ? two : back));
        bruce::LocalFiles::replace_atomically(f.root + "/truststore.pub", at_two ? t2 : t1);
        (void)Store::open_existing(f.root);
        const auto original_state = bruce::LocalFiles::read(f.root + "/trust/state.json");
        const auto original_trust = bruce::LocalFiles::read(f.root + "/truststore.pub");
        if (!exact_fit) {
            rejected([&] { store.rollback(); });
            rejected([&] { store.prepare(f.next, clock); });
            require(!std::filesystem::exists(f.root + "/trust/generations/00000000000000000003"), "capacity rejection published an orphan generation");
            require(bruce::LocalFiles::read(f.root + "/truststore.pub") == original_trust &&
                bruce::LocalFiles::read(f.root + "/trust/state.json") == original_state &&
                bruce::LocalFiles::read(f.root + "/trust/journal.v1") == journal, "capacity failure crossed Commit or appended Prepare");
        } else {
            { std::ofstream append(f.root + "/trust/journal.v1", std::ios::binary | std::ios::app); append << "6:Comm"; }
            bruce::runtime_mod_store_detail::set_test_failpoint("trust.after_projection");
            rejected([&] { store.rollback(); });
            bruce::runtime_mod_store_detail::clear_test_failpoint();
            { std::ofstream append(f.root + "/trust/journal.v1", std::ios::binary | std::ios::app); append << "9:Projec"; }
            (void)Store::open_existing(f.root);
            require(std::filesystem::file_size(f.root + "/trust/journal.v1") == journal_capacity, "exact coordinator fit could not complete Projected");
        }
        (void)Store::open_existing(f.root).verify();
        (void)bruce::RuntimeModRegistryV1::open_existing(f.root).snapshot();
        (void)bruce::DurableLogicModRegistryV1::open_existing(f.root).snapshot();
    }
}
void committed_history_generations_are_always_validated() {
    FixedClock clock;
    for (const bool missing : {false, true}) for (const bool activated : {false, true}) {
        DiskRoot f; auto store = Store::import_v1(f.root, clock);
        store.prepare(f.next, clock);
        if (activated) store.activate();
        bruce::LocalFiles::replace_atomically(f.next, "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\nomega:" + std::string(64, '3') + '\n');
        store.prepare(f.next, clock);
        if (activated) store.activate();
        // Activated history loses generation 1 from the current state; an
        // overridden prepared generation 2 also remains committed history.
        const std::string id = activated ? "00000000000000000001" : "00000000000000000002";
        const auto path = f.root + "/trust/generations/" + id;
        const auto journal = bruce::LocalFiles::read(f.root + "/trust/journal.v1");
        if (missing) std::filesystem::rename(path, f.base + "/removed-history");
        else {
            const auto replacement = bruce::make_runtime_trust_generation_v2(id, bruce::LocalFiles::read(f.next), clock);
            bruce::LocalFiles::replace_atomically(path + "/truststore.pub", replacement.trust_bytes);
            bruce::LocalFiles::replace_atomically(path + "/metadata.json", bruce::render_runtime_trust_metadata_v2(replacement));
        }
        rejected([&] { (void)store.verify(); });
        rejected([&] { (void)Store::open_existing(f.root); });
        rejected([&] { (void)bruce::RuntimeModRegistryV1::open_existing(f.root); });
        rejected([&] { (void)bruce::DurableLogicModRegistryV1::open_existing(f.root); });
        require(bruce::LocalFiles::read(f.root + "/trust/journal.v1") == journal, "history rejection rewrote coordinator authority");
    }
}
void generation_limit_is_checked_before_publication() {
    FixedClock clock; DiskRoot f; auto store = Store::import_v1(f.root, clock);
    for (unsigned i = 2; i <= 4095; ++i) {
        const auto digits = std::to_string(i), id = std::string(20 - digits.size(), '0') + digits;
        const auto path = f.root + "/trust/generations/" + id;
        require(::mkdir(path.c_str(), 0700) == 0, "orphan generation mkdir failed");
        const auto generation = bruce::make_runtime_trust_generation_v2(id, trust, clock);
        fixture_replace(path + "/truststore.pub", trust);
        fixture_replace(path + "/metadata.json", bruce::render_runtime_trust_metadata_v2(generation));
    }
    (void)store.verify();
    store.prepare(f.next, clock);
    require(store.state().prepared->generation == "00000000000000004096", "last supported generation was refused");
    const auto journal = bruce::LocalFiles::read(f.root + "/trust/journal.v1");
    const auto state = bruce::LocalFiles::read(f.root + "/trust/state.json");
    rejected([&] { store.prepare(f.next, clock); });
    require(!std::filesystem::exists(f.root + "/trust/generations/00000000000000004097"), "generation 4097 was published before rejection");
    require(bruce::LocalFiles::read(f.root + "/trust/journal.v1") == journal &&
        bruce::LocalFiles::read(f.root + "/trust/state.json") == state, "generation-count rejection changed authority");
    (void)Store::open_existing(f.root).verify();
    store.activate();
    require(store.state().active->generation == "00000000000000004096", "boundary rejection made old prepared state unusable");
}
void truncated_import_cannot_resurrect_retired_keys(bool committed_prefix) {
    FixedClock clock; DiskRoot f; signed_package_files(f);
    const auto signed_trust = bruce::LocalFiles::read(f.base + "/signed.pub");
    bruce::LocalFiles::replace_atomically(f.root + "/truststore.pub", signed_trust);
    auto data = bruce::RuntimeModRegistryV1::open_existing(f.root);
    auto logic = bruce::DurableLogicModRegistryV1::open_existing(f.root);
    auto store = Store::import_v1(f.root, clock);
    std::string header = "BRUCE_RUNTIME_TRUST_JOURNAL_V2\n", chain = bruce::sha256(header);
    const auto import_payload = coordinator_fixture_transaction({}, store.state(), signed_trust, signed_trust, true);
    const auto prepare = coordinator_fixture_record(chain, "Prepare", import_payload);
    require(bruce::LocalFiles::read(f.root + "/trust/journal.v1").starts_with(header + prepare), "first import Prepare fixture mismatch");
    store.prepare(f.next, clock); store.activate(); // The RFC signer is now retired.
    const auto state = bruce::LocalFiles::read(f.root + "/trust/state.json");
    auto prefix = header + prepare;
    if (committed_prefix) prefix += coordinator_fixture_record(chain, "Commit", bruce::sha256(import_payload));
    bruce::LocalFiles::replace_atomically(f.root + "/trust/journal.v1", prefix);
    bruce::LocalFiles::replace_atomically(f.root + "/truststore.pub", "BRUCE_RUNTIME_MOD_TRUST_MIGRATION_V2\n");
    rejected([&] { data.install(bruce::unpack_runtime_data_mod_package_v1(bruce::LocalFiles::read(f.base + "/data.brucemod"))); });
    rejected([&] { logic.install(bruce::unpack_logic_mod_package_v1(bruce::LocalFiles::read(f.base + "/logic.brucemod"))); });
    rejected([&] { (void)Store::open_existing(f.root); });
    rejected([&] { (void)bruce::RuntimeModRegistryV1::open_existing(f.root); });
    rejected([&] { (void)bruce::DurableLogicModRegistryV1::open_existing(f.root); });
    require(bruce::LocalFiles::read(f.root + "/trust/state.json") == state, "truncated import rolled back live V2 state");
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "downgrade") {
            noncommitted_coordinator_cannot_downgrade_migrated_root(); return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "barrier-capacity") { migration_barrier_capacity_is_reserved_before_fence(); return 0; }
        if (argc == 2 && std::string_view(argv[1]) == "coordinator-capacity") { complete_coordinator_capacity_is_reserved(); return 0; }
        if (argc == 2 && std::string_view(argv[1]) == "history") { committed_history_generations_are_always_validated(); return 0; }
        if (argc == 2 && std::string_view(argv[1]) == "generation-limit") { generation_limit_is_checked_before_publication(); return 0; }
        if (argc == 2 && std::string_view(argv[1]) == "truncated-import") { truncated_import_cannot_resurrect_retired_keys(false); return 0; }
        if (argc == 2 && std::string_view(argv[1]) == "truncated-commit") { truncated_import_cannot_resurrect_retired_keys(true); return 0; }
        generation_and_metadata(); invalid_metadata(); comment_encoding_contract(); state_contract();
        prospective_trust(); changed_key_ids_affect_both_mod_kinds();
        std::cerr << "disk_lifecycle\n"; disk_lifecycle();
        std::cerr << "disk_crash_recovery\n"; disk_crash_recovery();
        std::cerr << "disk_integrity_and_paths\n"; disk_integrity_and_paths();
        external_trust_paths_are_safe(); coordinator_detects_hash_and_projection_tampering();
        migration_barriers_require_the_coordinator();
        incomplete_coordinator_append_is_not_commit(); ownership_errors_fail_closed_when_privileged();
        recovery_guard_is_scoped_to_the_root();
        committed_recovery_binds_exact_registry_snapshots();
        verification_never_performs_committed_recovery();
        std::cerr << "disk_concurrent_prepare\n"; disk_concurrent_prepare();
        std::cerr << "import_before_fence_is_legacy\n"; import_before_fence_is_legacy();
        std::cerr << "torn_import_barriers_recover\n"; torn_import_barriers_recover();
        std::cerr << "single_registry_legacy_import\n"; single_registry_legacy_import();
        std::cerr << "installed_package_compatibility\n"; installed_package_compatibility();
        noncommitted_coordinator_cannot_downgrade_migrated_root();
        migration_barrier_capacity_is_reserved_before_fence(); complete_coordinator_capacity_is_reserved();
        committed_history_generations_are_always_validated();
        generation_limit_is_checked_before_publication();
        truncated_import_cannot_resurrect_retired_keys(false); truncated_import_cannot_resurrect_retired_keys(true);
        std::cout << "runtime trust store v2 contracts passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
