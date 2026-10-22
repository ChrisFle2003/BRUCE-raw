#include "bruce/mod/logic_mod_plugin_v1.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/tree/normalizer.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace bruce {
namespace {
void field(std::string& out, std::string_view value) { out += std::to_string(value.size()) + ':'; out += value; }
struct Fd {
    int value;
    explicit Fd(int n) : value(n) { if (n < 0) throw Error("LogicModFileOpenFailed"); }
    ~Fd() { ::close(value); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
};
// Walk every directory component without following symlinks. Ancestors such as
// /tmp need not be private, but the final package directory must be owner-only.
int open_directory(std::string_view path) {
    if (path.empty() || path.front() != '/' || path.find('\0') != std::string_view::npos) throw Error("InvalidLogicModDirectory");
    Fd current(::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    path.remove_prefix(1);
    if (path.empty()) throw Error("InvalidLogicModDirectory");
    while (!path.empty()) {
        const auto slash = path.find('/');
        const auto part = path.substr(0, slash);
        if (part.empty() || part == "." || part == "..") throw Error("InvalidLogicModDirectory");
        Fd next(::openat(current.value, std::string(part).c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
        std::swap(current.value, next.value);
        if (slash == std::string_view::npos) break;
        path.remove_prefix(slash + 1);
        if (path.empty()) throw Error("InvalidLogicModDirectory");
    }
    struct stat info{};
    if (::fstat(current.value, &info) != 0 || !S_ISDIR(info.st_mode) || info.st_uid != ::geteuid() || (info.st_mode & 077) != 0)
        throw Error("UnsafeLogicModFilePermissions");
    return std::exchange(current.value, -1);
}
std::string read_worker(const LogicModPackageV1& package, const std::string& path) {
    Fd directory(open_directory(path));
    Fd file(::openat(directory.value, package.worker_path().c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));
    struct stat before{}, after{};
    const auto safe = [](const struct stat& s) {
        return S_ISREG(s.st_mode) && s.st_uid == ::geteuid() && s.st_nlink == 1 &&
            (s.st_mode & 07077) == 0 && (s.st_mode & 0500) == 0500 && s.st_size > 0 &&
            static_cast<std::uint64_t>(s.st_size) <= max_runtime_mod_package_bytes_v1;
    };
    if (::fstat(file.value, &before) != 0 || !safe(before)) throw Error("UnsafeLogicModFilePermissions");
    std::string result;
    std::array<char, 8192> buffer{};
    for (;;) {
        const auto n = ::read(file.value, buffer.data(), buffer.size());
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) throw Error("LogicModFileReadFailed");
        if (n == 0) break;
        if (static_cast<std::size_t>(n) > max_runtime_mod_package_bytes_v1 - result.size()) throw Error("LogicModWorkerTooLarge");
        result.append(buffer.data(), static_cast<std::size_t>(n));
    }
    if (::fstat(file.value, &after) != 0 || !safe(after) || before.st_size != after.st_size ||
        before.st_mtim.tv_sec != after.st_mtim.tv_sec || before.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
        before.st_ctim.tv_sec != after.st_ctim.tv_sec || before.st_ctim.tv_nsec != after.st_ctim.tv_nsec)
        throw Error("LogicModWorkerChanged");
    if (sha256(result) != package.worker_sha256()) throw Error("LogicModWorkerDigestMismatch");
    return result;
}
void expect(LogicModStateV1 actual, LogicModStateV1 expected) {
    if (actual != expected) throw Error("InvalidLogicModTransition");
}
}
std::string LogicModBindingV1::canonical_identity() const {
    std::string bytes = "BRUCE_LOGIC_MOD_BINDING_V1\n";
    for (const auto* value : {&mod_id_, &version_, &package_digest_, &worker_sha256_, &template_id_, &capability_, &registry_digest_}) field(bytes, *value);
    return bytes + '\n';
}
LogicModRegistryV1::Record& LogicModRegistryV1::find(std::string_view id) {
    const auto it = records_.find(std::string(id));
    if (it == records_.end()) throw Error("UnknownLogicModPackage");
    return it->second;
}
const LogicModRegistryV1::Record& LogicModRegistryV1::find(std::string_view id) const {
    const auto it = records_.find(std::string(id));
    if (it == records_.end()) throw Error("UnknownLogicModPackage");
    return it->second;
}
void LogicModRegistryV1::discover(LogicModPackageV1 package, std::string directory) {
    std::lock_guard lock(mutex_);
    if (records_.size() >= 512) throw Error("LogicModRegistryLimit");
    for (const auto& [id, record] : records_) {
        (void)id;
        if (record.package.manifest().mod_id == package.manifest().mod_id && record.package.manifest().version == package.manifest().version)
            throw Error("DuplicateLogicModVersion");
    }
    const auto id = package.package_digest();
    records_.emplace(id, Record{std::move(package), std::move(directory), LogicModStateV1::discovered, std::nullopt, {}});
}
void LogicModRegistryV1::verify(std::string_view id, const RuntimeModTrustStoreV1& trust) {
    std::lock_guard lock(mutex_); auto& record = find(id);
    expect(record.state, LogicModStateV1::discovered);
    verify_logic_mod_package_v1(record.package, trust);
    (void)read_worker(record.package, record.directory);
    record.trust = trust;
    record.state = LogicModStateV1::verified;
}
void LogicModRegistryV1::test_golden(std::string_view id, const TreeRegistryV1& base) {
    std::lock_guard lock(mutex_); auto& record = find(id);
    expect(record.state, LogicModStateV1::verified);
    const auto& package = record.package;
    const auto tree = base.mount(package.data_.tree_fragment(*record.trust), package.manifest().mod_id);
    std::set<std::string> covered;
    for (const auto& test : package.data_.golden_cases(*record.trust)) {
        const auto route = tree.route(normalize_route_text_v1(test.input));
        if (route.status != test.status || (test.status == RouteStatusV1::matched &&
            (route.candidates.size() != 1 || route.candidates.front().template_id != test.template_id || route.candidates.front().owner_id != package.manifest().mod_id)))
            throw Error("LogicModGoldenCaseFailed");
        if (test.status == RouteStatusV1::matched) covered.insert(test.template_id);
    }
    if (std::vector<std::string>(covered.begin(), covered.end()) != package.template_ids()) throw Error("LogicModGoldenCoverageMissing");
    record.golden_tree_digest = tree.digest();
    record.state = LogicModStateV1::golden_tested;
}
void LogicModRegistryV1::install(std::string_view id) {
    std::lock_guard lock(mutex_); auto& record = find(id);
    expect(record.state, LogicModStateV1::golden_tested);
    (void)read_worker(record.package, record.directory);
    record.state = LogicModStateV1::installed_inactive;
}
void LogicModRegistryV1::activate(std::string_view id) {
    std::lock_guard lock(mutex_); auto& record = find(id);
    if (record.state != LogicModStateV1::installed_inactive && record.state != LogicModStateV1::disabled) throw Error("InvalidLogicModTransition");
    for (const auto& [other_id, other] : records_) {
        (void)other_id;
        if (other.state == LogicModStateV1::active && other.package.manifest().mod_id == record.package.manifest().mod_id) throw Error("LogicModVersionAlreadyActive");
    }
    (void)read_worker(record.package, record.directory);
    record.state = LogicModStateV1::active;
}
void LogicModRegistryV1::disable(std::string_view id) {
    std::lock_guard lock(mutex_); auto& record = find(id);
    if (record.state != LogicModStateV1::active && record.state != LogicModStateV1::installed_inactive) throw Error("InvalidLogicModTransition");
    record.state = LogicModStateV1::disabled;
}
void LogicModRegistryV1::revoke(std::string_view id) {
    std::lock_guard lock(mutex_); auto& record = find(id);
    if (record.state != LogicModStateV1::active && record.state != LogicModStateV1::installed_inactive && record.state != LogicModStateV1::disabled) throw Error("InvalidLogicModTransition");
    record.state = LogicModStateV1::revoked;
}
LogicModStateV1 LogicModRegistryV1::state(std::string_view id) const {
    std::lock_guard lock(mutex_); return find(id).state;
}
std::string LogicModRegistryV1::digest_locked() const {
    std::string bytes = "BRUCE_LOGIC_MOD_REGISTRY_V1\n";
    field(bytes, std::to_string(records_.size()));
    for (const auto& [id, record] : records_) {
        field(bytes, id); field(bytes, record.package.manifest().mod_id); field(bytes, record.package.manifest().version);
        field(bytes, std::to_string(static_cast<unsigned>(record.state))); field(bytes, record.golden_tree_digest);
        std::string trust;
        if (record.trust) for (const auto& [key, value] : record.trust->public_keys()) { field(trust, key); field(trust, value); }
        field(bytes, sha256(trust));
    }
    return sha256(bytes + '\n');
}
std::string LogicModRegistryV1::digest() const { std::lock_guard lock(mutex_); return digest_locked(); }
LogicModBindingV1 LogicModRegistryV1::bind(std::string_view mod_id, std::string_view version, std::string_view template_id, std::string_view capability) const {
    std::lock_guard lock(mutex_);
    for (const auto& [id, record] : records_) {
        const auto& package = record.package;
        if (package.manifest().mod_id != mod_id || package.manifest().version != version) continue;
        expect(record.state, LogicModStateV1::active);
        if (std::find(package.template_ids().begin(), package.template_ids().end(), template_id) == package.template_ids().end()) throw Error("LogicModTemplateDenied");
        if (std::find(package.capabilities().begin(), package.capabilities().end(), capability) == package.capabilities().end()) throw Error("LogicModCapabilityDenied");
        LogicModBindingV1 binding;
        binding.worker_bytes_ = read_worker(package, record.directory);
        binding.mod_id_ = mod_id; binding.version_ = version; binding.package_digest_ = id;
        binding.worker_sha256_ = package.worker_sha256(); binding.template_id_ = template_id;
        binding.capability_ = capability; binding.registry_digest_ = digest_locked();
        return binding;
    }
    throw Error("UnknownLogicModPackage");
}
}
