#include "bruce/phase_dag/phase_dag_v3.hpp"

#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <algorithm>
#include <cstddef>
#include <future>
#include <limits>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bruce {
namespace {

constexpr std::size_t max_nodes = 16;
constexpr std::size_t max_dependencies_per_node = 15;
constexpr std::size_t max_edges = 64;
constexpr std::size_t max_request_bytes = 65536;
constexpr std::size_t max_payload_bytes = 65536;

bool identifier(std::string_view value) {
    return value.size() >= 1 && value.size() <= 64 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
    });
}

bool sorted_unique(const std::vector<std::string>& values) {
    return std::adjacent_find(values.begin(), values.end(), [](const auto& left, const auto& right) {
        return left >= right;
    }) == values.end();
}

auto find_node(const std::vector<PhaseNodeSpecV3>& nodes, std::string_view id) {
    return std::lower_bound(nodes.begin(), nodes.end(), id,
        [](const PhaseNodeSpecV3& candidate, std::string_view value) { return candidate.node_id < value; });
}

void validate_catalog(std::span<const PhaseAdapterV3> catalog) {
    std::pair<std::string_view, std::string_view> previous;
    bool has_previous = false;
    for (const auto& adapter : catalog) {
        const auto key = std::pair{adapter.adapter_id, adapter.adapter_version};
        if (!identifier(adapter.adapter_id) || !identifier(adapter.adapter_version) || !adapter.execute ||
            (has_previous && !(previous < key)))
            throw Error("InvalidPhaseDagCatalog");
        previous = key;
        has_previous = true;
    }
}

void validate_graph(const PhaseDagSpecV3& spec) {
    std::vector<std::size_t> indegree(spec.nodes.size());
    std::vector<std::vector<std::size_t>> successors(spec.nodes.size());
    std::size_t edges = 0;
    for (std::size_t index = 0; index < spec.nodes.size(); ++index) {
        const auto& node = spec.nodes[index];
        if (!identifier(node.node_id) || !identifier(node.adapter_id) || !identifier(node.adapter_version) ||
            node.request_bytes.size() > max_request_bytes || node.request_digest != sha256(node.request_bytes))
            throw Error("InvalidPhaseDagNode");
        if (node.dependencies.size() > max_dependencies_per_node || !sorted_unique(node.dependencies))
            throw Error("NoncanonicalPhaseDagDependencies");
        edges += node.dependencies.size();
        for (const auto& dependency : node.dependencies) {
            const auto predecessor = find_node(spec.nodes, dependency);
            if (!identifier(dependency) || dependency == node.node_id || predecessor == spec.nodes.end() ||
                predecessor->node_id != dependency)
                throw Error("InvalidPhaseDagDependency");
            const auto predecessor_index = static_cast<std::size_t>(predecessor - spec.nodes.begin());
            successors[predecessor_index].push_back(index);
            ++indegree[index];
        }
    }
    if (edges > max_edges) throw Error("PhaseDagEdgeLimit");

    std::queue<std::size_t> ready;
    for (std::size_t index = 0; index < indegree.size(); ++index)
        if (indegree[index] == 0) ready.push(index);
    std::size_t visited = 0;
    while (!ready.empty()) {
        const auto index = ready.front();
        ready.pop();
        ++visited;
        for (const auto successor : successors[index])
            if (--indegree[successor] == 0) ready.push(successor);
    }
    if (visited != spec.nodes.size()) throw Error("PhaseDagCycle");
}

std::string_view output_status_name(PhaseOutputStatusV3 status) {
    switch (status) {
    case PhaseOutputStatusV3::complete: return "COMPLETE";
    case PhaseOutputStatusV3::incomplete: return "INCOMPLETE";
    case PhaseOutputStatusV3::rejected: return "REJECTED";
    }
    throw Error("InvalidPhaseOutputStatus");
}

std::string_view receipt_outcome_name(PhaseReceiptOutcomeV3 outcome) {
    switch (outcome) {
    case PhaseReceiptOutcomeV3::ready: return "READY";
    case PhaseReceiptOutcomeV3::blocked: return "BLOCKED";
    case PhaseReceiptOutcomeV3::failed: return "FAILED";
    case PhaseReceiptOutcomeV3::cancelled: return "CANCELLED";
    }
    throw Error("InvalidPhaseReceiptOutcome");
}

