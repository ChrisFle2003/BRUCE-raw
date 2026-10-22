#include "bruce/dag_scheduler/dag_scheduler_v4.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <utility>

namespace bruce {
namespace {
constexpr std::string_view header = "BRUCE_DAG_SCHEDULER_RUN_V4\n";
constexpr std::size_t envelope_bytes = header.size() + 65;
constexpr std::size_t payload_limit = dag_scheduler_v4_max_bundle_bytes - envelope_bytes;
constexpr std::size_t field_limit = 65536;
constexpr std::array<std::string_view, 3> callback_ids{"append-v1", "join-v1", "literal-v1"};
[[noreturn]] void invalid() { throw Error("InvalidDagSchedulerV4Bundle"); }
void require(bool condition) { if (!condition) invalid(); }
bool identifier(std::string_view value) {
    return !value.empty() && value.size() <= 64 &&
        std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_' || c == '-';
        });
}
void validate_registry(DagSchedulerCallbackRegistryV2 registry) {
    require(registry.size() == callback_ids.size());
    for (std::size_t i = 0; i < callback_ids.size(); ++i)
        require(registry[i].callback_id == callback_ids[i] && registry[i].execute);
}
// Used only for V2's structural spec validation when packing. Never executed.
DagSchedulerCallbackResultV2 unused(std::string_view, std::span<const DagSchedulerInputV2>) {
    invalid();
}
constexpr std::array<DagSchedulerCallbackEntryV2, 3> structural_registry{{
    {"append-v1", unused}, {"join-v1", unused}, {"literal-v1", unused}}};
void validate_run(const DagSchedulerRunV2& run, DagSchedulerCallbackRegistryV2 registry) {
    validate_registry(registry);
    validate_dag_scheduler_spec_v2(run.spec, registry);
    const auto& nodes = run.spec.nodes;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        require(i == 0 || nodes[i - 1].node_id < node.node_id);
        for (std::size_t j = 1; j < node.dependencies.size(); ++j)
            require(node.dependencies[j - 1] < node.dependencies[j]);
        const auto n = node.dependencies.size();
        require((node.callback_id == "literal-v1" && n == 0) ||
                (node.callback_id == "append-v1" && n == 1) ||
                (node.callback_id == "join-v1" && n >= 2 && n <= 15));
    }
    require(run.receipts.size() == nodes.size());
    std::uint64_t retained = 0;
    auto payload = [&](const auto& value) {
        require(identifier(value.result_code));
        require(value.payload_bytes.size() <= field_limit);
        require(value.payload_digest == sha256(value.payload_bytes));
        require(value.payload_bytes.size() <= run.spec.max_total_receipt_bytes - retained);
        retained += value.payload_bytes.size();
    };
    for (std::size_t i = 0; i < run.receipts.size(); ++i) {
        const auto& receipt = run.receipts[i];
        const auto& node = nodes[i];
        require(receipt.node_id == node.node_id && receipt.callback_id == node.callback_id &&
                receipt.request_digest == node.request_digest);
        require(receipt.input_refs.size() <= 15);
        const auto outcome = static_cast<unsigned>(receipt.outcome);
        require(outcome <= static_cast<unsigned>(DagSchedulerReceiptOutcomeV2::cancelled));
        if (receipt.outcome == DagSchedulerReceiptOutcomeV2::ready) {
            require(receipt.output.has_value() && receipt.failure_code.empty());
            require(receipt.output->output_version == "dag-scheduler-output-v2");
            payload(*receipt.output);
            require(receipt.input_refs.size() == node.dependencies.size());
            for (std::size_t j = 0; j < receipt.input_refs.size(); ++j) {
                const auto& input = receipt.input_refs[j];
                require(input.producer_node_id == node.dependencies[j]);
                payload(input);
            }
        } else {
            require(!receipt.output && receipt.input_refs.empty() && identifier(receipt.failure_code));
        }
    }
    require(run.trace.size() >= 2 && run.trace.size() <= 2 * nodes.size() + 2);
    require(run.trace.front().event == DagSchedulerTraceEventV2::validated && run.trace.front().node_id.empty());
    require(run.trace.back().event == DagSchedulerTraceEventV2::completed && run.trace.back().node_id.empty());
    for (std::size_t i = 1; i + 1 < run.trace.size(); ++i) {
        const auto& entry = run.trace[i];
        const auto event = static_cast<unsigned>(entry.event);
        require(event >= static_cast<unsigned>(DagSchedulerTraceEventV2::started) &&
                event <= static_cast<unsigned>(DagSchedulerTraceEventV2::cancelled));
        require(std::any_of(nodes.begin(), nodes.end(), [&](const auto& node) {
            return node.node_id == entry.node_id;
        }));
    }
}

