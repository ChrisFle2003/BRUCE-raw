#pragma once

#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"
#include "bruce/dag_scheduler/dag_scheduler_v2.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/io/local_files.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bruce {
namespace dag_scheduler_v3_cli_detail {

struct ParsedDagSchedulerSpecV3 {
    DagSchedulerSpecV2 spec;
    std::string raw_bytes;
};

inline constexpr std::size_t byte_limit = 65536;

inline std::size_t checked_size(std::size_t total, std::size_t added) {
    if (total > byte_limit || added > byte_limit - total)
        throw Error("DagSchedulerV3PayloadLimitExceeded");
    return total + added;
}

inline DagSchedulerCallbackResultV2 ready(std::string code, std::string payload) {
    auto digest = sha256(payload);
    return {DagSchedulerCallbackStatusV2::ready, std::move(code),
            std::move(payload), std::move(digest)};
}

inline DagSchedulerCallbackResultV2 literal(
        std::string_view request, std::span<const DagSchedulerInputV2> inputs) {
    if (!inputs.empty()) throw Error("InvalidDagSchedulerV3SpecCallbackArity");
    (void)checked_size(0, request.size());
    return ready("LITERAL_READY", std::string(request));
}

inline DagSchedulerCallbackResultV2 append(
        std::string_view request, std::span<const DagSchedulerInputV2> inputs) {
    if (inputs.size() != 1) throw Error("InvalidDagSchedulerV3SpecCallbackArity");
    const auto size = checked_size(inputs[0].payload_bytes.size(), request.size());
    std::string payload;
    payload.reserve(size);
    payload.append(inputs[0].payload_bytes);
    payload.append(request);
    return ready("APPENDED_READY", std::move(payload));
}

inline DagSchedulerCallbackResultV2 join(
        std::string_view request, std::span<const DagSchedulerInputV2> inputs) {
    if (inputs.size() < 2 || inputs.size() > 15)
        throw Error("InvalidDagSchedulerV3SpecCallbackArity");
    std::size_t size = 0;
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        if (i) size = checked_size(size, request.size());
        size = checked_size(size, inputs[i].payload_bytes.size());
    }
    // All payload and separator copies are accounted for before allocation.
    std::string payload;
    payload.reserve(size);
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        if (i) payload.append(request);
        payload.append(inputs[i].payload_bytes);
    }
    return ready("JOINED_READY", std::move(payload));
}

inline constexpr std::array<DagSchedulerCallbackEntryV2, 3> registry{{
    {"append-v1", append}, {"join-v1", join}, {"literal-v1", literal}}};

inline void validate_callbacks(const DagSchedulerSpecV2& spec,
                              const std::map<std::string, std::size_t>* extra_inputs = nullptr) {
    for (const auto& node : spec.nodes) {
        const auto extra = extra_inputs ? extra_inputs->find(node.node_id) :
            std::map<std::string, std::size_t>::const_iterator{};
        const auto count = node.dependencies.size() +
            (extra_inputs && extra != extra_inputs->end() ? extra->second : 0);
        if (node.callback_id == "literal-v1") {
            if (count == 0) continue;
        } else if (node.callback_id == "append-v1") {
            if (count == 1) continue;
        } else if (node.callback_id == "join-v1") {
            if (count >= 2 && count <= 15) continue;
        } else {
            throw Error("InvalidDagSchedulerV3SpecCallback");
        }
        throw Error("InvalidDagSchedulerV3SpecCallbackArity");
    }
}

template<std::size_t N>
inline std::array<std::string_view, N> fields(
        const std::vector<std::string_view>& lines,
        const std::array<std::string_view, N>& keys) {
    std::array<std::string_view, N> values{};
    std::array<bool, N> seen{};
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto equal = lines[i].find('=');
        if (equal == std::string_view::npos)
            throw Error("InvalidDagSchedulerV3SpecLine");
        const auto key = lines[i].substr(0, equal);
        const auto found = std::find(keys.begin(), keys.end(), key);
        if (found == keys.end()) throw Error("UnknownDagSchedulerV3SpecKey");
        const auto index = static_cast<std::size_t>(found - keys.begin());
        if (seen[index]) throw Error("DuplicateDagSchedulerV3SpecKey");
        seen[index] = true;
        if (index != i) throw Error("InvalidDagSchedulerV3SpecOrder");
        values[index] = lines[i].substr(equal + 1);
    }
    if (lines.size() != N) throw Error("InvalidDagSchedulerV3SpecLine");
    return values;
}

