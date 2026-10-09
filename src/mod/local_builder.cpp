#include "bruce/mod/local_builder.hpp"
#include "bruce/core/types.hpp"
#include "bruce/mod/runtime_package.hpp"
#include "runtime_package_builder.hpp"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
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
void load_key(int root, Secret& secret) {
    Descriptor file(::openat(root, ".env", O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK));
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
std::string read_source(const std::string& path) {
    const std::filesystem::path name(path);
    Descriptor parent(open_directory(name.parent_path()));
    Descriptor file(::openat(parent.fd, name.filename().c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK));
    struct stat info{};
    if (::fstat(file.fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0 ||
        static_cast<unsigned long long>(info.st_size) > max_runtime_mod_package_bytes_v1) failed();
    std::string result;
    std::array<char, 8192> buffer{};
    while (true) {
        const auto count = ::read(file.fd, buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) failed();
        if (!count) return result;
        if (static_cast<std::size_t>(count) > max_runtime_mod_package_bytes_v1 - result.size()) failed();
        result.append(buffer.data(), static_cast<std::size_t>(count));
    }
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
}
LocalDataModBuildResultV1 build_local_data_mod_v1(const LocalDataModBuildPathsV1& paths) {
    try {
        Descriptor root(open_directory(paths.project_root));
        Secret secret;
        load_key(root.fd, secret);
        const auto metadata = read_source(paths.manifest_source);
        const auto tree = read_source(paths.tree_source);
        const auto golden = read_source(paths.golden_source);
        const auto unsigned_bytes = runtime_package_builder_detail::unsigned_bytes(metadata, tree, golden);
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, secret.seed.data(), secret.seed.size()), EVP_PKEY_free);
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
        unsigned char signature[64], public_key[32];
        std::size_t signature_size = sizeof(signature), public_size = sizeof(public_key);
        if (!key || !context || EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1 ||
            EVP_DigestSign(context.get(), signature, &signature_size, reinterpret_cast<const unsigned char*>(unsigned_bytes.data()), unsigned_bytes.size()) != 1 ||
            signature_size != sizeof(signature) || EVP_PKEY_get_raw_public_key(key.get(), public_key, &public_size) != 1 || public_size != sizeof(public_key)) failed();
        const auto bytes = unsigned_bytes + "17:signature.ed25519128:" + hex(signature, sizeof(signature)) + '\n';
        const auto package = unpack_runtime_data_mod_package_v1(bytes);
        const auto trust_bytes = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n" + package.manifest().signing_key_id + ':' + hex(public_key, sizeof(public_key)) + '\n';
        verify_runtime_data_mod_package_v1(package, read_runtime_mod_truststore_v1(trust_bytes));
        // Preflight both paths before staging; both complete inodes are durable
        // before publication. Each rename is atomic and cannot replace any file.
        // Two paths cannot form one filesystem transaction across process death;
        // ordinary failures roll back only the inodes created by this invocation.
        Output package_output(paths.package_output), trust_output(paths.truststore_output);
        if (package_output.same_target(trust_output)) failed();
        package_output.stage(bytes); trust_output.stage(trust_bytes);
        try { package_output.publish(); trust_output.publish(); }
        catch (...) { trust_output.rollback(); package_output.rollback(); throw; }
        return {package.manifest().mod_id, package.manifest().version, package.package_digest()};
    } catch (...) {
        // Source/key/path input may be attacker-controlled or accidentally secret.
        // Never propagate a nested exception or filesystem path into diagnostics.
        throw Error("LocalModBuildFailed");
    }
}
}
