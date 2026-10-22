// Frozen verbatim from commit 08bc7f2 for legacy-reader migration tests.
#include "bruce/mod/runtime_registry.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/tree/normalizer.hpp"
#include "runtime_mod_store_internal.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <filesystem>
#include <limits>
#include <set>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace bruce {
namespace {
constexpr std::string_view state_header = "BRUCE_RUNTIME_MOD_REGISTRY_V1\n";
constexpr std::string_view journal_header = "BRUCE_RUNTIME_MOD_JOURNAL_V1\n";
constexpr std::size_t max_registry_bytes = 256 * 1024;
constexpr std::size_t max_journal_bytes = 32 * 1024 * 1024;
constexpr std::size_t max_records = 512;
struct Fd {
    int value;
    explicit Fd(int fd) : value(fd) { if (fd < 0) throw Error("RuntimeModFileOpenFailed"); }
    ~Fd() { ::close(value); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
};
void check_file(int fd, bool directory) {
    runtime_mod_store_detail::check_private_node(fd, directory);
}
void check_directory(const std::string& path, bool create) {
    runtime_mod_store_detail::check_private_directory(path, create);
}
bool exists(const std::string& path) {
    struct stat info{};
    if (::lstat(path.c_str(), &info) == 0) return true;
    if (errno != ENOENT) throw Error("RuntimeModFileStatFailed");
    return false;
}
std::string read_private(const std::string& path, std::size_t limit) {
    Fd file(::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOATIME));
    check_file(file.value, false);
    std::string result;
    std::array<char, 8192> buffer{};
    for (;;) {
        const auto count = ::read(file.value, buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) throw Error("RuntimeModFileReadFailed");
        if (count == 0) break;
        if (static_cast<std::size_t>(count) > limit - result.size()) throw Error("RuntimeModStoreByteLimit");
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
    if (read_private(root + "/truststore.pub", 65536) != trust_bytes(trust)) throw Error("RuntimeModTrustStoreChanged");
}
void field(std::string& bytes, std::string_view value) { bytes += std::to_string(value.size()) + ':'; bytes += value; }
std::uint64_t number(std::string_view value) {
    std::uint64_t n = 0;
    if (value.empty() || (value.size() > 1 && value.front() == '0')) throw Error("InvalidRuntimeModRegistry");
    const auto result = std::from_chars(value.data(), value.data() + value.size(), n);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) throw Error("InvalidRuntimeModRegistry");
    return n;
}
bool identifier(std::string_view value) {
    return !value.empty() && value.size() <= 128 && value.front() != '.' &&
        std::all_of(value.begin(), value.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_'; });
}
bool digest_valid(std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
// Incomplete journal fields are recoverable only at the final append. All complete
// records must pass their digest, generation, and canonical snapshot checks.
struct Incomplete {};
struct Reader {
    std::string_view bytes;
    std::size_t pos = 0;
    std::string_view field_value() {
        const auto colon = bytes.find(':', pos);
        if (colon == std::string_view::npos) {
            const auto tail = bytes.substr(pos);
            if (tail.size() <= 7 && std::all_of(tail.begin(), tail.end(), [](char c) { return c >= '0' && c <= '9'; })) throw Incomplete{};
            throw Error("InvalidRuntimeModRegistry");
        }
        const auto length = number(bytes.substr(pos, colon - pos));
        if (length > max_registry_bytes) throw Error("RuntimeModStoreByteLimit");
        pos = colon + 1;
        if (length > bytes.size() - pos) throw Incomplete{};
        const auto value = bytes.substr(pos, static_cast<std::size_t>(length));
        pos += static_cast<std::size_t>(length);
        return value;
    }
};
std::string serialize(const RuntimeModSnapshotV1& state) {
    std::string bytes(state_header);
    field(bytes, std::to_string(state.generation)); field(bytes, std::to_string(state.records.size()));
    for (const auto& r : state.records) {
        field(bytes, r.mod_id); field(bytes, r.version); field(bytes, r.package_digest);
        field(bytes, std::to_string(static_cast<unsigned>(r.state)));
    }
    bytes += '\n';
    if (bytes.size() > max_registry_bytes) throw Error("RuntimeModStoreByteLimit");
    return bytes;
}
RuntimeModSnapshotV1 parse_state(std::string_view bytes) {
    if (!bytes.starts_with(state_header)) throw Error("InvalidRuntimeModRegistry");
    Reader reader{bytes, state_header.size()}; RuntimeModSnapshotV1 state;
    try {
        state.generation = number(reader.field_value());
        const auto count = number(reader.field_value());
        if (count > max_records) throw Error("RuntimeModRecordLimit");
        std::set<std::string> active_ids;
        for (std::size_t i = 0; i < count; ++i) {
            RuntimeModRecordV1 record;
            record.mod_id = reader.field_value(); record.version = reader.field_value(); record.package_digest = reader.field_value();
            const auto status = number(reader.field_value());
            if (!identifier(record.mod_id) || record.mod_id == "core" || !identifier(record.version) || !digest_valid(record.package_digest) || status > 3)
                throw Error("InvalidRuntimeModRegistry");
            record.state = static_cast<RuntimeModStateV1>(status);
            if (!state.records.empty() && std::pair{state.records.back().mod_id, state.records.back().version} >= std::pair{record.mod_id, record.version})
                throw Error("InvalidRuntimeModRegistry");
            if (record.state == RuntimeModStateV1::active && !active_ids.insert(record.mod_id).second) throw Error("InvalidRuntimeModRegistry");
            state.records.push_back(std::move(record));
        }
    } catch (const Incomplete&) { throw Error("InvalidRuntimeModRegistry"); }
    if (reader.pos + 1 != bytes.size() || bytes.back() != '\n' || serialize(state) != bytes) throw Error("InvalidRuntimeModRegistry");
    return state;
}
void sync_directory(const std::string& root) {
    Fd directory(::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (::fsync(directory.value) != 0) throw Error("RuntimeModCommitDurabilityUnknown");
}
RuntimeModSnapshotV1 load(const std::string& root, const bool recover = true) {
    const auto journal_path = root + "/journal.v1";
    const auto journal = read_private(journal_path, max_journal_bytes);
    if (!journal.starts_with(journal_header)) throw Error("InvalidRuntimeModJournal");
    RuntimeModSnapshotV1 current;
    current.journal_digest = sha256(journal_header);
    const bool snapshot_exists = exists(root + "/registry.v1");
    const auto disk = snapshot_exists ? read_private(root + "/registry.v1", max_registry_bytes) : std::string{};
    bool known_snapshot = !snapshot_exists || disk == serialize(current);
    Reader reader{journal, journal_header.size()}; std::size_t good_end = reader.pos;
    while (reader.pos < journal.size()) {
        try {
            const auto previous = reader.field_value();
            const auto snapshot = reader.field_value();
            const auto hash = reader.field_value();
            if (reader.pos == journal.size()) throw Incomplete{};
            if (journal[reader.pos++] != '\n') throw Error("InvalidRuntimeModJournal");
            // Hash covers exactly the two encoded fields, excluding its own field and newline.
            std::string unsigned_record; field(unsigned_record, previous); field(unsigned_record, snapshot);
            if (previous != current.journal_digest || hash != sha256(unsigned_record)) throw Error("InvalidRuntimeModJournal");
            auto next = parse_state(snapshot);
            if (current.generation == std::numeric_limits<std::uint64_t>::max() || next.generation != current.generation + 1) throw Error("InvalidRuntimeModJournalGeneration");
            next.journal_digest = std::string(hash);
            current = std::move(next);
            if (disk == snapshot) known_snapshot = true;
            good_end = reader.pos;
        } catch (const Incomplete&) {
            if (!recover) throw Error("RuntimeModRecoveryRequired");
            Fd file(::open(journal_path.c_str(), O_WRONLY | O_NOFOLLOW | O_CLOEXEC)); check_file(file.value, false);
            if (::ftruncate(file.value, static_cast<off_t>(good_end)) != 0 || ::fsync(file.value) != 0) throw Error("RuntimeModJournalRecoveryFailed");
            break;
        }
    }
    if (!known_snapshot) throw Error("RuntimeModRegistryJournalMismatch");
    const auto canonical = serialize(current);
    if (disk != canonical) {
        if (!recover) throw Error("RuntimeModRecoveryRequired");
        LocalFiles::replace_atomically(root + "/registry.v1", canonical);
    }
    return current;
}
void append(const std::string& root, const RuntimeModSnapshotV1& state) {
    runtime_mod_store_detail::test_failpoint("data.before_journal");
    std::string record; field(record, state.journal_digest); field(record, serialize(state));
    const auto hash = sha256(record); field(record, hash); record += '\n';
    Fd journal(::open((root + "/journal.v1").c_str(), O_WRONLY | O_APPEND | O_NOFOLLOW | O_CLOEXEC));
    check_file(journal.value, false); struct stat info{};
    if (::fstat(journal.value, &info) != 0 || info.st_size < 0 || static_cast<std::uint64_t>(info.st_size) + record.size() > max_journal_bytes)
        throw Error("RuntimeModJournalLimit");
    std::size_t offset = 0;
    while (offset < record.size()) {
        const auto written = ::write(journal.value, record.data() + offset, record.size() - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) throw Error("RuntimeModJournalWriteFailed");
        offset += static_cast<std::size_t>(written);
    }
    if (::fsync(journal.value) != 0) throw Error("RuntimeModCommitDurabilityUnknown");
    runtime_mod_store_detail::test_failpoint("data.after_journal");
    LocalFiles::replace_atomically(root + "/registry.v1", serialize(state));
    runtime_mod_store_detail::test_failpoint("data.after_snapshot");
}
RuntimeDataModPackageV1 read_package(const std::string& root, const RuntimeModRecordV1& record, const RuntimeModTrustStoreV1& trust) {
    const auto package = unpack_runtime_data_mod_package_v1(read_private(root + "/packages/" + record.package_digest + ".brucemod", max_runtime_mod_package_bytes_v1));
    if (package.package_digest() != record.package_digest || package.manifest().mod_id != record.mod_id || package.manifest().version != record.version)
        throw Error("RuntimeModInstalledPackageMismatch");
    verify_runtime_data_mod_package_v1(package, trust);
    return package;
}
void golden(const RuntimeDataModPackageV1& package, const RuntimeModTrustStoreV1& trust, const TreeRegistryV1& tree) {
    for (const auto& test : package.golden_cases(trust)) {
        const auto result = tree.route(normalize_route_text_v1(test.input));
        if (result.status != test.status || (result.status == RouteStatusV1::matched && result.candidates.front().template_id != test.template_id))
            throw Error("RuntimeModGoldenCaseFailed");
    }
}
TreeRegistryV1 assemble(const std::string& root, const RuntimeModSnapshotV1& state, const RuntimeModTrustStoreV1& trust,
    const RuntimeDataModPackageV1* incoming = nullptr) {
    auto tree = core_tree_registry_v1(); std::vector<RuntimeDataModPackageV1> packages;
    for (const auto& record : state.records) if (record.state == RuntimeModStateV1::active) packages.push_back(read_package(root, record, trust));
    if (incoming) packages.push_back(*incoming);
    // Mount in dependency order: IDs do not define dependency order. Address
    // conflicts still fail immediately and never select a winner by ordering.
    std::vector<bool> mounted(packages.size(), false); std::size_t remaining = packages.size();
    while (remaining) {
        bool progress = false;
        for (std::size_t i = 0; i < packages.size(); ++i) if (!mounted[i]) {
            const auto fragment = packages[i].tree_fragment(trust);
            if (!tree.find(fragment.mount)) continue;
            tree = tree.mount(fragment, packages[i].manifest().mod_id);
            mounted[i] = true; --remaining; progress = true;
        }
        if (!progress) throw Error("TreeMountNotFound");
    }
    for (const auto& package : packages) golden(package, trust, tree);
    return tree;
}
void prepare(const std::string& root, const RuntimeModTrustStoreV1& trust) {
    check_directory(root, false); check_directory(root + "/packages", false); check_trust(root, trust);
}
void advance(RuntimeModSnapshotV1& state) {
    if (state.generation == std::numeric_limits<std::uint64_t>::max()) throw Error("RuntimeModGenerationLimit");
    ++state.generation;
}
}
RuntimeModRegistryV1::RuntimeModRegistryV1(std::string root, RuntimeModTrustStoreV1 trust) : root_(std::move(root)), trust_(std::move(trust)) {}
RuntimeModRegistryV1 RuntimeModRegistryV1::open(const std::string& mod_root, const RuntimeModTrustStoreV1& trust) {
    if (mod_root.empty()) throw Error("RuntimeModRootRequired");
    const auto root = std::filesystem::absolute(mod_root).string();
    check_directory(root, true); Lock lock(root);
    check_directory(root + "/packages", true);
    if (!exists(root + "/truststore.pub")) {
        if (exists(root + "/journal.v1") || exists(root + "/registry.v1")) throw Error("RuntimeModTrustStoreMissing");
        LocalFiles::publish_new_atomically(root + "/truststore.pub", trust_bytes(trust));
    }
    check_trust(root, trust);
    if (!exists(root + "/journal.v1")) {
        if (exists(root + "/registry.v1")) throw Error("RuntimeModJournalMissing");
        LocalFiles::publish_new_atomically(root + "/journal.v1", journal_header);
    }
    (void)load(root); sync_directory(root);
    return RuntimeModRegistryV1(root, trust);
}
RuntimeModRegistryV1 RuntimeModRegistryV1::open_existing(const std::string& root) {
    if (root.empty()) throw Error("RuntimeModRootRequired");
    check_directory(root, false); Lock lock(root);
    check_directory(root + "/packages", false);
    auto trust = read_runtime_mod_truststore_v1(read_private(root + "/truststore.pub", 65536));
    (void)load(root);
    return RuntimeModRegistryV1(std::filesystem::absolute(root).string(), std::move(trust));
}
RuntimeModSnapshotV1 RuntimeModRegistryV1::read_only_snapshot(const std::string& root) {
    check_directory(root, false);
    check_directory(root + "/packages", false);
    (void)read_runtime_mod_truststore_v1(read_private(root + "/truststore.pub", 65536));
    return load(root, false);
}
void RuntimeModRegistryV1::install(const RuntimeDataModPackageV1& package) {
    check_directory(root_, false); Lock lock(root_); install_locked(package);
}
void RuntimeModRegistryV1::install_locked(const RuntimeDataModPackageV1& package) {
    prepare(root_, trust_);
    auto state = load(root_);
    verify_runtime_data_mod_package_v1(package, trust_);
    for (const auto& record : state.records) if (record.mod_id == package.manifest().mod_id && record.version == package.manifest().version) throw Error("RuntimeModAlreadyInstalled");
    if (state.records.size() >= max_records) throw Error("RuntimeModRecordLimit");
    auto candidate = state;
    for (auto& record : candidate.records) if (record.mod_id == package.manifest().mod_id && record.state == RuntimeModStateV1::active)
        record.state = RuntimeModStateV1::disabled;
    (void)assemble(root_, candidate, trust_, &package);
    const auto path = root_ + "/packages/" + package.package_digest() + ".brucemod";
    if (exists(path)) {
        if (read_private(path, max_runtime_mod_package_bytes_v1) != package.bytes()) throw Error("RuntimeModInstalledPackageMismatch");
    } else LocalFiles::publish_new_atomically(path, package.bytes());
    runtime_mod_store_detail::test_failpoint("data.after_package");
    state.records.push_back({package.manifest().mod_id, package.manifest().version, package.package_digest(), RuntimeModStateV1::installed_inactive});
    std::sort(state.records.begin(), state.records.end(), [](const auto& a, const auto& b) { return std::pair{a.mod_id,a.version} < std::pair{b.mod_id,b.version}; });
    advance(state); append(root_, state);
}
void RuntimeModRegistryV1::update_locked(const RuntimeDataModPackageV1& package) {
    prepare(root_, trust_);
    auto state = load(root_);
    verify_runtime_data_mod_package_v1(package, trust_);
    auto found = std::find_if(state.records.begin(), state.records.end(), [&](const auto& record) {
        return record.mod_id == package.manifest().mod_id && record.version == package.manifest().version;
    });
    if (found != state.records.end() && found->package_digest != package.package_digest())
        throw Error("InstalledModVersionConflict");
    if (found != state.records.end() && found->state == RuntimeModStateV1::active) {
        (void)assemble(root_, state, trust_);
        return;
    }
    auto candidate = state;
    for (auto& record : candidate.records)
        if (record.mod_id == package.manifest().mod_id && record.state == RuntimeModStateV1::active)
            record.state = RuntimeModStateV1::disabled;
    if (found == state.records.end()) {
        if (candidate.records.size() >= max_records) throw Error("RuntimeModRecordLimit");
        (void)assemble(root_, candidate, trust_, &package);
        const auto path = root_ + "/packages/" + package.package_digest() + ".brucemod";
        if (exists(path)) {
            if (read_private(path, max_runtime_mod_package_bytes_v1) != package.bytes())
                throw Error("RuntimeModInstalledPackageMismatch");
        } else LocalFiles::publish_new_atomically(path, package.bytes());
        runtime_mod_store_detail::test_failpoint("data.after_package");
        candidate.records.push_back({package.manifest().mod_id, package.manifest().version,
            package.package_digest(), RuntimeModStateV1::active});
        std::sort(candidate.records.begin(), candidate.records.end(), [](const auto& left, const auto& right) {
            return std::pair{left.mod_id, left.version} < std::pair{right.mod_id, right.version};
        });
    } else {
        auto target = std::find_if(candidate.records.begin(), candidate.records.end(), [&](const auto& record) {
            return record.mod_id == package.manifest().mod_id && record.version == package.manifest().version;
        });
        target->state = RuntimeModStateV1::active;
        (void)assemble(root_, candidate, trust_);
    }
    advance(candidate); append(root_, candidate);
}
void RuntimeModRegistryV1::activate(std::string_view id, std::string_view version) {
    check_directory(root_, false); Lock lock(root_); activate_locked(id, version);
}
void RuntimeModRegistryV1::activate_locked(std::string_view id, std::string_view version) {
    prepare(root_, trust_);
    auto state = load(root_);
    auto target = std::find_if(state.records.begin(), state.records.end(), [&](const auto& r) { return r.mod_id == id && r.version == version; });
    if (target == state.records.end()) throw Error("RuntimeModNotInstalled");
    if (target->state == RuntimeModStateV1::revoked) throw Error("RuntimeModRevoked");
    if (target->state == RuntimeModStateV1::active) { (void)assemble(root_, state, trust_); return; }
    for (auto& record : state.records) if (record.mod_id == id && record.state == RuntimeModStateV1::active) record.state = RuntimeModStateV1::disabled;
    target->state = RuntimeModStateV1::active;
    (void)assemble(root_, state, trust_);
    advance(state); append(root_, state);
}
void RuntimeModRegistryV1::disable(std::string_view id) {
    check_directory(root_, false); Lock lock(root_); disable_locked(id);
}
void RuntimeModRegistryV1::disable_locked(std::string_view id) {
    prepare(root_, trust_);
    auto state = load(root_); bool found = false;
    for (auto& record : state.records) if (record.mod_id == id && record.state == RuntimeModStateV1::active) { record.state = RuntimeModStateV1::disabled; found = true; }
    if (!found) throw Error("RuntimeModNotActive");
    (void)assemble(root_, state, trust_);
    advance(state); append(root_, state);
}
RuntimeModSnapshotV1 RuntimeModRegistryV1::snapshot() const {
    check_directory(root_, false); Lock lock(root_); return snapshot_locked();
}
RuntimeModSnapshotV1 RuntimeModRegistryV1::snapshot_locked() const {
    prepare(root_, trust_); return load(root_);
}
TreeRegistryV1 RuntimeModRegistryV1::active_tree() const {
    return active_view().tree;
}
RuntimeModActiveViewV1 RuntimeModRegistryV1::active_view() const {
    check_directory(root_, false); Lock lock(root_); prepare(root_, trust_);
    auto state = load(root_);
    auto tree = assemble(root_, state, trust_);
    return {std::move(state), std::move(tree)};
}
}
