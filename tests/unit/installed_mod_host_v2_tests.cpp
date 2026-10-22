#include "bruce/mod/installed_mod_host_v2.hpp"
#include "bruce/mod/local_builder.hpp"
#include "bruce/mod/package_inspection_v2.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/core/types.hpp"
#include "bruce/tree/normalizer.hpp"
#include "../../src/mod/runtime_mod_store_internal.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

namespace {
template<class T>
concept ExposesReusableDurableBinding = requires(const T& value) {
    bruce::run_logic_mod_worker_v1(value.binding, "input", bruce::LogicModBudgetsV1{});
};
static_assert(!ExposesReusableDurableBinding<bruce::InstalledModRunV2>);

const std::string seed = "9d61b19deffd5a60ba844af492ec2cc4" "4449c5697b326919703bac031cae7f60";
void require(bool condition, const char* message) { if (!condition) throw bruce::Error(message); }
void write(const std::string& path, std::string_view bytes, unsigned mode = 0600) {
    std::ofstream output(path, std::ios::binary); output << bytes; output.close();
    require(!output.fail() && ::chmod(path.c_str(), mode) == 0, "Fixture write failed");
}
std::string trust_bytes(const bruce::RuntimeModTrustStoreV1& trust) {
    std::string bytes = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n";
    for (const auto& [id, key] : trust.public_keys()) bytes += id + ':' + key + '\n';
    return bytes;
}
template<class F> void rejected(F action, std::string_view expected = {}) {
    try { action(); } catch (const bruce::Error& error) {
        if (!expected.empty()) require(error.what() == expected, "Wrong host rejection");
        return;
    }
    throw bruce::Error("Unsafe installed-host operation accepted");
}
struct Fixture {
    std::string root, mods, signing, key_id;
    unsigned serial = 0;
    explicit Fixture(std::string key = "test-rfc8032", std::string fixture_seed = seed)
        : key_id(std::move(key)) {
        char pattern[] = "/tmp/bruce-installed-host-XXXXXX";
        const auto made = ::mkdtemp(pattern); require(made, "mkdtemp failed");
        root = made; mods = root + "/mods"; signing = root + "/signing.env";
        write(signing, "BRUCE_MOD_ED25519_SEED_HEX=" + fixture_seed + "\n");
    }
    ~Fixture() { std::filesystem::remove_all(root); }
    std::pair<bruce::DetectedModPackageV2, bruce::RuntimeModTrustStoreV1> data(
        std::string id, std::string version, unsigned address = 5, std::string alias = "hoch") {
        const auto tag = std::to_string(serial++); const auto source = root + "/data-" + tag;
        std::filesystem::create_directory(source);
        const auto template_id = id + ".template";
        write(source + "/manifest.json", "{\"mod_id\":\"" + id + "\",\"signing_key_id\":\"" + key_id + "\",\"version\":\"" + version + "\"}");
        write(source + "/tree.json", "{\"mount\":[1,1],\"nodes\":[{\"aliases\":[\"" + alias + "\"],\"claim_scope\":\"candidate_only\",\"id\":[1,1," + std::to_string(address) + "],\"key\":\"key" + std::to_string(address) + "\",\"route_pattern\":[\"" + alias + "\"],\"template_id\":\"" + template_id + "\"}]}");
        write(source + "/golden-cases.json", "[{\"input\":\"" + alias + "\",\"status\":\"MATCHED\",\"template_id\":\"" + template_id + "\"}]");
        const auto package = root + "/data-" + tag + ".brucemod";
        const auto trust = root + "/data-" + tag + ".pub";
        bruce::build_local_data_mod_v1({source, source + "/manifest.json", source + "/tree.json",
            source + "/golden-cases.json", package, trust, signing});
        return {bruce::detect_mod_package_v2(bruce::LocalFiles::read(package)),
            bruce::read_runtime_mod_truststore_v1(bruce::LocalFiles::read(trust))};
    }
    bruce::DetectedModPackageV2 logic_from(const bruce::DetectedModPackageV2& data_package,
                                           std::string template_id,
                                           std::string worker = "installed host worker\n") {
        const auto tag = std::to_string(serial++); const auto source = root + "/logic-" + tag;
        std::filesystem::create_directory(source);
        require(data_package.data_package(), "Logic source is not Data");
        write(source + "/data.brucemod", data_package.data_package()->bytes());
        write(source + "/worker.bin", worker, 0700);
        write(source + "/logic.json", "{\"capabilities\":[\"worker.execute\"],\"template_ids\":[\"" + template_id + "\"],\"worker_path\":\"worker.bin\",\"worker_sha256\":\"" + bruce::sha256(worker) + "\"}");
        const auto package = root + "/logic-" + tag + ".brucemod";
        const auto trust = root + "/logic-" + tag + ".pub";
        bruce::build_local_logic_mod_v1({source, package, trust, signing});
        return bruce::detect_mod_package_v2(bruce::LocalFiles::read(package));
    }
};
struct FixedClock final : bruce::RuntimeTrustClockV2 {
    std::string now_utc() const override { return "2026-10-19T12:00:00Z"; }
};
}

