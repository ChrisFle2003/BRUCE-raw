#include "bruce/phase_dag/phase_dag.hpp"

#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <queue>
#include <string_view>

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

std::vector<PhaseNodeSpecV1>::const_iterator find_node(
    const std::vector<PhaseNodeSpecV1>& nodes, const std::string& id) {
    return std::lower_bound(nodes.begin(), nodes.end(), id,
        [](const PhaseNodeSpecV1& candidate, const std::string& value) { return candidate.node_id < value; });
}

std::string output_status_name(PhaseOutputStatusV1 status) {
    switch (status) {
    case PhaseOutputStatusV1::complete: return "COMPLETE";
    case PhaseOutputStatusV1::incomplete: return "INCOMPLETE";
    case PhaseOutputStatusV1::rejected: return "REJECTED";
    }
    throw Error("InvalidPhaseOutputStatus");
}

void add_trace(PhaseDagRunV1& run, PhaseTraceEventV1 event, const std::string& node_id) {
    if (run.trace.size() < run.spec.max_trace_entries) run.trace.push_back({event, node_id});
}

void validate_output(const PhaseOutputRefV1& output) {
    if (output.output_version != "phase-output-v1" || output.output_bytes.size() > max_request_bytes ||
        output.evidence_bytes.size() > max_request_bytes || output.output_digest != sha256(output.output_bytes) ||
        output.evidence_digest != sha256(output.evidence_bytes) || !identifier(output.claim_scope))
        throw Error("InvalidPhaseAdapterOutput");
}

PhaseOutputRefV1 source_output(const PhaseNodeSpecV1& node) {
    if (node.request_bytes == "F") throw Error("FixtureSourceFailure");
    if (node.request_bytes.size() < 2 || node.request_bytes[1] != ':') throw Error("FixtureSourceRequestInvalid");
    PhaseOutputStatusV1 status;
    switch (node.request_bytes.front()) {
    case 'C': status = PhaseOutputStatusV1::complete; break;
    case 'I': status = PhaseOutputStatusV1::incomplete; break;
    case 'R': status = PhaseOutputStatusV1::rejected; break;
    default: throw Error("FixtureSourceRequestInvalid");
    }
    const auto output_bytes = node.request_bytes.substr(2);
    const auto evidence_bytes = "fixture-source-evidence:" + output_bytes;
    return {"phase-output-v1", status, output_bytes, sha256(output_bytes), evidence_bytes, sha256(evidence_bytes),
        "fixture-source-v1"};
}

PhaseOutputRefV1 join_output(const PhaseNodeSpecV1& node, const std::vector<PhaseOutputRefV1>& inputs) {
    if (!node.request_bytes.empty() || inputs.empty()) throw Error("FixtureJoinRequestInvalid");
    std::string output_bytes;
    for (std::size_t index = 0; index < inputs.size(); ++index) {
        if (index != 0) output_bytes += '|';
        output_bytes += inputs[index].output_bytes;
    }
    const auto evidence_bytes = "fixture-join-evidence:" + output_bytes;
    return {"phase-output-v1", PhaseOutputStatusV1::complete, output_bytes, sha256(output_bytes), evidence_bytes,
        sha256(evidence_bytes), "fixture-join-v1"};
}

PhaseOutputRefV1 call_fixture(const PhaseNodeSpecV1& node, const std::vector<PhaseOutputRefV1>& inputs) {
    if (node.adapter_id == "fixture-source-v1" && node.adapter_version == "v1") return source_output(node);
    if (node.adapter_id == "fixture-join-v1" && node.adapter_version == "v1") return join_output(node, inputs);
    throw Error("UnknownPhaseDagAdapter");
}

PhaseReceiptV1 receipt_base(const PhaseNodeSpecV1& node) {
    return {node.node_id, node.adapter_id, node.adapter_version, node.request_digest, {},
        PhaseReceiptOutcomeV1::failed, std::nullopt, {}};
}

PhaseInputRefV1 input_ref(const PhaseReceiptV1& receipt) {
    const auto& output = *receipt.output;
    return {receipt.node_id, receipt.adapter_id, receipt.adapter_version, output_status_name(output.status),
        output.output_digest, output.evidence_digest};
}

constexpr std::string_view bundle_header = "BRUCE_PHASE_DAG_RUN_V1\n";
constexpr std::size_t max_bundle_bytes = 5 * 1024 * 1024;

void append_field(std::string& bytes, std::string_view value) {
    bytes += std::to_string(value.size());
    bytes += ':';
    bytes += value;
}

void append_uint(std::string& bytes, std::uint64_t value) { append_field(bytes, std::to_string(value)); }

