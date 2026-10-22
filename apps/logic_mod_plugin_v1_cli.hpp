#pragma once
#include "bruce/core/json.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/mod/logic_mod_worker_v1.hpp"
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <set>
#include <unistd.h>

namespace bruce {
namespace logic_mod_cli_detail {
inline std::string state_name(LogicModStateV1 state) {
    switch (state) {
    case LogicModStateV1::discovered: return "DISCOVERED";
    case LogicModStateV1::verified: return "VERIFIED";
    case LogicModStateV1::golden_tested: return "GOLDEN_TESTED";
    case LogicModStateV1::installed_inactive: return "INSTALLED_INACTIVE";
    case LogicModStateV1::active: return "ACTIVE";
    case LogicModStateV1::disabled: return "DISABLED";
    case LogicModStateV1::revoked: return "REVOKED";
    }
    throw Error("InvalidLogicState");
}
struct PackageDirectory {
    std::string path;
    PackageDirectory() { char pattern[] = "/tmp/bruce-logic-package-XXXXXX"; if (!::mkdtemp(pattern)) throw Error("LogicPackageExtractionFailed"); path = pattern; }
    ~PackageDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    void extract(const LogicModPackageV1& package, const RuntimeModTrustStoreV1& trust) const {
        const auto bytes = package.worker_bytes(trust);
        const int fd = ::open((path + '/' + package.worker_path()).c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0500);
        if (fd < 0) throw Error("LogicPackageExtractionFailed");
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto n = ::write(fd, bytes.data()+offset, bytes.size()-offset);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) { ::close(fd); throw Error("LogicPackageExtractionFailed"); }
            offset += n;
        }
        if (::close(fd) != 0) throw Error("LogicPackageExtractionFailed");
    }
};
}

