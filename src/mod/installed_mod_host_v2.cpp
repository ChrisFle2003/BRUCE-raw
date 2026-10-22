#include "bruce/mod/installed_mod_host_v2.hpp"

#include "bruce/tree/normalizer.hpp"
#include "runtime_mod_store_internal.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <filesystem>
#include <fcntl.h>
#include <limits>
#include <optional>
#include <set>
#include <sys/stat.h>
#include <variant>
#include <unistd.h>

namespace bruce {
namespace {

using Package = std::variant<RuntimeDataModPackageV1, LogicModPackageV1>;

DetectedModPackageV2 read_catalog_package(const std::string& path) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOATIME);
    if (fd < 0) throw Error("RuntimeModFileOpenFailed");
    std::string bytes;
    try {
        runtime_mod_store_detail::check_private_node(fd, false);
        struct stat before{};
        if (::fstat(fd, &before) != 0 || before.st_size < 0 ||
            static_cast<std::uint64_t>(before.st_size) > max_runtime_mod_package_bytes_v1)
            throw Error("RuntimeModStoreByteLimit");
        std::array<char, 8192> buffer{};
        for (;;) {
            const auto count = ::read(fd, buffer.data(), buffer.size());
            if (count < 0 && errno == EINTR) continue;
            if (count < 0) throw Error("RuntimeModFileReadFailed");
            if (count == 0) break;
            if (static_cast<std::size_t>(count) > max_runtime_mod_package_bytes_v1 - bytes.size())
                throw Error("RuntimeModStoreByteLimit");
            bytes.append(buffer.data(), static_cast<std::size_t>(count));
        }
        struct stat after{};
        if (::fstat(fd, &after) != 0 || before.st_dev != after.st_dev || before.st_ino != after.st_ino ||
            before.st_size != after.st_size || before.st_mtim.tv_sec != after.st_mtim.tv_sec ||
            before.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
            static_cast<std::uint64_t>(after.st_size) != bytes.size())
            throw Error("RuntimeModInstalledPackageMismatch");
    } catch (...) { ::close(fd); throw; }
    ::close(fd);
    return detect_mod_package_v2(bytes);
}

template<class State> std::string catalog_state(const State state) {
    switch (state) {
    case State::installed_inactive: return "installed_inactive";
    case State::active: return "active";
    case State::disabled: return "disabled";
    case State::revoked: return "revoked";
    default: throw Error("InvalidInstalledModState");
    }
}

bool builtin_id(const std::string_view id) {
    return id == "core" || id == "language" || id == "nanocell" || id == "mcp";
}

std::string id_of(const DetectedModPackageV2& package) {
    if (const auto* data = package.data_package()) return data->manifest().mod_id;
    return package.logic_package()->manifest().mod_id;
}

std::string version_of(const DetectedModPackageV2& package) {
    if (const auto* data = package.data_package()) return data->manifest().version;
    return package.logic_package()->manifest().version;
}

std::string digest_of(const DetectedModPackageV2& package) {
    if (const auto* data = package.data_package()) return data->package_digest();
    return package.logic_package()->package_digest();
}

RuntimeDataModPackageV1 read_data(const std::string& root, const RuntimeModRecordV1& record,
                                  const RuntimeModTrustStoreV1& trust) {
    const auto detected = detect_mod_package_v2(read_mod_package_file_v2(
        root + "/packages/" + record.package_digest + ".brucemod"));
    const auto* package = detected.data_package();
    if (!package || package->package_digest() != record.package_digest ||
        package->manifest().mod_id != record.mod_id || package->manifest().version != record.version)
        throw Error("RuntimeModInstalledPackageMismatch");
    verify_runtime_data_mod_package_v1(*package, trust);
    return *package;
}

LogicModPackageV1 read_logic(const std::string& root, const LogicModRecordV1& record,
                             const RuntimeModTrustStoreV1& trust) {
    const auto detected = detect_mod_package_v2(read_mod_package_file_v2(
        root + "/logic-packages/" + record.package_digest + ".brucemod"));
    const auto* package = detected.logic_package();
    if (!package || package->package_digest() != record.package_digest ||
        package->manifest().mod_id != record.mod_id || package->manifest().version != record.version ||
        package->worker_sha256() != record.worker_sha256)
        throw Error("LogicModInstalledPackageMismatch");
    verify_logic_mod_package_v1(*package, trust);
    return *package;
}

