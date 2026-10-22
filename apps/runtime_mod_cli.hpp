#pragma once
#include "bruce/core/json.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/mod/catalog.hpp"
#include "bruce/mod/installed_mod_host_v2.hpp"
#include "bruce/mod/package_inspection_v2.hpp"
#include "bruce/mod/logic_mod_worker_v1.hpp"
#include "bruce/mod/runtime_registry.hpp"
#include "bruce/mod/unified_catalog_v2.hpp"
#include "bruce/tree/normalizer.hpp"
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <ctime>

namespace bruce {
namespace runtime_mod_cli_detail {
class SystemTrustClock final : public RuntimeTrustClockV2 {
public:
    std::string now_utc() const override {
        const auto now = std::time(nullptr);
        std::tm utc{};
        char bytes[21]{};
        if (now == static_cast<std::time_t>(-1) || !::gmtime_r(&now, &utc) ||
            std::strftime(bytes, sizeof(bytes), "%Y-%m-%dT%H:%M:%SZ", &utc) != 20)
            throw Error("RuntimeTrustClockFailed");
        return bytes;
    }
};
inline std::string affected_json(const std::vector<RuntimeAffectedModV2>& records) {
    std::string result = "[";
    for (const auto& record : records) {
        if (result.size() > 1) result += ',';
        result += "{\"kind\":" + json_string(record.kind == RuntimeInstalledModKindV2::data ? "data" : "logic") +
            ",\"mod_id\":" + json_string(record.mod_id) + ",\"version\":" + json_string(record.version) +
            ",\"signing_key_id\":" + json_string(record.signing_key_id) + '}';
    }
    return result + ']';
}
inline std::string trust_result(std::string_view action, const RuntimeTrustStateV2& state) {
    auto bytes = render_runtime_trust_state_v2(state);
    bytes.pop_back(); // The nested canonical state has one final LF.
    return "{\"contract\":\"runtime-mod-trust-v2\",\"action\":" + json_string(action) +
        ",\"state\":" + bytes;
}
inline constexpr std::size_t report_byte_limit = 1024 * 1024;
using Options = std::map<std::string, std::string>;
inline Options options(int argc, char** argv, int start, const std::set<std::string>& allowed,
                       const std::set<std::string>& flags = {}) {
    Options result;
    for (int i = start; i < argc;) {
        const std::string key = argv[i];
        if (flags.contains(key)) {
            if (!result.emplace(key, "true").second) throw Error("DuplicateOption: " + key);
            ++i;
            continue;
        }
        if (!allowed.contains(key)) throw Error("UnknownOption: " + key);
        if (i + 1 == argc) throw Error("MissingOptionValue: " + key);
        if (!result.emplace(key, argv[i + 1]).second) throw Error("DuplicateOption: " + key);
        i += 2;
    }
    return result;
}
inline const std::string& required(const Options& args, const std::string& key) {
    const auto found = args.find(key);
    if (found == args.end() || found->second.empty()) throw Error("MissingOption: " + key);
    return found->second;
}
inline std::string_view state_name(RuntimeModStateV1 state) {
    switch (state) {
    case RuntimeModStateV1::installed_inactive: return "installed_inactive";
    case RuntimeModStateV1::active: return "active";
    case RuntimeModStateV1::disabled: return "disabled";
    case RuntimeModStateV1::revoked: return "revoked";
    }
    throw Error("InvalidRuntimeModState");
}
inline std::string_view state_name(LogicModStateV1 state) {
    switch (state) {
    case LogicModStateV1::installed_inactive: return "installed_inactive";
    case LogicModStateV1::active: return "active";
    case LogicModStateV1::disabled: return "disabled";
    case LogicModStateV1::revoked: return "revoked";
    default: throw Error("InvalidDurableLogicModState");
    }
}
inline bool builtin_mod_id(const std::string_view id) {
    return id == "core" || id == "language" || id == "nanocell" || id == "mcp";
}
inline std::string_view status_name(RouteStatusV1 status) {
    switch (status) {
    case RouteStatusV1::matched: return "MATCHED";
    case RouteStatusV1::unknown: return "UNKNOWN";
    case RouteStatusV1::ambiguous: return "AMBIGUOUS";
    }
    throw Error("InvalidRouteStatus");
}
inline std::string identity(const RuntimeModRecordV1& record) {
    return "{\"mod_id\":" + json_string(record.mod_id) + ",\"version\":" +
        json_string(record.version) + ",\"package_digest\":" + json_string(record.package_digest);
}
inline std::string render_snapshot(const RuntimeModSnapshotV1& state) {
    std::string result = "{\"contract\":\"runtime-mod-registry-v1\",\"generation\":" +
        std::to_string(state.generation) + ",\"journal_digest\":" + json_string(state.journal_digest) + ",\"records\":[";
    for (std::size_t i = 0; i < state.records.size(); ++i) {
        if (i) result += ',';
        result += identity(state.records[i]) + ",\"state\":" + json_string(state_name(state.records[i].state)) + '}';
    }
    return result + "]}";
}
inline UnifiedModCatalogV2 unified_catalog(const std::optional<std::string>& root,
                                          const bool read_only = true) {
    const auto installed = root ? InstalledModHostV2::catalog_entries(*root, read_only)
                                : std::vector<InstalledModProjectionV2>{};
    return make_unified_mod_catalog_v2(builtin_mod_features_v1(), installed);
}
inline std::string render_status(const UnifiedModCatalogV2& catalog,
                                 const std::string_view id,
                                 const std::optional<std::string_view> version) {
    UnifiedModCatalogV2 filtered;
    for (const auto& entry : catalog.entries) {
        if (entry.id == id && (!version || entry.version == *version)) {
            filtered.entries.push_back(entry);
        }
    }
    if (filtered.entries.empty()) throw Error("ModNotFound");
    return render_unified_mod_catalog_v2(filtered);
}
inline std::string render_route(const InstalledDataModRouteV1& routed, std::string_view text) {
    const auto& route = routed.route;
    std::string result = "{\"contract\":\"route-report-v1\",\"text\":" + json_string(text) +
        ",\"registry_digest\":" + json_string(routed.registry_digest) + ",\"status\":" +
        json_string(status_name(route.status)) + ",\"candidate_addresses\":[";
    for (std::size_t i = 0; i < route.candidates.size(); ++i) {
        if (i) result += ',';
        result += '[';
        for (std::size_t j = 0; j < route.candidates[i].id.size(); ++j) {
            if (j) result += ',';
            result += std::to_string(route.candidates[i].id[j]);
        }
        result += ']';
        if (result.size() > report_byte_limit) throw Error("RouteReportTooLarge");
    }
    result += "],\"selected_template\":";
    result += route.status == RouteStatusV1::matched ? json_string(route.candidates.front().template_id) : "null";
    result += ",\"active_packages\":[";
    bool first = true;
    for (const auto& record : routed.snapshot.records) if (record.state == RuntimeModStateV1::active) {
        if (!first) result += ',';
        first = false;
        result += identity(record) + '}';
    }
    result += "]}";
    if (result.size() + 1 > report_byte_limit) throw Error("RouteReportTooLarge");
    return result;
}
inline std::string owner_v2(const InstalledModSnapshotV2& snapshot, const std::string_view owner) {
    if (owner == "core")
        return "{\"id\":\"core\",\"origin\":\"builtin\",\"kind\":\"builtin\",\"version\":null,\"package_digest\":null}";
    for (const auto& record : snapshot.data.records) if (record.mod_id == owner && record.state == RuntimeModStateV1::active)
        return "{\"id\":" + json_string(record.mod_id) + ",\"origin\":\"installed\",\"kind\":\"data\",\"version\":" +
            json_string(record.version) + ",\"package_digest\":" + json_string(record.package_digest) + '}';
    for (const auto& record : snapshot.logic.records) if (record.mod_id == owner && record.state == LogicModStateV1::active)
        return "{\"id\":" + json_string(record.mod_id) + ",\"origin\":\"installed\",\"kind\":\"logic\",\"version\":" +
            json_string(record.version) + ",\"package_digest\":" + json_string(record.package_digest) + '}';
    for (const auto& feature : builtin_mod_features_v1()) if (feature.id == owner)
        return "{\"id\":" + json_string(feature.id) + ",\"origin\":\"builtin\",\"kind\":\"builtin\",\"version\":" +
            json_string(feature.version) + ",\"package_digest\":null}";
    throw Error("RouteOwnerNotFound");
}
inline std::string address_json(const TreeAddressV1& address) {
    std::string result = "[";
    for (std::size_t index = 0; index < address.size(); ++index) {
        if (index != 0) result += ',';
        result += std::to_string(address[index]);
    }
    return result + ']';
}
inline std::string render_route_v2(const InstalledModRouteV2& routed, const std::string_view text) {
    const auto& route = routed.route;
    std::string result = "{\"contract\":\"route-report-v2\",\"text\":" + json_string(text) +
        ",\"registry_digest\":" + json_string(routed.registry_digest) + ",\"status\":" +
        json_string(status_name(route.status)) + ",\"candidates\":[";
    for (std::size_t index = 0; index < route.candidates.size(); ++index) {
        if (index != 0) result += ',';
        const auto& candidate = route.candidates[index];
        result += "{\"address\":" + address_json(candidate.id) + ",\"template_id\":" +
            json_string(candidate.template_id) + ",\"owner\":" + owner_v2(routed.snapshot, candidate.owner_id) + '}';
    }
    result += "],\"selected_template\":";
    result += route.status == RouteStatusV1::matched ? json_string(route.candidates.front().template_id) : "null";
    result += ",\"selected_owner\":";
    result += route.status == RouteStatusV1::matched ? owner_v2(routed.snapshot, route.candidates.front().owner_id) : "null";
    result += ",\"active_packages\":[";
    bool first = true;
    for (const auto& record : routed.snapshot.data.records) if (record.state == RuntimeModStateV1::active) {
        if (!first) result += ',';
        first = false;
        result += "{\"id\":" + json_string(record.mod_id) + ",\"version\":" + json_string(record.version) +
            ",\"kind\":\"data\",\"package_digest\":" + json_string(record.package_digest) + '}';
    }
    for (const auto& record : routed.snapshot.logic.records) if (record.state == LogicModStateV1::active) {
        if (!first) result += ',';
        first = false;
        result += "{\"id\":" + json_string(record.mod_id) + ",\"version\":" + json_string(record.version) +
            ",\"kind\":\"logic\",\"package_digest\":" + json_string(record.package_digest) + '}';
    }
    result += "]}";
    if (result.size() + 1 > report_byte_limit) throw Error("RouteReportTooLarge");
    return result;
}
// Replay accepts only the canonical report emitted here. Read its bounded input
// and digest prefix, then compare every byte with a freshly generated report.
// This rejects duplicate/extra fields and altered candidate/template/package data
// without introducing a permissive general-purpose JSON parser.
class ReportPrefix {
public:
    explicit ReportPrefix(std::string_view bytes) : bytes_(bytes) {}
    void literal(std::string_view expected) {
        if (!bytes_.starts_with(expected)) throw Error("InvalidRouteReport");
        bytes_.remove_prefix(expected.size());
    }
    std::string string(std::size_t limit) {
        literal("\"");
        std::string result;
        while (!bytes_.empty()) {
            const auto c = next();
            if (c == '"') return result;
            if (static_cast<unsigned char>(c) < 32) throw Error("InvalidRouteReport");
            if (c != '\\') result += c;
            else {
                const auto escaped = next();
                if (escaped == '"' || escaped == '\\') result += escaped;
                else if (escaped == 'u') {
                    literal("00");
                    const auto hex = [](char digit) -> unsigned {
                        if (digit >= '0' && digit <= '9') return digit - '0';
                        if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
                        throw Error("InvalidRouteReport");
                    };
                    const auto high = hex(next());
                    const auto low = hex(next());
                    result += static_cast<char>(high * 16 + low);
                } else throw Error("InvalidRouteReport");
            }
            if (result.size() > limit) throw Error("InvalidRouteReport");
        }
        throw Error("InvalidRouteReport");
    }
private:
    char next() {
        if (bytes_.empty()) throw Error("InvalidRouteReport");
        const char result = bytes_.front(); bytes_.remove_prefix(1); return result;
    }
    std::string_view bytes_;
};
inline std::string replay(const InstalledModHostV2& host, std::string bytes) {
    if (bytes.empty() || bytes.back() != '\n') throw Error("InvalidRouteReport");
    bytes.pop_back();
    ReportPrefix prefix(bytes);
    prefix.literal("{\"contract\":\"route-report-v1\",\"text\":");
    const auto text = prefix.string(4096);
    prefix.literal(",\"registry_digest\":");
    const auto digest = prefix.string(64);
    if (digest.size() != 64) throw Error("InvalidRouteReport");
    const auto routed = host.route_data(text);
    if (digest != routed.registry_digest) throw Error("RouteRegistryDigestMismatch");
    const auto report = render_route(routed, text);
    if (bytes != report) throw Error("RouteReportMismatch");
    return report;
}
inline std::string replay_v2(const InstalledModHostV2& host, std::string bytes) {
    if (bytes.empty() || bytes.back() != '\n') throw Error("InvalidRouteReport");
    bytes.pop_back();
    ReportPrefix prefix(bytes);
    prefix.literal("{\"contract\":\"route-report-v2\",\"text\":");
    const auto text = prefix.string(4096);
    prefix.literal(",\"registry_digest\":");
    const auto digest = prefix.string(64);
    if (digest.size() != 64) throw Error("InvalidRouteReport");
    const auto routed = host.route(text);
    if (digest != routed.registry_digest) throw Error("RouteRegistryDigestMismatch");
    const auto report = render_route_v2(routed, text);
    if (bytes != report) throw Error("RouteReportMismatch");
    return report;
}

inline int trust_cli(int argc, char** argv) {
    // Only symbolic public codes cross this boundary. In particular parser
    // diagnostics and filesystem exceptions must never echo input paths/keys.
    try {
        if (argc < 4) throw Error("MissingRuntimeTrustAction");
        const std::string action = argv[3];
        if (action != "status" && action != "import-v1" && action != "prepare" &&
            action != "verify" && action != "activate" && action != "rollback")
            throw Error("UnknownRuntimeTrustAction");
        const bool transition = action == "activate" || action == "rollback";
        int start = 4;
        if (action == "prepare") {
            if (argc < 5 || std::string_view(argv[4]).empty() || std::string_view(argv[4]).starts_with("--"))
                throw Error("MissingRuntimeTrustStore");
            ++start;
        }
        // Reject a flag consumed as a path before any filesystem access.
        for (int i = start; i < argc; ++i) {
            if (std::string_view(argv[i]) == "--mod-root" && i + 1 < argc &&
                std::string_view(argv[i + 1]).starts_with("--")) throw Error("MissingOptionValue");
        }
        const auto args = options(argc, argv, start, {"--mod-root"}, transition
            ? std::set<std::string>{"--deactivate-untrusted"} : std::set<std::string>{});
        const auto root = required(args, "--mod-root");
        if (action == "status") {
            std::cout << trust_result(action, InstalledModHostV2::trust_status(root).state) << "}\n";
        } else {
            auto host = InstalledModHostV2::for_trust_operations(root);
            if (transition) {
                const auto reply = action == "activate" ? host.activate_trust(args.contains("--deactivate-untrusted"))
                    : host.rollback_trust(args.contains("--deactivate-untrusted"));
                const auto bytes = trust_result(action, reply.state) + ",\"deactivated\":" + affected_json(reply.deactivated) + '}';
                std::cout << bytes << '\n';
            } else {
                SystemTrustClock clock;
                if (action == "import-v1") host.import_trust_v1(clock);
                if (action == "prepare") host.prepare_trust(argv[4], clock);
                if (action == "verify") {
                    const auto reply = host.verify_trust();
                    const auto bytes = trust_result(action, reply.state) + ",\"active_incompatible\":" +
                        affected_json(reply.active_incompatible) + ",\"prepared_incompatible\":" +
                        affected_json(reply.prepared_incompatible) + '}';
                    std::cout << bytes << '\n';
                } else {
                    // The successful mutation is authoritative. Do not acquire
                    // another lock just to decorate a committed success reply.
                    std::cout << "{\"contract\":\"runtime-mod-trust-v2\",\"action\":" << json_string(action)
                        << ",\"result\":\"ok\"}\n";
                }
            }
        }
        return 0;
    } catch (const InstalledModTrustBlockedV2& error) {
        const auto bytes = "{\"error\":\"RuntimeTrustActiveModsIncompatible\",\"affected\":" +
            affected_json(error.affected()) + '}';
        if (bytes.size() + 1 > report_byte_limit) throw Error("RuntimeTrustReportTooLarge");
        std::cerr << bytes << '\n';
        return 2;
    } catch (const Error& error) {
        const std::string_view message = error.what();
        const auto code = message.substr(0, message.find(':'));
        if (!code.empty() && code.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") == std::string_view::npos)
            throw Error(std::string(code));
        throw Error("RuntimeTrustOperationFailed");
    } catch (const std::exception&) {
        throw Error("RuntimeTrustOperationFailed");
    }
}
}

inline int runtime_mod_cli(int argc, char** argv) {
    using namespace runtime_mod_cli_detail;
    const std::string command = argv[1];
    if (command == "mods") {
        if (argc < 3) throw Error("MissingRuntimeModAction");
        const std::string action = argv[2];
        if (action == "trust") return trust_cli(argc, argv);
        if (action == "inspect") {
            if (argc < 4) throw Error("MissingRuntimeModPackage");
            (void)options(argc, argv, 4, {});
            const auto package = detect_mod_package_v2(read_mod_package_file_v2(argv[3]));
            std::cout << render_mod_package_inspection_v2(inspect_mod_package_v2(package)) << '\n';
        } else if (action == "verify" || action == "install" || action == "update") {
            if (argc < 4) throw Error("MissingRuntimeModPackage");
            const auto args = options(argc, argv, 4, action == "verify"
                ? std::set<std::string>{"--trust-store"} : std::set<std::string>{"--trust-store", "--mod-root"});
            const auto trust_path = required(args, "--trust-store");
            if (action == "install" || action == "update") (void)required(args, "--mod-root");
            const auto trust = read_runtime_mod_truststore_v1(read_mod_trust_store_file_v1(trust_path));
            const auto package = detect_mod_package_v2(read_mod_package_file_v2(argv[3]));
            verify_mod_package_v2(package, trust);
            if (action == "verify") {
                std::cout << render_mod_package_verification_v2(inspect_mod_package_v2(package)) << '\n';
            } else {
                if (builtin_mod_id(inspect_mod_package_v2(package).mod_id)) throw Error("BuiltinModImmutable");
                const auto root = required(args, "--mod-root");
                if (action == "install") {
                    auto host = InstalledModHostV2::open(root, trust);
                    host.install(package);
                } else {
                    const auto status = std::filesystem::symlink_status(root);
                    auto host = std::filesystem::exists(status)
                        ? InstalledModHostV2::open_existing(root, trust)
                        : InstalledModHostV2::open(root, trust);
                    host.update(package);
                }
                std::cout << render_unified_mod_catalog_v2(unified_catalog(root, false)) << '\n';
            }
        } else if (action == "list") {
            const auto args = options(argc, argv, 3, {"--mod-root"});
            const auto found = args.find("--mod-root");
            const std::optional<std::string> root = found == args.end()
                ? std::nullopt : std::optional<std::string>(found->second);
            std::cout << render_unified_mod_catalog_v2(unified_catalog(root)) << '\n';
        } else if (action == "status") {
            if (argc < 4) throw Error("MissingRuntimeModIdentity");
            int start = 4;
            std::optional<std::string_view> version;
            if (start < argc && !std::string_view(argv[start]).starts_with("--")) {
                version = argv[start++];
            }
            const auto args = options(argc, argv, start, {"--mod-root"});
            const auto found = args.find("--mod-root");
            const std::optional<std::string> root = found == args.end()
                ? std::nullopt : std::optional<std::string>(found->second);
            std::cout << render_status(unified_catalog(root), argv[3], version) << '\n';
        } else if (action == "run") {
            if (argc < 4) throw Error("MissingRuntimeModIdentity");
            if (builtin_mod_id(argv[3])) throw Error("BuiltinModImmutable");
            const auto args = options(argc, argv, 4, {"--template", "--input", "--mod-root"});
            const auto root = required(args, "--mod-root");
            const auto template_id = required(args, "--template");
            const auto input = required(args, "--input");
            const auto execution = InstalledModHostV2::open_existing(root).run_logic(
                argv[3], template_id, input, {}, "worker.execute");
            const auto& identity = execution.identity;
            const auto& run = execution.run;
            if (!run.accepted) throw Error(run.status.empty() ? "LogicModRunFailed" : run.status);
            std::cout << "{\"contract\":\"installed-logic-mod-run-v1\",\"mod_id\":"
                << json_string(identity.mod_id) << ",\"version\":" << json_string(identity.version)
                << ",\"package_digest\":" << json_string(identity.package_digest)
                << ",\"worker_sha256\":" << json_string(identity.worker_sha256)
                << ",\"template_id\":" << json_string(identity.template_id)
                << ",\"accepted\":true,\"status\":" << json_string(run.status)
                << ",\"result\":" << json_string(run.result)
                << ",\"claim_scope\":" << json_string(run.claim_scope)
                << ",\"validator\":" << json_string(run.validator)
                << ",\"binding_identity\":" << json_string(run.binding_identity)
                << ",\"request_digest\":" << json_string(run.request_digest)
                << ",\"response_digest\":" << json_string(run.response_digest) << "}\n";
        } else if (action == "activate" || action == "disable") {
            const int start = action == "activate" ? 5 : action == "disable" ? 4 : 3;
            if (argc < start) throw Error("MissingRuntimeModIdentity");
            const auto args = options(argc, argv, start, {"--mod-root"});
            if (builtin_mod_id(argv[3])) throw Error("BuiltinModImmutable");
            const auto root = required(args, "--mod-root");
            auto host = InstalledModHostV2::open_existing(root);
            if (action == "activate") host.activate(argv[3], argv[4]);
            if (action == "disable") host.disable(argv[3]);
            std::cout << render_unified_mod_catalog_v2(unified_catalog(root, false)) << '\n';
        } else throw Error("UnknownRuntimeModAction");
    } else {
        const bool is_replay = argc > 2 && std::string_view(argv[2]) == "replay";
        if (is_replay && argc < 4) throw Error("MissingRouteReport");
        const auto args = options(argc, argv, is_replay ? 4 : 2, is_replay
            ? std::set<std::string>{"--mod-root"} : std::set<std::string>{"--text", "--mod-root", "--output"},
            {"--mod-details"});
        const auto root = required(args, "--mod-root");
        if (!is_replay) (void)required(args, "--text");
        if (args.contains("--output")) (void)required(args, "--output");
        const bool details = args.contains("--mod-details");
        std::string report;
        if (details) {
            const auto host = InstalledModHostV2::open_existing(root);
            if (is_replay) report = replay_v2(host, LocalFiles::read(argv[3], report_byte_limit));
            else {
                const auto text = required(args, "--text");
                report = render_route_v2(host.route(text), text);
            }
        } else {
            const auto host = InstalledModHostV2::open_existing(root);
            if (is_replay) report = replay(host, LocalFiles::read(argv[3], report_byte_limit));
            else {
                const auto text = required(args, "--text");
                report = render_route(host.route_data(text), text);
            }
        }
        if (args.contains("--output")) LocalFiles::publish_new_atomically(required(args, "--output"), report + '\n');
        std::cout << report << '\n';
    }
    return 0;
}
}