std::string_view trace_event_name(PhaseTraceEventV3 event) {
    switch (event) {
    case PhaseTraceEventV3::validated: return "VALIDATED";
    case PhaseTraceEventV3::started: return "STARTED";
    case PhaseTraceEventV3::ready: return "READY";
    case PhaseTraceEventV3::blocked: return "BLOCKED";
    case PhaseTraceEventV3::failed: return "FAILED";
    case PhaseTraceEventV3::cancelled: return "CANCELLED";
    case PhaseTraceEventV3::completed: return "COMPLETED";
    }
    throw Error("InvalidPhaseTraceEvent");
}

void append_field(std::string& bytes, std::string_view value) {
    bytes += std::to_string(value.size());
    bytes += ':';
    bytes += value;
}

void append_uint(std::string& bytes, std::uint64_t value) { append_field(bytes, std::to_string(value)); }

void append_output(std::string& bytes, const PhaseOutputRefV3& output) {
    append_field(bytes, output.output_version);
    append_field(bytes, output_status_name(output.status));
    append_field(bytes, output.output_bytes);
    append_field(bytes, output.output_digest);
    append_field(bytes, output.evidence_bytes);
    append_field(bytes, output.evidence_digest);
    append_field(bytes, output.payload_bytes);
    append_field(bytes, output.payload_digest);
    append_field(bytes, output.claim_scope);
}

void append_receipt(std::string& bytes, const PhaseReceiptV3& receipt) {
    append_field(bytes, receipt.node_id);
    append_field(bytes, receipt.adapter_id);
    append_field(bytes, receipt.adapter_version);
    append_field(bytes, receipt.request_digest);
    append_uint(bytes, receipt.input_refs.size());
    for (const auto& input : receipt.input_refs) {
        append_field(bytes, input.producer_node_id);
        append_field(bytes, input.producer_adapter_id);
        append_field(bytes, input.producer_adapter_version);
        append_field(bytes, input.producer_status);
        append_field(bytes, input.producer_output_digest);
        append_field(bytes, input.producer_evidence_digest);
        append_field(bytes, input.producer_payload_bytes);
        append_field(bytes, input.producer_payload_digest);
    }
    append_field(bytes, receipt_outcome_name(receipt.outcome));
    append_field(bytes, receipt.output ? "1" : "0");
    if (receipt.output) append_output(bytes, *receipt.output);
    append_field(bytes, receipt.failure_code);
}

bool digest_string(std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}


void validate_receipt_budget(const PhaseReceiptV3& receipt, std::uint64_t budget) {
    std::string bytes;
    append_receipt(bytes, receipt);
    if (bytes.size() <= budget) return;

    // The fixed metadata for stopping/blocking or reporting budget exhaustion
    // must remain publishable even when the configured budget is one byte.
    // Such fallback receipts never retain payloads or arbitrary error strings.
    if (receipt.input_refs.empty() && !receipt.output &&
        identifier(receipt.node_id) && identifier(receipt.adapter_id) && identifier(receipt.adapter_version) &&
        digest_string(receipt.request_digest) &&
        ((receipt.outcome == PhaseReceiptOutcomeV3::failed && receipt.failure_code == "DagReceiptBudgetExceeded") ||
         (receipt.outcome == PhaseReceiptOutcomeV3::blocked && receipt.failure_code.empty()) ||
         (receipt.outcome == PhaseReceiptOutcomeV3::cancelled &&
             (receipt.failure_code.empty() || receipt.failure_code == "DagBudgetExhausted"))))
        return;
    throw Error("DagReceiptBudgetExceeded");
}

void validate_output(const PhaseOutputRefV3& output) {
    if (output.output_version != "phase-output-v3" || output.output_bytes.size() > max_request_bytes ||
        output.evidence_bytes.size() > max_request_bytes || output.output_digest != sha256(output.output_bytes) ||
        output.evidence_digest != sha256(output.evidence_bytes) ||
        output.payload_bytes.size() > max_payload_bytes || output.payload_digest != sha256(output.payload_bytes) ||
        !identifier(output.claim_scope))
        throw Error("InvalidPhaseAdapterOutput");
    (void)output_status_name(output.status);
}