TreeRegistryV1 combined_tree(const std::string& root, const RuntimeModTrustStoreV1& trust,
                             const RuntimeModSnapshotV1& data,
                             const LogicModSnapshotV1& logic,
                             const DetectedModPackageV2* incoming = nullptr) {
    std::vector<Package> packages;
    const auto incoming_id = incoming ? id_of(*incoming) : std::string{};
    for (const auto& record : data.records)
        if (record.state == RuntimeModStateV1::active && record.mod_id != incoming_id)
            packages.emplace_back(read_data(root, record, trust));
    for (const auto& record : logic.records)
        if (record.state == LogicModStateV1::active && record.mod_id != incoming_id)
            packages.emplace_back(read_logic(root, record, trust));
    if (incoming) {
        verify_mod_package_v2(*incoming, trust);
        if (const auto* package = incoming->data_package()) packages.emplace_back(*package);
        else packages.emplace_back(*incoming->logic_package());
    }

    auto tree = core_tree_registry_v1();
    std::vector<bool> mounted(packages.size(), false);
    std::size_t remaining = packages.size();
    while (remaining != 0) {
        bool progress = false;
        for (std::size_t index = 0; index < packages.size(); ++index) if (!mounted[index]) {
            const auto fragment = std::visit([&](const auto& package) {
                return package.tree_fragment(trust);
            }, packages[index]);
            if (!tree.find(fragment.mount)) continue;
            const auto owner = std::visit([](const auto& package) {
                return package.manifest().mod_id;
            }, packages[index]);
            tree = tree.mount(fragment, owner);
            mounted[index] = true; --remaining; progress = true;
        }
        if (!progress) throw Error("TreeMountNotFound");
    }
    for (const auto& package : packages) {
        const auto cases = std::visit([&](const auto& value) { return value.golden_cases(trust); }, package);
        for (const auto& test : cases) {
            const auto route = tree.route(normalize_route_text_v1(test.input));
            if (route.status != test.status ||
                (route.status == RouteStatusV1::matched && route.candidates.front().template_id != test.template_id))
                throw Error("InstalledModGoldenCaseFailed");
        }
    }
    return tree;
}

template<class Record>
auto find_identity(std::vector<Record>& records, const std::string_view id,
                   const std::string_view version) {
    return std::ranges::find_if(records, [&](const auto& record) {
        return record.mod_id == id && record.version == version;
    });
}

void state_field(std::string& bytes, const std::string_view value) {
    bytes += std::to_string(value.size()) + ':';
    bytes += value;
}
std::string serialize_data_snapshot(const RuntimeModSnapshotV1& snapshot) {
    std::string bytes = "BRUCE_RUNTIME_MOD_REGISTRY_V1\n";
    state_field(bytes, std::to_string(snapshot.generation));
    state_field(bytes, std::to_string(snapshot.records.size()));
    for (const auto& record : snapshot.records) {
        state_field(bytes, record.mod_id); state_field(bytes, record.version);
        state_field(bytes, record.package_digest);
        state_field(bytes, std::to_string(static_cast<unsigned>(record.state)));
    }
    bytes += '\n';
    return bytes;
}
std::string serialize_logic_snapshot(const LogicModSnapshotV1& snapshot) {
    std::string bytes = "BRUCE_LOGIC_MOD_REGISTRY_V1\n";
    state_field(bytes, std::to_string(snapshot.generation));
    state_field(bytes, std::to_string(snapshot.records.size()));
    for (const auto& record : snapshot.records) {
        state_field(bytes, record.mod_id); state_field(bytes, record.version);
        state_field(bytes, record.package_digest); state_field(bytes, record.worker_sha256);
        state_field(bytes, std::to_string(static_cast<unsigned>(record.state)));
    }
    bytes += '\n';
    return bytes;
}
RuntimeModTrustStoreV1 resolved_trust_locked(const std::string& root) {
    return read_runtime_mod_truststore_v1(
        runtime_trust_store_detail::active_trust_bytes_locked(root));
}

} // namespace

InstalledModHostV2::InstalledModHostV2(std::string root, RuntimeModTrustStoreV1 trust)
    : root_(std::move(root)), trust_(std::move(trust)) {}