int main() {
    try {
        Fixture fixture;
        auto [data_v1, trust] = fixture.data("user.data", "1.0.0");
        auto [data_v2, ignored] = fixture.data("user.data", "2.0.0"); (void)ignored;
        auto host = bruce::InstalledModHostV2::open(fixture.mods, trust);
        host.install(data_v1);
        const auto generation = host.snapshot().data.generation;
        host.install(data_v1);
        require(host.snapshot().data.generation == generation, "Idempotent install changed generation");
        host.install(data_v2);
        require(host.snapshot().data.records.size() == 2, "Two Data versions were not retained");
        host.activate("user.data", "1.0.0");
        host.activate("user.data", "2.0.0");
        require(host.snapshot().data.records[0].state == bruce::RuntimeModStateV1::disabled &&
                host.snapshot().data.records[1].state == bruce::RuntimeModStateV1::active,
                "Data rollback states are wrong");
        std::future<bruce::InstalledModRouteV2> pending_route;
        {
            bruce::runtime_mod_store_detail::RootLock held(fixture.mods);
            std::promise<void> entered;
            auto entered_future = entered.get_future();
            pending_route = std::async(std::launch::async, [&] {
                entered.set_value();
                return host.route("hoch");
            });
            entered_future.wait();
            require(pending_route.wait_for(std::chrono::milliseconds(50)) ==
                        std::future_status::timeout,
                    "Host route bypassed registry.lock");
        }
        require(pending_route.get().route.status == bruce::RouteStatusV1::matched,
                "Locked route did not complete after coordinator release");

        Fixture legacy_fixture;
        auto [legacy_v1, legacy_trust] = legacy_fixture.data("user.legacy", "1.0.0", 6, "legacy-one");
        auto [legacy_v2, legacy_ignored] = legacy_fixture.data("user.legacy", "2.0.0", 7, "legacy-two");
        (void)legacy_ignored;
        {
            auto modern_host = bruce::InstalledModHostV2::open(legacy_fixture.mods, legacy_trust);
            modern_host.install(legacy_v1);
            modern_host.install(legacy_v2);
        }
        for (const auto* name : {"logic-packages", "logic-workers", "logic-journal.v1",
                                 "logic-registry.v1"})
            std::filesystem::remove_all(legacy_fixture.mods + '/' + name);
        const auto require_legacy_data_only = [&] {
            for (const auto* name : {"logic-packages", "logic-workers", "logic-journal.v1",
                                     "logic-registry.v1"})
                require(!std::filesystem::exists(legacy_fixture.mods + '/' + name),
                        "Data lifecycle initialized Logic sidecars in a legacy root");
        };
        auto legacy_host = bruce::InstalledModHostV2::open_existing(legacy_fixture.mods);
        require_legacy_data_only();
        legacy_host.activate("user.legacy", "2.0.0");
        require_legacy_data_only();
        legacy_host.update(legacy_v1);
        auto legacy_snapshot = legacy_host.snapshot();
        require(legacy_snapshot.data.records[0].state == bruce::RuntimeModStateV1::active &&
                legacy_snapshot.data.records[1].state == bruce::RuntimeModStateV1::disabled,
                "Legacy Data update did not roll back the active version");
        const auto legacy_generation = legacy_snapshot.data.generation;
        legacy_host.update(legacy_v1);
        require(legacy_host.snapshot().data.generation == legacy_generation,
                "Idempotent legacy Data update changed generation");
        legacy_host.disable("user.legacy");
        require(legacy_host.snapshot().data.records[0].state == bruce::RuntimeModStateV1::disabled,
                "Legacy Data disable did not persist");
        require_legacy_data_only();

        auto [builtin, builtin_trust] = fixture.data("language", "1.0.0", 6, "builtin");
        (void)builtin_trust;
        rejected([&] { host.install(builtin); }, "BuiltinModImmutable");

        auto same_id_logic = fixture.logic_from(data_v1, "user.data.template");
        rejected([&] { host.install(same_id_logic); }, "InstalledModKindConflict");

        Fixture conflict_fixture;
        auto [active_data, conflict_trust] = conflict_fixture.data("user.left", "1.0.0", 9, "left");
        auto [logic_data, ignored_two] = conflict_fixture.data("user.right", "1.0.0", 9, "right");
        (void)ignored_two;
        auto conflicting_logic = conflict_fixture.logic_from(logic_data, "user.right.template");
        auto conflict_host = bruce::InstalledModHostV2::open(conflict_fixture.mods, conflict_trust);
        conflict_host.install(active_data); conflict_host.activate("user.left", "1.0.0");
        const auto before = conflict_host.snapshot();
        rejected([&] { conflict_host.install(conflicting_logic); });
        require(conflict_host.snapshot() == before, "Rejected cross-kind tree changed state");

        Fixture digest_fixture;
        auto [digest_one, digest_trust] = digest_fixture.data("user.digest", "1.0.0", 10, "one");
        auto [digest_two, digest_ignored] = digest_fixture.data("user.digest", "1.0.0", 11, "two");
        (void)digest_ignored;
        auto digest_host = bruce::InstalledModHostV2::open(digest_fixture.mods, digest_trust);
        digest_host.install(digest_one);
        rejected([&] { digest_host.install(digest_two); }, "InstalledModVersionConflict");
        require(digest_host.snapshot().data.records.size() == 1, "Digest conflict changed state");

        for (const auto* point : {"data.after_package", "data.before_journal", "data.after_journal", "data.after_snapshot"}) {
            Fixture crash;
            auto [candidate, crash_trust] = crash.data("user.crash", "1.0.0", 12, "crash");
            auto crash_host = bruce::InstalledModHostV2::open(crash.mods, crash_trust);
            bruce::runtime_mod_store_detail::set_test_failpoint(point);
            rejected([&] { crash_host.install(candidate); }, "RuntimeModTestFailpoint");
            bruce::runtime_mod_store_detail::clear_test_failpoint();
            const auto recovered = bruce::InstalledModHostV2::open_existing(crash.mods).snapshot();
            require(recovered.data.records.size() <= 1 && recovered.logic.records.empty(),
                    "Data failpoint recovered a split state");
        }
        for (const auto* point : {"logic.after_package", "logic.after_worker", "logic.before_journal",
                                  "logic.after_journal", "logic.after_snapshot"}) {
            Fixture crash;
            auto [embedded, crash_trust] = crash.data("user.logic-crash", "1.0.0", 13, "logiccrash");
            auto candidate = crash.logic_from(embedded, "user.logic-crash.template");
            auto crash_host = bruce::InstalledModHostV2::open(crash.mods, crash_trust);
            bruce::runtime_mod_store_detail::set_test_failpoint(point);
            rejected([&] { crash_host.install(candidate); }, "RuntimeModTestFailpoint");
            bruce::runtime_mod_store_detail::clear_test_failpoint();
            const auto recovered = bruce::InstalledModHostV2::open_existing(crash.mods).snapshot();
            require(recovered.logic.records.size() <= 1 && recovered.data.records.empty(),
                    "Logic failpoint recovered a split state");
        }

        // A crash while writing the first immutable generation is still
        // before the root-wide migration fence and must remain a usable V1 root.
        {
            Fixture orphan;
            auto [package, orphan_trust] = orphan.data("user.orphan", "1.0.0", 19, "orphan");
            auto orphan_host = bruce::InstalledModHostV2::open(orphan.mods, orphan_trust);
            orphan_host.install(package); orphan_host.activate("user.orphan", "1.0.0");
            FixedClock orphan_clock;
            bruce::runtime_mod_store_detail::set_test_failpoint("trust.after_generation");
            rejected([&] { orphan_host.import_trust_v1(orphan_clock); },
                     "RuntimeModTestFailpoint");
            bruce::runtime_mod_store_detail::clear_test_failpoint();
            auto reopened = bruce::InstalledModHostV2::open_existing(orphan.mods);
            require(reopened.snapshot().data.records[0].state == bruce::RuntimeModStateV1::active &&
                    reopened.route("orphan").route.status == bruce::RouteStatusV1::matched,
                    "Pre-fence orphan generation blocked legacy host operations");
            std::filesystem::create_symlink("generations/00000000000000000001",
                                            orphan.mods + "/trust/current");
            rejected([&] { (void)bruce::InstalledModHostV2::open_existing(orphan.mods); },
                     "InvalidRuntimeTrustV2");
        }
        {
            Fixture malformed;
            auto [package, malformed_trust] = malformed.data(
                "user.malformed-orphan", "1.0.0", 18, "malformed-orphan");
            auto malformed_host = bruce::InstalledModHostV2::open(malformed.mods, malformed_trust);
            malformed_host.install(package);
            FixedClock malformed_clock;
            bruce::runtime_mod_store_detail::set_test_failpoint("trust.after_generation");
            rejected([&] { malformed_host.import_trust_v1(malformed_clock); },
                     "RuntimeModTestFailpoint");
            bruce::runtime_mod_store_detail::clear_test_failpoint();
            bruce::LocalFiles::replace_atomically(malformed.mods +
                "/trust/generations/00000000000000000001/metadata.json", "{}\n");
            rejected([&] { (void)bruce::InstalledModHostV2::open_existing(malformed.mods); },
                     "InvalidRuntimeTrustV2");
        }

        // A pre-fence Prepare must neither block normal host use nor make an
        // explicit retry impossible. Retry must capture intervening V1 writes.
        for (const auto* point : {"trust.after_generation", "trust.after_prepare", "trust.after_snapshot"}) {
            Fixture interrupted;
            auto [first, keys] = interrupted.data("user.retry", "1.0.0", 19, "retry-one");
            auto [second, ignored_keys] = interrupted.data("user.retry", "2.0.0", 24, "retry-two");
            (void)ignored_keys;
            auto [embedded, ignored_logic_keys] = interrupted.data("user.retry-logic", "1.0.0", 25, "retry-logic");
            (void)ignored_logic_keys;
            const auto logic = interrupted.logic_from(embedded, "user.retry-logic.template");
            auto original = bruce::InstalledModHostV2::open(interrupted.mods, keys);
            original.install(first); original.activate("user.retry", "1.0.0");
            original.install(logic); original.activate("user.retry-logic", "1.0.0");
            const auto before = original.snapshot();
            FixedClock retry_clock;
            bruce::runtime_mod_store_detail::set_test_failpoint(point);
            rejected([&] { original.import_trust_v1(retry_clock); }, "RuntimeModTestFailpoint");
            bruce::runtime_mod_store_detail::clear_test_failpoint();
            auto reopened = bruce::InstalledModHostV2::open_existing(interrupted.mods);
            require(reopened.snapshot() == before &&
                reopened.route("retry-one").route.status == bruce::RouteStatusV1::matched &&
                reopened.route("retry-logic").route.status == bruce::RouteStatusV1::matched,
                "Pre-fence import blocked modern host reads");
            reopened.install(second); reopened.update(second);
            reopened.disable("user.retry-logic"); reopened.activate("user.retry-logic", "1.0.0");
            const auto updated = reopened.snapshot();
            reopened.import_trust_v1(retry_clock);
            require(reopened.snapshot() == updated &&
                reopened.route("retry-two").route.status == bruce::RouteStatusV1::matched,
                "Retried import restored stale pre-fence registry snapshots");
            require(reopened.verify_trust().state.active->generation == "00000000000000000001",
                "Retried import did not finish generation one");
            rejected([&] { reopened.import_trust_v1(retry_clock); }, "RuntimeTrustAlreadyImported");
        }

        // Trust activation belongs to the installed host because it must
        // commit Data/Logic deactivation and the trust switch as one unit.
        Fixture rotation;
        Fixture replacement("test-new-key",
            "4ccd089b28ff96da9db6c346ec114e0f" "5b8a319f35aba624da8cf6ed4fb8a6fb");
        auto [old_data, rotation_trust] = rotation.data("user.rotate-data", "1.0.0", 20, "rotate-data");
        auto [old_data_v2, ignored_v2_trust] = rotation.data("user.rotate-data", "2.0.0", 23, "rotate-data-v2");
        (void)ignored_v2_trust;
        auto [old_logic_data, ignored_logic_trust] = rotation.data("user.rotate-logic", "1.0.0", 21, "rotate-logic");
        (void)ignored_logic_trust;
        const auto old_logic = rotation.logic_from(old_logic_data, "user.rotate-logic.template");
        auto [replacement_data, replacement_trust] = replacement.data(
            "user.replacement", "1.0.0", 22, "replacement");
        (void)replacement_data;
        auto rotation_host = bruce::InstalledModHostV2::open(rotation.mods, rotation_trust);
        rotation_host.install(old_data); rotation_host.activate("user.rotate-data", "1.0.0");
        rotation_host.install(old_data_v2);
        rotation_host.install(old_logic); rotation_host.activate("user.rotate-logic", "1.0.0");
        auto legacy_data_api = bruce::RuntimeModRegistryV1::open_existing(rotation.mods);
        require(legacy_data_api.route("rotate-data").route.status ==
                    bruce::RouteStatusV1::matched,
                "Legacy V1 completed route failed before migration");
        FixedClock clock;
        rotation_host.import_trust_v1(clock);
        rejected([&] { (void)legacy_data_api.route("rotate-data"); },
                 "RuntimeTrustCoordinatorRequired");
        const auto combined_trust_path = replacement.root + "/combined.pub";
        auto combined_keys = rotation_trust.public_keys();
        combined_keys.insert(replacement_trust.public_keys().begin(), replacement_trust.public_keys().end());
        std::string combined = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n";
        for (const auto& [id, key] : combined_keys) combined += id + ':' + key + '\n';
        write(combined_trust_path, combined);
        rotation_host.prepare_trust(combined_trust_path, clock);
        require(rotation_host.activate_trust(false).deactivated.empty(),
                "Retained-key activation unexpectedly deactivated a Mod");
        rotation_host.install(replacement_data);
        rotation_host.activate("user.replacement", "1.0.0");

        const auto next_trust_path = replacement.root + "/next.pub";
        write(next_trust_path, trust_bytes(replacement_trust));
        rotation_host.prepare_trust(next_trust_path, clock);

        const auto byte_snapshot = [&] {
            return bruce::LocalFiles::read(rotation.mods + "/trust/journal.v1") +
                bruce::LocalFiles::read(rotation.mods + "/trust/state.json") +
                bruce::LocalFiles::read(rotation.mods + "/truststore.pub") +
                bruce::LocalFiles::read(rotation.mods + "/journal.v1") +
                bruce::LocalFiles::read(rotation.mods + "/registry.v1") +
                bruce::LocalFiles::read(rotation.mods + "/logic-journal.v1") +
                bruce::LocalFiles::read(rotation.mods + "/logic-registry.v1");
        };
        const auto before_blocked = byte_snapshot();
        rejected([&] { (void)rotation_host.activate_trust(false); },
                 "RuntimeTrustActiveModsIncompatible");
        require(byte_snapshot() == before_blocked,
                "Blocked trust activation was not byte-identical");

        const auto activated = rotation_host.activate_trust(true);
        require(activated.deactivated == std::vector<bruce::RuntimeAffectedModV2>{
            {bruce::RuntimeInstalledModKindV2::data, "user.rotate-data", "1.0.0", "test-rfc8032"},
            {bruce::RuntimeInstalledModKindV2::logic, "user.rotate-logic", "1.0.0", "test-rfc8032"}},
            "Emergency activation did not report the canonical exact affected set");
        const auto rotated = rotation_host.snapshot();
        const auto data_state = [&](std::string_view id, std::string_view version) {
            const auto found = std::ranges::find_if(rotated.data.records, [&](const auto& record) {
                return record.mod_id == id && record.version == version;
            });
            require(found != rotated.data.records.end(), "Rotated Data record is missing");
            return found->state;
        };
        require(data_state("user.rotate-data", "1.0.0") == bruce::RuntimeModStateV1::disabled &&
                data_state("user.rotate-data", "2.0.0") == bruce::RuntimeModStateV1::installed_inactive &&
                data_state("user.replacement", "1.0.0") == bruce::RuntimeModStateV1::active &&
                rotated.logic.records[0].state == bruce::LogicModStateV1::disabled,
                "Emergency activation did not deactivate both registries");
        rejected([&] { (void)legacy_data_api.route("rotate-data"); },
                 "RuntimeTrustCoordinatorRequired");
        const auto old_record = std::ranges::find_if(rotated.data.records, [](const auto& record) {
            return record.mod_id == "user.rotate-data" && record.version == "1.0.0";
        });
        require(old_record != rotated.data.records.end() &&
                bruce::LocalFiles::read(rotation.mods + "/packages/" +
                    old_record->package_digest + ".brucemod") == old_data.data_package()->bytes(),
                "Emergency activation rewrote a retained package");
        const auto logic_record = rotated.logic.records.front();
        require(bruce::LocalFiles::read(rotation.mods + "/logic-packages/" +
                    logic_record.package_digest + ".brucemod") == old_logic.logic_package()->bytes() &&
                bruce::LocalFiles::read(rotation.mods + "/logic-workers/" +
                    logic_record.package_digest + '/' + old_logic.logic_package()->worker_path()) ==
                    old_logic.logic_package()->worker_bytes(rotation_trust),
                "Emergency activation rewrote a Logic package or worker");
        require(rotation_host.verify_trust().state.active->generation == "00000000000000000003",
                "Emergency activation did not switch trust");
        require(rotation_host.route("rotate-data").route.status == bruce::RouteStatusV1::unknown &&
                rotation_host.route("replacement").route.status == bruce::RouteStatusV1::matched,
                "Fresh host routing did not use the active V2 trust/state");
        rejected([&] { (void)rotation_host.run_logic(
            "user.rotate-logic", "user.rotate-logic.template", "rotate-logic", {}); },
            "LogicModNotActive");
        const auto stale_before = rotation_host.snapshot();
        rejected([&] { rotation_host.update(old_data); });
        rejected([&] { rotation_host.activate("user.rotate-data", "2.0.0"); });
        require(rotation_host.snapshot() == stale_before,
                "Stale update or activation changed installed state");

        // A stale caller-held V1 trust object cannot authorize a package after
        // migration; every mutation resolves the active V2 trust under lock.
        rejected([&] { rotation_host.install(old_data); });
        const auto rollback = rotation_host.rollback_trust(false);
        require(rollback.deactivated.empty() &&
                rotation_host.verify_trust().state.active->generation == "00000000000000000002",
                "Safe rollback did not restore the previous trust generation");

        // Replacing bytes under the same key ID is a trust loss, not key retention.
        {
            Fixture same_id;
            Fixture changed_same_id("test-rfc8032",
                "4ccd089b28ff96da9db6c346ec114e0f" "5b8a319f35aba624da8cf6ed4fb8a6fb");
            auto [same_package, same_trust] = same_id.data(
                "user.same-id", "1.0.0", 27, "same-id");
            auto [ignored_changed_package, changed_trust] = changed_same_id.data(
                "user.changed", "1.0.0", 28, "changed");
            (void)ignored_changed_package;
            auto same_host = bruce::InstalledModHostV2::open(same_id.mods, same_trust);
            same_host.install(same_package); same_host.activate("user.same-id", "1.0.0");
            same_host.import_trust_v1(clock);
            const auto changed_path = changed_same_id.root + "/changed.pub";
            write(changed_path, trust_bytes(changed_trust));
            same_host.prepare_trust(changed_path, clock);
            rejected([&] { (void)same_host.activate_trust(false); },
                     "RuntimeTrustActiveModsIncompatible");
            require(same_host.verify_trust().state.active->generation == "00000000000000000001",
                    "Same-ID replacement inherited old-key authorization");
        }

        // Rollback applies the same default block and atomic emergency rule.
        {
            Fixture rollback_old;
            Fixture rollback_new("test-new-key",
                "4ccd089b28ff96da9db6c346ec114e0f" "5b8a319f35aba624da8cf6ed4fb8a6fb");
            auto [old_package, old_trust] = rollback_old.data(
                "user.rollback-old", "1.0.0", 29, "rollback-old");
            auto [new_package, new_trust] = rollback_new.data(
                "user.rollback-new", "1.0.0", 30, "rollback-new");
            auto rollback_host = bruce::InstalledModHostV2::open(rollback_old.mods, old_trust);
            rollback_host.install(old_package); rollback_host.activate("user.rollback-old", "1.0.0");
            rollback_host.import_trust_v1(clock);
            auto both_keys = old_trust.public_keys();
            both_keys.insert(new_trust.public_keys().begin(), new_trust.public_keys().end());
            std::string both = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n";
            for (const auto& [id, key] : both_keys) both += id + ':' + key + '\n';
            const auto both_path = rollback_new.root + "/both.pub"; write(both_path, both);
            rollback_host.prepare_trust(both_path, clock);
            (void)rollback_host.activate_trust(false);
            rollback_host.install(new_package); rollback_host.activate("user.rollback-new", "1.0.0");
            const auto rollback_before = rollback_host.snapshot();
            rejected([&] { (void)rollback_host.rollback_trust(false); },
                     "RuntimeTrustActiveModsIncompatible");
            require(rollback_host.snapshot() == rollback_before,
                    "Blocked rollback changed registry state");
            const auto emergency = rollback_host.rollback_trust(true);
            require(emergency.deactivated == std::vector<bruce::RuntimeAffectedModV2>{
                {bruce::RuntimeInstalledModKindV2::data, "user.rollback-new", "1.0.0", "test-new-key"}},
                "Emergency rollback deactivated the wrong exact set");
            const auto rolled = rollback_host.snapshot();
            const auto new_record = std::ranges::find_if(rolled.data.records, [](const auto& record) {
                return record.mod_id == "user.rollback-new";
            });
            require(new_record != rolled.data.records.end() &&
                    new_record->state == bruce::RuntimeModStateV1::disabled &&
                    rollback_host.verify_trust().state.active->generation == "00000000000000000001",
                    "Emergency rollback did not atomically disable and switch trust");
        }

        // Real host operations serialize route computation and successful
        // worker execution against an incompatible trust transition.
        {
            Fixture concurrent;
            Fixture concurrent_next("test-new-key",
                "4ccd089b28ff96da9db6c346ec114e0f" "5b8a319f35aba624da8cf6ed4fb8a6fb");
            auto [route_package, concurrent_trust] = concurrent.data(
                "user.concurrent-route", "1.0.0", 31, "concurrent-route");
            auto route_host = bruce::InstalledModHostV2::open(concurrent.mods, concurrent_trust);
            route_host.install(route_package); route_host.activate("user.concurrent-route", "1.0.0");
            route_host.import_trust_v1(clock);
            auto [ignored_next, concurrent_next_trust] = concurrent_next.data(
                "user.concurrent-next", "1.0.0", 32, "concurrent-next");
            (void)ignored_next;
            const auto concurrent_next_path = concurrent_next.root + "/next.pub";
            write(concurrent_next_path, trust_bytes(concurrent_next_trust));
            route_host.prepare_trust(concurrent_next_path, clock);
            std::promise<void> release;
            const auto gate = release.get_future().share();
            auto routed = std::async(std::launch::async, [&] {
                gate.wait(); return route_host.route("concurrent-route");
            });
            auto switched = std::async(std::launch::async, [&] {
                gate.wait(); return route_host.activate_trust(true);
            });
            release.set_value();
            require(routed.wait_for(std::chrono::seconds(2)) == std::future_status::ready &&
                    switched.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
                    "Concurrent route/trust transition deadlocked");
            const auto routed_status = routed.get().route.status;
            (void)switched.get();
            require((routed_status == bruce::RouteStatusV1::matched ||
                     routed_status == bruce::RouteStatusV1::unknown) &&
                    route_host.route("concurrent-route").route.status == bruce::RouteStatusV1::unknown,
                    "Concurrent route observed a mixed transition state");
        }
        {
            Fixture executing;
            Fixture executing_next("test-new-key",
                "4ccd089b28ff96da9db6c346ec114e0f" "5b8a319f35aba624da8cf6ed4fb8a6fb");
            auto [logic_data, executing_trust] = executing.data(
                "user.concurrent-logic", "1.0.0", 33, "concurrent-logic");
            const auto worker = bruce::LocalFiles::read(BRUCE_INSTALLED_LOGIC_WORKER);
            auto logic_package = executing.logic_from(
                logic_data, "user.concurrent-logic.template", worker);
            auto execution_host = bruce::InstalledModHostV2::open(executing.mods, executing_trust);
            execution_host.install(logic_package);
            execution_host.activate("user.concurrent-logic", "1.0.0");
            execution_host.import_trust_v1(clock);
            auto [ignored_next, executing_next_trust] = executing_next.data(
                "user.execution-next", "1.0.0", 34, "execution-next");
            (void)ignored_next;
            const auto executing_next_path = executing_next.root + "/next.pub";
            write(executing_next_path, trust_bytes(executing_next_trust));
            execution_host.prepare_trust(executing_next_path, clock);
            auto running = std::async(std::launch::async, [&] {
                bruce::LogicModBudgetsV1 budgets; budgets.wall_ms = 300;
                return execution_host.run_logic("user.concurrent-logic",
                    "user.concurrent-logic.template", "timeout", budgets);
            });
            require(running.wait_for(std::chrono::milliseconds(60)) == std::future_status::timeout,
                    "Static worker did not reach a running timeout state");
            auto switch_during_run = std::async(std::launch::async, [&] {
                return execution_host.activate_trust(true);
            });
            require(switch_during_run.wait_for(std::chrono::milliseconds(60)) ==
                        std::future_status::timeout,
                    "Trust switched while an authorized worker execution held registry.lock");
            const auto finished = running.get();
            require(finished.run.status == "TIMEOUT" && !finished.run.accepted,
                    "Static worker was not successfully started and supervised");
            const auto switched = switch_during_run.get();
            require(switched.deactivated.size() == 1 &&
                    switched.deactivated[0].kind == bruce::RuntimeInstalledModKindV2::logic,
                    "Post-start transition did not deactivate the exact Logic Mod");
        }

        // Recovery must expose a complete old or complete new cross-registry
        // state at every coordinator/projection boundary.
        for (const auto* point : {"trust.after_prepare", "trust.after_snapshot",
                                  "trust.after_commit", "trust.after_data_journal",
                                  "trust.after_data_projection", "trust.after_logic_journal",
                                  "trust.after_logic_projection", "trust.after_projection"}) {
            Fixture crash_rotation;
            Fixture crash_next("test-new-key",
                "4ccd089b28ff96da9db6c346ec114e0f" "5b8a319f35aba624da8cf6ed4fb8a6fb");
            auto [crash_data, crash_trust] = crash_rotation.data(
                "user.crash-data", "1.0.0", 24, "crash-data");
            auto [crash_logic_data, ignored_crash_logic] = crash_rotation.data(
                "user.crash-logic", "1.0.0", 25, "crash-logic");
            (void)ignored_crash_logic;
            auto crash_logic = crash_rotation.logic_from(
                crash_logic_data, "user.crash-logic.template");
            auto [ignored_next_package, crash_next_trust] = crash_next.data(
                "user.next", "1.0.0", 26, "crash-next");
            (void)ignored_next_package;
            auto crash_host = bruce::InstalledModHostV2::open(crash_rotation.mods, crash_trust);
            crash_host.install(crash_data); crash_host.activate("user.crash-data", "1.0.0");
            crash_host.install(crash_logic); crash_host.activate("user.crash-logic", "1.0.0");
            crash_host.import_trust_v1(clock);
            const auto crash_next_path = crash_next.root + "/next.pub";
            write(crash_next_path, trust_bytes(crash_next_trust));
            crash_host.prepare_trust(crash_next_path, clock);
            bruce::runtime_mod_store_detail::set_test_failpoint(point);
            rejected([&] { (void)crash_host.activate_trust(true); },
                     "RuntimeModTestFailpoint");
            bruce::runtime_mod_store_detail::clear_test_failpoint();
            auto recovered_host = bruce::InstalledModHostV2::open_existing(crash_rotation.mods);
            const auto recovered = recovered_host.snapshot();
            const auto recovered_trust = recovered_host.verify_trust().state;
            const bool committed = std::string_view(point) != "trust.after_prepare" &&
                                   std::string_view(point) != "trust.after_snapshot";
            require(recovered_trust.active->generation ==
                        (committed ? "00000000000000000002" : "00000000000000000001") &&
                    recovered.data.records[0].state == (committed
                        ? bruce::RuntimeModStateV1::disabled : bruce::RuntimeModStateV1::active) &&
                    recovered.logic.records[0].state == (committed
                        ? bruce::LogicModStateV1::disabled : bruce::LogicModStateV1::active),
                    "Combined trust recovery exposed a mixed state");
        }
        std::cout << "installed mod host v2 contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