std::string receipt_outcome_name(PhaseReceiptOutcomeV1 outcome) {
    switch (outcome) {
    case PhaseReceiptOutcomeV1::ready: return "READY";
    case PhaseReceiptOutcomeV1::blocked: return "BLOCKED";
    case PhaseReceiptOutcomeV1::failed: return "FAILED";
    case PhaseReceiptOutcomeV1::cancelled: return "CANCELLED";
    }
    throw Error("InvalidPhaseReceiptOutcome");
}

std::size_t encoded_field_size(std::string_view value) {
    return std::to_string(value.size()).size() + 1 + value.size();
}

std::size_t receipt_size(const PhaseReceiptV1& receipt) {
    std::size_t total = encoded_field_size(receipt.node_id) + encoded_field_size(receipt.adapter_id) +
        encoded_field_size(receipt.adapter_version) + encoded_field_size(receipt.request_digest) +
        encoded_field_size(std::to_string(receipt.input_refs.size())) +
        encoded_field_size(receipt_outcome_name(receipt.outcome)) + encoded_field_size(receipt.output ? "1" : "0") +
        encoded_field_size(receipt.failure_code);
    for (const auto& input : receipt.input_refs) {
        total += encoded_field_size(input.producer_node_id) + encoded_field_size(input.producer_adapter_id) +
            encoded_field_size(input.producer_adapter_version) + encoded_field_size(input.producer_status) +
            encoded_field_size(input.producer_output_digest) + encoded_field_size(input.producer_evidence_digest);
    }
    if (receipt.output) {
        const auto& output = *receipt.output;
        total += encoded_field_size(output.output_version) + encoded_field_size(output_status_name(output.status)) +
            encoded_field_size(output.output_bytes) + encoded_field_size(output.output_digest) +
            encoded_field_size(output.evidence_bytes) + encoded_field_size(output.evidence_digest) +
            encoded_field_size(output.claim_scope);
    }
    return total;
}

void append_spec(std::string& bytes, const PhaseDagSpecV1& spec) {
    append_field(bytes, spec.dag_version);
    append_field(bytes, spec.dag_id);
    append_uint(bytes, spec.nodes.size());
    for (const auto& node : spec.nodes) {
        append_field(bytes, node.node_id);
        append_field(bytes, node.adapter_id);
        append_field(bytes, node.adapter_version);
        append_uint(bytes, node.dependencies.size());
        for (const auto& dependency : node.dependencies) append_field(bytes, dependency);
        append_field(bytes, node.request_bytes);
        append_field(bytes, node.request_digest);
    }
    append_uint(bytes, spec.max_started_nodes);
    append_uint(bytes, spec.max_receipt_bytes);
    append_uint(bytes, spec.max_trace_entries);
}

void append_output(std::string& bytes, const PhaseOutputRefV1& output) {
    append_field(bytes, output.output_version);
    append_field(bytes, output_status_name(output.status));
    append_field(bytes, output.output_bytes);
    append_field(bytes, output.output_digest);
    append_field(bytes, output.evidence_bytes);
    append_field(bytes, output.evidence_digest);
    append_field(bytes, output.claim_scope);
}

void append_receipt(std::string& bytes, const PhaseReceiptV1& receipt) {
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

std::string trace_event_name(PhaseTraceEventV1 event) {
    switch (event) {
    case PhaseTraceEventV1::validated: return "VALIDATED";
    case PhaseTraceEventV1::started: return "STARTED";
    case PhaseTraceEventV1::ready: return "READY";
    case PhaseTraceEventV1::blocked: return "BLOCKED";
    case PhaseTraceEventV1::failed: return "FAILED";
    case PhaseTraceEventV1::cancelled: return "CANCELLED";
    case PhaseTraceEventV1::completed: return "COMPLETED";
    }
    throw Error("InvalidPhaseTraceEvent");
}

std::string encode_run(const PhaseDagRunV1& run) {
    std::string bytes;
    append_spec(bytes, run.spec);
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
        const auto length_start = offset_;
        std::size_t length = 0;
        while (offset_ < bytes_.size() && bytes_[offset_] != ':') {
            const char character = bytes_[offset_++];
            if (character < '0' || character > '9' || length > (std::numeric_limits<std::size_t>::max() - 9) / 10)
                throw Error("InvalidPhaseDagBundleLength");
            length = length * 10 + static_cast<std::size_t>(character - '0');
        }
        if (offset_ == length_start || offset_ == bytes_.size() ||
            (offset_ - length_start > 1 && bytes_[length_start] == '0'))
            throw Error("InvalidPhaseDagBundleLength");
        ++offset_;
        if (length > bytes_.size() - offset_) throw Error("PhaseDagBundleTruncated");
        const auto result = std::string(bytes_.substr(offset_, length));
        offset_ += length;
        return result;
    }

    std::uint64_t uint_value() {
        const auto value = field();
        if (value.empty() || (value.size() > 1 && value.front() == '0')) throw Error("InvalidPhaseDagBundleInteger");
        std::uint64_t result = 0;
        for (const char character : value) {
            if (character < '0' || character > '9' || result > (std::numeric_limits<std::uint64_t>::max() - 9) / 10)
                throw Error("InvalidPhaseDagBundleInteger");
            result = result * 10 + static_cast<std::uint64_t>(character - '0');
        }
        return result;
    }

    bool finished() const { return offset_ == bytes_.size(); }

private:
    std::string_view bytes_;
    std::size_t offset_{};
};