inline std::uint64_t decimal(std::string_view text) {
    if (text.empty() || !std::all_of(text.begin(), text.end(), [](unsigned char c) {
            return c >= '0' && c <= '9';
        })) throw Error("InvalidDagSchedulerV3SpecDecimal");
    std::uint64_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value == 0)
        throw Error("InvalidDagSchedulerV3SpecDecimal");
    return value;
}

inline std::vector<std::string> dependencies(std::string_view text) {
    std::vector<std::string> result;
    if (text.empty()) return result;
    while (true) {
        const auto comma = text.find(',');
        const auto id = text.substr(0, comma);
        if (id.empty() || (!result.empty() && id <= result.back()))
            throw Error("InvalidDagSchedulerV3SpecOrder");
        result.emplace_back(id);
        if (comma == std::string_view::npos) return result;
        text.remove_prefix(comma + 1);
    }
}

inline std::string decode_hex(std::string_view text) {
    if (text.size() % 2 || !std::all_of(text.begin(), text.end(), [](unsigned char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        })) throw Error("InvalidDagSchedulerV3SpecHex");
    const auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    std::string bytes;
    bytes.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2)
        bytes.push_back(static_cast<char>(16 * nibble(text[i]) + nibble(text[i + 1])));
    return bytes;
}

inline ParsedDagSchedulerSpecV3 parse_spec(
        const std::filesystem::path& path,
        const std::map<std::string, std::size_t>* extra_inputs = nullptr) {
    ParsedDagSchedulerSpecV3 parsed;
    parsed.raw_bytes = LocalFiles::read(path.string(), byte_limit);
    if (std::any_of(parsed.raw_bytes.begin(), parsed.raw_bytes.end(), [](unsigned char c) {
            return c == 0 || c == '\r' || c > 127;
        })) throw Error("InvalidDagSchedulerV3SpecText");
    std::string_view text = parsed.raw_bytes;
    if (text.ends_with('\n')) text.remove_suffix(1);
    const auto first_lf = text.find('\n');
    if (text.substr(0, first_lf) != "BRUCE_DAG_SCHEDULER_SPEC_V3")
        throw Error("InvalidDagSchedulerV3SpecHeader");
    if (first_lf == std::string_view::npos)
        throw Error("InvalidDagSchedulerV3SpecLine");
    text.remove_prefix(first_lf + 1);
    std::vector<std::vector<std::string_view>> blocks;
    std::vector<std::string_view> block;
    while (true) {
        const auto lf = text.find('\n');
        const auto line = text.substr(0, lf);
        if (line.empty()) {
            if (block.empty()) throw Error("InvalidDagSchedulerV3SpecSeparator");
            blocks.push_back(std::move(block));
            block.clear();
        } else {
            block.push_back(line);
        }
        if (lf == std::string_view::npos) break;
        text.remove_prefix(lf + 1);
    }
    if (block.empty()) throw Error("InvalidDagSchedulerV3SpecSeparator");
    blocks.push_back(std::move(block));
    if (blocks.size() < 2) throw Error("InvalidDagSchedulerV3SpecLine");

    constexpr std::array<std::string_view, 3> global_keys{{
        "dag-id", "max-started-nodes", "max-total-receipt-bytes"}};
    const auto globals = fields(blocks[0], global_keys);
    parsed.spec.dag_id = globals[0];
    parsed.spec.max_started_nodes = decimal(globals[1]);
    parsed.spec.max_total_receipt_bytes = decimal(globals[2]);
    constexpr std::array<std::string_view, 4> node_keys{{
        "node", "depends", "callback", "request-hex"}};
    for (std::size_t i = 1; i < blocks.size(); ++i) {
        const auto values = fields(blocks[i], node_keys);
        if (!parsed.spec.nodes.empty() && values[0] <= parsed.spec.nodes.back().node_id)
            throw Error("InvalidDagSchedulerV3SpecOrder");
        auto request = decode_hex(values[3]);
        auto digest = sha256(request);
        parsed.spec.nodes.push_back({std::string(values[0]), std::string(values[2]),
            dependencies(values[1]), std::move(request), std::move(digest)});
    }
    validate_callbacks(parsed.spec, extra_inputs);
    validate_dag_scheduler_spec_v2(parsed.spec, registry);
    return parsed;
}