void validate_input(const PhaseInputV3& input) {
    if (!identifier(input.producer_node_id) || !identifier(input.producer_adapter_id) ||
        !identifier(input.producer_adapter_version) ||
        (input.producer_status != "COMPLETE" && input.producer_status != "INCOMPLETE" &&
            input.producer_status != "REJECTED") ||
        !digest_string(input.producer_output_digest) || !digest_string(input.producer_evidence_digest) ||
        input.producer_payload_bytes.size() > max_payload_bytes ||
        input.producer_payload_digest != sha256(input.producer_payload_bytes))
        throw Error("InvalidPhaseDagInput");
}

void add_trace(PhaseDagRunV3& run, PhaseTraceEventV3 event, const std::string& node_id) {
    if (event == PhaseTraceEventV3::completed) {
        if (run.trace.size() < run.spec.max_trace_entries) run.trace.push_back({event, node_id});
    } else if (run.trace.size() + 1 < run.spec.max_trace_entries) {
        run.trace.push_back({event, node_id});
    }
}

PhaseReceiptV3 receipt_base(const PhaseNodeSpecV3& node) {
    return {node.node_id, node.adapter_id, node.adapter_version, node.request_digest, {},
        PhaseReceiptOutcomeV3::failed, std::nullopt, {}};
}

PhaseInputV3 input_ref(const PhaseReceiptV3& receipt) {
    const auto& output = *receipt.output;
    return {receipt.node_id, receipt.adapter_id, receipt.adapter_version,
        std::string(output_status_name(output.status)), output.output_digest, output.evidence_digest,
        output.payload_bytes, output.payload_digest};
}

const PhaseAdapterV3& find_adapter(const PhaseNodeSpecV3& node, std::span<const PhaseAdapterV3> catalog) {
    const auto key = std::pair{std::string_view(node.adapter_id), std::string_view(node.adapter_version)};
    const auto result = std::lower_bound(catalog.begin(), catalog.end(), key,
        [](const PhaseAdapterV3& adapter, const auto& value) {
            return std::pair{adapter.adapter_id, adapter.adapter_version} < value;
        });
    if (result == catalog.end() || std::pair{result->adapter_id, result->adapter_version} != key)
        throw Error("UnknownPhaseDagAdapter");
    return *result;
}

constexpr std::string_view bundle_header = "BRUCE_PHASE_DAG_RUN_V3\n";
constexpr std::size_t max_bundle_bytes = 5 * 1024 * 1024;

std::string encode_run(const PhaseDagRunV3& run) {
    std::string bytes;
    append_field(bytes, run.spec.dag_version);
    append_field(bytes, run.spec.dag_id);
    append_uint(bytes, run.spec.nodes.size());
    for (const auto& node : run.spec.nodes) {
        append_field(bytes, node.node_id);
        append_field(bytes, node.adapter_id);
        append_field(bytes, node.adapter_version);
        append_uint(bytes, node.dependencies.size());
        for (const auto& dependency : node.dependencies) append_field(bytes, dependency);
        append_field(bytes, node.request_bytes);
        append_field(bytes, node.request_digest);
    }
    append_uint(bytes, run.spec.max_started_nodes);
    append_uint(bytes, run.spec.max_receipt_bytes);
    append_uint(bytes, run.spec.max_trace_entries);
    append_uint(bytes, run.receipts.size());
    for (const auto& receipt : run.receipts) append_receipt(bytes, receipt);
    append_uint(bytes, run.trace.size());
    for (const auto& entry : run.trace) {
        append_field(bytes, trace_event_name(entry.event));
        append_field(bytes, entry.node_id);
    }
    return bytes;
}

class FieldReader {
public:
    explicit FieldReader(std::string_view bytes) : bytes_(bytes) {}

    std::string field() {
        if (offset_ == bytes_.size()) throw Error("PhaseDagBundleTruncated");
        const auto start = offset_;
        std::size_t length = 0;
        while (offset_ < bytes_.size() && bytes_[offset_] != ':') {
            const char c = bytes_[offset_++];
            if (c < '0' || c > '9') throw Error("InvalidPhaseDagBundleLength");
            const auto digit = static_cast<std::size_t>(c - '0');
            if (length > (std::numeric_limits<std::size_t>::max() - digit) / 10)
                throw Error("InvalidPhaseDagBundleLength");
            length = length * 10 + digit;
        }
        if (offset_ == start || offset_ == bytes_.size() ||
            (offset_ - start > 1 && bytes_[start] == '0'))
            throw Error("InvalidPhaseDagBundleLength");
        ++offset_;
        if (length > bytes_.size() - offset_) throw Error("PhaseDagBundleTruncated");
        const auto value = std::string(bytes_.substr(offset_, length));
        offset_ += length;
        return value;
    }

