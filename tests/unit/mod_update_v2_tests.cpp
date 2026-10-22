#include "bruce/mod/installed_mod_host_v2.hpp"
#include "bruce/mod/package_inspection_v2.hpp"
#include "bruce/mod/local_builder.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/core/types.hpp"
#include "bruce/tree/normalizer.hpp"
#include "../../src/mod/runtime_mod_store_internal.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

namespace {
void require(bool condition, const char* message) { if (!condition) throw bruce::Error(message); }
template<class F> void rejected(F action) {
    try { action(); } catch (const bruce::Error& error) {
        require(std::string_view(error.what()) == "RuntimeModTestFailpoint", "Wrong update interruption");
        return;
    }
    throw bruce::Error("Update failpoint did not interrupt");
}

void write(const std::string& path, std::string_view bytes, unsigned mode = 0600) {
    std::ofstream output(path, std::ios::binary);
    output << bytes; output.close();
    require(!output.fail() && ::chmod(path.c_str(), mode) == 0, "Fixture write failed");
}

struct LogicFixture {
    std::string root;
    LogicFixture() {
        char pattern[] = "/tmp/bruce-logic-update-v2-XXXXXX";
        const auto made = ::mkdtemp(pattern); require(made, "mkdtemp failed"); root = made;
        write(root + "/signing.env", "BRUCE_MOD_ED25519_SEED_HEX="
            "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60\n");
    }
    ~LogicFixture() {
        bruce::runtime_mod_store_detail::clear_test_failpoint();
        std::filesystem::remove_all(root);
    }
    std::pair<bruce::DetectedModPackageV2, bruce::RuntimeModTrustStoreV1> package(
        const std::string& version, const std::string& route, const std::string& worker) const {
        const auto source = root + '/' + version;
        std::filesystem::create_directory(source);
        std::filesystem::permissions(source, std::filesystem::perms::owner_all);
        write(source + "/manifest.json", "{\"mod_id\":\"user.logic-update\",\"signing_key_id\":\"test-rfc8032\",\"version\":\"" + version + "\"}");
        write(source + "/tree.json", "{\"mount\":[1,1],\"nodes\":[{\"aliases\":[\"" + route + "\"],\"claim_scope\":\"candidate_only\",\"id\":[1,1,15],\"key\":\"logic-update\",\"route_pattern\":[\"" + route + "\"],\"template_id\":\"user.logic-update.template\"}]}");
        write(source + "/golden-cases.json", "[{\"input\":\"" + route + "\",\"status\":\"MATCHED\",\"template_id\":\"user.logic-update.template\"}]");
        bruce::build_local_data_mod_v1({source, source + "/manifest.json", source + "/tree.json",
            source + "/golden-cases.json", source + "/data.brucemod", source + "/data.pub", root + "/signing.env"});
        write(source + "/worker.bin", worker, 0700);
        write(source + "/logic.json", "{\"capabilities\":[\"worker.execute\"],\"template_ids\":[\"user.logic-update.template\"],\"worker_path\":\"worker.bin\",\"worker_sha256\":\"" + bruce::sha256(worker) + "\"}");
        bruce::build_local_logic_mod_v1({source, source + "/logic.brucemod", source + "/logic.pub", root + "/signing.env"});
        return {bruce::detect_mod_package_v2(bruce::LocalFiles::read(source + "/logic.brucemod")),
            bruce::read_runtime_mod_truststore_v1(bruce::LocalFiles::read(source + "/logic.pub"))};
    }
};

void assert_active_logic(const bruce::InstalledModHostV2& host,
                         const bruce::LogicModPackageV1& package,
                         const std::string& worker, const std::string& route,
                         const std::string& inactive_route) {
    const auto snapshot = host.snapshot();
    const auto found = std::ranges::find_if(snapshot.logic.records, [&](const auto& record) {
        return record.mod_id == "user.logic-update" && record.state == bruce::LogicModStateV1::active;
    });
    require(found != snapshot.logic.records.end() && found->version == package.manifest().version &&
            found->package_digest == package.package_digest() &&
            found->worker_sha256 == bruce::sha256(worker),
            "Active Logic record differs from recovered version");
    const auto selected = host.route(route).route;
    require(selected.status == bruce::RouteStatusV1::matched && selected.candidates.size() == 1 &&
            selected.candidates[0].owner_id == "user.logic-update" &&
            selected.candidates[0].template_id == "user.logic-update.template",
            "Recovered active tree does not route the active Logic version");
    require(host.route(inactive_route).route.status == bruce::RouteStatusV1::unknown,
            "Recovered tree exposes the inactive Logic version");
}

void logic_update_crash_boundaries() {
    LogicFixture fixture;
    const std::string worker_v1 = "logic update worker version one\n";
    const std::string worker_v2 = "logic update worker version two\n";
    const auto [v1, trust] = fixture.package("1.0.0", "logicold", worker_v1);
    const auto [v2, trust_v2] = fixture.package("2.0.0", "logicnew", worker_v2);
    const auto& one = *v1.logic_package();
    const auto& two = *v2.logic_package();
    bruce::verify_mod_package_v2(v1, trust_v2);
    bruce::verify_mod_package_v2(v2, trust);
    require(one.package_digest() != two.package_digest() && one.bytes() != two.bytes() &&
            one.worker_sha256() != two.worker_sha256(), "Logic update fixtures are not distinct");
    struct Boundary { const char* point; bool committed; bool worker_published; };
    for (const auto boundary : {
            Boundary{"logic.after_package", false, false},
            Boundary{"logic.after_worker", false, true},
            Boundary{"logic.before_journal", false, true},
            Boundary{"logic.after_journal", true, true},
            Boundary{"logic.after_snapshot", true, true}}) {
        try {
            const auto mods = fixture.root + '/' + boundary.point;
            auto host = bruce::InstalledModHostV2::open(mods, trust);
            host.update(v1);
            const auto baseline = host.snapshot();
            require(baseline.data.generation == 0 && baseline.data.records.empty() &&
                    baseline.logic.generation == 1 && baseline.logic.records == std::vector<bruce::LogicModRecordV1>{
                        {"user.logic-update", "1.0.0", one.package_digest(), one.worker_sha256(), bruce::LogicModStateV1::active}},
                    "Logic update did not start with exactly v1 active");
            bruce::runtime_mod_store_detail::set_test_failpoint(boundary.point);
            rejected([&] { host.update(v2); });
            bruce::runtime_mod_store_detail::clear_test_failpoint();
            host = bruce::InstalledModHostV2::open_existing(mods);
            const auto recovered = host.snapshot();
            const std::vector<bruce::LogicModRecordV1> updated{
                {"user.logic-update", "1.0.0", one.package_digest(), one.worker_sha256(), bruce::LogicModStateV1::disabled},
                {"user.logic-update", "2.0.0", two.package_digest(), two.worker_sha256(), bruce::LogicModStateV1::active}};
            require(recovered.data == baseline.data, "Logic update changed the Data registry");
            if (boundary.committed) {
                require(recovered.logic.generation == 2 && recovered.logic.records == updated &&
                        recovered.logic.journal_digest != baseline.logic.journal_digest,
                        "Committed Logic update did not recover exactly disabled v1 and active v2");
                assert_active_logic(host, two, worker_v2, "logicnew", "logicold");
            } else {
                require(recovered == baseline, "Pre-journal Logic update exposed v2 or changed v1");
                assert_active_logic(host, one, worker_v1, "logicold", "logicnew");
            }
            const auto catalog = bruce::InstalledModHostV2::catalog_entries(mods);
            require(catalog.size() == recovered.logic.records.size(), "Recovered catalog has missing or duplicate records");
            for (std::size_t index = 0; index < catalog.size(); ++index) {
                const auto& record = recovered.logic.records[index];
                require(catalog[index].kind == bruce::ModKindV2::logic && catalog[index].id == record.mod_id &&
                        catalog[index].version == record.version && catalog[index].package_digest == record.package_digest &&
                        catalog[index].lifecycle_state == (record.state == bruce::LogicModStateV1::active ? "active" : "disabled"),
                        "Recovered catalog disagrees with exact Logic registry state");
            }
            require(bruce::LocalFiles::read(mods + "/logic-packages/" + one.package_digest() + ".brucemod") == one.bytes() &&
                    bruce::LocalFiles::read(mods + "/logic-workers/" + one.package_digest() + "/worker.bin") == worker_v1 &&
                    bruce::LocalFiles::read(mods + "/logic-packages/" + two.package_digest() + ".brucemod") == two.bytes(),
                    "Logic publication changed immutable package or old worker bytes");
            const auto new_worker = mods + "/logic-workers/" + two.package_digest() + "/worker.bin";
            require(std::filesystem::exists(new_worker) == boundary.worker_published,
                    "Logic interruption did not reach the intended worker publication boundary");
            if (boundary.worker_published)
                require(bruce::LocalFiles::read(new_worker) == worker_v2, "Published v2 worker has wrong bytes");
            require(bruce::InstalledModHostV2::open_existing(mods).snapshot() == recovered,
                    "Repeated reopen changed recovered Logic state");

            host.update(v2);
            const auto complete = host.snapshot();
            require(complete.data == baseline.data && complete.logic.generation == 2 && complete.logic.records == updated,
                    "Retry did not complete exactly one Logic update");
            assert_active_logic(host, two, worker_v2, "logicnew", "logicold");
            host.update(v2);
            require(host.snapshot() == complete, "Identical Logic re-update was not idempotent");
            require(bruce::LocalFiles::read(mods + "/logic-workers/" + one.package_digest() +
                        "/worker.bin") == worker_v1,
                    "Update changed the retained immutable v1 worker");

            host.activate("user.logic-update", "1.0.0");
            host = bruce::InstalledModHostV2::open_existing(mods);
            auto rolled_back = updated;
            rolled_back[0].state = bruce::LogicModStateV1::active;
            rolled_back[1].state = bruce::LogicModStateV1::disabled;
            const auto rollback = host.snapshot();
            require(rollback.data == baseline.data && rollback.logic.generation == 3 && rollback.logic.records == rolled_back,
                    "Explicit Logic rollback did not retain both versions and activate v1");
            assert_active_logic(host, one, worker_v1, "logicold", "logicnew");
        } catch (const std::exception& error) {
            throw bruce::Error(std::string(boundary.point) + ": " + error.what());
        }
    }
}
}

