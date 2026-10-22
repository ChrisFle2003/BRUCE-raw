// Frozen verbatim from commit 08bc7f2 for legacy-reader migration tests.
#include "bruce/mod/logic_mod_registry_v1.hpp"

#include "bruce/evidence/bundle.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/tree/normalizer.hpp"
#include "runtime_mod_store_internal.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <filesystem>
#include <fcntl.h>
#include <limits>
#include <set>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace bruce {
namespace {

constexpr std::string_view state_header = "BRUCE_LOGIC_MOD_REGISTRY_V1\n";
constexpr std::string_view journal_header = "BRUCE_LOGIC_MOD_JOURNAL_V1\n";
constexpr std::size_t max_state_bytes = 256 * 1024;
constexpr std::size_t max_journal_bytes = 32 * 1024 * 1024;
constexpr std::size_t max_records = 512;

class Fd {
public:
    explicit Fd(const int value) : value_(value) {
        if (value_ < 0) throw Error("LogicModFileOpenFailed");
    }
    ~Fd() { ::close(value_); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    [[nodiscard]] int get() const noexcept { return value_; }
private:
    int value_;
};

void check_node(const int fd, const bool directory, const std::optional<unsigned> exact_mode = std::nullopt) {
    if (!exact_mode) {
        runtime_mod_store_detail::check_private_node(fd, directory);
        return;
    }
    struct stat info {};
    if (::fstat(fd, &info) != 0 || info.st_uid != ::geteuid() ||
        (directory ? !S_ISDIR(info.st_mode) : !S_ISREG(info.st_mode) || info.st_nlink != 1) ||
        (exact_mode ? (info.st_mode & 0777) != *exact_mode : (info.st_mode & 077) != 0))
        throw Error("UnsafeLogicModFilePermissions");
}

void check_directory(const std::string& path, const bool create) {
    runtime_mod_store_detail::check_private_directory(path, create);
}

bool exists(const std::string& path) {
    struct stat info {};
    if (::lstat(path.c_str(), &info) == 0) return true;
    if (errno != ENOENT) throw Error("LogicModFileStatFailed");
    return false;
}

bool legacy_data_only_root(const std::string& root) {
    return !exists(root + "/logic-packages") && !exists(root + "/logic-workers") &&
        !exists(root + "/logic-journal.v1") && !exists(root + "/logic-registry.v1");
}

LogicModSnapshotV1 empty_snapshot() {
    LogicModSnapshotV1 empty;
    empty.journal_digest = sha256(journal_header);
    return empty;
}

std::string read_private(const std::string& path, const std::size_t limit,
                         const std::optional<unsigned> exact_mode = std::nullopt) {
    Fd file(::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOATIME));
    check_node(file.get(), false, exact_mode);
    std::string result;
    std::array<char, 8192> buffer {};
    while (true) {
        const auto count = ::read(file.get(), buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) throw Error("LogicModFileReadFailed");
        if (count == 0) break;
        if (static_cast<std::size_t>(count) > limit - result.size())
            throw Error("LogicModStoreByteLimit");
        result.append(buffer.data(), static_cast<std::size_t>(count));
    }
    return result;
}

using Lock = runtime_mod_store_detail::RootLock;

std::string trust_bytes(const RuntimeModTrustStoreV1& trust) {
    std::string bytes = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n";
    for (const auto& [id, key] : trust.public_keys()) bytes += id + ':' + key + '\n';
    return bytes;
}

void check_trust(const std::string& root, const RuntimeModTrustStoreV1& trust) {
    if (read_private(root + "/truststore.pub", 65536) != trust_bytes(trust))
        throw Error("RuntimeModTrustStoreChanged");
}

void field(std::string& bytes, const std::string_view value) {
    bytes += std::to_string(value.size()) + ':';
    bytes += value;
}

std::uint64_t number(const std::string_view value) {
    std::uint64_t result = 0;
    if (value.empty() || (value.size() > 1 && value.front() == '0'))
        throw Error("InvalidLogicModRegistry");
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        throw Error("InvalidLogicModRegistry");
    return result;
}

bool token(const std::string_view value) {
    return !value.empty() && value.size() <= 128 && value.front() != '.' &&
        std::ranges::all_of(value, [](const char ch) {
            return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '_';
        });
}

bool digest(const std::string_view value) {
    return value.size() == 64 && std::ranges::all_of(value, [](const char ch) {
        return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
    });
}

struct Incomplete {};
struct Reader {
    std::string_view bytes;
    std::size_t position = 0;
    std::string_view next() {
        const auto colon = bytes.find(':', position);
        if (colon == std::string_view::npos) {
            const auto tail = bytes.substr(position);
            if (tail.size() <= 7 && std::ranges::all_of(tail, [](const char ch) {
                    return ch >= '0' && ch <= '9';
                })) throw Incomplete{};
            throw Error("InvalidLogicModRegistry");
        }
        const auto length = number(bytes.substr(position, colon - position));
        if (length > max_state_bytes) throw Error("LogicModStoreByteLimit");
        position = colon + 1;
        if (length > bytes.size() - position) throw Incomplete{};
        const auto value = bytes.substr(position, static_cast<std::size_t>(length));
        position += static_cast<std::size_t>(length);
        return value;
    }
};

std::string serialize(const LogicModSnapshotV1& snapshot) {
    std::string bytes(state_header);
    field(bytes, std::to_string(snapshot.generation));
    field(bytes, std::to_string(snapshot.records.size()));
    for (const auto& record : snapshot.records) {
        field(bytes, record.mod_id); field(bytes, record.version);
        field(bytes, record.package_digest); field(bytes, record.worker_sha256);
        field(bytes, std::to_string(static_cast<unsigned>(record.state)));
    }
    bytes += '\n';
    if (bytes.size() > max_state_bytes) throw Error("LogicModStoreByteLimit");
    return bytes;
}

LogicModSnapshotV1 parse_state(const std::string_view bytes) {
    if (!bytes.starts_with(state_header)) throw Error("InvalidLogicModRegistry");
    Reader reader{bytes, state_header.size()};
    LogicModSnapshotV1 snapshot;
    try {
        snapshot.generation = number(reader.next());
        const auto count = number(reader.next());
        if (count > max_records) throw Error("LogicModRecordLimit");
        std::set<std::string> active;
        for (std::uint64_t index = 0; index < count; ++index) {
            LogicModRecordV1 record;
            record.mod_id = reader.next(); record.version = reader.next();
            record.package_digest = reader.next(); record.worker_sha256 = reader.next();
            const auto state = number(reader.next());
            if (!token(record.mod_id) || !token(record.version) || !digest(record.package_digest) ||
                !digest(record.worker_sha256) || state < static_cast<unsigned>(LogicModStateV1::installed_inactive) ||
                state > static_cast<unsigned>(LogicModStateV1::revoked)) throw Error("InvalidLogicModRegistry");
            record.state = static_cast<LogicModStateV1>(state);
            if (!snapshot.records.empty() &&
                std::pair{snapshot.records.back().mod_id, snapshot.records.back().version} >=
                std::pair{record.mod_id, record.version}) throw Error("InvalidLogicModRegistry");
            if (record.state == LogicModStateV1::active && !active.insert(record.mod_id).second)
                throw Error("InvalidLogicModRegistry");
            snapshot.records.push_back(std::move(record));
        }
    } catch (const Incomplete&) {
        throw Error("InvalidLogicModRegistry");
    }
    if (reader.position + 1 != bytes.size() || bytes.back() != '\n' || serialize(snapshot) != bytes)
        throw Error("InvalidLogicModRegistry");
    return snapshot;
}

LogicModSnapshotV1 load(const std::string& root, const bool recover = true) {
    const auto journal_path = root + "/logic-journal.v1";
    const auto journal = read_private(journal_path, max_journal_bytes);
    if (!journal.starts_with(journal_header)) throw Error("InvalidLogicModJournal");
    LogicModSnapshotV1 current;
    current.journal_digest = sha256(journal_header);
    const bool has_snapshot = exists(root + "/logic-registry.v1");
    const auto disk = has_snapshot ? read_private(root + "/logic-registry.v1", max_state_bytes) : std::string{};
    bool known_snapshot = !has_snapshot || disk == serialize(current);
    Reader reader{journal, journal_header.size()};
    std::size_t good_end = reader.position;
    while (reader.position < journal.size()) {
        try {
            const auto previous = reader.next();
            const auto serialized = reader.next();
            const auto hash = reader.next();
            if (reader.position == journal.size()) throw Incomplete{};
            if (journal[reader.position++] != '\n') throw Error("InvalidLogicModJournal");
            std::string unsigned_record; field(unsigned_record, previous); field(unsigned_record, serialized);
            if (previous != current.journal_digest || hash != sha256(unsigned_record))
                throw Error("InvalidLogicModJournal");
            auto next = parse_state(serialized);
            if (current.generation == std::numeric_limits<std::uint64_t>::max() ||
                next.generation != current.generation + 1) throw Error("InvalidLogicModJournalGeneration");
            next.journal_digest = std::string(hash);
            current = std::move(next);
            if (disk == serialized) known_snapshot = true;
            good_end = reader.position;
        } catch (const Incomplete&) {
            if (!recover) throw Error("LogicModRecoveryRequired");
            Fd file(::open(journal_path.c_str(), O_WRONLY | O_NOFOLLOW | O_CLOEXEC));
            check_node(file.get(), false);
            if (::ftruncate(file.get(), static_cast<off_t>(good_end)) != 0 || ::fsync(file.get()) != 0)
                throw Error("LogicModJournalRecoveryFailed");
            break;
        }
    }
    if (!known_snapshot) throw Error("LogicModRegistryJournalMismatch");
    const auto canonical = serialize(current);
    if (disk != canonical) {
        if (!recover) throw Error("LogicModRecoveryRequired");
        LocalFiles::replace_atomically(root + "/logic-registry.v1", canonical);
    }
    return current;
}

void append(const std::string& root, const LogicModSnapshotV1& snapshot) {
    runtime_mod_store_detail::test_failpoint("logic.before_journal");
    std::string record; field(record, snapshot.journal_digest); field(record, serialize(snapshot));
    const auto hash = sha256(record); field(record, hash); record += '\n';
    Fd journal(::open((root + "/logic-journal.v1").c_str(),
                      O_WRONLY | O_APPEND | O_NOFOLLOW | O_CLOEXEC));
    check_node(journal.get(), false);
    struct stat info {};
    if (::fstat(journal.get(), &info) != 0 || info.st_size < 0 ||
        static_cast<std::uint64_t>(info.st_size) + record.size() > max_journal_bytes)
        throw Error("LogicModJournalLimit");
    std::size_t offset = 0;
    while (offset < record.size()) {
        const auto count = ::write(journal.get(), record.data() + offset, record.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) throw Error("LogicModJournalWriteFailed");
        offset += static_cast<std::size_t>(count);
    }
    if (::fsync(journal.get()) != 0) throw Error("LogicModCommitDurabilityUnknown");
    runtime_mod_store_detail::test_failpoint("logic.after_journal");
    LocalFiles::replace_atomically(root + "/logic-registry.v1", serialize(snapshot));
    runtime_mod_store_detail::test_failpoint("logic.after_snapshot");
}

void sync_directory(const std::string& root) {
    Fd directory(::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (::fsync(directory.get()) != 0) throw Error("LogicModCommitDurabilityUnknown");
}

void prepare(const std::string& root, const RuntimeModTrustStoreV1& trust) {
    check_directory(root, false);
    check_directory(root + "/logic-packages", false);
    check_directory(root + "/logic-workers", false);
    check_trust(root, trust);
}

struct InstalledPackage {
    LogicModPackageV1 package;
    std::string worker;
};

InstalledPackage read_installed(const std::string& root, const LogicModRecordV1& record,
                                const RuntimeModTrustStoreV1& trust) {
    auto package = unpack_logic_mod_package_v1(read_private(
        root + "/logic-packages/" + record.package_digest + ".brucemod",
        max_runtime_mod_package_bytes_v1));
    if (package.package_digest() != record.package_digest || package.manifest().mod_id != record.mod_id ||
        package.manifest().version != record.version || package.worker_sha256() != record.worker_sha256)
        throw Error("LogicModInstalledPackageMismatch");
    verify_logic_mod_package_v1(package, trust);
    const auto worker_path = root + "/logic-workers/" + record.package_digest + '/' + package.worker_path();
    const auto worker = read_private(worker_path, max_runtime_mod_package_bytes_v1, 0500);
    if (sha256(worker) != record.worker_sha256 || worker != package.worker_bytes(trust))
        throw Error("LogicModInstalledWorkerMismatch");
    return {std::move(package), worker};
}

void golden(const LogicModPackageV1& package, const RuntimeModTrustStoreV1& trust,
            const TreeRegistryV1& tree) {
    for (const auto& test : package.golden_cases(trust)) {
        const auto result = tree.route(normalize_route_text_v1(test.input));
        if (result.status != test.status ||
            (result.status == RouteStatusV1::matched && result.candidates.front().template_id != test.template_id))
            throw Error("LogicModGoldenCaseFailed");
    }
}

void validate(const std::string& root, const LogicModSnapshotV1& snapshot,
              const RuntimeModTrustStoreV1& trust, const LogicModPackageV1* incoming = nullptr) {
    auto tree = core_tree_registry_v1();
    std::vector<LogicModPackageV1> packages;
    for (const auto& record : snapshot.records) {
        const auto installed = read_installed(root, record, trust);
        if (record.state == LogicModStateV1::active &&
            (!incoming || record.mod_id != incoming->manifest().mod_id))
            packages.push_back(installed.package);
    }
    if (incoming) {
        verify_logic_mod_package_v1(*incoming, trust);
        packages.push_back(*incoming);
    }
    std::vector<bool> mounted(packages.size(), false);
    std::size_t remaining = packages.size();
    while (remaining != 0) {
        bool progress = false;
        for (std::size_t index = 0; index < packages.size(); ++index) if (!mounted[index]) {
            const auto fragment = packages[index].tree_fragment(trust);
            if (!tree.find(fragment.mount)) continue;
            tree = tree.mount(fragment, packages[index].manifest().mod_id);
            mounted[index] = true; --remaining; progress = true;
        }
        if (!progress) throw Error("TreeMountNotFound");
    }
    for (const auto& package : packages) golden(package, trust, tree);
}

void advance(LogicModSnapshotV1& snapshot) {
    if (snapshot.generation == std::numeric_limits<std::uint64_t>::max())
        throw Error("LogicModGenerationLimit");
    ++snapshot.generation;
}

void publish_worker(const std::string& directory, const std::string& path,
                    const std::string_view bytes) {
    check_directory(directory, true);
    if (exists(path)) {
        if (read_private(path, max_runtime_mod_package_bytes_v1, 0500) != bytes)
            throw Error("LogicModInstalledWorkerMismatch");
        return;
    }
    Fd file(::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0500));
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto count = ::write(file.get(), bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) throw Error("LogicModWorkerWriteFailed");
        offset += static_cast<std::size_t>(count);
    }
    if (::fsync(file.get()) != 0) throw Error("LogicModCommitDurabilityUnknown");
    sync_directory(directory);
}

} // namespace

