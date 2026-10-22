#include "bruce/mod/logic_mod_registry_v1.hpp"
#include "bruce/mod/local_builder.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/core/types.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
template<class T>
concept ExposesDetachedBinding = requires(const T& value) {
    value.bind("user.logic", "1.0.0", "math.rational.power.v1", "worker.execute");
};
static_assert(!ExposesDetachedBinding<bruce::DurableLogicModRegistryV1>);

const std::string seed = "9d61b19deffd5a60ba844af492ec2cc4" "4449c5697b326919703bac031cae7f60";
const std::string tree = "{\"mount\":[1,1],\"nodes\":[{\"aliases\":[\"hoch\"],\"claim_scope\":\"candidate_only\",\"id\":[1,1,5],\"key\":\"power\",\"route_pattern\":[\"<rational>\",\"hoch\",\"<integer>\"],\"template_id\":\"math.rational.power.v1\"}]}";
const std::string golden = "[{\"input\":\"3/4 hoch 2\",\"status\":\"MATCHED\",\"template_id\":\"math.rational.power.v1\"}]";
void require(bool condition, const char* message) { if (!condition) throw bruce::Error(message); }
void write(const std::string& path, std::string_view bytes, unsigned mode = 0600) {
    std::ofstream output(path, std::ios::binary); output << bytes; output.close();
    require(!output.fail() && ::chmod(path.c_str(), mode) == 0, "Fixture write failed");
}
template<class F> void rejected(F action) {
    try { action(); } catch (const bruce::Error&) { return; }
    throw bruce::Error("Unsafe durable Logic operation accepted");
}
struct Fixture {
    std::string base, mod_root, signing;
    Fixture() {
        char pattern[] = "/tmp/bruce-durable-logic-XXXXXX";
        const auto made = ::mkdtemp(pattern); require(made, "mkdtemp failed");
        base = made; mod_root = base + "/mods"; signing = base + "/signing.env";
        write(signing, "BRUCE_MOD_ED25519_SEED_HEX=" + seed + "\n");
    }
    ~Fixture() { std::filesystem::remove_all(base); }
    std::pair<bruce::LogicModPackageV1, bruce::RuntimeModTrustStoreV1> package(
        std::string version, std::string worker = "durable worker bytes\n") {
        const auto tag = version;
        const auto data_source = base + "/data-" + tag;
        const auto logic_source = base + "/logic-" + tag;
        std::filesystem::create_directory(data_source);
        std::filesystem::create_directory(logic_source);
        write(data_source + "/manifest.json", "{\"mod_id\":\"user.logic\",\"signing_key_id\":\"test-rfc8032\",\"version\":\"" + version + "\"}");
        write(data_source + "/tree.json", tree); write(data_source + "/golden-cases.json", golden);
        const auto data_path = base + "/data-" + tag + ".brucemod";
        const auto data_trust = base + "/data-" + tag + ".pub";
        bruce::build_local_data_mod_v1({data_source, data_source + "/manifest.json", data_source + "/tree.json",
            data_source + "/golden-cases.json", data_path, data_trust, signing});
        std::filesystem::copy_file(data_path, logic_source + "/data.brucemod");
        write(logic_source + "/worker.bin", worker, 0700);
        write(logic_source + "/logic.json", "{\"capabilities\":[\"worker.execute\"],\"template_ids\":[\"math.rational.power.v1\"],\"worker_path\":\"worker.bin\",\"worker_sha256\":\"" + bruce::sha256(worker) + "\"}");
        const auto logic_path = base + "/logic-" + tag + ".brucemod";
        const auto logic_trust = base + "/logic-" + tag + ".pub";
        bruce::build_local_logic_mod_v1({logic_source, logic_path, logic_trust, signing});
        return {bruce::unpack_logic_mod_package_v1(bruce::LocalFiles::read(logic_path)),
            bruce::read_runtime_mod_truststore_v1(bruce::LocalFiles::read(logic_trust))};
    }
};
}

