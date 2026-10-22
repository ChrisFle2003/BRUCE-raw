#include "bruce/mod/local_builder.hpp"
#include "bruce/core/types.hpp"
#include "bruce/mod/runtime_package.hpp"
#include "bruce/mod/logic_mod_plugin_v1.hpp"
#include "runtime_package_builder.hpp"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <filesystem>
#include <fcntl.h>
#include <linux/fs.h>
#include <memory>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace bruce {
namespace {
[[noreturn]] void failed() { throw Error("LocalModBuildFailed"); }
class Descriptor {
public:
    explicit Descriptor(int value) : fd(value) { if (fd < 0) failed(); }
    ~Descriptor() { if (fd >= 0) ::close(fd); }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    int fd;
};
// Walk with directory descriptors so symlink ancestors cannot redirect key or
// output access. The held final descriptor anchors subsequent operations.
int open_directory(const std::filesystem::path& path) {
    int fd = ::open(path.is_absolute() ? "/" : ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) failed();
    for (const auto& component : path.relative_path()) {
        if (component == "." || component.empty()) continue;
        if (component == "..") { ::close(fd); failed(); }
        const int next = ::openat(fd, component.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        ::close(fd); fd = next;
        if (fd < 0) failed();
    }
    return fd;
}
struct Secret {
    std::array<unsigned char, 32> seed{};
    std::array<char, 4097> env{};
    ~Secret() { OPENSSL_cleanse(seed.data(), seed.size()); OPENSSL_cleanse(env.data(), env.size()); }
};
void load_key(const std::string& path, Secret& secret) {
    const std::filesystem::path name(path);
    Descriptor parent(open_directory(name.parent_path()));
    Descriptor file(::openat(parent.fd, name.filename().c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK));
    struct stat info{};
    if (::fstat(file.fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_uid != ::geteuid() ||
        (info.st_mode & 0777) != 0600 || info.st_nlink != 1 || info.st_size > 4096) failed();
    std::size_t size = 0;
    while (size < secret.env.size()) {
        const auto count = ::read(file.fd, secret.env.data() + size, secret.env.size() - size);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) failed();
        if (!count) break;
        size += static_cast<std::size_t>(count);
    }
    constexpr std::string_view prefix = "BRUCE_MOD_ED25519_SEED_HEX=";
    std::string_view value(secret.env.data(), size);
    // Single unquoted key assignment, optional final LF, no shell evaluation.
    if (value.ends_with('\n')) value.remove_suffix(1);
    if (!value.starts_with(prefix)) failed();
    value.remove_prefix(prefix.size());
    if (value.size() != secret.seed.size() * 2) failed();
    const auto nibble = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        failed();
    };
    for (std::size_t i = 0; i < secret.seed.size(); ++i)
        secret.seed[i] = static_cast<unsigned char>(nibble(value[2 * i]) * 16 + nibble(value[2 * i + 1]));
}
struct Signature {
    std::string signature_hex;
    std::string public_key_hex;
};
std::string hex(const unsigned char* data, std::size_t size);
Signature sign_bytes(const std::string_view bytes, const Secret& secret) {
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, secret.seed.data(), secret.seed.size()),
        EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    unsigned char signature[64], public_key[32];
    std::size_t signature_size = sizeof(signature), public_size = sizeof(public_key);
    if (!key || !context || EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1 ||
        EVP_DigestSign(context.get(), signature, &signature_size,
            reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()) != 1 ||
        signature_size != sizeof(signature) ||
        EVP_PKEY_get_raw_public_key(key.get(), public_key, &public_size) != 1 ||
        public_size != sizeof(public_key)) failed();
    return {hex(signature, sizeof(signature)), hex(public_key, sizeof(public_key))};
}
std::string read_source(const std::string& path, const bool owner_single_link = false) {
    const std::filesystem::path name(path);
    Descriptor parent(open_directory(name.parent_path()));
    Descriptor file(::openat(parent.fd, name.filename().c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK));
    struct stat info{};
    if (::fstat(file.fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0 ||
        (owner_single_link && (info.st_uid != ::geteuid() || info.st_nlink != 1)) ||
        static_cast<unsigned long long>(info.st_size) > max_runtime_mod_package_bytes_v1) failed();
    std::string result;
    std::array<char, 8192> buffer{};
    while (true) {
        const auto count = ::read(file.fd, buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) failed();
        if (!count) break;
        if (static_cast<std::size_t>(count) > max_runtime_mod_package_bytes_v1 - result.size()) failed();
        result.append(buffer.data(), static_cast<std::size_t>(count));
    }
    struct stat after{};
    if (::fstat(file.fd, &after) != 0 || info.st_dev != after.st_dev || info.st_ino != after.st_ino ||
        info.st_size != after.st_size || info.st_mtim.tv_sec != after.st_mtim.tv_sec ||
        info.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
        static_cast<unsigned long long>(after.st_size) != result.size()) failed();
    return result;
}
std::string hex(const unsigned char* data, std::size_t size) {
    constexpr char alphabet[] = "0123456789abcdef";
    std::string result; result.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) { result += alphabet[data[i] >> 4]; result += alphabet[data[i] & 15]; }
    return result;
}
class Output {
public:
    explicit Output(const std::string& path)
        : directory(open_directory(std::filesystem::path(path).parent_path())), name(std::filesystem::path(path).filename()) {
        struct stat parent{};
        if (name.empty() || name == "." || name == ".." || ::fstat(directory.fd, &parent) != 0 ||
            parent.st_uid != ::geteuid() || (parent.st_mode & 0022)) failed();
        parent_device = parent.st_dev; parent_inode = parent.st_ino;
        struct stat info{};
        if (::fstatat(directory.fd, name.c_str(), &info, AT_SYMLINK_NOFOLLOW) == 0 || errno != ENOENT) failed();
    }
    ~Output() { if (!temporary.empty()) ::unlinkat(directory.fd, temporary.c_str(), 0); }
    bool same_target(const Output& other) const { return parent_device == other.parent_device && parent_inode == other.parent_inode && name == other.name; }
    void stage(std::string_view bytes) {
        unsigned char random[16];
        if (RAND_bytes(random, sizeof(random)) != 1) failed();
        const auto candidate = ".bruce-builder-" + hex(random, sizeof(random));
        Descriptor file(::openat(directory.fd, candidate.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
        temporary = candidate;
        if (::fstat(file.fd, &staged) != 0) failed();
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto count = ::write(file.fd, bytes.data() + offset, bytes.size() - offset);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) failed();
            offset += static_cast<std::size_t>(count);
        }
        if (::fsync(file.fd) != 0) failed();
    }
    void publish() {
        if (::syscall(SYS_renameat2, directory.fd, temporary.c_str(), directory.fd, name.c_str(), RENAME_NOREPLACE) != 0) failed();
        temporary.clear(); published = true;
        if (::fsync(directory.fd) != 0) failed();
    }
    void rollback() noexcept {
        struct stat current{};
        if (published && ::fstatat(directory.fd, name.c_str(), &current, AT_SYMLINK_NOFOLLOW) == 0 &&
            current.st_dev == staged.st_dev && current.st_ino == staged.st_ino) {
            ::unlinkat(directory.fd, name.c_str(), 0);
            ::fsync(directory.fd);
        }
    }
private:
    Descriptor directory;
    std::filesystem::path name;
    dev_t parent_device{};
    ino_t parent_inode{};
    struct stat staged{};
    std::string temporary;
    bool published = false;
};
std::string package_member(const std::string_view name, const std::string_view bytes) {
    return std::to_string(name.size()) + ':' + std::string(name) +
        std::to_string(bytes.size()) + ':' + std::string(bytes);
}
std::string logic_worker_path(const std::string_view logic) {
    constexpr std::string_view marker = "\"worker_path\":\"";
    const auto begin = logic.find(marker);
    if (begin == std::string_view::npos || logic.find(marker, begin + marker.size()) != std::string_view::npos) failed();
    const auto value_begin = begin + marker.size();
    const auto end = logic.find('"', value_begin);
    if (end == std::string_view::npos) failed();
    const auto value = logic.substr(value_begin, end - value_begin);
    if (value.empty() || value.size() > 128 || value == "data.brucemod" || value == "logic.json" ||
        value == "signature.ed25519" || !std::ranges::all_of(value, [](const char ch) {
            return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '_';
        })) failed();
    return std::string(value);
}
void publish_pair(const std::string& package_path, const std::string& trust_path,
                  const std::string_view package_bytes, const std::string_view trust_bytes) {
    Output package_output(package_path), trust_output(trust_path);
    if (package_output.same_target(trust_output)) failed();
    package_output.stage(package_bytes); trust_output.stage(trust_bytes);
    try { package_output.publish(); trust_output.publish(); }
    catch (...) { trust_output.rollback(); package_output.rollback(); throw; }
}
}
LocalDataModBuildResultV1 build_local_data_mod_v1(const LocalDataModBuildPathsV1& paths) {
    return build_local_data_mod_v1(paths, {});
}
LocalDataModBuildResultV1 build_local_data_mod_v1(
    const LocalDataModBuildPathsV1& paths, const LocalDataModBeforePublishV1& before_publish) {
    try {
        Secret secret;
        const auto signing_env = paths.signing_env.empty()
            ? (std::filesystem::path(paths.project_root) / ".env").string()
            : paths.signing_env;
        load_key(signing_env, secret);
        const auto metadata = read_source(paths.manifest_source);
        const auto tree = read_source(paths.tree_source);
        const auto golden = read_source(paths.golden_source);
        const auto unsigned_bytes = runtime_package_builder_detail::unsigned_bytes(metadata, tree, golden);
        const auto signature = sign_bytes(unsigned_bytes, secret);
        const auto bytes = unsigned_bytes + "17:signature.ed25519128:" + signature.signature_hex + '\n';
        const auto package = unpack_runtime_data_mod_package_v1(bytes);
        const auto trust_bytes = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n" + package.manifest().signing_key_id + ':' + signature.public_key_hex + '\n';
        verify_runtime_data_mod_package_v1(package, read_runtime_mod_truststore_v1(trust_bytes));
        const LocalDataModBuildResultV1 result{
            package.manifest().mod_id, package.manifest().version, package.package_digest()};
        if (before_publish) before_publish(result);
        // Preflight both paths before staging; both complete inodes are durable
        // before publication. Each rename is atomic and cannot replace any file.
        // Two paths cannot form one filesystem transaction across process death;
        // ordinary failures roll back only the inodes created by this invocation.
        publish_pair(paths.package_output, paths.truststore_output, bytes, trust_bytes);
        return result;
    } catch (...) {
        // Source/key/path input may be attacker-controlled or accidentally secret.
        // Never propagate a nested exception or filesystem path into diagnostics.
        throw Error("LocalModBuildFailed");
    }
}