    std::uint64_t uint_value() {
        const auto value = field();
        if (value.empty() || (value.size() > 1 && value.front() == '0'))
            throw Error("InvalidPhaseDagBundleInteger");
        std::uint64_t result = 0;
        for (const char c : value) {
            if (c < '0' || c > '9') throw Error("InvalidPhaseDagBundleInteger");
            const auto digit = static_cast<std::uint64_t>(c - '0');
            if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
                throw Error("InvalidPhaseDagBundleInteger");
            result = result * 10 + digit;
        }
        return result;
    }

    bool finished() const { return offset_ == bytes_.size(); }

private:
    std::string_view bytes_;
    std::size_t offset_{};
};

PhaseOutputStatusV3 parse_output_status(std::string_view value) {
    if (value == "COMPLETE") return PhaseOutputStatusV3::complete;
    if (value == "INCOMPLETE") return PhaseOutputStatusV3::incomplete;
    if (value == "REJECTED") return PhaseOutputStatusV3::rejected;
    throw Error("InvalidPhaseOutputStatus");
}

PhaseReceiptOutcomeV3 parse_receipt_outcome(std::string_view value) {
    if (value == "READY") return PhaseReceiptOutcomeV3::ready;
    if (value == "BLOCKED") return PhaseReceiptOutcomeV3::blocked;
    if (value == "FAILED") return PhaseReceiptOutcomeV3::failed;
    if (value == "CANCELLED") return PhaseReceiptOutcomeV3::cancelled;
    throw Error("InvalidPhaseReceiptOutcome");
}

PhaseTraceEventV3 parse_trace_event(std::string_view value) {
    if (value == "VALIDATED") return PhaseTraceEventV3::validated;
    if (value == "STARTED") return PhaseTraceEventV3::started;
    if (value == "READY") return PhaseTraceEventV3::ready;
    if (value == "BLOCKED") return PhaseTraceEventV3::blocked;
    if (value == "FAILED") return PhaseTraceEventV3::failed;
    if (value == "CANCELLED") return PhaseTraceEventV3::cancelled;
    if (value == "COMPLETED") return PhaseTraceEventV3::completed;
    throw Error("InvalidPhaseTraceEvent");
}

PhaseDagSpecV3 parse_spec(FieldReader& reader) {
    PhaseDagSpecV3 spec;
    spec.dag_version = reader.field();
    spec.dag_id = reader.field();
    const auto count = reader.uint_value();
    if (count > max_nodes) throw Error("InvalidPhaseDagBundleCount");
    for (std::uint64_t index = 0; index < count; ++index) {
        PhaseNodeSpecV3 node;
        node.node_id = reader.field();
        node.adapter_id = reader.field();
        node.adapter_version = reader.field();
        const auto dependencies = reader.uint_value();
        if (dependencies > max_dependencies_per_node) throw Error("InvalidPhaseDagBundleCount");
        for (std::uint64_t dependency = 0; dependency < dependencies; ++dependency)
            node.dependencies.push_back(reader.field());
        node.request_bytes = reader.field();
        node.request_digest = reader.field();
        spec.nodes.push_back(std::move(node));
    }
    spec.max_started_nodes = reader.uint_value();
    spec.max_receipt_bytes = reader.uint_value();
    spec.max_trace_entries = reader.uint_value();
    return spec;
}

PhaseOutputRefV3 parse_output(FieldReader& reader) {
    PhaseOutputRefV3 output;
    output.output_version = reader.field();
    output.status = parse_output_status(reader.field());
    output.output_bytes = reader.field();
    output.output_digest = reader.field();
    output.evidence_bytes = reader.field();
    output.evidence_digest = reader.field();
    output.payload_bytes = reader.field();
    output.payload_digest = reader.field();
    output.claim_scope = reader.field();
    validate_output(output);
    return output;
}

