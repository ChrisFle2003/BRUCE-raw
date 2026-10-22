#pragma once

#include "logic_mod_artifact_dag_v2_cli.hpp"
#include "bruce/mod/logic_mod_artifact_dag_v4.hpp"

namespace bruce {
namespace logic_mod_dag_v4_cli_detail {
// V4 retains the bounded V2 Spec grammar and safe file/publication contract.
using logic_mod_dag_cli_detail::parse_spec;
using logic_mod_dag_cli_detail::publish_record;
using logic_mod_dag_cli_detail::spec_limit;
struct Arguments : logic_mod_dag_cli_detail::Arguments {
    LogicModArtifactDagControlV4 control;
};
inline Arguments parse(std::span<const std::string_view> args) {
    const auto bad = [] { throw Error("InvalidLogicModDagArguments"); };
    if (args.size() < 6 || args.size() > 16 || args.size() % 2) bad();
    const auto common = logic_mod_dag_cli_detail::parse(args.first(6));
    Arguments out{common, {}};
    if (common.replay && args.size() != 6) bad();
    std::set<std::string_view> seen;
    for (std::size_t i = 6; i < args.size(); i += 2) {
        const auto flag = args[i], spelling = args[i + 1];
        if (!seen.insert(flag).second || spelling.empty() ||
            (spelling.size() > 1 && spelling.front() == '0') ||
            spelling.find_first_not_of("0123456789") != std::string_view::npos) bad();
        std::uint64_t value{};
        const auto parsed = std::from_chars(spelling.data(), spelling.data() + spelling.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != spelling.data() + spelling.size()) bad();
        if (flag == "--max-parallel-nodes") {
            if (value < 1 || value > 4) bad();
            out.control.max_parallel_nodes = static_cast<std::uint32_t>(value);
        } else if (flag == "--max-total-wall-ms") {
            if (value < 1 || value > 60000) bad();
            out.control.max_total_wall_ms = value;
        } else if (flag == "--max-total-payload-bytes") {
            if (value > 8388608) bad();
            out.control.max_total_payload_bytes = value;
        } else if (flag == "--max-started-workers") {
            if (value < 1 || value > 16) bad();
            out.control.max_started_workers = static_cast<std::uint32_t>(value);
        } else if (flag == "--cancel-after-ms") {
            if (value < 1 || value > 60000) bad();
            out.control.cancel_after_ms = value;
        } else bad();
    }
    return out;
}
inline std::string output_json(const LogicModArtifactDagRunV4& run, bool replay) {
    std::string out = "{\"contract\":\"logic-mod-artifact-dag-v4\",\"action\":" + json_string(replay ? "replay" : "run") +
        ",\"accepted\":" + (run.accepted ? "true" : "false") + ",\"status\":" + json_string(run.status) +
        ",\"dag_id\":" + (run.dag_id.empty() ? "null" : json_string(run.dag_id)) + ",\"spec_digest\":" + json_string(run.spec_digest) + ",\"node_order\":[";
    bool first = true;
    for (const auto& id : run.node_order) { if (!first) out += ','; first = false; out += json_string(id); }
    out += "],\"nodes\":["; first = true;
    for (const auto& n : run.nodes) {
        if (!first) out += ',';
        first = false;
        out += "{\"id\":" + json_string(n.id) + ",\"result_artifact\":" + logic_mod_chain_cli_detail::output_json(n.result_artifact) +
            ",\"internal_input_artifact\":" + (n.internal_input_artifact ? logic_mod_chain_cli_detail::output_json(*n.internal_input_artifact) : "null") + '}';
    }
    return out + "]}\n";
}
}
inline int logic_mod_artifact_dag_v4_cli(int argc, char** argv) {
    using namespace logic_mod_dag_v4_cli_detail;
    std::vector<std::string_view> args; for (int i = 2; i < argc; ++i) args.emplace_back(argv[i]);
    const auto options = parse(args);
    try {
        const auto spec = parse_spec(logic_mod_chain_cli_detail::read_file(options.spec, spec_limit));
        struct Session { logic_mod_cli_detail::PackageDirectory directory; LogicModRegistryV1 registry; };
        std::vector<std::unique_ptr<Session>> sessions;
        std::vector<LogicModDagNodeV2> nodes;
        for (const auto& n : spec.nodes) {
            const auto trust = read_runtime_mod_truststore_v1(logic_mod_chain_cli_detail::read_file(n.trust, 65536));
            const auto package = unpack_logic_mod_package_v1(logic_mod_chain_cli_detail::read_file(n.package, 1024*1024));
            verify_logic_mod_package_v1(package, trust);
            auto session = std::make_unique<Session>();
            logic_mod_chain_cli_detail::activate(session->registry, package, trust, session->directory);
            nodes.push_back({n.id, session->registry, package.manifest().mod_id, package.manifest().version, "math.rational.power.v1", n.input, n.predecessors,
                n.internal ? std::optional<LogicModOutputV1>{{options.store, *n.internal}} : std::nullopt, {options.store, n.output}, n.budgets});
            sessions.push_back(std::move(session));
        }
        LogicModArtifactDagRunV4 run;
        if (options.replay) {
            run = read_logic_mod_artifact_dag_record_v4(logic_mod_chain_cli_detail::read_file(options.record, logic_mod_artifact_dag_record_limit_v4, true));
            verify_logic_mod_artifact_dag_replay_v4(nodes, options.store, spec.digest, run);
        } else {
            run = run_logic_mod_artifact_dag_v4(nodes, options.store, spec.digest, options.control);
            if (run.accepted) publish_record(options.record, logic_mod_artifact_dag_record_v4(run));
        }
        std::cout << logic_mod_dag_v4_cli_detail::output_json(run, options.replay); return run.accepted ? 0 : 2;
    } catch (...) { throw Error(options.replay ? "DAG_REPLAY_MISMATCH" : "LogicModDagFailed"); }
}
}