RuntimeTrustVerificationV2 InstalledModHostV2::trust_status(const std::string& mod_root) {
    return RuntimeTrustStoreV2::read_only_status(mod_root);
}

InstalledModHostV2 InstalledModHostV2::for_trust_operations(const std::string& mod_root) {
    if (mod_root.empty()) throw Error("RuntimeModRootRequired");
    return InstalledModHostV2(std::filesystem::absolute(mod_root).string(), {});
}

InstalledModHostV2 InstalledModHostV2::open(const std::string& mod_root,
                                             const RuntimeModTrustStoreV1& trust) {
    (void)RuntimeModRegistryV1::open(mod_root, trust);
    (void)DurableLogicModRegistryV1::open(mod_root, trust);
    return InstalledModHostV2(std::filesystem::absolute(mod_root).string(), trust);
}

InstalledModHostV2 InstalledModHostV2::open_existing(const std::string& mod_root) {
    auto logic = DurableLogicModRegistryV1::open_existing(mod_root);
    auto data = RuntimeModRegistryV1::open_existing(mod_root);
    if (logic.trust_.public_keys() != data.trust_.public_keys())
        throw Error("RuntimeModTrustStoreChanged");
    return InstalledModHostV2(std::filesystem::absolute(mod_root).string(), data.trust_);
}

InstalledModHostV2 InstalledModHostV2::open_existing(
    const std::string& mod_root, const RuntimeModTrustStoreV1& trust) {
    auto host = open_existing(mod_root);
    if (host.trust_.public_keys() != trust.public_keys())
        throw Error("RuntimeModTrustStoreChanged");
    return host;
}

std::vector<InstalledModProjectionV2> InstalledModHostV2::catalog_entries(
    const std::string& mod_root, const bool read_only) {
    if (mod_root.empty()) throw Error("RuntimeModRootRequired");
    auto root = std::filesystem::absolute(mod_root).string();
    std::optional<runtime_mod_store_detail::ReadOnlyRoot> pinned_root;
    std::optional<InstalledModHostV2> recovering_host;
    if (read_only) {
        pinned_root.emplace(root);
        root = pinned_root->anchored_path();
    } else {
        recovering_host.emplace(open_existing(root));
    }
    runtime_mod_store_detail::check_private_directory(root, false);
    runtime_mod_store_detail::RootLock lock(root, read_only);
    const auto data = read_only ? RuntimeModRegistryV1::read_only_snapshot(root)
        : RuntimeModRegistryV1(root, recovering_host->trust_).snapshot_locked();
    const auto logic = read_only ? DurableLogicModRegistryV1::read_only_snapshot(root)
        : DurableLogicModRegistryV1(root, recovering_host->trust_).snapshot_locked();
    std::vector<InstalledModProjectionV2> entries;
    const auto project = [&](const auto& records, const std::string& directory, const ModKindV2 kind) {
        for (const auto& record : records) {
            const auto package = read_catalog_package(root + directory + record.package_digest + ".brucemod");
            const auto info = inspect_mod_package_v2(package);
            if (info.mod_id != record.mod_id || info.version != record.version ||
                info.package_digest != record.package_digest ||
                (kind == ModKindV2::data) != (info.kind == ModPackageKindV2::data))
                throw Error("RuntimeModInstalledPackageMismatch");
            entries.push_back({.id = record.mod_id, .version = record.version, .kind = kind,
                .lifecycle_state = catalog_state(record.state), .package_digest = record.package_digest,
                .capabilities = info.capabilities});
        }
    };
    project(data.records, "/packages/", ModKindV2::data);
    project(logic.records, "/logic-packages/", ModKindV2::logic);
    return entries;
}