inline std::vector<std::string> planned_start_order(const DagSchedulerSpecV2& spec) {
    validate_dag_scheduler_spec_v2(spec, registry);
    std::map<std::string, std::size_t> remaining;
    std::map<std::string, std::vector<std::string>> successors;
    std::set<std::string> ready_nodes;
    for (const auto& node : spec.nodes) {
        remaining.emplace(node.node_id, node.dependencies.size());
        if (node.dependencies.empty()) ready_nodes.insert(node.node_id);
        for (const auto& dependency : node.dependencies)
            successors[dependency].push_back(node.node_id);
    }
    std::vector<std::string> order;
    order.reserve(spec.nodes.size());
    while (!ready_nodes.empty()) {
        const auto id = *ready_nodes.begin();
        ready_nodes.erase(ready_nodes.begin());
        order.push_back(id);
        for (const auto& successor : successors[id]) {
            if (--remaining.at(successor) == 0) ready_nodes.insert(successor);
        }
    }
    return order;
}

inline std::string render_inspect(const ParsedDagSchedulerSpecV3& parsed) {
    const auto order = planned_start_order(parsed.spec);
    const auto& spec = parsed.spec;
    std::string report = "{\n  \"contract\": " + json_string("dag-scheduler-v3-inspect-v1") +
        ",\n  \"action\": " + json_string("inspect") +
        ",\n  \"spec_version\": " + json_string("dag-scheduler-spec-v3") +
        ",\n  \"spec_digest\": " + json_string(sha256(parsed.raw_bytes)) +
        ",\n  \"scheduler_version\": " + json_string(spec.scheduler_version) +
        ",\n  \"dag_id\": " + json_string(spec.dag_id) +
        ",\n  \"max_started_nodes\": " + std::to_string(spec.max_started_nodes) +
        ",\n  \"max_total_receipt_bytes\": " + std::to_string(spec.max_total_receipt_bytes) +
        ",\n  \"nodes\": [";
    for (std::size_t i = 0; i < spec.nodes.size(); ++i) {
        const auto& node = spec.nodes[i];
        report += (i ? ",\n    " : "\n    ") + std::string("{\"node_id\": ") +
            json_string(node.node_id) + ", \"callback_id\": " +
            json_string(node.callback_id) + ", \"dependencies\": [";
        for (std::size_t j = 0; j < node.dependencies.size(); ++j) {
            if (j) report += ", ";
            report += json_string(node.dependencies[j]);
        }
        report += "], \"request_size\": " + std::to_string(node.request_bytes.size()) +
            ", \"request_digest\": " + json_string(node.request_digest) + '}';
    }
    report += "\n  ],\n  \"planned_start_order\": [";
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (i) report += ", ";
        report += json_string(order[i]);
    }
    return report + "]\n}";
}

inline DagSchedulerRunV2 execute(const ParsedDagSchedulerSpecV3& parsed) {
    validate_callbacks(parsed.spec);
    validate_dag_scheduler_spec_v2(parsed.spec, registry);
    return execute_dag_scheduler_v2(parsed.spec, registry);
}

inline std::string_view outcome(DagSchedulerReceiptOutcomeV2 value) {
    switch (value) {
        case DagSchedulerReceiptOutcomeV2::ready: return "READY";
        case DagSchedulerReceiptOutcomeV2::failed: return "FAILED";
        case DagSchedulerReceiptOutcomeV2::blocked: return "BLOCKED";
        case DagSchedulerReceiptOutcomeV2::cancelled: return "CANCELLED";
    }
    throw Error("InvalidDagSchedulerV3Outcome");
}
inline std::string_view event(DagSchedulerTraceEventV2 value) {
    switch (value) {
        case DagSchedulerTraceEventV2::validated: return "VALIDATED";
        case DagSchedulerTraceEventV2::started: return "STARTED";
        case DagSchedulerTraceEventV2::ready: return "READY";
        case DagSchedulerTraceEventV2::failed: return "FAILED";
        case DagSchedulerTraceEventV2::blocked: return "BLOCKED";
        case DagSchedulerTraceEventV2::cancelled: return "CANCELLED";
        case DagSchedulerTraceEventV2::completed: return "COMPLETED";
    }
    throw Error("InvalidDagSchedulerV3TraceEvent");
}