PhaseOutputStatusV1 parse_output_status(const std::string& value) {
    if (value == "COMPLETE") return PhaseOutputStatusV1::complete;
    if (value == "INCOMPLETE") return PhaseOutputStatusV1::incomplete;
    if (value == "REJECTED") return PhaseOutputStatusV1::rejected;
    throw Error("InvalidPhaseOutputStatus");
}

PhaseReceiptOutcomeV1 parse_receipt_outcome(const std::string& value) {
    if (value == "READY") return PhaseReceiptOutcomeV1::ready;
    if (value == "BLOCKED") return PhaseReceiptOutcomeV1::blocked;
    if (value == "FAILED") return PhaseReceiptOutcomeV1::failed;
    if (value == "CANCELLED") return PhaseReceiptOutcomeV1::cancelled;
    throw Error("InvalidPhaseReceiptOutcome");
}

PhaseTraceEventV1 parse_trace_event(const std::string& value) {
    if (value == "VALIDATED") return PhaseTraceEventV1::validated;
    if (value == "STARTED") return PhaseTraceEventV1::started;
    if (value == "READY") return PhaseTraceEventV1::ready;
    if (value == "BLOCKED") return PhaseTraceEventV1::blocked;
    if (value == "FAILED") return PhaseTraceEventV1::failed;
    if (value == "CANCELLED") return PhaseTraceEventV1::cancelled;
    if (value == "COMPLETED") return PhaseTraceEventV1::completed;
    throw Error("InvalidPhaseTraceEvent");
}