// Integers are unsigned minimal decimal followed by LF. Strings are the same
// decimal byte length followed by ':' and exactly that many uninterpreted bytes.
class Writer {
    std::string bytes_;
public:
    void append(std::string_view value) {
        require(value.size() <= payload_limit - bytes_.size());
        bytes_.append(value);
    }
    void number(std::uint64_t value) { append(std::to_string(value)); append("\n"); }
    void string(std::string_view value) {
        append(std::to_string(value.size())); append(":"); append(value);
    }
    std::string take() { return std::move(bytes_); }
};
std::string encode(const DagSchedulerRunV2& run) {
    Writer out;
    out.string(run.spec.scheduler_version); out.string(run.spec.dag_id);
    out.number(run.spec.max_started_nodes); out.number(run.spec.max_total_receipt_bytes);
    out.number(run.spec.nodes.size());
    for (const auto& node : run.spec.nodes) {
        out.string(node.node_id); out.string(node.callback_id); out.number(node.dependencies.size());
        for (const auto& dependency : node.dependencies) out.string(dependency);
        out.string(node.request_bytes); out.string(node.request_digest);
    }
    out.number(run.receipts.size());
    for (const auto& receipt : run.receipts) {
        out.string(receipt.node_id); out.string(receipt.callback_id); out.string(receipt.request_digest);
        out.number(receipt.input_refs.size());
        for (const auto& input : receipt.input_refs) {
            out.string(input.producer_node_id); out.string(input.result_code);
            out.string(input.payload_bytes); out.string(input.payload_digest);
        }
        out.number(static_cast<unsigned>(receipt.outcome)); out.number(receipt.output ? 1 : 0);
        if (receipt.output) {
            out.string(receipt.output->output_version); out.string(receipt.output->result_code);
            out.string(receipt.output->payload_bytes); out.string(receipt.output->payload_digest);
        }
        out.string(receipt.failure_code);
    }
    out.number(run.trace.size());
    for (const auto& entry : run.trace) {
        out.number(static_cast<unsigned>(entry.event)); out.string(entry.node_id);
    }
    return out.take();
}
class Reader {
    std::string_view bytes_;
    std::uint64_t decimal(char separator, std::uint64_t limit) {
        const auto end = bytes_.find(separator);
        require(end != std::string_view::npos && end != 0 && end <= 20);
        const auto digits = bytes_.substr(0, end);
        require(digits.size() == 1 || digits.front() != '0');
        require(std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; }));
        std::uint64_t value{};
        const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
        require(result.ec == std::errc{} && result.ptr == digits.data() + digits.size() && value <= limit);
        bytes_.remove_prefix(end + 1);
        return value;
    }