inline std::string render(const DagSchedulerRunV2& run, std::string_view spec_digest) {
    std::string report = "{\n  \"contract\": " + json_string("dag-scheduler-v3-cli-v1") +
        ",\n  \"action\": " + json_string("execute") +
        ",\n  \"spec_version\": " + json_string("dag-scheduler-spec-v3") +
        ",\n  \"spec_digest\": " + json_string(spec_digest) +
        ",\n  \"scheduler_version\": " + json_string(run.spec.scheduler_version) +
        ",\n  \"dag_id\": " + json_string(run.spec.dag_id) +
        ",\n  \"max_started_nodes\": " + std::to_string(run.spec.max_started_nodes) +
        ",\n  \"max_total_receipt_bytes\": " + std::to_string(run.spec.max_total_receipt_bytes) +
        ",\n  \"receipts\": [";
    for (std::size_t i = 0; i < run.receipts.size(); ++i) {
        const auto& receipt = run.receipts[i];
        const auto node = std::find_if(run.spec.nodes.begin(), run.spec.nodes.end(),
            [&receipt](const auto& spec_node) { return spec_node.node_id == receipt.node_id; });
        if (node == run.spec.nodes.end()) throw Error("MissingDagSchedulerV3Node");
        report += (i ? ",\n    " : "\n    ") + std::string("{\"node_id\": ") +
            json_string(receipt.node_id) + ", \"callback_id\": " +
            json_string(receipt.callback_id) + ", \"request_size\": " +
            std::to_string(node->request_bytes.size()) + ", \"request_digest\": " +
            json_string(receipt.request_digest) + ", \"input_refs\": [";
        for (std::size_t j = 0; j < receipt.input_refs.size(); ++j) {
            const auto& input = receipt.input_refs[j];
            if (j) report += ", ";
            report += "{\"producer_node_id\": " + json_string(input.producer_node_id) +
                ", \"result_code\": " + json_string(input.result_code) +
                ", \"payload_size\": " + std::to_string(input.payload_bytes.size()) +
                ", \"payload_digest\": " + json_string(input.payload_digest) + '}';
        }
        report += "], \"outcome\": " + json_string(outcome(receipt.outcome)) +
            ", \"failure_code\": " + json_string(receipt.failure_code) + ", \"output\": ";
        if (receipt.output) {
            const auto& output = *receipt.output;
            report += "{\"output_version\": " + json_string(output.output_version) +
                ", \"result_code\": " + json_string(output.result_code) +
                ", \"payload_size\": " + std::to_string(output.payload_bytes.size()) +
                ", \"payload_digest\": " + json_string(output.payload_digest) + '}';
        } else {
            report += "null";
        }
        report += '}';
    }
    report += "\n  ],\n  \"trace\": [";
    for (std::size_t i = 0; i < run.trace.size(); ++i) {
        const auto& entry = run.trace[i];
        report += (i ? ",\n    " : "\n    ") + std::string("{\"event\": ") +
            json_string(event(entry.event)) + ", \"node_id\": " + json_string(entry.node_id) + '}';
    }
    return report + "\n  ]\n}";
}

} // namespace dag_scheduler_v3_cli_detail

inline int dag_scheduler_v3_cli(int argc, char** argv) {
    if (argc != 4 || (std::string_view(argv[2]) != "execute" &&
                      std::string_view(argv[2]) != "inspect") ||
        std::string_view(argv[3]).empty() || std::string_view(argv[3]).starts_with('-')) {
        std::cerr << "{\"error\":\"Usage: bruce dag-scheduler-v3 execute|inspect SPEC\"}\n";
        return 2;
    }
    const auto parsed = dag_scheduler_v3_cli_detail::parse_spec(argv[3]);
    if (std::string_view(argv[2]) == "inspect") {
        std::cout << dag_scheduler_v3_cli_detail::render_inspect(parsed) << '\n';
        return 0;
    }
    const auto run = dag_scheduler_v3_cli_detail::execute(parsed);
    std::cout << dag_scheduler_v3_cli_detail::render(run, sha256(parsed.raw_bytes)) << '\n';
    return 0;
}

} // namespace bruce