void InstalledModHostV2::install(const DetectedModPackageV2& package) {
    const auto id = id_of(package);
    if (builtin_id(id)) throw Error("BuiltinModImmutable");
    runtime_mod_store_detail::RootLock lock(root_);
    const auto trust = resolved_trust_locked(root_);
    verify_mod_package_v2(package, trust);
    RuntimeModRegistryV1 data(root_, trust);
    DurableLogicModRegistryV1 logic(root_, trust);
    auto data_snapshot = data.snapshot_locked();
    auto logic_snapshot = logic.snapshot_locked();
    const auto version = version_of(package);
    const auto digest = digest_of(package);

    if (package.kind() == ModPackageKindV2::data) {
        if (std::ranges::any_of(logic_snapshot.records, [&](const auto& record) { return record.mod_id == id; }))
            throw Error("InstalledModKindConflict");
        const auto found = find_identity(data_snapshot.records, id, version);
        if (found != data_snapshot.records.end()) {
            if (found->package_digest == digest) return;
            throw Error("InstalledModVersionConflict");
        }
    } else {
        if (std::ranges::any_of(data_snapshot.records, [&](const auto& record) { return record.mod_id == id; }))
            throw Error("InstalledModKindConflict");
        const auto found = find_identity(logic_snapshot.records, id, version);
        if (found != logic_snapshot.records.end()) {
            if (found->package_digest == digest) return;
            throw Error("InstalledModVersionConflict");
        }
    }
    (void)combined_tree(root_, trust, data_snapshot, logic_snapshot, &package);
    if (const auto* value = package.data_package()) data.install_locked(*value);
    else logic.install_locked(*package.logic_package());
}

void InstalledModHostV2::update(const DetectedModPackageV2& package) {
    const auto id = id_of(package);
    if (builtin_id(id)) throw Error("BuiltinModImmutable");
    runtime_mod_store_detail::RootLock lock(root_);
    const auto trust = resolved_trust_locked(root_);
    verify_mod_package_v2(package, trust);
    RuntimeModRegistryV1 data(root_, trust);
    DurableLogicModRegistryV1 logic(root_, trust);
    const auto data_snapshot = data.snapshot_locked();
    const auto logic_snapshot = logic.snapshot_locked();
    const auto version = version_of(package);
    const auto package_digest = digest_of(package);
    if (package.kind() == ModPackageKindV2::data) {
        if (std::ranges::any_of(logic_snapshot.records, [&](const auto& record) { return record.mod_id == id; }))
            throw Error("InstalledModKindConflict");
        const auto found = std::ranges::find_if(data_snapshot.records, [&](const auto& record) {
            return record.mod_id == id && record.version == version;
        });
        if (found != data_snapshot.records.end() && found->package_digest != package_digest)
            throw Error("InstalledModVersionConflict");
    } else {
        if (std::ranges::any_of(data_snapshot.records, [&](const auto& record) { return record.mod_id == id; }))
            throw Error("InstalledModKindConflict");
        const auto found = std::ranges::find_if(logic_snapshot.records, [&](const auto& record) {
            return record.mod_id == id && record.version == version;
        });
        if (found != logic_snapshot.records.end() && found->package_digest != package_digest)
            throw Error("InstalledModVersionConflict");
    }
    (void)combined_tree(root_, trust, data_snapshot, logic_snapshot, &package);
    if (const auto* value = package.data_package()) data.update_locked(*value);
    else logic.update_locked(*package.logic_package());
}

void InstalledModHostV2::activate(const std::string_view id, const std::string_view version) {
    if (builtin_id(id)) throw Error("BuiltinModImmutable");
    runtime_mod_store_detail::RootLock lock(root_);
    const auto trust = resolved_trust_locked(root_);
    RuntimeModRegistryV1 data(root_, trust);
    DurableLogicModRegistryV1 logic(root_, trust);
    auto data_snapshot = data.snapshot_locked();
    auto logic_snapshot = logic.snapshot_locked();
    auto data_target = find_identity(data_snapshot.records, id, version);
    auto logic_target = find_identity(logic_snapshot.records, id, version);
    if (data_target == data_snapshot.records.end() && logic_target == logic_snapshot.records.end())
        throw Error("InstalledModNotInstalled");
    if (data_target != data_snapshot.records.end()) {
        for (auto& record : data_snapshot.records) if (record.mod_id == id && record.state == RuntimeModStateV1::active)
            record.state = RuntimeModStateV1::disabled;
        data_target->state = RuntimeModStateV1::active;
    } else {
        for (auto& record : logic_snapshot.records) if (record.mod_id == id && record.state == LogicModStateV1::active)
            record.state = LogicModStateV1::disabled;
        logic_target->state = LogicModStateV1::active;
    }
    (void)combined_tree(root_, trust, data_snapshot, logic_snapshot);
    if (data_target != data_snapshot.records.end()) data.activate_locked(id, version);
    else logic.activate_locked(id, version);
}

