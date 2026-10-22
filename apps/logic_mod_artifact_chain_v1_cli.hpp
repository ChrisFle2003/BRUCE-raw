#pragma once

#include "logic_mod_plugin_v1_cli.hpp"
#include "bruce/mod/logic_mod_artifact_chain_v1.hpp"
#include <array>
#include <span>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/fs.h>

namespace bruce {
namespace logic_mod_chain_cli_detail {
struct Arguments {
    bool replay;
    std::string first_package, first_trust, first_input, second_package, second_trust, second_input;
    std::string store, first_manifest, second_manifest, record;
};
inline Arguments parse(std::span<const std::string_view> args) {
    const auto invalid = [] { throw Error("InvalidLogicModChainArguments"); };
    if (args.empty() || (args[0] != "run" && args[0] != "replay")) invalid();
    const bool replay = args[0] == "replay";
    if (args.size() != (replay ? 15u : 19u)) invalid();
    // Validate the complete grammar before constructing a path or opening any
    // package, truststore, Store or record. Every slot is mandatory and unique.
    for (const auto value : args)
        if (value.empty() || value.find_first_not_of(" \t\r\n\v\f") == std::string_view::npos) invalid();
    for (const std::size_t slot : {1u, 3u, 5u, 6u, 8u, 10u, 12u, 14u})
        if (args[slot].starts_with("--")) invalid();
    if (args[2] != "--first-trust" || args[4] != "--first-input" ||
        args[7] != "--second-trust" || args[9] != "--second-input" || args[11] != "--store") invalid();
    if (replay) {
        if (args[13] != "--record") invalid();
    } else {
        if (args[13] != "--first-output-manifest" || args[15] != "--second-output-manifest" ||
            args[17] != "--record" || args[16].starts_with("--") || args[18].starts_with("--")) invalid();
    }
    return {replay, std::string(args[1]), std::string(args[3]), std::string(args[5]),
        std::string(args[6]), std::string(args[8]), std::string(args[10]), std::string(args[12]),
        replay ? "" : std::string(args[14]), replay ? "" : std::string(args[16]), std::string(args[replay ? 14 : 18])};
}
[[noreturn]] inline void file_error() { throw Error("LogicModChainFileInvalid"); }
struct Descriptor {
    int fd;
    explicit Descriptor(int value) : fd(value) { if (fd < 0) file_error(); }
    ~Descriptor() { if (fd >= 0) ::close(fd); }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
};
struct Target {
    Descriptor parent;
    std::string name;
    explicit Target(const std::filesystem::path& path)
        : parent(::open(path.is_absolute() ? "/" : ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK)) {
        // Dropping a terminal dot would turn "record/." into "record" and
        // silently change both read and publication semantics. Interior dot
        // components are safe to skip only when a real final filename follows.
        if (path.empty() || path.filename().empty() || path.filename() == "." || path.filename() == "..") file_error();
        std::vector<std::string> names;
        for (const auto& part : path) {
            const auto name = part.string();
            if (name == "/" || name == ".") continue;
            if (name.empty() || name == ".." || name.find('\0') != std::string::npos) file_error();
            names.push_back(name);
        }
        if (names.empty()) file_error();
        for (std::size_t i = 0; i + 1 < names.size(); ++i) {
            Descriptor next(::openat(parent.fd, names[i].c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
            std::swap(parent.fd, next.fd);
        }
        name = names.back();
    }
};
inline std::string read_file(const std::filesystem::path& path, std::size_t limit, bool private_record = false) {
    Target target(path);
    Descriptor file(::openat(target.parent.fd, target.name.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY));
    struct stat info{};
    if (::fstat(file.fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0 ||
        static_cast<std::uint64_t>(info.st_size) > limit ||
        (private_record && (info.st_uid != ::geteuid() || (info.st_mode & 0077) || info.st_nlink != 1))) file_error();
    std::string bytes;
    bytes.reserve(static_cast<std::size_t>(info.st_size));
    std::array<char, 8192> buffer{};
    for (;;) {
        const auto n = ::read(file.fd, buffer.data(), buffer.size());
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) file_error();
        if (!n) return bytes;
        const auto size = static_cast<std::size_t>(n);
        if (size > limit - bytes.size()) file_error();
        bytes.append(buffer.data(), size);
    }
}
inline void publish_record(const std::filesystem::path& path, std::string_view bytes) {
    if (bytes.size() > logic_mod_artifact_chain_record_limit_v1) file_error();
    Target target(path);
    std::array<unsigned char, 16> random{};
    std::size_t offset = 0;
    while (offset < random.size()) {
        const auto n = ::getrandom(random.data() + offset, random.size() - offset, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) file_error();
        offset += static_cast<std::size_t>(n);
    }
    std::string temporary = ".bruce-chain-";
    for (const auto byte : random) { temporary += "0123456789abcdef"[byte >> 4]; temporary += "0123456789abcdef"[byte & 15]; }
    temporary += ".tmp";
    Descriptor file(::openat(target.parent.fd, temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
    try {
        offset = 0;
        while (offset < bytes.size()) {
            const auto n = ::write(file.fd, bytes.data() + offset, bytes.size() - offset);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) file_error();
            offset += static_cast<std::size_t>(n);
        }
        if (::fsync(file.fd) != 0) file_error();
        const auto closed = ::close(file.fd); file.fd = -1;
        if (closed != 0) file_error();
        // Pinned parent directories, private temporary inode and a single
        // no-replace namespace operation; existing files and aliases survive.
        if (::syscall(SYS_renameat2, target.parent.fd, temporary.c_str(), target.parent.fd,
            target.name.c_str(), RENAME_NOREPLACE) != 0) file_error();
        if (::fsync(target.parent.fd) != 0) file_error();
    } catch (...) {
        ::unlinkat(target.parent.fd, temporary.c_str(), 0);
        throw;
    }
}
inline void activate(LogicModRegistryV1& registry, const LogicModPackageV1& package,
    const RuntimeModTrustStoreV1& trust, const logic_mod_cli_detail::PackageDirectory& directory) {
    directory.extract(package, trust);
    registry.discover(package, directory.path);
    const auto id = package.package_digest();
    registry.verify(id, trust); registry.test_golden(id, core_tree_registry_v1()); registry.install(id); registry.activate(id);
}
inline std::string output_json(const LogicModPublishedOutputV1& output) {
    if (output.manifest_id.empty()) return "null";
    return "{\"manifest_id\":" + json_string(output.manifest_id) + ",\"sha256\":" + json_string(output.sha256) +
        ",\"size_bytes\":" + std::to_string(output.size_bytes) + ",\"codec\":" + json_string(output.codec) + "}";
}
}
inline int logic_mod_artifact_chain_v1_cli(int argc, char** argv) {
    using namespace logic_mod_chain_cli_detail;
    std::vector<std::string_view> args;
    for (int i = 2; i < argc; ++i) args.emplace_back(argv[i]);
    const auto options = parse(args);
    try {
        const auto first_trust = read_runtime_mod_truststore_v1(read_file(options.first_trust, 65536));
        const auto second_trust = read_runtime_mod_truststore_v1(read_file(options.second_trust, 65536));
        const auto first_package = unpack_logic_mod_package_v1(read_file(options.first_package, 1024 * 1024));
        const auto second_package = unpack_logic_mod_package_v1(read_file(options.second_package, 1024 * 1024));
        verify_logic_mod_package_v1(first_package, first_trust);
        verify_logic_mod_package_v1(second_package, second_trust);
        logic_mod_cli_detail::PackageDirectory first_directory, second_directory;
        LogicModRegistryV1 first_registry, second_registry;
        activate(first_registry, first_package, first_trust, first_directory);
        activate(second_registry, second_package, second_trust, second_directory);
        const LogicModChainStepV1 first{first_registry, first_package.manifest().mod_id,
            first_package.manifest().version, "math.rational.power.v1", options.first_input, {options.store, options.first_manifest}};
        const LogicModChainStepV1 second{second_registry, second_package.manifest().mod_id,
            second_package.manifest().version, "math.rational.power.v1", options.second_input, {options.store, options.second_manifest}};
        LogicModArtifactChainRunV1 run;
        if (options.replay) {
            run = read_logic_mod_artifact_chain_record_v1(read_file(options.record, logic_mod_artifact_chain_record_limit_v1, true));
            verify_logic_mod_artifact_chain_replay_v1(first, second, options.store, run);
        } else {
            run = run_logic_mod_artifact_chain_v1(first, second, options.store);
            if (run.accepted) publish_record(options.record, logic_mod_artifact_chain_record_v1(run));
        }
        const auto json = "{\"contract\":\"logic-mod-artifact-chain-v1\",\"action\":" + json_string(options.replay ? "replay" : "run") +
            ",\"accepted\":" + (run.accepted ? "true" : "false") + ",\"status\":" + json_string(run.status) +
            ",\"chain_id\":" + (run.chain_id.empty() ? "null" : json_string(run.chain_id)) +
            ",\"first\":" + output_json(run.first_output) + ",\"second\":" + output_json(run.second_output) + "}\n";
        std::cout << json;
        return run.accepted ? 0 : 2;
    } catch (...) {
        // Library/path/parser diagnostics may contain untrusted bytes. Expose
        // only fixed public errors and never partially emit a JSON result.
        throw Error(options.replay ? "CHAIN_REPLAY_MISMATCH" : "LogicModChainFailed");
    }
}
}