LocalLogicModBuildResultV1 build_local_logic_mod_v1(const LocalLogicModBuildPathsV1& paths) {
    return build_local_logic_mod_v1(paths, {});
}
LocalLogicModBuildResultV1 build_local_logic_mod_v1(
    const LocalLogicModBuildPathsV1& paths, const LocalLogicModBeforePublishV1& before_publish) {
    try {
        Secret secret;
        load_key(paths.signing_env, secret);
        const std::filesystem::path source(paths.source_directory);
        const auto data_bytes = read_source((source / "data.brucemod").string());
        const auto logic_bytes = read_source((source / "logic.json").string());
        const auto worker_name = logic_worker_path(logic_bytes);
        const auto worker_bytes = read_source((source / worker_name).string(), true);

        const auto data = unpack_runtime_data_mod_package_v1(data_bytes);
        // Derive the public half before constructing the outer package. Using
        // the embedded signing_key_id makes a different seed fail closed when
        // the already-signed Data layer is verified below.
        const auto probe = sign_bytes(std::string_view{}, secret);
        const auto trust_bytes = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n" +
            data.manifest().signing_key_id + ':' + probe.public_key_hex + '\n';
        const auto trust = read_runtime_mod_truststore_v1(trust_bytes);
        verify_runtime_data_mod_package_v1(data, trust);

        const auto unsigned_bytes = std::string("BRUCE_LOGIC_MOD_PLUGIN_V1\n") +
            package_member("data.brucemod", data_bytes) + package_member("logic.json", logic_bytes) +
            package_member(worker_name, worker_bytes);
        const auto signature = sign_bytes(unsigned_bytes, secret);
        const auto bytes = unsigned_bytes + package_member("signature.ed25519", signature.signature_hex) + '\n';
        if (bytes.size() > max_runtime_mod_package_bytes_v1) failed();
        const auto package = unpack_logic_mod_package_v1(bytes);
        verify_logic_mod_package_v1(package, trust);
        const LocalLogicModBuildResultV1 result{package.manifest().mod_id, package.manifest().version,
            package.manifest().signing_key_id, package.package_digest()};
        if (before_publish) before_publish(result);
        publish_pair(paths.package_output, paths.truststore_output, bytes, trust_bytes);
        return result;
    } catch (...) {
        throw Error("LocalModBuildFailed");
    }
}
}