PhaseReceiptV3 parse_receipt(FieldReader& reader) {
    PhaseReceiptV3 receipt;
    receipt.node_id = reader.field();
    receipt.adapter_id = reader.field();
    receipt.adapter_version = reader.field();
    receipt.request_digest = reader.field();
    const auto count = reader.uint_value();
    if (count > max_dependencies_per_node) throw Error("InvalidPhaseDagBundleCount");
    for (std::uint64_t index = 0; index < count; ++index) {
        PhaseInputV3 input;
        input.producer_node_id = reader.field();
        input.producer_adapter_id = reader.field();
        input.producer_adapter_version = reader.field();
        input.producer_status = reader.field();
        input.producer_output_digest = reader.field();
        input.producer_evidence_digest = reader.field();
        input.producer_payload_bytes = reader.field();
        input.producer_payload_digest = reader.field();
        validate_input(input);
        receipt.input_refs.push_back(std::move(input));
    }
    receipt.outcome = parse_receipt_outcome(reader.field());
    const auto marker = reader.field();
    if (marker == "1") receipt.output = parse_output(reader);
    else if (marker != "0") throw Error("InvalidPhaseDagBundleOutputMarker");
    receipt.failure_code = reader.field();
    return receipt;
}

PhaseDagRunV3 decode_run(std::string_view bytes) {
    FieldReader reader(bytes);
    PhaseDagRunV3 run;
    run.spec = parse_spec(reader);
    const auto receipt_count = reader.uint_value();
    if (receipt_count > max_nodes) throw Error("InvalidPhaseDagBundleCount");
    for (std::uint64_t index = 0; index < receipt_count; ++index)
        run.receipts.push_back(parse_receipt(reader));
    const auto trace_count = reader.uint_value();
    if (trace_count > 512) throw Error("InvalidPhaseDagBundleCount");
    for (std::uint64_t index = 0; index < trace_count; ++index)
        run.trace.push_back({parse_trace_event(reader.field()), reader.field()});
    if (!reader.finished()) throw Error("PhaseDagBundleTrailingBytes");
    return run;
}

} // namespace

void validate_phase_dag_spec_v3(const PhaseDagSpecV3& spec, std::span<const PhaseAdapterV3> catalog) {
    if (spec.dag_version != "phase-dag-v3" || !identifier(spec.dag_id))
        throw Error("InvalidPhaseDagSpec");
    if (spec.nodes.empty() || spec.nodes.size() > max_nodes || spec.max_started_nodes > max_nodes ||
        spec.max_receipt_bytes == 0 || spec.max_receipt_bytes > 262144 ||
        spec.max_trace_entries < 2 || spec.max_trace_entries > 512)
        throw Error("InvalidPhaseDagLimit");
    if (std::adjacent_find(spec.nodes.begin(), spec.nodes.end(), [](const auto& left, const auto& right) {
            return left.node_id >= right.node_id;
        }) != spec.nodes.end())
        throw Error("NoncanonicalPhaseDagNodes");

    validate_catalog(catalog);
    validate_graph(spec);
    for (const auto& node : spec.nodes) {
        const auto key = std::pair{std::string_view(node.adapter_id), std::string_view(node.adapter_version)};
        const auto adapter = std::lower_bound(catalog.begin(), catalog.end(), key,
            [](const PhaseAdapterV3& candidate, const auto& value) {
                return std::pair{candidate.adapter_id, candidate.adapter_version} < value;
            });
        if (adapter == catalog.end() || std::pair{adapter->adapter_id, adapter->adapter_version} != key)
            throw Error("UnknownPhaseDagAdapter");
    }
}

