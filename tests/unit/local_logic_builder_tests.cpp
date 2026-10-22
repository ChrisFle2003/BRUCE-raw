#include "bruce/mod/local_builder.hpp"
#include "bruce/mod/logic_mod_plugin_v1.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/core/types.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

namespace {
const std::string seed = "9d61b19deffd5a60ba844af492ec2cc4" "4449c5697b326919703bac031cae7f60";
void require(bool condition, const char* message) { if (!condition) throw bruce::Error(message); }
void write(const std::string& path, std::string_view bytes, unsigned mode = 0600) {
    std::ofstream output(path, std::ios::binary); output << bytes; output.close();
    require(!output.fail() && ::chmod(path.c_str(), mode) == 0, "Fixture write failed");
}
std::string logic(std::string_view worker, std::string_view path = "worker.bin",
                  std::string_view extra = {}) {
    return "{\"capabilities\":[\"worker.execute\"]," + std::string(extra) +
        "\"template_ids\":[\"math.rational.power.v1\"],\"worker_path\":\"" +
        std::string(path) + "\",\"worker_sha256\":\"" + bruce::sha256(worker) + "\"}";
}
struct Fixture {
    std::string root, source, worker = "prebuilt worker bytes\n";
    bruce::LocalLogicModBuildPathsV1 paths;
    Fixture() {
        char pattern[] = "/tmp/bruce-logic-builder-XXXXXX";
        const auto made = ::mkdtemp(pattern); require(made, "mkdtemp failed");
        root = made; source = root + "/source"; std::filesystem::create_directory(source);
        write(root + "/signing.env", "BRUCE_MOD_ED25519_SEED_HEX=" + seed + "\n");
        write(source + "/data.brucemod", bruce::LocalFiles::read(
            std::string(BRUCE_RUNTIME_FIXTURE_DIR) + "/math-rational-power-v1.brucemod"));
        write(source + "/logic.json", logic(worker));
        write(source + "/worker.bin", worker, 0700);
        paths = {source, root + "/output.brucemod", root + "/trust.pub", root + "/signing.env"};
    }
    ~Fixture() { std::filesystem::remove_all(root); }
};
template<class F> void rejected(F action) {
    try { action(); } catch (const std::exception& error) {
        require(std::string_view(error.what()) == "LocalModBuildFailed", "Unsanitized Logic builder error");
        return;
    }
    throw bruce::Error("Invalid Logic source accepted");
}
void empty(const Fixture& fixture) {
    require(!std::filesystem::exists(fixture.paths.package_output) &&
            !std::filesystem::exists(fixture.paths.truststore_output), "Failed Logic build published output");
}
}

int main() {
    try {
        Fixture first, second;
        const auto result = bruce::build_local_logic_mod_v1(first.paths);
        (void)bruce::build_local_logic_mod_v1(second.paths);
        const auto bytes = bruce::LocalFiles::read(first.paths.package_output);
        require(bytes == bruce::LocalFiles::read(second.paths.package_output), "Logic package is nondeterministic");
        const auto trust_bytes = bruce::LocalFiles::read(first.paths.truststore_output);
        const auto package = bruce::unpack_logic_mod_package_v1(bytes);
        bruce::verify_logic_mod_package_v1(package, bruce::read_runtime_mod_truststore_v1(trust_bytes));
        require(result.mod_id == "user.math-rational-power" && result.version == "1.0.0" &&
                result.signing_key_id == "test-rfc8032" && result.package_digest == package.package_digest(),
                "Wrong Logic build result");
        require(package.worker_bytes(bruce::read_runtime_mod_truststore_v1(trust_bytes)) == first.worker,
                "Worker bytes changed");
        rejected([&] { (void)bruce::build_local_logic_mod_v1(first.paths); });

        for (const auto& invalid : {
            std::string("{}"),
            logic(first.worker, "../worker.bin"),
            logic(first.worker, "/worker.bin"),
            logic(first.worker, "nested/worker.bin"),
            logic(first.worker, "worker.bin", "\"extra\":true,"),
        }) {
            Fixture bad; write(bad.source + "/logic.json", invalid);
            rejected([&] { (void)bruce::build_local_logic_mod_v1(bad.paths); }); empty(bad);
        }
        { Fixture bad; write(bad.source + "/worker.bin", "changed", 0700); rejected([&] { (void)bruce::build_local_logic_mod_v1(bad.paths); }); empty(bad); }
        { Fixture bad; std::filesystem::remove(bad.source + "/worker.bin"); std::filesystem::create_symlink("../signing.env", bad.source + "/worker.bin"); rejected([&] { (void)bruce::build_local_logic_mod_v1(bad.paths); }); empty(bad); }
        { Fixture bad; std::filesystem::remove(bad.source + "/worker.bin"); ::mkfifo((bad.source + "/worker.bin").c_str(), 0600); rejected([&] { (void)bruce::build_local_logic_mod_v1(bad.paths); }); empty(bad); }
        { Fixture bad; write(bad.source + "/data.brucemod", "invalid"); rejected([&] { (void)bruce::build_local_logic_mod_v1(bad.paths); }); empty(bad); }
        { Fixture bad; write(bad.root + "/signing.env", "BRUCE_MOD_ED25519_SEED_HEX=" + std::string(64, '0') + "\n"); rejected([&] { (void)bruce::build_local_logic_mod_v1(bad.paths); }); empty(bad); }
        { Fixture bad; write(bad.paths.package_output, "existing"); rejected([&] { (void)bruce::build_local_logic_mod_v1(bad.paths); }); require(bruce::LocalFiles::read(bad.paths.package_output) == "existing", "Logic output replaced"); }
        std::cout << "local logic builder contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