PhaseDagSpecV1 parse_spec(FieldReader& reader) {
    PhaseDagSpecV1 spec;
    spec.dag_version = reader.field();
    spec.dag_id = reader.field();
    const auto node_count = reader.uint_value();
    if (node_count > max_nodes) throw Error("InvalidPhaseDagBundleCount");
    for (std::uint64_t index = 0; index < node_count; ++index) {
        PhaseNodeSpecV1 node;
        node.node_id = reader.field();
        node.adapter_id = reader.field();
        node.adapter_version = reader.field();
        const auto dependency_count = reader.uint_value();
        if (dependency_count > max_dependencies_per_node) throw Error("InvalidPhaseDagBundleCount");
        for (std::uint64_t dependency = 0; dependency < dependency_count; ++dependency)
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

PhaseOutputRefV1 parse_output(FieldReader& reader) {
    PhaseOutputRefV1 output;
    output.output_version = reader.field();
    output.status = parse_output_status(reader.field());
    output.output_bytes = reader.field();
    output.output_digest = reader.field();
    output.evidence_bytes = reader.field();
    output.evidence_digest = reader.field();
    output.claim_scope = reader.field();
    return output;
}

PhaseReceiptV1 parse_receipt(FieldReader& reader) {
    PhaseReceiptV1 receipt;
    receipt.node_id = reader.field();
    receipt.adapter_id = reader.field();
    receipt.adapter_version = reader.field();
    receipt.request_digest = reader.field();
    const auto input_count = reader.uint_value();
    if (input_count > max_dependencies_per_node) throw Error("InvalidPhaseDagBundleCount");
    for (std::uint64_t index = 0; index < input_count; ++index) {
        PhaseInputRefV1 input;
        input.producer_node_id = reader.field();
        input.producer_adapter_id = reader.field();
        input.producer_adapter_version = reader.field();
        input.producer_status = reader.field();
        input.producer_output_digest = reader.field();
        input.producer_evidence_digest = reader.field();
        receipt.input_refs.push_back(std::move(input));
    }
    receipt.outcome = parse_receipt_outcome(reader.field());
    const auto has_output = reader.field();
    if (has_output == "1") receipt.output = parse_output(reader);
    else if (has_output != "0") throw Error("InvalidPhaseDagBundleOutputMarker");
    receipt.failure_code = reader.field();
    return receipt;
}

PhaseDagRunV1 decode_run(std::string_view bytes) {
    FieldReader reader(bytes);
    PhaseDagRunV1 run;
    run.spec = parse_spec(reader);
    const auto receipt_count = reader.uint_value();
    if (receipt_count > max_nodes) throw Error("InvalidPhaseDagBundleCount");
    for (std::uint64_t index = 0; index < receipt_count; ++index) run.receipts.push_back(parse_receipt(reader));
    const auto trace_count = reader.uint_value();
    if (trace_count > 512) throw Error("InvalidPhaseDagBundleCount");
    for (std::uint64_t index = 0; index < trace_count; ++index)
        run.trace.push_back({parse_trace_event(reader.field()), reader.field()});
    if (!reader.finished()) throw Error("PhaseDagBundleTrailingBytes");
    return run;
}

std::string wrap_bundle(std::string_view payload) {
    return std::string(bundle_header) + sha256(payload) + '\n' + std::string(payload);
}

}

void validate_phase_dag_spec_v1(const PhaseDagSpecV1& spec) {
    if (spec.dag_version != "phase-dag-v1" || !identifier(spec.dag_id))
        throw Error("InvalidPhaseDagSpec");
    if (spec.nodes.empty() || spec.nodes.size() > max_nodes || spec.max_started_nodes > max_nodes ||
        spec.max_receipt_bytes == 0 || spec.max_receipt_bytes > 262144 ||
        spec.max_trace_entries < 2 || spec.max_trace_entries > 512)
        throw Error("InvalidPhaseDagLimit");
    if (!sorted_unique([&] {
            std::vector<std::string> ids;
            ids.reserve(spec.nodes.size());
            for (const auto& item : spec.nodes) ids.push_back(item.node_id);
            return ids;
        }()))
        throw Error("NoncanonicalPhaseDagNodes");

    std::size_t edge_count = 0;
    for (const auto& item : spec.nodes) {
        if (!identifier(item.node_id) || !identifier(item.adapter_id) || !identifier(item.adapter_version) ||
            item.request_bytes.size() > max_request_bytes || item.request_digest != sha256(item.request_bytes))
            throw Error("InvalidPhaseDagNode");
        if (item.dependencies.size() > max_dependencies_per_node || !sorted_unique(item.dependencies))
            throw Error("NoncanonicalPhaseDagDependencies");
        edge_count += item.dependencies.size();
        for (const auto& dependency : item.dependencies) {
            const auto predecessor = find_node(spec.nodes, dependency);
            if (!identifier(dependency) || dependency == item.node_id || predecessor == spec.nodes.end() ||
                predecessor->node_id != dependency)
                throw Error("InvalidPhaseDagDependency");
        }
    }
    if (edge_count > max_edges) throw Error("PhaseDagEdgeLimit");

    std::vector<std::size_t> indegree(spec.nodes.size());
    std::vector<std::vector<std::size_t>> successors(spec.nodes.size());
    for (std::size_t index = 0; index < spec.nodes.size(); ++index) {
        for (const auto& dependency : spec.nodes[index].dependencies) {
            const auto predecessor = find_node(spec.nodes, dependency);
            const auto predecessor_index = static_cast<std::size_t>(predecessor - spec.nodes.begin());
            successors[predecessor_index].push_back(index);
            ++indegree[index];
        }
    }
    std::queue<std::size_t> ready;
    for (std::size_t index = 0; index < indegree.size(); ++index) if (indegree[index] == 0) ready.push(index);
    std::size_t visited = 0;
    while (!ready.empty()) {
        const auto index = ready.front();
        ready.pop();
        ++visited;
        for (const auto successor : successors[index]) if (--indegree[successor] == 0) ready.push(successor);
    }
    if (visited != spec.nodes.size()) throw Error("PhaseDagCycle");
}

PhaseDagRunV1 execute_phase_dag_v1(const PhaseDagSpecV1& spec, const PhaseDagControlV1& control) {
    validate_phase_dag_spec_v1(spec);
    PhaseDagRunV1 run{spec, {}, {}};
    add_trace(run, PhaseTraceEventV1::validated, {});

    std::vector<std::optional<PhaseReceiptV1>> receipts(spec.nodes.size());
    std::size_t started = 0;
    while (run.receipts.size() < spec.nodes.size()) {
        if ((control.cancelled && control.cancelled()) || started >= spec.max_started_nodes) {
            for (std::size_t index = 0; index < spec.nodes.size(); ++index) {
                if (receipts[index]) continue;
                auto receipt = receipt_base(spec.nodes[index]);
                receipt.outcome = PhaseReceiptOutcomeV1::cancelled;
                receipts[index] = receipt;
                run.receipts.push_back(std::move(receipt));
                add_trace(run, PhaseTraceEventV1::cancelled, spec.nodes[index].node_id);
            }
            break;
        }

        std::size_t selected = spec.nodes.size();
        for (std::size_t index = 0; index < spec.nodes.size(); ++index) {
            if (receipts[index]) continue;
            bool dependencies_resolved = true;
            for (const auto& dependency : spec.nodes[index].dependencies) {
                const auto predecessor = find_node(spec.nodes, dependency);
                const auto predecessor_index = static_cast<std::size_t>(predecessor - spec.nodes.begin());
                if (!receipts[predecessor_index]) {
                    dependencies_resolved = false;
                    break;
                }
            }
            if (dependencies_resolved) {
                selected = index;
                break;
            }
        }
        if (selected == spec.nodes.size()) throw Error("PhaseDagSchedulingStalled");

        const auto& node = spec.nodes[selected];
        auto receipt = receipt_base(node);
        bool dependencies_ready = true;
        std::vector<PhaseOutputRefV1> inputs;
        for (const auto& dependency : node.dependencies) {
            const auto predecessor = find_node(spec.nodes, dependency);
            const auto predecessor_index = static_cast<std::size_t>(predecessor - spec.nodes.begin());
            const auto& parent = *receipts[predecessor_index];
            if (parent.outcome != PhaseReceiptOutcomeV1::ready) {
                dependencies_ready = false;
                break;
            }
            receipt.input_refs.push_back(input_ref(parent));
            inputs.push_back(*parent.output);
        }
        if (!dependencies_ready) {
            receipt.input_refs.clear();
            receipt.outcome = PhaseReceiptOutcomeV1::blocked;
            receipts[selected] = receipt;
            run.receipts.push_back(std::move(receipt));
            add_trace(run, PhaseTraceEventV1::blocked, node.node_id);
            continue;
        }

        ++started;
        add_trace(run, PhaseTraceEventV1::started, node.node_id);
        try {
            auto output = call_fixture(node, inputs);
            validate_output(output);
            receipt.outcome = PhaseReceiptOutcomeV1::ready;
            receipt.output = std::move(output);
            if (receipt_size(receipt) > spec.max_receipt_bytes) {
                receipt.outcome = PhaseReceiptOutcomeV1::failed;
                receipt.output.reset();
                receipt.failure_code = "DagReceiptBudgetExceeded";
            }
        } catch (const Error& error) {
            receipt.outcome = PhaseReceiptOutcomeV1::failed;
            receipt.output.reset();
            receipt.failure_code = error.what();
        }
        const auto event = receipt.outcome == PhaseReceiptOutcomeV1::ready ?
            PhaseTraceEventV1::ready : PhaseTraceEventV1::failed;
        receipts[selected] = receipt;
        run.receipts.push_back(std::move(receipt));
        add_trace(run, event, node.node_id);
    }
    add_trace(run, PhaseTraceEventV1::completed, {});
    return run;
}

std::string pack_phase_dag_run_v1(const PhaseDagRunV1& run) {
    const auto expected = execute_phase_dag_v1(run.spec);
    const auto encoded = encode_run(run);
    if (encode_run(expected) != encoded) throw Error("PhaseDagRunNotReplayable");
    return wrap_bundle(encoded);
}

PhaseDagRunV1 replay_phase_dag_run_v1(std::string_view bundle) {
    if (bundle.size() > max_bundle_bytes || !bundle.starts_with(bundle_header) || bundle.size() < bundle_header.size() + 65)
        throw Error("InvalidPhaseDagBundleHeader");
    const auto digest_start = bundle_header.size();
    const auto supplied_digest = bundle.substr(digest_start, 64);
    if (bundle[digest_start + 64] != '\n' || !std::all_of(supplied_digest.begin(), supplied_digest.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        }))
        throw Error("InvalidPhaseDagBundleDigest");
    const auto payload = bundle.substr(digest_start + 65);
    if (sha256(payload) != supplied_digest) throw Error("PhaseDagBundleDigestMismatch");
    const auto decoded = decode_run(payload);
    const auto canonical = encode_run(decoded);
    if (canonical != payload) throw Error("NoncanonicalPhaseDagBundle");
    const auto expected = execute_phase_dag_v1(decoded.spec);
    if (encode_run(expected) != canonical) throw Error("PhaseDagReplayMismatch");
    return expected;
}

} // namespace bruce