PhaseDagRunV3 execute_phase_dag_v3(const PhaseDagSpecV3& spec, std::span<const PhaseAdapterV3> catalog,
    const PhaseDagControlV3& control) {
    if (control.max_parallel_nodes < 1 || control.max_parallel_nodes > 4)
        throw Error("InvalidPhaseDagParallelLimit");
    validate_phase_dag_spec_v3(spec, catalog);
    PhaseDagRunV3 run{spec, {}, {}};
    add_trace(run, PhaseTraceEventV3::validated, {});
    std::vector<std::optional<PhaseReceiptV3>> settled(spec.nodes.size());
    std::vector<std::optional<PhaseReceiptV3>> prefetched(spec.nodes.size());

    // Preserve the existing lexicographic serial Kahn order even when physical
    // waves finish independent nodes before a newly ready, earlier node.
    std::vector<std::size_t> logical_order;
    std::vector<bool> ordered(spec.nodes.size());
    while (logical_order.size() < spec.nodes.size()) {
        for (std::size_t index = 0; index < spec.nodes.size(); ++index) {
            if (ordered[index]) continue;
            const auto& dependencies = spec.nodes[index].dependencies;
            if (!std::all_of(dependencies.begin(), dependencies.end(), [&](const auto& dependency) {
                return ordered[static_cast<std::size_t>(find_node(spec.nodes, dependency) - spec.nodes.begin())];
            })) continue;
            ordered[index] = true;
            logical_order.push_back(index);
            break;
        }
    }

    auto invoke = [&](std::size_t index, PhaseReceiptV3 receipt) {
        const auto& node = spec.nodes[index];
        try {
            const auto& adapter = find_adapter(node, catalog);
            auto output = adapter.execute(node.request_bytes, receipt.input_refs);
            validate_output(output);
            receipt.outcome = PhaseReceiptOutcomeV3::ready;
            receipt.output = std::move(output);
        } catch (const Error& error) {
            receipt.outcome = PhaseReceiptOutcomeV3::failed;
            receipt.output.reset();
            receipt.failure_code = error.what();
        } catch (...) {
            receipt.outcome = PhaseReceiptOutcomeV3::failed;
            receipt.output.reset();
            receipt.failure_code = "PhaseAdapterException";
        }
        // Failed adapter calls carry the same input bytes and obey the same
        // receipt budget as successful calls.
        std::string encoded_receipt;
        append_receipt(encoded_receipt, receipt);
        if (encoded_receipt.size() > spec.max_receipt_bytes) {
            receipt.outcome = PhaseReceiptOutcomeV3::failed;
            receipt.output.reset();
            receipt.input_refs.clear();
            receipt.failure_code = "DagReceiptBudgetExceeded";
        }
        return receipt;
    };
    auto adopt = [&](std::size_t index, PhaseReceiptV3 receipt) {
        add_trace(run, PhaseTraceEventV3::started, spec.nodes[index].node_id);
        const auto event = receipt.outcome == PhaseReceiptOutcomeV3::ready ?
            PhaseTraceEventV3::ready : PhaseTraceEventV3::failed;
        settled[index] = receipt;
        run.receipts.push_back(std::move(receipt));
        prefetched[index].reset();
        add_trace(run, event, spec.nodes[index].node_id);
    };
    std::size_t started = 0;
    auto stop = [&](bool externally_cancelled) {
        if (!externally_cancelled && started < spec.max_started_nodes) return false;
        for (std::size_t index = 0; index < spec.nodes.size(); ++index) {
            if (settled[index]) continue;
            // A cancellation never discards an already completed physical wave.
            if (prefetched[index]) {
                adopt(index, std::move(*prefetched[index]));
                continue;
            }
            auto cancelled = receipt_base(spec.nodes[index]);
            cancelled.outcome = PhaseReceiptOutcomeV3::cancelled;
            if (!externally_cancelled) cancelled.failure_code = "DagBudgetExhausted";
            settled[index] = cancelled;
            run.receipts.push_back(std::move(cancelled));
            add_trace(run, PhaseTraceEventV3::cancelled, spec.nodes[index].node_id);
        }
        return true;
    };
    for (const auto selected : logical_order) {
        const bool has_prefetched = std::any_of(prefetched.begin(), prefetched.end(),
            [](const auto& receipt) { return receipt.has_value(); });
        // Preserve serial start-budget precedence over BLOCKED classification.
        // This check must not consume the external cancellation callback.
        if (!has_prefetched && started >= spec.max_started_nodes && stop(false)) break;
        const auto& node = spec.nodes[selected];
        auto receipt = receipt_base(node);
        bool dependencies_ready = true;
        for (const auto& dependency : node.dependencies) {
            const auto& parent = *settled[static_cast<std::size_t>(find_node(spec.nodes, dependency) - spec.nodes.begin())];
            if (parent.outcome != PhaseReceiptOutcomeV3::ready) {
                dependencies_ready = false;
                break;
            }
            receipt.input_refs.push_back(input_ref(parent));
        }
        if (!dependencies_ready) {
            receipt.input_refs.clear();
            receipt.outcome = PhaseReceiptOutcomeV3::blocked;
            settled[selected] = receipt;
            run.receipts.push_back(std::move(receipt));
            add_trace(run, PhaseTraceEventV3::blocked, node.node_id);
            continue;
        }
        if (!prefetched[selected]) {
            // External cancellation is checked only before an actual new
            // physical wave, never while classifying BLOCKED descendants or
            // adopting cached results from an already completed wave.
            if (started >= spec.max_started_nodes && stop(false)) break;
            const bool externally_cancelled = control.cancelled && control.cancelled();
            if (externally_cancelled && stop(true)) break;
            // Conservatively reserve a start slot for every earlier unresolved
            // logical node. Unknown future failures can release slots later;
            // prefetch must never steal a serial successor's start budget.
            std::vector<bool> inside_budget(spec.nodes.size());
            std::size_t reserved = 0;
            for (const auto index : logical_order) {
                if (settled[index] || prefetched[index]) continue;
                if (++reserved <= spec.max_started_nodes - started) inside_budget[index] = true;
            }
            std::vector<std::size_t> wave;
            std::vector<std::future<PhaseReceiptV3>> results;
            for (std::size_t index = 0; index < spec.nodes.size(); ++index) {
                if (settled[index] || prefetched[index] || !inside_budget[index]) continue;
                auto candidate = receipt_base(spec.nodes[index]);
                bool ready = true;
                for (const auto& dependency : spec.nodes[index].dependencies) {
                    const auto& parent = settled[static_cast<std::size_t>(find_node(spec.nodes, dependency) - spec.nodes.begin())];
                    if (!parent || parent->outcome != PhaseReceiptOutcomeV3::ready) {
                        ready = false;
                        break;
                    }
                    candidate.input_refs.push_back(input_ref(*parent));
                }
                if (!ready) continue;
                wave.push_back(index);
                ++started;
                if (control.max_parallel_nodes == 1) {
                    prefetched[index] = invoke(index, std::move(candidate));
                } else {
                    results.push_back(std::async(std::launch::async, invoke, index, std::move(candidate)));
                }
                if (wave.size() == control.max_parallel_nodes) break;
            }
            if (wave.empty()) throw Error("PhaseDagSchedulingStalled");
            // Wait and collect in fixed node order, never completion order.
            for (std::size_t index = 0; index < results.size(); ++index)
                prefetched[wave[index]] = results[index].get();
        }
        adopt(selected, std::move(*prefetched[selected]));
    }
    add_trace(run, PhaseTraceEventV3::completed, {});
    return run;
}