DurableLogicModRegistryV1::DurableLogicModRegistryV1(std::string root,
    RuntimeModTrustStoreV1 trust) : root_(std::move(root)), trust_(std::move(trust)) {}

DurableLogicModRegistryV1 DurableLogicModRegistryV1::open(
    const std::string& mod_root, const RuntimeModTrustStoreV1& trust) {
    if (mod_root.empty()) throw Error("RuntimeModRootRequired");
    const auto root = std::filesystem::absolute(mod_root).string();
    check_directory(root, true); Lock lock(root);
    if (!exists(root + "/truststore.pub"))
        LocalFiles::publish_new_atomically(root + "/truststore.pub", trust_bytes(trust));
    check_trust(root, trust);
    check_directory(root + "/logic-packages", true);
    check_directory(root + "/logic-workers", true);
    if (!exists(root + "/logic-journal.v1"))
        LocalFiles::publish_new_atomically(root + "/logic-journal.v1", journal_header);
    (void)load(root); sync_directory(root);
    return DurableLogicModRegistryV1(root, trust);
}

DurableLogicModRegistryV1 DurableLogicModRegistryV1::open_existing(const std::string& mod_root) {
    if (mod_root.empty()) throw Error("RuntimeModRootRequired");
    const auto root = std::filesystem::absolute(mod_root).string();
    check_directory(root, false); Lock lock(root);
    auto trust = read_runtime_mod_truststore_v1(read_private(root + "/truststore.pub", 65536));
    if (legacy_data_only_root(root))
        return DurableLogicModRegistryV1(root, std::move(trust));
    check_directory(root + "/logic-packages", false);
    check_directory(root + "/logic-workers", false);
    (void)load(root);
    return DurableLogicModRegistryV1(root, std::move(trust));
}
LogicModSnapshotV1 DurableLogicModRegistryV1::read_only_snapshot(const std::string& root) {
    check_directory(root, false);
    // Only total absence identifies a legacy Data V1 root. A partial sidecar
    // set is damaged modern state and must never silently disappear.
    if (legacy_data_only_root(root)) return empty_snapshot();
    check_directory(root + "/logic-packages", false);
    check_directory(root + "/logic-workers", false);
    return load(root, false);
}

