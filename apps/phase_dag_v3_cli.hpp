#pragma once
#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/phase_dag/payload_gate_adapter_v3.hpp"
#include "bruce/phase_dag/nanocell_adapter_v3.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {
namespace phase_dag_v3_cli_detail {

using SpecFields = std::map<std::string, std::string>;

inline std::uint64_t unsigned_number(std::string_view text) {
    std::uint64_t value{};
    if (text.empty() || !std::all_of(text.begin(), text.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) throw Error("InvalidPhaseDagSpecNumber");
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size())
        throw Error("InvalidPhaseDagSpecNumber");
    return value;
}

inline std::uint64_t parallel_limit(std::string_view text) {
    std::uint64_t value{};
    if (text.empty()) throw Error("InvalidPhaseDagParallelLimit");
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value < 1 || value > 4)
        throw Error("InvalidPhaseDagParallelLimit");
    return value;
}

inline void require_fields(const SpecFields& fields, std::initializer_list<std::string_view> keys) {
    for (const auto key : keys)
        if (!fields.contains(std::string(key))) throw Error("MissingPhaseDagSpecKey: " + std::string(key));
    if (fields.size() != keys.size()) throw Error("UnknownPhaseDagSpecKey");
}

inline PhaseNodeSpecV3 parse_node(const SpecFields& fields, const std::filesystem::path& parent) {
    if (const auto adapter = fields.find("adapter"); adapter != fields.end()) {
        if (adapter->second != "payload-gate-v1") throw Error("UnknownPhaseDagSpecAdapter");
        require_fields(fields, {"node", "depends", "adapter"});
        PhaseNodeSpecV3 node;
        node.node_id = fields.at("node");
        node.adapter_id = adapter->second;
        node.adapter_version = "v1";
        const auto& depends = fields.at("depends");
        if (!depends.empty()) {
            std::size_t first = 0;
            for (;;) {
                const auto comma = depends.find(',', first);
                const auto dependency = depends.substr(first, comma == std::string::npos ? comma : comma - first);
                if (dependency.empty()) throw Error("InvalidPhaseDagSpecDependency");
                node.dependencies.push_back(dependency);
                if (comma == std::string::npos) break;
                first = comma + 1;
            }
        }
        node.request_bytes = pack_payload_gate_request_v1();
        node.request_digest = sha256(node.request_bytes);
        return node;
    }
    require_fields(fields, {"node", "depends", "core", "active-index", "context-id", "context-file",
        "technique", "max-operations", "max-trace-entries"});
    PhaseNodeSpecV3 node;
    node.node_id = fields.at("node");
    node.adapter_id = "nanocell-phase-v3";
    node.adapter_version = "v1";
    const auto& depends = fields.at("depends");
    if (!depends.empty()) {
        std::size_t first = 0;
        for (;;) {
            const auto comma = depends.find(',', first);
            const auto dependency = depends.substr(first, comma == std::string::npos ? comma : comma - first);
            if (dependency.empty()) throw Error("InvalidPhaseDagSpecDependency");
            node.dependencies.push_back(dependency);
            if (comma == std::string::npos) break;
            first = comma + 1;
        }
    }
    NanoCellPhaseRequestV1 request;
    request.core_layout_version = "nanoneuron-core-v1";
    const auto& core = fields.at("core");
    if (core.size() != 16) throw Error("InvalidPhaseDagSpecCore");
    const auto hex = [](char ch) -> unsigned {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        throw Error("InvalidPhaseDagSpecCore");
    };
    for (std::size_t i = 0; i < request.core_bytes.size(); ++i)
        request.core_bytes[i] = static_cast<std::uint8_t>(hex(core[2 * i]) * 16 + hex(core[2 * i + 1]));
    const auto active = unsigned_number(fields.at("active-index"));
    if (active > 255) throw Error("InvalidPhaseDagSpecActiveIndex");
    request.active_index = static_cast<std::uint8_t>(active);
    request.context_id = fields.at("context-id");
    const auto& context_file = fields.at("context-file");
    if (context_file.empty() || context_file.find('\0') != std::string::npos)
        throw Error("InvalidPhaseDagSpecContextFile");
    request.context_bytes = LocalFiles::read((parent / context_file).string(), 4096);
    request.context_digest = sha256(request.context_bytes);
    request.technique_id = fields.at("technique");
    request.max_operations = unsigned_number(fields.at("max-operations"));
    request.max_trace_entries = unsigned_number(fields.at("max-trace-entries"));
    node.request_bytes = pack_nanocell_phase_request_v1(request);
    // Validate request semantics before execution can turn input errors into receipts.
    (void)unpack_nanocell_phase_request_v1(node.request_bytes);
    node.request_digest = sha256(node.request_bytes);
    return node;
}