std::string pack_phase_dag_run_v3(const PhaseDagRunV3& run) {
    for (const auto& receipt : run.receipts) {
        validate_receipt_budget(receipt, run.spec.max_receipt_bytes);
        for (const auto& input : receipt.input_refs) validate_input(input);
        if (receipt.output) validate_output(*receipt.output);
    }
    const auto payload = encode_run(run);
    if (payload.size() > max_bundle_bytes - bundle_header.size() - 65)
        throw Error("PhaseDagBundleTooLarge");
    return std::string(bundle_header) + sha256(payload) + '\n' + payload;
}

PhaseDagRunV3 replay_phase_dag_run_v3(std::string_view bundle, std::span<const PhaseAdapterV3> catalog) {
    if (bundle.size() > max_bundle_bytes || !bundle.starts_with(bundle_header) ||
        bundle.size() < bundle_header.size() + 65)
        throw Error("InvalidPhaseDagBundleHeader");
    const auto digest = bundle.substr(bundle_header.size(), 64);
    if (bundle[bundle_header.size() + 64] != '\n' ||
        !std::all_of(digest.begin(), digest.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        }))
        throw Error("InvalidPhaseDagBundleDigest");
    const auto payload = bundle.substr(bundle_header.size() + 65);
    if (sha256(payload) != digest) throw Error("PhaseDagBundleDigestMismatch");
    const auto decoded = decode_run(payload);
    for (const auto& receipt : decoded.receipts)
        validate_receipt_budget(receipt, decoded.spec.max_receipt_bytes);
    if (encode_run(decoded) != payload) throw Error("NoncanonicalPhaseDagBundle");
    const auto expected = execute_phase_dag_v3(decoded.spec, catalog);
    if (encode_run(expected) != payload) throw Error("PhaseDagReplayMismatch");
    if (pack_phase_dag_run_v3(expected) != bundle) throw Error("NoncanonicalPhaseDagBundle");
    return expected;
}

} // namespace bruce