// Explicit, bounded, ephemeral session; no persistent installation or implicit
// activation. Only the listed actions and the single host validator are exposed.
inline int logic_mod_plugin_v1_cli(int argc, char** argv) {
    using namespace logic_mod_cli_detail;
    if (argc < 4 || std::string_view(argv[2]) != "session") throw Error("ExpectedLogicModSession");
    const std::set<std::string> allowed{"--trust-store", "--actions", "--input", "--record", "--replay-record", "--input-artifact", "--output-manifest", "--store"};
    std::map<std::string, std::string> options;
    for (int i = 4; i < argc; i += 2) {
        if (!allowed.contains(argv[i]) || i+1 >= argc || !options.emplace(argv[i], argv[i+1]).second) throw Error("InvalidLogicModOption");
    }
    const auto required = [&](const std::string& key) -> const std::string& {
        const auto it = options.find(key); if (it == options.end() || it->second.empty()) throw Error("MissingLogicModOption: " + key); return it->second;
    };
    if ((options.contains("--input-artifact") || options.contains("--output-manifest")) != options.contains("--store")) throw Error("LogicModInputOptionsMustPair");
    std::optional<LogicModInputV1> payload_input;
    std::optional<LogicModOutputV1> output;
    if (options.contains("--output-manifest")) output = LogicModOutputV1{required("--store"), required("--output-manifest")};
    if (options.contains("--input-artifact")) {
        const auto& reference = required("--input-artifact");
        const auto colon = reference.find(':');
        if (colon == 0 || colon == std::string::npos || colon + 1 == reference.size()) throw Error("InvalidLogicModInputReference");
        const std::string_view index(reference.data() + colon + 1, reference.size() - colon - 1);
        std::uint64_t value = 0;
        const auto parsed = std::from_chars(index.data(), index.data() + index.size(), value);
        if ((index.size() > 1 && index.front() == '0') || parsed.ec != std::errc{} || parsed.ptr != index.data() + index.size()) throw Error("InvalidLogicModInputReference");
        payload_input = LogicModInputV1{required("--store"), reference.substr(0, colon), value};
    }
    const auto action_text = required("--actions");
    std::vector<std::string> actions;
    std::size_t offset = 0;
    const std::set<std::string> allowed_actions{"install", "activate", "run", "inspect", "replay", "disable", "revoke"};
    for (;;) {
        const auto end = action_text.find(',', offset);
        const auto action = action_text.substr(offset, end == std::string::npos ? end : end-offset);
        if (!allowed_actions.contains(action) || actions.size() >= 16) throw Error("InvalidLogicModAction");
        actions.push_back(action);
        if (end == std::string::npos) break;
        offset = end+1;
    }
    if (actions.front() != "install") throw Error("LogicSessionMustInstallFirst");
    const auto count = [&](std::string_view action) { return std::count(actions.begin(), actions.end(), action); };
    if (count("install") != 1 || count("run") > 1 || count("replay") > 1 ||
        (options.contains("--record") && count("run") != 1) ||
        (options.contains("--replay-record") && (count("replay") != 1 || count("run") != 0))) throw Error("InvalidLogicSession");
    const std::string input = (count("run") || count("replay")) ? required("--input") : "";
    const auto trust = read_runtime_mod_truststore_v1(LocalFiles::read(required("--trust-store"), 65536));
    const auto package = unpack_logic_mod_package_v1(LocalFiles::read(argv[3], 1024*1024));
    verify_logic_mod_package_v1(package, trust);
    PackageDirectory directory; directory.extract(package, trust);
    LogicModRegistryV1 registry; registry.discover(package, directory.path);
    const auto id = package.package_digest(); registry.verify(id, trust); registry.test_golden(id, core_tree_registry_v1());
    std::optional<LogicModRunV1> run;
    bool replayed = false;
    for (const auto& action : actions) {
        if (action == "install") registry.install(id);
        else if (action == "activate") registry.activate(id);
        else if (action == "disable") registry.disable(id);
        else if (action == "revoke") registry.revoke(id);
        else if (action == "inspect") (void)registry.state(id);
        else if (action == "run") {
            run = run_logic_mod_worker_v1(registry, package.manifest().mod_id, package.manifest().version, "math.rational.power.v1", "worker.execute", input, {}, payload_input, output);
            if (options.contains("--record")) LocalFiles::publish_new(required("--record"), logic_mod_run_record_v1(*run));
        } else if (action == "replay") {
            const auto record = options.contains("--replay-record")
                ? read_logic_mod_run_record_v1(LocalFiles::read(required("--replay-record"), 2*65536))
                : run.value_or(LogicModRunV1{});
            verify_logic_mod_replay_v1(registry, package.manifest().mod_id, package.manifest().version, "math.rational.power.v1", input, {}, record, payload_input, output);
            replayed = true;
        }
    }
    std::cout << "{\"contract\":\"logic-mod-plugin-v1-session\",\"state\":" << json_string(state_name(registry.state(id)))
        << ",\"package_digest\":" << json_string(id) << ",\"worker_sha256\":" << json_string(package.worker_sha256())
        << ",\"registry_digest\":" << json_string(registry.digest()) << ",\"replay_verified\":" << (replayed ? "true" : "false") << ",\"runs\":[";
    if (run) {
        std::cout << "{\"accepted\":" << (run->accepted ? "true" : "false") << ",\"status\":" << json_string(run->status)
        << ",\"result\":" << json_string(run->result) << ",\"claim_scope\":" << json_string(run->claim_scope)
        << ",\"validator\":" << json_string(run->validator) << ",\"request_digest\":" << json_string(run->request_digest)
        << ",\"response_digest\":" << json_string(run->response_digest);
        if (run->output_present) std::cout << ",\"output_manifest_id\":" << json_string(run->output_manifest_id)
            << ",\"output_object_index\":" << run->output_object_index << ",\"output_sha256\":" << json_string(run->output_sha256)
            << ",\"output_size_bytes\":" << run->output_size_bytes << ",\"output_codec\":" << json_string(run->output_codec);
        std::cout << '}';
    }
    std::cout << "]}\n";
    return run && !run->accepted ? 2 : 0;
}
}