public:
    explicit Reader(std::string_view bytes) : bytes_(bytes) {}
    std::uint64_t number(std::uint64_t limit) { return decimal('\n', limit); }
    std::string string(std::size_t limit = 64) {
        const auto size = decimal(':', limit);
        require(size <= bytes_.size());
        std::string result(bytes_.substr(0, static_cast<std::size_t>(size)));
        bytes_.remove_prefix(static_cast<std::size_t>(size));
        return result;
    }
    bool empty() const { return bytes_.empty(); }
};
DagSchedulerRunV2 decode(std::string_view payload) {
    Reader in(payload);
    DagSchedulerRunV2 run;
    run.spec.scheduler_version = in.string(); run.spec.dag_id = in.string();
    run.spec.max_started_nodes = in.number(64); run.spec.max_total_receipt_bytes = in.number(4194304);
    const auto node_count = in.number(64);
    run.spec.nodes.reserve(node_count);
    for (std::size_t i = 0; i < node_count; ++i) {
        DagSchedulerNodeSpecV2 node;
        node.node_id = in.string(); node.callback_id = in.string();
        const auto dependencies = in.number(15);
        node.dependencies.reserve(dependencies);
        for (std::size_t j = 0; j < dependencies; ++j) node.dependencies.push_back(in.string());
        node.request_bytes = in.string(field_limit); node.request_digest = in.string();
        run.spec.nodes.push_back(std::move(node));
    }
    const auto receipt_count = in.number(node_count);
    require(receipt_count == node_count);
    run.receipts.reserve(receipt_count);
    for (std::size_t i = 0; i < receipt_count; ++i) {
        DagSchedulerReceiptV2 receipt;
        receipt.node_id = in.string(); receipt.callback_id = in.string(); receipt.request_digest = in.string();
        const auto inputs = in.number(15);
        receipt.input_refs.reserve(inputs);
        for (std::size_t j = 0; j < inputs; ++j) {
            DagSchedulerInputV2 input;
            input.producer_node_id = in.string(); input.result_code = in.string();
            input.payload_bytes = in.string(field_limit); input.payload_digest = in.string();
            receipt.input_refs.push_back(std::move(input));
        }
        receipt.outcome = static_cast<DagSchedulerReceiptOutcomeV2>(in.number(3));
        if (in.number(1)) {
            DagSchedulerOutputRefV2 output;
            output.output_version = in.string(); output.result_code = in.string();
            output.payload_bytes = in.string(field_limit); output.payload_digest = in.string();
            receipt.output = std::move(output);
        }
        receipt.failure_code = in.string();
        run.receipts.push_back(std::move(receipt));
    }
    const auto trace_count = in.number(2 * node_count + 2);
    run.trace.reserve(trace_count);
    for (std::size_t i = 0; i < trace_count; ++i) {
        const auto event = static_cast<DagSchedulerTraceEventV2>(in.number(6));
        run.trace.push_back({event, in.string()});
    }
    require(in.empty());
    return run;
}
}

std::string pack_dag_scheduler_run_v4(const DagSchedulerRunV2& run) {
    validate_run(run, structural_registry);
    const auto payload = encode(run);
    return std::string(header) + sha256(payload) + '\n' + payload;
}
DagSchedulerRunV2 inspect_dag_scheduler_run_v4(
        std::string_view bytes, DagSchedulerCallbackRegistryV2 registry) {
    require(bytes.size() <= dag_scheduler_v4_max_bundle_bytes && bytes.size() >= envelope_bytes);
    require(bytes.starts_with(header) && bytes[envelope_bytes - 1] == '\n');
    const auto payload = bytes.substr(envelope_bytes);
    require(bytes.substr(header.size(), 64) == sha256(payload));
    auto run = decode(payload);
    validate_run(run, registry);
    require(encode(run) == payload);
    return run;
}
DagSchedulerRunV2 replay_dag_scheduler_run_v4(
        std::string_view bytes, DagSchedulerCallbackRegistryV2 registry) {
    const auto decoded = inspect_dag_scheduler_run_v4(bytes, registry);
    auto replayed = execute_dag_scheduler_v2(decoded.spec, registry);
    // The injective canonical encoding covers every spec, receipt, input, output
    // and trace member: byte equality therefore demands complete run equality.
    if (pack_dag_scheduler_run_v4(replayed) != bytes)
        throw Error("DagSchedulerV4ReplayMismatch");
    return replayed;
}
}