void InstalledModHostV2::disable(const std::string_view id) {
    if (builtin_id(id)) throw Error("BuiltinModImmutable");
    runtime_mod_store_detail::RootLock lock(root_);
    const auto trust = resolved_trust_locked(root_);
    RuntimeModRegistryV1 data(root_, trust);
    DurableLogicModRegistryV1 logic(root_, trust);
    auto data_snapshot = data.snapshot_locked();
    auto logic_snapshot = logic.snapshot_locked();
    bool data_active = false, logic_active = false;
    for (auto& record : data_snapshot.records) if (record.mod_id == id && record.state == RuntimeModStateV1::active) {
        record.state = RuntimeModStateV1::disabled; data_active = true;
    }
    for (auto& record : logic_snapshot.records) if (record.mod_id == id && record.state == LogicModStateV1::active) {
        record.state = LogicModStateV1::disabled; logic_active = true;
    }
    if (!data_active && !logic_active) throw Error("InstalledModNotActive");
    (void)combined_tree(root_, trust, data_snapshot, logic_snapshot);
    if (data_active) data.disable_locked(id); else logic.disable_locked(id);
}

InstalledModSnapshotV2 InstalledModHostV2::snapshot() const {
    runtime_mod_store_detail::RootLock lock(root_);
    const auto trust = resolved_trust_locked(root_);
    RuntimeModRegistryV1 data(root_, trust);
    DurableLogicModRegistryV1 logic(root_, trust);
    return {data.snapshot_locked(), logic.snapshot_locked()};
}

InstalledDataModRouteV1 InstalledModHostV2::route_data(const std::string_view text) const {
    if (text.size() > 4096) throw Error("RouteInputTooLarge");
    runtime_mod_store_detail::RootLock lock(root_);
    const auto trust = resolved_trust_locked(root_);
    RuntimeModRegistryV1 data(root_, trust);
    auto snapshot = data.snapshot_locked();
    const auto tree = combined_tree(root_, trust, snapshot, LogicModSnapshotV1{});
    return {std::move(snapshot), tree.digest(), tree.route(normalize_route_text_v1(text))};
}

InstalledModRouteV2 InstalledModHostV2::route(const std::string_view text) const {
    if (text.size() > 4096) throw Error("RouteInputTooLarge");
    runtime_mod_store_detail::RootLock lock(root_);
    const auto trust = resolved_trust_locked(root_);
    RuntimeModRegistryV1 data(root_, trust);
    DurableLogicModRegistryV1 logic(root_, trust);
    InstalledModSnapshotV2 snapshot{data.snapshot_locked(), logic.snapshot_locked()};
    const auto tree = combined_tree(root_, trust, snapshot.data, snapshot.logic);
    return {std::move(snapshot), tree.digest(), tree.route(normalize_route_text_v1(text))};
}

InstalledModRunV2 InstalledModHostV2::run_logic(
    const std::string_view id, const std::string_view template_id,
    const std::string_view input, const LogicModBudgetsV1 budgets,
    const std::string_view capability) const {
    if (builtin_id(id)) throw Error("BuiltinModImmutable");
    runtime_mod_store_detail::RootLock lock(root_);
    const auto trust = resolved_trust_locked(root_);
    RuntimeModRegistryV1 data(root_, trust);
    DurableLogicModRegistryV1 logic(root_, trust);
    const auto data_snapshot = data.snapshot_locked();
    const auto logic_snapshot = logic.snapshot_locked();
    if (std::ranges::any_of(data_snapshot.records, [&](const auto& record) {
            return record.mod_id == id;
        })) throw Error("InstalledLogicModRequired");
    const auto found = std::ranges::find_if(logic_snapshot.records, [&](const auto& record) {
        return record.mod_id == id && record.state == LogicModStateV1::active;
    });
    if (found == logic_snapshot.records.end()) {
        if (std::ranges::none_of(logic_snapshot.records, [&](const auto& record) {
                return record.mod_id == id;
            })) throw Error("InstalledModNotInstalled");
        throw Error("LogicModNotActive");
    }
    auto binding = logic.bind_locked(id, found->version, template_id, capability);
    InstalledLogicModIdentityV2 identity{
        binding.mod_id(), binding.version(), binding.package_digest(),
        binding.worker_sha256(), binding.template_id(), binding.capability(),
        binding.registry_digest()};
    // Keep registry.lock through process start and completion. A later trust
    // rotation cannot invalidate this already-started immutable worker copy.
    auto run = run_logic_mod_worker_v1(binding, input, budgets);
    return {std::move(identity), std::move(run)};
}

