#pragma once
#include "bruce/core/json.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/mod/runtime_registry.hpp"
#include "bruce/tree/normalizer.hpp"
#include <iostream>
#include <map>
#include <set>

namespace bruce {
namespace runtime_mod_cli_detail {
inline constexpr std::size_t report_byte_limit = 1024 * 1024;
using Options = std::map<std::string, std::string>;
inline Options options(int argc, char** argv, int start, const std::set<std::string>& allowed) {
    Options result;
    for (int i = start; i < argc; i += 2) {
        const std::string key = argv[i];
        if (!allowed.contains(key)) throw Error("UnknownOption: " + key);
        if (i + 1 == argc) throw Error("MissingOptionValue: " + key);
        if (!result.emplace(key, argv[i + 1]).second) throw Error("DuplicateOption: " + key);
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
inline std::string render_route(const RuntimeModActiveViewV1& view, std::string_view text) {
    if (text.size() > 4096) throw Error("RouteInputTooLarge");
    const auto route = view.tree.route(normalize_route_text_v1(text));
    std::string result = "{\"contract\":\"route-report-v1\",\"text\":" + json_string(text) +
        ",\"registry_digest\":" + json_string(view.tree.digest()) + ",\"status\":" +
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
    for (const auto& record : view.snapshot.records) if (record.state == RuntimeModStateV1::active) {
        if (!first) result += ',';
        first = false;
        result += identity(record) + '}';
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
inline std::string replay(const RuntimeModActiveViewV1& view, std::string bytes) {
    if (bytes.empty() || bytes.back() != '\n') throw Error("InvalidRouteReport");
    bytes.pop_back();
    ReportPrefix prefix(bytes);
    prefix.literal("{\"contract\":\"route-report-v1\",\"text\":");
    const auto text = prefix.string(4096);
    prefix.literal(",\"registry_digest\":");
    const auto digest = prefix.string(64);
    if (digest.size() != 64) throw Error("InvalidRouteReport");
    if (digest != view.tree.digest()) throw Error("RouteRegistryDigestMismatch");
    const auto report = render_route(view, text);
    if (bytes != report) throw Error("RouteReportMismatch");
    return report;
}
}

inline int runtime_mod_cli(int argc, char** argv) {
    using namespace runtime_mod_cli_detail;
    const std::string command = argv[1];
    if (command == "mods") {
        if (argc < 3) throw Error("MissingRuntimeModAction");
        const std::string action = argv[2];
        if (action == "verify" || action == "install") {
            if (argc < 4) throw Error("MissingRuntimeModPackage");
            const auto args = options(argc, argv, 4, action == "verify"
                ? std::set<std::string>{"--trust-store"} : std::set<std::string>{"--trust-store", "--mod-root"});
            const auto trust_path = required(args, "--trust-store");
            if (action == "install") (void)required(args, "--mod-root");
            const auto trust = read_runtime_mod_truststore_v1(LocalFiles::read(trust_path, 65536));
            const auto package = unpack_runtime_data_mod_package_v1(LocalFiles::read(argv[3], max_runtime_mod_package_bytes_v1));
            verify_runtime_data_mod_package_v1(package, trust);
            if (action == "verify") {
                std::cout << "{\"contract\":\"runtime-mod-verification-v1\",\"verified\":true,\"mod_id\":"
                    << json_string(package.manifest().mod_id) << ",\"version\":" << json_string(package.manifest().version)
                    << ",\"package_digest\":" << json_string(package.package_digest()) << "}\n";
            } else {
                auto registry = RuntimeModRegistryV1::open(required(args, "--mod-root"), trust);
                registry.install(package);
                std::cout << render_snapshot(registry.snapshot()) << '\n';
            }
        } else if (action == "list" || action == "activate" || action == "disable") {
            const int start = action == "activate" ? 5 : action == "disable" ? 4 : 3;
            if (argc < start) throw Error("MissingRuntimeModIdentity");
            const auto args = options(argc, argv, start, {"--mod-root"});
            auto registry = RuntimeModRegistryV1::open_existing(required(args, "--mod-root"));
            if (action == "activate") registry.activate(argv[3], argv[4]);
            if (action == "disable") registry.disable(argv[3]);
            std::cout << render_snapshot(registry.snapshot()) << '\n';
        } else throw Error("UnknownRuntimeModAction");
    } else {
        const bool is_replay = argc > 2 && std::string_view(argv[2]) == "replay";
        if (is_replay && argc < 4) throw Error("MissingRouteReport");
        const auto args = options(argc, argv, is_replay ? 4 : 2, is_replay
            ? std::set<std::string>{"--mod-root"} : std::set<std::string>{"--text", "--mod-root", "--output"});
        const auto root = required(args, "--mod-root");
        if (!is_replay) (void)required(args, "--text");
        if (args.contains("--output")) (void)required(args, "--output");
        const auto view = RuntimeModRegistryV1::open_existing(root).active_view();
        const auto report = is_replay ? replay(view, LocalFiles::read(argv[3], report_byte_limit))
            : render_route(view, required(args, "--text"));
        if (args.contains("--output")) LocalFiles::publish_new_atomically(required(args, "--output"), report + '\n');
        std::cout << report << '\n';
    }
    return 0;
}
}