void DurableLogicModRegistryV1::install(const LogicModPackageV1& package) {
    check_directory(root_, false); Lock lock(root_); install_locked(package);
}
void DurableLogicModRegistryV1::install_locked(const LogicModPackageV1& package) {
    prepare(root_, trust_);
    auto snapshot = load(root_);
    verify_logic_mod_package_v1(package, trust_);
    for (const auto& record : snapshot.records)
        if (record.mod_id == package.manifest().mod_id && record.version == package.manifest().version)
            throw Error("LogicModAlreadyInstalled");
    if (snapshot.records.size() >= max_records) throw Error("LogicModRecordLimit");
    validate(root_, snapshot, trust_, &package);
    const auto package_path = root_ + "/logic-packages/" + package.package_digest() + ".brucemod";
    if (exists(package_path)) {
        if (read_private(package_path, max_runtime_mod_package_bytes_v1) != package.bytes())
            throw Error("LogicModInstalledPackageMismatch");
    } else {
        LocalFiles::publish_new_atomically(package_path, package.bytes());
    }
    runtime_mod_store_detail::test_failpoint("logic.after_package");
    const auto worker = package.worker_bytes(trust_);
    const auto worker_directory = root_ + "/logic-workers/" + package.package_digest();
    publish_worker(worker_directory, worker_directory + '/' + package.worker_path(), worker);
    runtime_mod_store_detail::test_failpoint("logic.after_worker");
    snapshot.records.push_back({package.manifest().mod_id, package.manifest().version,
        package.package_digest(), package.worker_sha256(), LogicModStateV1::installed_inactive});
    std::ranges::sort(snapshot.records, {}, [](const auto& record) {
        return std::pair{record.mod_id, record.version};
    });
    advance(snapshot); append(root_, snapshot);
}
void DurableLogicModRegistryV1::update_locked(const LogicModPackageV1& package) {
    prepare(root_, trust_);
    auto snapshot = load(root_);
    verify_logic_mod_package_v1(package, trust_);
    auto found = std::ranges::find_if(snapshot.records, [&](const auto& record) {
        return record.mod_id == package.manifest().mod_id && record.version == package.manifest().version;
    });
    if (found != snapshot.records.end() && found->package_digest != package.package_digest())
        throw Error("InstalledModVersionConflict");
    if (found != snapshot.records.end() && found->state == LogicModStateV1::active) {
        validate(root_, snapshot, trust_);
        return;
    }
    auto candidate = snapshot;
    for (auto& record : candidate.records)
        if (record.mod_id == package.manifest().mod_id && record.state == LogicModStateV1::active)
            record.state = LogicModStateV1::disabled;
    if (found == snapshot.records.end()) {
        if (candidate.records.size() >= max_records) throw Error("LogicModRecordLimit");
        validate(root_, candidate, trust_, &package);
        const auto package_path = root_ + "/logic-packages/" + package.package_digest() + ".brucemod";
        if (exists(package_path)) {
            if (read_private(package_path, max_runtime_mod_package_bytes_v1) != package.bytes())
                throw Error("LogicModInstalledPackageMismatch");
        } else LocalFiles::publish_new_atomically(package_path, package.bytes());
        runtime_mod_store_detail::test_failpoint("logic.after_package");
        const auto worker = package.worker_bytes(trust_);
        const auto worker_directory = root_ + "/logic-workers/" + package.package_digest();
        publish_worker(worker_directory, worker_directory + '/' + package.worker_path(), worker);
        runtime_mod_store_detail::test_failpoint("logic.after_worker");
        candidate.records.push_back({package.manifest().mod_id, package.manifest().version,
            package.package_digest(), package.worker_sha256(), LogicModStateV1::active});
        std::ranges::sort(candidate.records, {}, [](const auto& record) {
            return std::pair{record.mod_id, record.version};
        });
    } else {
        auto target = std::ranges::find_if(candidate.records, [&](const auto& record) {
            return record.mod_id == package.manifest().mod_id && record.version == package.manifest().version;
        });
        target->state = LogicModStateV1::active;
        validate(root_, candidate, trust_);
    }
    advance(candidate); append(root_, candidate);
}

