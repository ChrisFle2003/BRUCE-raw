#include "bruce/phase_dag/phase_dag_v2.hpp"

#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <algorithm>
#include <cstddef>
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

auto find_node(const std::vector<PhaseNodeSpecV2>& nodes, std::string_view id) {
    return std::lower_bound(nodes.begin(), nodes.end(), id,
        [](const PhaseNodeSpecV2& candidate, std::string_view value) { return candidate.node_id < value; });
}

void validate_catalog(std::span<const PhaseAdapterV2> catalog) {
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

void validate_graph(const PhaseDagSpecV2& spec) {
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

std::string_view output_status_name(PhaseOutputStatusV2 status) {
    switch (status) {
    case PhaseOutputStatusV2::complete: return "COMPLETE";
    case PhaseOutputStatusV2::incomplete: return "INCOMPLETE";
    case PhaseOutputStatusV2::rejected: return "REJECTED";
    }
    throw Error("InvalidPhaseOutputStatus");
}

std::string_view receipt_outcome_name(PhaseReceiptOutcomeV2 outcome) {
    switch (outcome) {
    case PhaseReceiptOutcomeV2::ready: return "READY";
    case PhaseReceiptOutcomeV2::blocked: return "BLOCKED";
    case PhaseReceiptOutcomeV2::failed: return "FAILED";
    case PhaseReceiptOutcomeV2::cancelled: return "CANCELLED";
    }
    throw Error("InvalidPhaseReceiptOutcome");
}

std::string_view trace_event_name(PhaseTraceEventV2 event) {
    switch (event) {
    case PhaseTraceEventV2::validated: return "VALIDATED";
    case PhaseTraceEventV2::started: return "STARTED";
    case PhaseTraceEventV2::ready: return "READY";
    case PhaseTraceEventV2::blocked: return "BLOCKED";
    case PhaseTraceEventV2::failed: return "FAILED";
    case PhaseTraceEventV2::cancelled: return "CANCELLED";
    case PhaseTraceEventV2::completed: return "COMPLETED";
    }
    throw Error("InvalidPhaseTraceEvent");
}

void append_field(std::string& bytes, std::string_view value) {
    bytes += std::to_string(value.size());
    bytes += ':';
    bytes += value;
}

void append_uint(std::string& bytes, std::uint64_t value) { append_field(bytes, std::to_string(value)); }

void append_output(std::string& bytes, const PhaseOutputRefV2& output) {
    append_field(bytes, output.output_version);
    append_field(bytes, output_status_name(output.status));
    append_field(bytes, output.output_bytes);
    append_field(bytes, output.output_digest);
    append_field(bytes, output.evidence_bytes);
    append_field(bytes, output.evidence_digest);
    append_field(bytes, output.claim_scope);
}

void append_receipt(std::string& bytes, const PhaseReceiptV2& receipt) {
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
    }
    append_field(bytes, receipt_outcome_name(receipt.outcome));
    append_field(bytes, receipt.output ? "1" : "0");
    if (receipt.output) append_output(bytes, *receipt.output);
    append_field(bytes, receipt.failure_code);
}

void validate_output(const PhaseOutputRefV2& output) {
    if (output.output_version != "phase-output-v2" || output.output_bytes.size() > max_request_bytes ||
        output.evidence_bytes.size() > max_request_bytes || output.output_digest != sha256(output.output_bytes) ||
        output.evidence_digest != sha256(output.evidence_bytes) || !identifier(output.claim_scope))
        throw Error("InvalidPhaseAdapterOutput");
    (void)output_status_name(output.status);
}

void add_trace(PhaseDagRunV2& run, PhaseTraceEventV2 event, const std::string& node_id) {
    if (event == PhaseTraceEventV2::completed) {
        if (run.trace.size() < run.spec.max_trace_entries) run.trace.push_back({event, node_id});
    } else if (run.trace.size() + 1 < run.spec.max_trace_entries) {
        run.trace.push_back({event, node_id});
    }
}

PhaseReceiptV2 receipt_base(const PhaseNodeSpecV2& node) {
    return {node.node_id, node.adapter_id, node.adapter_version, node.request_digest, {},
        PhaseReceiptOutcomeV2::failed, std::nullopt, {}};
}

PhaseInputRefV2 input_ref(const PhaseReceiptV2& receipt) {
    const auto& output = *receipt.output;
    return {receipt.node_id, receipt.adapter_id, receipt.adapter_version,
        std::string(output_status_name(output.status)), output.output_digest, output.evidence_digest};
}

const PhaseAdapterV2& find_adapter(const PhaseNodeSpecV2& node, std::span<const PhaseAdapterV2> catalog) {
    const auto key = std::pair{std::string_view(node.adapter_id), std::string_view(node.adapter_version)};
    const auto result = std::lower_bound(catalog.begin(), catalog.end(), key,
        [](const PhaseAdapterV2& adapter, const auto& value) {
            return std::pair{adapter.adapter_id, adapter.adapter_version} < value;
        });
    if (result == catalog.end() || std::pair{result->adapter_id, result->adapter_version} != key)
        throw Error("UnknownPhaseDagAdapter");
    return *result;
}

constexpr std::string_view bundle_header = "BRUCE_PHASE_DAG_RUN_V2\n";
constexpr std::size_t max_bundle_bytes = 5 * 1024 * 1024;

std::string encode_run(const PhaseDagRunV2& run) {
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

PhaseOutputStatusV2 parse_output_status(std::string_view value) {
    if (value == "COMPLETE") return PhaseOutputStatusV2::complete;
    if (value == "INCOMPLETE") return PhaseOutputStatusV2::incomplete;
    if (value == "REJECTED") return PhaseOutputStatusV2::rejected;
    throw Error("InvalidPhaseOutputStatus");
}

PhaseReceiptOutcomeV2 parse_receipt_outcome(std::string_view value) {
    if (value == "READY") return PhaseReceiptOutcomeV2::ready;
    if (value == "BLOCKED") return PhaseReceiptOutcomeV2::blocked;
    if (value == "FAILED") return PhaseReceiptOutcomeV2::failed;
    if (value == "CANCELLED") return PhaseReceiptOutcomeV2::cancelled;
    throw Error("InvalidPhaseReceiptOutcome");
}

PhaseTraceEventV2 parse_trace_event(std::string_view value) {
    if (value == "VALIDATED") return PhaseTraceEventV2::validated;
    if (value == "STARTED") return PhaseTraceEventV2::started;
    if (value == "READY") return PhaseTraceEventV2::ready;
    if (value == "BLOCKED") return PhaseTraceEventV2::blocked;
    if (value == "FAILED") return PhaseTraceEventV2::failed;
    if (value == "CANCELLED") return PhaseTraceEventV2::cancelled;
    if (value == "COMPLETED") return PhaseTraceEventV2::completed;
    throw Error("InvalidPhaseTraceEvent");
}

PhaseDagSpecV2 parse_spec(FieldReader& reader) {
    PhaseDagSpecV2 spec;
    spec.dag_version = reader.field();
    spec.dag_id = reader.field();
    const auto count = reader.uint_value();
    if (count > max_nodes) throw Error("InvalidPhaseDagBundleCount");
    for (std::uint64_t index = 0; index < count; ++index) {
        PhaseNodeSpecV2 node;
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

PhaseOutputRefV2 parse_output(FieldReader& reader) {
    PhaseOutputRefV2 output;
    output.output_version = reader.field();
    output.status = parse_output_status(reader.field());
    output.output_bytes = reader.field();
    output.output_digest = reader.field();
    output.evidence_bytes = reader.field();
    output.evidence_digest = reader.field();
    output.claim_scope = reader.field();
    return output;
}

PhaseReceiptV2 parse_receipt(FieldReader& reader) {
    PhaseReceiptV2 receipt;
    receipt.node_id = reader.field();
    receipt.adapter_id = reader.field();
    receipt.adapter_version = reader.field();
    receipt.request_digest = reader.field();
    const auto count = reader.uint_value();
    if (count > max_dependencies_per_node) throw Error("InvalidPhaseDagBundleCount");
    for (std::uint64_t index = 0; index < count; ++index) {
        PhaseInputRefV2 input;
        input.producer_node_id = reader.field();
        input.producer_adapter_id = reader.field();
        input.producer_adapter_version = reader.field();
        input.producer_status = reader.field();
        input.producer_output_digest = reader.field();
        input.producer_evidence_digest = reader.field();
        receipt.input_refs.push_back(std::move(input));
    }
    receipt.outcome = parse_receipt_outcome(reader.field());
    const auto marker = reader.field();
    if (marker == "1") receipt.output = parse_output(reader);
    else if (marker != "0") throw Error("InvalidPhaseDagBundleOutputMarker");
    receipt.failure_code = reader.field();
    return receipt;
}

PhaseDagRunV2 decode_run(std::string_view bytes) {
    FieldReader reader(bytes);
    PhaseDagRunV2 run;
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

void validate_phase_dag_spec_v2(const PhaseDagSpecV2& spec, std::span<const PhaseAdapterV2> catalog) {
    if (spec.dag_version != "phase-dag-v2" || !identifier(spec.dag_id))
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
            [](const PhaseAdapterV2& candidate, const auto& value) {
                return std::pair{candidate.adapter_id, candidate.adapter_version} < value;
            });
        if (adapter == catalog.end() || std::pair{adapter->adapter_id, adapter->adapter_version} != key)
            throw Error("UnknownPhaseDagAdapter");
    }
}

PhaseDagRunV2 execute_phase_dag_v2(const PhaseDagSpecV2& spec, std::span<const PhaseAdapterV2> catalog,
    const PhaseDagControlV2& control) {
    validate_phase_dag_spec_v2(spec, catalog);
    PhaseDagRunV2 run{spec, {}, {}};
    add_trace(run, PhaseTraceEventV2::validated, {});
    std::vector<std::optional<PhaseReceiptV2>> settled(spec.nodes.size());
    std::size_t started = 0;
    while (run.receipts.size() < spec.nodes.size()) {
        const bool externally_cancelled = control.cancelled && control.cancelled();
        if (externally_cancelled || started >= spec.max_started_nodes) {
            for (std::size_t index = 0; index < spec.nodes.size(); ++index) {
                if (settled[index]) continue;
                auto cancelled = receipt_base(spec.nodes[index]);
                cancelled.outcome = PhaseReceiptOutcomeV2::cancelled;
                if (!externally_cancelled) cancelled.failure_code = "DagBudgetExhausted";
                settled[index] = cancelled;
                run.receipts.push_back(std::move(cancelled));
                add_trace(run, PhaseTraceEventV2::cancelled, spec.nodes[index].node_id);
            }
            break;
        }
        std::size_t selected = spec.nodes.size();
        for (std::size_t index = 0; index < spec.nodes.size(); ++index) {
            if (settled[index]) continue;
            const auto& dependencies = spec.nodes[index].dependencies;
            const auto resolved = std::all_of(dependencies.begin(), dependencies.end(), [&](const auto& dependency) {
                const auto predecessor = find_node(spec.nodes, dependency);
                return settled[static_cast<std::size_t>(predecessor - spec.nodes.begin())].has_value();
            });
            if (resolved) { selected = index; break; }
        }
        if (selected == spec.nodes.size()) throw Error("PhaseDagSchedulingStalled");
        const auto& node = spec.nodes[selected];
        auto receipt = receipt_base(node);
        bool dependencies_ready = true;
        for (const auto& dependency : node.dependencies) {
            const auto predecessor = find_node(spec.nodes, dependency);
            const auto& parent = *settled[static_cast<std::size_t>(predecessor - spec.nodes.begin())];
            if (parent.outcome != PhaseReceiptOutcomeV2::ready) {
                dependencies_ready = false;
                break;
            }
            receipt.input_refs.push_back(input_ref(parent));
        }
        if (!dependencies_ready) {
            receipt.input_refs.clear();
            receipt.outcome = PhaseReceiptOutcomeV2::blocked;
            settled[selected] = receipt;
            run.receipts.push_back(std::move(receipt));
            add_trace(run, PhaseTraceEventV2::blocked, node.node_id);
            continue;
        }
        ++started;
        add_trace(run, PhaseTraceEventV2::started, node.node_id);
        try {
            const auto& adapter = find_adapter(node, catalog);
            auto output = adapter.execute(node.request_bytes, receipt.input_refs);
            validate_output(output);
            receipt.outcome = PhaseReceiptOutcomeV2::ready;
            receipt.output = std::move(output);
            std::string encoded_receipt;
            append_receipt(encoded_receipt, receipt);
            if (encoded_receipt.size() > spec.max_receipt_bytes) {
                receipt.outcome = PhaseReceiptOutcomeV2::failed;
                receipt.output.reset();
                receipt.failure_code = "DagReceiptBudgetExceeded";
            }
        } catch (const Error& error) {
            receipt.outcome = PhaseReceiptOutcomeV2::failed;
            receipt.output.reset();
            receipt.failure_code = error.what();
        } catch (const std::exception&) {
            receipt.outcome = PhaseReceiptOutcomeV2::failed;
            receipt.output.reset();
            receipt.failure_code = "PhaseAdapterException";
        } catch (...) {
            receipt.outcome = PhaseReceiptOutcomeV2::failed;
            receipt.output.reset();
            receipt.failure_code = "PhaseAdapterException";
        }
        const auto event = receipt.outcome == PhaseReceiptOutcomeV2::ready ?
            PhaseTraceEventV2::ready : PhaseTraceEventV2::failed;
        settled[selected] = receipt;
        run.receipts.push_back(std::move(receipt));
        add_trace(run, event, node.node_id);
    }
    add_trace(run, PhaseTraceEventV2::completed, {});
    return run;
}

std::string pack_phase_dag_run_v2(const PhaseDagRunV2& run) {
    const auto payload = encode_run(run);
    if (payload.size() > max_bundle_bytes - bundle_header.size() - 65)
        throw Error("PhaseDagBundleTooLarge");
    return std::string(bundle_header) + sha256(payload) + '\n' + payload;
}

PhaseDagRunV2 replay_phase_dag_run_v2(std::string_view bundle, std::span<const PhaseAdapterV2> catalog) {
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
    if (encode_run(decoded) != payload) throw Error("NoncanonicalPhaseDagBundle");
    const auto expected = execute_phase_dag_v2(decoded.spec, catalog);
    if (encode_run(expected) != payload) throw Error("PhaseDagReplayMismatch");
    if (pack_phase_dag_run_v2(expected) != bundle) throw Error("NoncanonicalPhaseDagBundle");
    return expected;
}

} // namespace bruce
