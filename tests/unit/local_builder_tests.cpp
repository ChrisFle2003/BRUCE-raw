#include "bruce/mod/local_builder.hpp"
#include "bruce/mod/runtime_package.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/core/types.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

namespace {
// RFC 8032 public test vector. Never use this seed for a personal signing key.
const std::string test_seed = "9d61b19deffd5a60ba844af492ec2cc4" "4449c5697b326919703bac031cae7f60";
const std::string metadata = "{\"mod_id\":\"user.math-rational-power\",\"signing_key_id\":\"test-rfc8032\",\"version\":\"1.0.0\"}";
const std::string tree = "{\"mount\":[1,1],\"nodes\":[{\"aliases\":[\"hoch\"],\"claim_scope\":\"candidate_only\",\"id\":[1,1,5],\"key\":\"power\",\"route_pattern\":[\"<rational>\",\"hoch\",\"<integer>\"],\"template_id\":\"math.rational.power.v1\"}]}";
const std::string golden = "[{\"input\":\"3/4 hoch 2\",\"status\":\"MATCHED\",\"template_id\":\"math.rational.power.v1\"}]";
void require(bool ok, const char* message) { if (!ok) throw bruce::Error(message); }
void write(const std::string& path, const std::string& bytes, unsigned mode = 0600) {
    std::ofstream stream(path, std::ios::binary); stream << bytes; stream.close();
    require(!stream.fail() && ::chmod(path.c_str(), mode) == 0, "Fixture write failed");
}
struct Fixture {
    std::string root;
    bruce::LocalDataModBuildPathsV1 paths;
    Fixture() {
        char temp[] = "/tmp/bruce-builder-test-XXXXXX";
        const auto name = ::mkdtemp(temp); require(name != nullptr, "Temp fixture failed"); root = name;
        paths = {root, root + "/manifest.json", root + "/tree.json", root + "/golden.json",
                 root + "/output.brucemod", root + "/trust.pub", ""};
        write(root + "/.env", "BRUCE_MOD_ED25519_SEED_HEX=" + test_seed + "\n");
        write(paths.manifest_source, metadata); write(paths.tree_source, tree); write(paths.golden_source, golden);
    }
    ~Fixture() { std::filesystem::remove_all(root); }
};
template<class F> void rejected(F action) {
    try { action(); } catch (const std::exception& error) {
        const std::string text = error.what();
        require(text.find(test_seed) == std::string::npos && text.find("private-sentinel") == std::string::npos, "Secret diagnostic");
        return;
    }
    throw bruce::Error("Invalid build accepted");
}
void empty_outputs(const Fixture& f) {
    require(!std::filesystem::exists(f.paths.package_output) && !std::filesystem::exists(f.paths.truststore_output), "Failed build created outputs");
}
void contracts() {
    Fixture one, two, explicit_key;
    std::filesystem::rename(explicit_key.root + "/.env", explicit_key.root + "/signing.env");
    explicit_key.paths.signing_env = explicit_key.root + "/signing.env";
    const auto result = bruce::build_local_data_mod_v1(one.paths);
    (void)bruce::build_local_data_mod_v1(two.paths);
    (void)bruce::build_local_data_mod_v1(explicit_key.paths);
    const auto bytes = bruce::LocalFiles::read(one.paths.package_output);
    require(bytes == bruce::LocalFiles::read(two.paths.package_output), "Nondeterministic package");
    require(bytes == bruce::LocalFiles::read(explicit_key.paths.package_output) &&
            bruce::LocalFiles::read(one.paths.truststore_output) == bruce::LocalFiles::read(explicit_key.paths.truststore_output),
            "Explicit signing file changed output bytes");
    const auto fixture = bruce::LocalFiles::read(std::string(BRUCE_RUNTIME_FIXTURE_DIR) + "/math-rational-power-v1.brucemod");
    require(bytes == fixture, "Builder bytes disagree with independent signed vector");
    const auto trust_bytes = bruce::LocalFiles::read(one.paths.truststore_output);
    require(trust_bytes == "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\ntest-rfc8032:d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a\n", "Wrong public truststore");
    require(bytes.find(test_seed) == std::string::npos && trust_bytes.find(test_seed) == std::string::npos, "Secret copied into output");
    const auto package = bruce::unpack_runtime_data_mod_package_v1(bytes);
    bruce::verify_runtime_data_mod_package_v1(package, bruce::read_runtime_mod_truststore_v1(trust_bytes));
    require(result.mod_id == "user.math-rational-power" && result.version == "1.0.0" && result.package_digest == package.package_digest(), "Wrong build result");
    struct stat info{};
    require(::stat(one.paths.package_output.c_str(), &info) == 0 && (info.st_mode & 0777) == 0600 && info.st_nlink == 1, "Output not private single inode");
    rejected([&] { (void)bruce::build_local_data_mod_v1(one.paths); });
    require(bruce::LocalFiles::read(one.paths.package_output) == bytes && bruce::LocalFiles::read(one.paths.truststore_output) == trust_bytes, "Existing outputs replaced");
    for (const auto& invalid_key : {std::string(), std::string("BRUCE_MOD_ED25519_SEED_HEX=private-sentinel\n"), std::string("BRUCE_MOD_ED25519_SEED_HEX=") + test_seed + "\nBRUCE_MOD_ED25519_SEED_HEX=" + test_seed + "\n", std::string("OTHER=private-sentinel\n")}) {
        Fixture bad; write(bad.root + "/.env", invalid_key);
        rejected([&] { (void)bruce::build_local_data_mod_v1(bad.paths); }); empty_outputs(bad);
    }
    { Fixture bad; std::filesystem::remove(bad.root + "/.env"); rejected([&] { (void)bruce::build_local_data_mod_v1(bad.paths); }); empty_outputs(bad); }
    { Fixture bad; ::chmod((bad.root + "/.env").c_str(), 0644); rejected([&] { (void)bruce::build_local_data_mod_v1(bad.paths); }); empty_outputs(bad); }
    { Fixture bad; std::filesystem::rename(bad.root + "/.env", bad.root + "/key"); std::filesystem::create_symlink("key", bad.root + "/.env"); rejected([&] { (void)bruce::build_local_data_mod_v1(bad.paths); }); empty_outputs(bad); }
    { Fixture bad; std::filesystem::create_hard_link(bad.root + "/.env", bad.root + "/key"); rejected([&] { (void)bruce::build_local_data_mod_v1(bad.paths); }); empty_outputs(bad); }
    { Fixture bad; bad.paths.signing_env = bad.root + "/key.env"; std::filesystem::create_symlink(".env", bad.paths.signing_env); rejected([&] { (void)bruce::build_local_data_mod_v1(bad.paths); }); empty_outputs(bad); }
    for (const auto source : {0, 1, 2}) {
        Fixture bad;
        write(source == 0 ? bad.paths.manifest_source : source == 1 ? bad.paths.tree_source : bad.paths.golden_source, "private-sentinel");
        rejected([&] { (void)bruce::build_local_data_mod_v1(bad.paths); }); empty_outputs(bad);
    }
    { Fixture bad; write(bad.paths.manifest_source, "{\"mod_id\":\"a\",\"mod_id\":\"b\",\"signing_key_id\":\"x\",\"version\":\"1\"}"); rejected([&] { (void)bruce::build_local_data_mod_v1(bad.paths); }); empty_outputs(bad); }
    { Fixture bad; write(bad.paths.tree_source, " " + tree); rejected([&] { (void)bruce::build_local_data_mod_v1(bad.paths); }); empty_outputs(bad); }
    { Fixture bad; write(bad.paths.tree_source, std::string(1024 * 1024 + 1, 'x')); rejected([&] { (void)bruce::build_local_data_mod_v1(bad.paths); }); empty_outputs(bad); }
    { Fixture bad; write(bad.paths.truststore_output, "existing"); rejected([&] { (void)bruce::build_local_data_mod_v1(bad.paths); }); require(!std::filesystem::exists(bad.paths.package_output) && bruce::LocalFiles::read(bad.paths.truststore_output) == "existing", "Truststore replacement or orphan package"); }
    { Fixture bad; write(bad.paths.package_output, "existing"); rejected([&] { (void)bruce::build_local_data_mod_v1(bad.paths); }); require(!std::filesystem::exists(bad.paths.truststore_output) && bruce::LocalFiles::read(bad.paths.package_output) == "existing", "Package replacement or orphan truststore"); }
    { Fixture bad; bad.paths.truststore_output = bad.root + "/missing/trust.pub"; rejected([&] { (void)bruce::build_local_data_mod_v1(bad.paths); }); empty_outputs(bad); }
    { Fixture bad; bad.paths.truststore_output = bad.paths.package_output; rejected([&] { (void)bruce::build_local_data_mod_v1(bad.paths); }); empty_outputs(bad); }
    { Fixture bad; std::filesystem::create_symlink("missing", bad.paths.package_output); rejected([&] { (void)bruce::build_local_data_mod_v1(bad.paths); }); require(!std::filesystem::exists(bad.paths.truststore_output) && std::filesystem::is_symlink(bad.paths.package_output), "Symlink overwritten"); }
}
}
int main() {
    try { contracts(); std::cout << "local builder contracts passed\n"; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