void DurableLogicModRegistryV1::activate(const std::string_view id,
                                         const std::string_view version) {
    check_directory(root_, false); Lock lock(root_); activate_locked(id, version);
}
void DurableLogicModRegistryV1::activate_locked(const std::string_view id,
                                                const std::string_view version) {
    prepare(root_, trust_);
    auto snapshot = load(root_);
    auto target = std::ranges::find_if(snapshot.records, [&](const auto& record) {
        return record.mod_id == id && record.version == version;
    });
    if (target == snapshot.records.end()) throw Error("LogicModNotInstalled");
    if (target->state == LogicModStateV1::revoked) throw Error("LogicModRevoked");
    if (target->state == LogicModStateV1::active) { validate(root_, snapshot, trust_); return; }
    for (auto& record : snapshot.records)
        if (record.mod_id == id && record.state == LogicModStateV1::active)
            record.state = LogicModStateV1::disabled;
    target->state = LogicModStateV1::active;
    validate(root_, snapshot, trust_);
    advance(snapshot); append(root_, snapshot);
}

void DurableLogicModRegistryV1::disable(const std::string_view id) {
    check_directory(root_, false); Lock lock(root_); disable_locked(id);
}
void DurableLogicModRegistryV1::disable_locked(const std::string_view id) {
    prepare(root_, trust_);
    auto snapshot = load(root_);
    bool changed = false;
    for (auto& record : snapshot.records)
        if (record.mod_id == id && record.state == LogicModStateV1::active) {
            record.state = LogicModStateV1::disabled; changed = true;
        }
    if (!changed) throw Error("LogicModNotActive");
    validate(root_, snapshot, trust_);
    advance(snapshot); append(root_, snapshot);
}