int main() {
    try {
        Fixture fixture;
        auto [one, trust] = fixture.package("1.0.0");
        auto [two, ignored_trust] = fixture.package("2.0.0");
        (void)ignored_trust;
        auto registry = bruce::DurableLogicModRegistryV1::open(fixture.mod_root, trust);
        registry.install(one);
        auto state = registry.snapshot();
        require(state.generation == 1 && state.records.size() == 1 &&
                state.records[0].state == bruce::LogicModStateV1::installed_inactive,
                "Logic install was not inactive");
        registry.activate("user.logic", "1.0.0");
        require(registry.snapshot().records[0].state == bruce::LogicModStateV1::active,
                "Logic activation did not persist");
        registry.install(two);
        registry.activate("user.logic", "2.0.0");
        state = registry.snapshot();
        require(state.records.size() == 2 && state.records[0].state == bruce::LogicModStateV1::disabled &&
                state.records[1].state == bruce::LogicModStateV1::active, "Side-by-side activation failed");
        registry.disable("user.logic");
        state = registry.snapshot();
        require(state.records[0].state == bruce::LogicModStateV1::disabled &&
                state.records[1].state == bruce::LogicModStateV1::disabled, "Disable did not persist");

        auto reopened = bruce::DurableLogicModRegistryV1::open_existing(fixture.mod_root);
        require(reopened.snapshot().records == state.records && reopened.snapshot().generation == state.generation,
                "Logic state did not survive restart");
        struct stat worker_status{};
        const auto worker_path = fixture.mod_root + "/logic-workers/" + one.package_digest() + "/worker.bin";
        require(::stat(worker_path.c_str(), &worker_status) == 0 && (worker_status.st_mode & 0777) == 0500 &&
                bruce::LocalFiles::read(worker_path) == "durable worker bytes\n", "Extracted worker contract mismatch");

        std::filesystem::remove(fixture.mod_root + "/logic-registry.v1");
        require(bruce::DurableLogicModRegistryV1::open_existing(fixture.mod_root).snapshot().records.size() == 2,
                "Journal did not recover missing snapshot");

        Fixture fifo_fixture;
        auto [fifo_package, fifo_trust] = fifo_fixture.package("1.0.0");
        auto fifo_registry = bruce::DurableLogicModRegistryV1::open(fifo_fixture.mod_root, fifo_trust);
        fifo_registry.install(fifo_package);
        const auto fifo_worker = fifo_fixture.mod_root + "/logic-workers/" + fifo_package.package_digest() + "/worker.bin";
        std::filesystem::remove(fifo_worker); ::mkfifo(fifo_worker.c_str(), 0500);
        rejected([&] { fifo_registry.activate("user.logic", "1.0.0"); });

        Fixture path_fixture;
        auto [path_package, path_trust] = path_fixture.package("1.0.0");
        auto path_registry = bruce::DurableLogicModRegistryV1::open(path_fixture.mod_root, path_trust);
        std::filesystem::remove(path_fixture.mod_root + "/logic-packages");
        std::filesystem::create_symlink(path_fixture.base, path_fixture.mod_root + "/logic-packages");
        rejected([&] { path_registry.install(path_package); });

        Fixture corrupt_fixture;
        auto [corrupt_package, corrupt_trust] = corrupt_fixture.package("1.0.0");
        auto corrupt = bruce::DurableLogicModRegistryV1::open(corrupt_fixture.mod_root, corrupt_trust);
        corrupt.install(corrupt_package);
        write(corrupt_fixture.mod_root + "/logic-journal.v1", "corrupt");
        rejected([&] { (void)corrupt.snapshot(); });

        Fixture concurrent_fixture;
        auto [concurrent_package, concurrent_trust] = concurrent_fixture.package("1.0.0");
        auto concurrent = bruce::DurableLogicModRegistryV1::open(concurrent_fixture.mod_root, concurrent_trust);
        concurrent.install(concurrent_package);
        const auto child = ::fork(); require(child >= 0, "fork failed");
        if (child == 0) {
            try { bruce::DurableLogicModRegistryV1::open_existing(concurrent_fixture.mod_root).activate("user.logic", "1.0.0"); _exit(0); }
            catch (...) { _exit(1); }
        }
        require(bruce::DurableLogicModRegistryV1::open_existing(concurrent_fixture.mod_root).snapshot().records.size() == 1,
                "Concurrent snapshot invalid");
        int status = 0; require(::waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
                                "Concurrent activation failed");
        std::cout << "durable logic mod registry contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