inline PhaseDagSpecV3 parse_spec(const std::filesystem::path& path) {
    std::istringstream input(LocalFiles::read(path.string(), 65536));
    std::string line;
    if (!std::getline(input, line) || line != "BRUCE_PHASE_DAG_SPEC_V3")
        throw Error("InvalidPhaseDagSpecHeader");
    std::vector<SpecFields> blocks;
    SpecFields fields;
    while (std::getline(input, line)) {
        if (line.empty()) {
            if (!fields.empty()) { blocks.push_back(std::move(fields)); fields = {}; }
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos || equals == 0 || line.find('=', equals + 1) != std::string::npos)
            throw Error("InvalidPhaseDagSpecAssignment");
        if (!fields.emplace(line.substr(0, equals), line.substr(equals + 1)).second)
            throw Error("DuplicatePhaseDagSpecKey: " + line.substr(0, equals));
    }
    if (!fields.empty()) blocks.push_back(std::move(fields));
    if (blocks.empty()) throw Error("MissingPhaseDagSpecGlobals");
    const auto& globals = blocks.front();
    require_fields(globals, {"dag-id", "max-started-nodes", "max-receipt-bytes", "max-trace-entries"});
    PhaseDagSpecV3 spec;
    spec.dag_id = globals.at("dag-id");
    spec.max_started_nodes = unsigned_number(globals.at("max-started-nodes"));
    spec.max_receipt_bytes = unsigned_number(globals.at("max-receipt-bytes"));
    spec.max_trace_entries = unsigned_number(globals.at("max-trace-entries"));
    for (std::size_t i = 1; i < blocks.size(); ++i)
        spec.nodes.push_back(parse_node(blocks[i], path.parent_path()));
    std::sort(spec.nodes.begin(), spec.nodes.end(), [](const auto& left, const auto& right) {
        return left.node_id < right.node_id;
    });
    validate_phase_dag_spec_v3(spec, phase_dag_nanocell_catalog_v3());
    return spec;
}

inline std::string_view outcome_name(PhaseReceiptOutcomeV3 outcome) {
    switch (outcome) {
    case PhaseReceiptOutcomeV3::ready: return "READY";
    case PhaseReceiptOutcomeV3::blocked: return "BLOCKED";
    case PhaseReceiptOutcomeV3::failed: return "FAILED";
    case PhaseReceiptOutcomeV3::cancelled: return "CANCELLED";
    }
    throw Error("InvalidPhaseDagReceiptOutcome");
}

inline std::string render_report(const PhaseDagRunV3& run, std::string_view action,
    std::string_view bundle_digest) {
    std::string report = "{\"contract\":\"phase-dag-v3-cli-v1\",\"action\":" + json_string(action) +
        ",\"dag_version\":" + json_string(run.spec.dag_version) + ",\"dag_id\":" + json_string(run.spec.dag_id) +
        ",\"node_count\":" + std::to_string(run.spec.nodes.size()) +
        ",\"bundle_digest\":" + json_string(bundle_digest) + ",\"receipts\":[";
    auto receipts = run.receipts;
    std::sort(receipts.begin(), receipts.end(), [](const auto& left, const auto& right) {
        return left.node_id < right.node_id;
    });
    for (std::size_t i = 0; i < receipts.size(); ++i) {
        if (i) report += ',';
        const auto& receipt = receipts[i];
        report += "{\"node_id\":" + json_string(receipt.node_id) +
            ",\"outcome\":" + json_string(outcome_name(receipt.outcome)) +
            ",\"failure_code\":" + json_string(receipt.failure_code) + "}";
    }
    return report + "]}";
}

} // namespace phase_dag_v3_cli_detail

inline int phase_dag_v3_cli(int argc, char** argv) {
    if (argc < 3)
        throw Error("Usage: bruce phase-dag-v3 execute|inspect|replay");
    const std::string_view action = argv[2];
    if (action == "inspect" || action == "replay") {
        if (argc != 4 || std::string_view(argv[3]).starts_with('-'))
            throw Error("Usage: bruce phase-dag-v3 " + std::string(action) + " BUNDLE");
        const auto bundle = LocalFiles::read(argv[3], 5 * 1024 * 1024);
        const auto run = replay_phase_dag_run_v3(bundle, phase_dag_nanocell_catalog_v3());
        std::cout << phase_dag_v3_cli_detail::render_report(run, action, sha256(bundle)) << '\n';
        return 0;
    }
    if (action != "execute")
        throw Error("Usage: bruce phase-dag-v3 execute|inspect|replay");
    if ((argc != 6 && argc != 8) || std::string_view(argv[4]) != "--output" ||
        (argc == 8 && std::string_view(argv[6]) != "--max-parallel-nodes"))
        throw Error("Usage: bruce phase-dag-v3 execute SPEC --output BUNDLE [--max-parallel-nodes N]");
    PhaseDagControlV3 control;
    if (argc == 8) control.max_parallel_nodes = phase_dag_v3_cli_detail::parallel_limit(argv[7]);
    const auto spec = phase_dag_v3_cli_detail::parse_spec(argv[3]);
    const auto run = execute_phase_dag_v3(spec, phase_dag_nanocell_catalog_v3(), control);
    const auto bundle = pack_phase_dag_run_v3(run);
    const auto report = phase_dag_v3_cli_detail::render_report(run, "execute", sha256(bundle));
    LocalFiles::publish_new_atomically(argv[5], bundle);
    std::cout << report << '\n';
    return 0;
}

} // namespace bruce