void InstalledModHostV2::import_trust_v1(
    const RuntimeTrustClockV2& clock, std::optional<std::string> comment) {
    runtime_mod_store_detail::ReadOnlyRoot pinned(root_);
    const auto root = pinned.anchored_path();
    runtime_mod_store_detail::RootLock lock(root, false, false);
    runtime_trust_store_detail::import_v1_locked(root, clock, std::move(comment), true);
}

void InstalledModHostV2::prepare_trust(
    const std::string& external_truststore, const RuntimeTrustClockV2& clock,
    std::optional<std::string> comment) {
    runtime_mod_store_detail::ReadOnlyRoot pinned(root_);
    const auto root = pinned.anchored_path();
    runtime_mod_store_detail::RootLock lock(root, false, false);
    runtime_trust_store_detail::prepare_locked(root, external_truststore, clock, std::move(comment), true);
}

RuntimeTrustVerificationV2 InstalledModHostV2::verify_trust() const {
    runtime_mod_store_detail::ReadOnlyRoot pinned(root_);
    const auto root = pinned.anchored_path();
    runtime_mod_store_detail::RootLock lock(root, false, false);
    return runtime_trust_store_detail::verify_for_host_locked(root);
}

InstalledModTrustTransitionV2 InstalledModHostV2::activate_trust(
    const bool deactivate_untrusted) {
    return transition_trust(false, deactivate_untrusted);
}

InstalledModTrustTransitionV2 InstalledModHostV2::rollback_trust(
    const bool deactivate_untrusted) {
    return transition_trust(true, deactivate_untrusted);
}

InstalledModTrustTransitionV2 InstalledModHostV2::transition_trust(
    const bool rollback, const bool deactivate_untrusted) {
    runtime_mod_store_detail::ReadOnlyRoot pinned(root_);
    const auto root = pinned.anchored_path();
    runtime_mod_store_detail::RootLock lock(root, false, false);
    const auto plan = runtime_trust_store_detail::plan_transition_locked(root, rollback, true);
    if (!plan.affected.empty() && !deactivate_untrusted)
        throw InstalledModTrustBlockedV2(plan.affected);

    const auto current_trust = resolved_trust_locked(root);

    RuntimeModRegistryV1 data(root, current_trust);
    DurableLogicModRegistryV1 logic(root, current_trust);
    auto data_snapshot = data.snapshot_locked();
    auto logic_snapshot = logic.snapshot_locked();
    bool data_changed = false, logic_changed = false;
    for (const auto& affected : plan.affected) {
        if (affected.kind == RuntimeInstalledModKindV2::data) {
            const auto found = find_identity(data_snapshot.records, affected.mod_id, affected.version);
            if (found == data_snapshot.records.end() || found->state != RuntimeModStateV1::active)
                throw Error("RuntimeTrustInstalledPackageIntegrity");
            found->state = RuntimeModStateV1::disabled; data_changed = true;
        } else {
            const auto found = find_identity(logic_snapshot.records, affected.mod_id, affected.version);
            if (found == logic_snapshot.records.end() || found->state != LogicModStateV1::active)
                throw Error("RuntimeTrustInstalledPackageIntegrity");
            found->state = LogicModStateV1::disabled; logic_changed = true;
        }
    }
    if (data_changed) {
        if (data_snapshot.generation == std::numeric_limits<std::uint64_t>::max())
            throw Error("RuntimeModGenerationOverflow");
        ++data_snapshot.generation;
    }
    if (logic_changed) {
        if (logic_snapshot.generation == std::numeric_limits<std::uint64_t>::max())
            throw Error("LogicModGenerationOverflow");
        ++logic_snapshot.generation;
    }
    const auto candidate_trust = read_runtime_mod_truststore_v1(plan.candidate_trust_bytes);
    (void)combined_tree(root, candidate_trust, data_snapshot, logic_snapshot);
    runtime_trust_store_detail::commit_transition_locked(
        root, rollback, serialize_data_snapshot(data_snapshot),
        serialize_logic_snapshot(logic_snapshot));
    return {plan.next_state, deactivate_untrusted ? plan.affected
                                                  : std::vector<RuntimeAffectedModV2>{}};
}

} // namespace bruce
