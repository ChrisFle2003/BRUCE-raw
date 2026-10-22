#pragma once

#include "logic_mod_artifact_dag_v5_cli.hpp"
#include "bruce/mod/logic_mod_artifact_dag_v6.hpp"

namespace bruce {
namespace logic_mod_dag_v6_cli_detail {
using logic_mod_dag_cli_detail::parse_spec;
using logic_mod_dag_cli_detail::publish_record;
using logic_mod_dag_cli_detail::spec_limit;

struct Arguments : logic_mod_dag_cli_detail::Arguments { LogicModArtifactDagControlV6 control; };

inline Arguments parse(std::span<const std::string_view> args) {
    const auto bad = [] { throw Error("InvalidLogicModDagArguments"); };
    // Six fixed V2 words, mandatory --report, then V5's six run-only pairs.
    if (args.size() < 6 || args.size() > 20 || args.size() % 2) bad();
    const auto common = logic_mod_dag_cli_detail::parse(args.first(6));
    if (common.replay) {
        if (args.size() != 6) bad(); // Reject every V6/live option before opening SPEC.
        return {common, {}};
    }
    if (args.size() < 8) bad();
    Arguments out{common, {}};
    std::set<std::string_view> seen;
    for (std::size_t i = 6; i < args.size(); i += 2) {
        const auto flag = args[i], value = args[i + 1];
        if (!seen.insert(flag).second || value.empty()) bad();
        if (flag == "--report") {
            if (value.starts_with("--") || value.find_first_not_of(" \t\r\n\v\f") == std::string_view::npos) bad();
            out.control.report_root = std::filesystem::path(value);
            continue;
        }
        if (flag == "--remote-restore") {
            if (value.starts_with("--")) bad();
            out.control.remote_restore = std::string(value);
            continue;
        }
        if ((value.size() > 1 && value.front() == '0') || value.find_first_not_of("0123456789") != std::string_view::npos) bad();
        std::uint64_t number{};
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) bad();
        if (flag == "--max-parallel-nodes") {
            if (number < 1 || number > 4) bad();
            out.control.max_parallel_nodes = static_cast<std::uint32_t>(number);
        } else if (flag == "--max-total-wall-ms") {
            if (number < 1 || number > 60000) bad();
            out.control.max_total_wall_ms = number;
        } else if (flag == "--max-total-payload-bytes") {
            if (number > 8388608) bad();
            out.control.max_total_payload_bytes = number;
        } else if (flag == "--max-started-workers") {
            if (number < 1 || number > 16) bad();
            out.control.max_started_workers = static_cast<std::uint32_t>(number);
        } else if (flag == "--cancel-after-ms") {
            if (number < 1 || number > 60000) bad();
            out.control.cancel_after_ms = number;
        } else bad();
    }
    if (out.control.report_root.empty()) bad();
    return out;
}

inline std::string output_json(const LogicModArtifactDagRunV6& run, bool replay) {
    std::string out = "{\"contract\":\"logic-mod-artifact-dag-v6\",\"action\":" + json_string(replay ? "replay" : "run") +
        ",\"accepted\":" + (run.accepted ? "true" : "false") + ",\"status\":" + json_string(run.status) +
        ",\"dag_id\":" + (run.dag_id.empty() ? "null" : json_string(run.dag_id)) + ",\"spec_digest\":" + json_string(run.spec_digest) + ",\"node_order\":[";
    bool first = true;
    for (const auto& id : run.node_order) { if (!first) out += ','; first = false; out += json_string(id); }
    out += "],\"nodes\":["; first = true;
    for (const auto& node : run.nodes) {
        if (!first) out += ',';
        first = false;
        out += "{\"id\":" + json_string(node.id) + ",\"result_artifact\":" + logic_mod_chain_cli_detail::output_json(node.result_artifact) +
            ",\"internal_input_artifact\":" + (node.internal_input_artifact ? logic_mod_chain_cli_detail::output_json(*node.internal_input_artifact) : "null") + '}';
    }
    return out + "]}\n";
}
}

inline int logic_mod_artifact_dag_v6_cli(int argc, char** argv) {
    using namespace logic_mod_dag_v6_cli_detail;
    std::vector<std::string_view> args; for (int i = 2; i < argc; ++i) args.emplace_back(argv[i]);
    const auto options = parse(args);
    try {
        const auto spec = parse_spec(logic_mod_chain_cli_detail::read_file(options.spec, spec_limit));
        struct Session { logic_mod_cli_detail::PackageDirectory directory; LogicModRegistryV1 registry; };
        std::vector<std::unique_ptr<Session>> sessions;
        std::vector<LogicModDagNodeV2> nodes;
        for (const auto& node : spec.nodes) {
            const auto trust = read_runtime_mod_truststore_v1(logic_mod_chain_cli_detail::read_file(node.trust, 65536));
            const auto package = unpack_logic_mod_package_v1(logic_mod_chain_cli_detail::read_file(node.package, 1024*1024));
            verify_logic_mod_package_v1(package, trust);
            auto session = std::make_unique<Session>();
            logic_mod_chain_cli_detail::activate(session->registry, package, trust, session->directory);
            nodes.push_back({node.id, session->registry, package.manifest().mod_id, package.manifest().version, "math.rational.power.v1", node.input, node.predecessors,
                node.internal ? std::optional<LogicModOutputV1>{{options.store, *node.internal}} : std::nullopt, {options.store, node.output}, node.budgets});
            sessions.push_back(std::move(session));
        }
        LogicModArtifactDagRunV6 run;
        if (options.replay) {
            run = read_logic_mod_artifact_dag_record_v6(logic_mod_chain_cli_detail::read_file(options.record, logic_mod_artifact_dag_record_limit_v6, true));
            verify_logic_mod_artifact_dag_replay_v6(nodes, options.store, spec.digest, run);
        } else {
            run = run_logic_mod_artifact_dag_v6(nodes, options.store, spec.digest, options.control);
            if (run.accepted) publish_record(options.record, logic_mod_artifact_dag_record_v6(run));
        }
        std::cout << output_json(run, options.replay); return run.accepted ? 0 : 2;
    } catch (...) { throw Error(options.replay ? "DAG_REPLAY_MISMATCH" : "LogicModDagFailed"); }
}
}