int main() {
    try {
        logic_update_crash_boundaries();
        char pattern[] = "/tmp/bruce-update-v2-XXXXXX";
        const auto root = ::mkdtemp(pattern); require(root, "mkdtemp failed");
        const auto package = bruce::detect_mod_package_v2(bruce::LocalFiles::read(
            std::string(BRUCE_RUNTIME_FIXTURE_DIR) + "/math-rational-power-v1.brucemod"));
        const auto trust = bruce::read_runtime_mod_truststore_v1(bruce::LocalFiles::read(
            std::string(BRUCE_RUNTIME_FIXTURE_DIR) + "/test-truststore-v1.pub"));
        auto host = bruce::InstalledModHostV2::open(root, trust);
        host.update(package);
        auto state = host.snapshot();
        require(state.data.generation == 1 && state.data.records.size() == 1 &&
                state.data.records[0].state == bruce::RuntimeModStateV1::active,
                "Update did not atomically install active");
        host.update(package);
        require(host.snapshot().data.generation == 1, "Active identical update was not idempotent");
        host.disable("user.math-rational-power");
        require(host.snapshot().data.generation == 2, "Disable generation mismatch");
        host.update(package);
        state = host.snapshot();
        require(state.data.generation == 3 && state.data.records[0].state == bruce::RuntimeModStateV1::active,
                "Inactive identical update did not reactivate once");
        host.disable("user.math-rational-power");
        bruce::runtime_mod_store_detail::set_test_failpoint("data.before_journal");
        rejected([&] { host.update(package); });
        bruce::runtime_mod_store_detail::clear_test_failpoint();
        host = bruce::InstalledModHostV2::open_existing(root);
        require(host.snapshot().data.records[0].state == bruce::RuntimeModStateV1::disabled,
                "Pre-journal interruption exposed update");
        bruce::runtime_mod_store_detail::set_test_failpoint("data.after_journal");
        rejected([&] { host.update(package); });
        bruce::runtime_mod_store_detail::clear_test_failpoint();
        host = bruce::InstalledModHostV2::open_existing(root);
        require(host.snapshot().data.records[0].state == bruce::RuntimeModStateV1::active,
                "Post-journal interruption did not recover complete update");
        std::filesystem::remove_all(root);
        std::cout << "mod update v2 contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