LogicModSnapshotV1 DurableLogicModRegistryV1::snapshot() const {
    check_directory(root_, false); Lock lock(root_); return snapshot_locked();
}
LogicModSnapshotV1 DurableLogicModRegistryV1::snapshot_locked() const {
    if (legacy_data_only_root(root_)) {
        check_directory(root_, false);
        check_trust(root_, trust_);
        return empty_snapshot();
    }
    prepare(root_, trust_); return load(root_);
}

LogicModBindingV1 DurableLogicModRegistryV1::bind(
    const std::string_view id, const std::string_view version,
    const std::string_view template_id, const std::string_view capability) const {
    check_directory(root_, false); Lock lock(root_);
    return bind_locked(id, version, template_id, capability);
}
LogicModBindingV1 DurableLogicModRegistryV1::bind_locked(
    const std::string_view id, const std::string_view version,
    const std::string_view template_id, const std::string_view capability) const {
    prepare(root_, trust_);
    const auto snapshot = load(root_);
    const auto found = std::ranges::find_if(snapshot.records, [&](const auto& record) {
        return record.mod_id == id && record.version == version && record.state == LogicModStateV1::active;
    });
    if (found == snapshot.records.end()) throw Error("LogicModNotActive");
    auto installed = read_installed(root_, *found, trust_);
    if (std::ranges::find(installed.package.template_ids(), template_id) == installed.package.template_ids().end() ||
        std::ranges::find(installed.package.capabilities(), capability) == installed.package.capabilities().end())
        throw Error("LogicModBindingDenied");
    LogicModBindingV1 binding;
    binding.mod_id_ = found->mod_id; binding.version_ = found->version;
    binding.package_digest_ = found->package_digest; binding.worker_sha256_ = found->worker_sha256;
    binding.template_id_ = template_id; binding.capability_ = capability;
    binding.registry_digest_ = snapshot.journal_digest; binding.worker_bytes_ = std::move(installed.worker);
    return binding;
}

} // namespace bruce
