#include "mcp_inspector.hpp"

#include "bruce/dag_scheduler/dag_scheduler_v4.hpp"
#include "../../../apps/dag_scheduler_v3_cli.hpp"
#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"
#include "bruce/phase_dag/nanocell_adapter_v2.hpp"
#include "bruce/phase_dag/nanocell_adapter_v3.hpp"

#include <algorithm>
#include <vector>

namespace bruce {
namespace {

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

template<class Range, class Render>
std::string array_json(const Range& values, Render render) {
    std::string json = "[";
    bool first = true;
    for (const auto& value : values) {
        if (!first) json += ',';
        first = false;
        json += render(value);
    }
    return json + ']';
}

std::string node_json(const PhaseNodeSpecV2& node) {
    return "{\"node_id\":" + json_string(node.node_id) +
        ",\"adapter_id\":" + json_string(node.adapter_id) +
        ",\"adapter_version\":" + json_string(node.adapter_version) +
        ",\"dependencies\":" + array_json(node.dependencies, [](const auto& id) { return json_string(id); }) + '}';
}

std::string input_json(const PhaseInputRefV2& input) {
    return "{\"producer_node_id\":" + json_string(input.producer_node_id) +
        ",\"producer_adapter_id\":" + json_string(input.producer_adapter_id) +
        ",\"producer_adapter_version\":" + json_string(input.producer_adapter_version) +
        ",\"producer_status\":" + json_string(input.producer_status) +
        ",\"producer_output_digest\":" + json_string(input.producer_output_digest) +
        ",\"producer_evidence_digest\":" + json_string(input.producer_evidence_digest) + '}';
}

std::string output_json(const PhaseOutputRefV2& output) {
    // Only the replay-verified references are presented; payloads stay private.
    return "{\"output_version\":" + json_string(output.output_version) +
        ",\"status\":" + json_string(output_status_name(output.status)) +
        ",\"output_digest\":" + json_string(output.output_digest) +
        ",\"evidence_digest\":" + json_string(output.evidence_digest) +
        ",\"claim_scope\":" + json_string(output.claim_scope) + '}';
}

std::string receipt_json(const PhaseReceiptV2& receipt) {
    return "{\"node_id\":" + json_string(receipt.node_id) +
        ",\"adapter_id\":" + json_string(receipt.adapter_id) +
        ",\"adapter_version\":" + json_string(receipt.adapter_version) +
        ",\"request_digest\":" + json_string(receipt.request_digest) +
        ",\"input_refs\":" + array_json(receipt.input_refs, input_json) +
        ",\"outcome\":" + json_string(receipt_outcome_name(receipt.outcome)) +
        ",\"output\":" + (receipt.output ? output_json(*receipt.output) : "null") +
        ",\"failure_code\":" + json_string(receipt.failure_code) + '}';
}

std::string trace_json(const PhaseTraceEntryV2& entry) {
    return "{\"event\":" + json_string(trace_event_name(entry.event)) +
        ",\"node_id\":" + json_string(entry.node_id) + '}';
}

std::string run_json(const PhaseDagRunV2& run) {
    return "{\"status\":\"VALID\",\"code\":\"VALID\",\"dag_version\":" + json_string(run.spec.dag_version) +
        ",\"dag_id\":" + json_string(run.spec.dag_id) +
        ",\"nodes\":" + array_json(run.spec.nodes, node_json) +
        ",\"receipts\":" + array_json(run.receipts, receipt_json) +
        ",\"trace\":" + array_json(run.trace, trace_json) + '}';
}

std::string_view output_status_name_v3(PhaseOutputStatusV3 status) {
    switch (status) {
    case PhaseOutputStatusV3::complete: return "COMPLETE";
    case PhaseOutputStatusV3::incomplete: return "INCOMPLETE";
    case PhaseOutputStatusV3::rejected: return "REJECTED";
    }
    throw Error("InvalidPhaseOutputStatus");
}

std::string_view receipt_outcome_name_v3(PhaseReceiptOutcomeV3 outcome) {
    switch (outcome) {
    case PhaseReceiptOutcomeV3::ready: return "READY";
    case PhaseReceiptOutcomeV3::blocked: return "BLOCKED";
    case PhaseReceiptOutcomeV3::failed: return "FAILED";
    case PhaseReceiptOutcomeV3::cancelled: return "CANCELLED";
    }
    throw Error("InvalidPhaseReceiptOutcome");
}

std::string_view trace_event_name_v3(PhaseTraceEventV3 event) {
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

std::string node_json_v3(const PhaseNodeSpecV3& node) {
    return "{\"node_id\":" + json_string(node.node_id) +
        ",\"adapter_id\":" + json_string(node.adapter_id) +
        ",\"adapter_version\":" + json_string(node.adapter_version) +
        ",\"dependencies\":" + array_json(node.dependencies, [](const auto& id) { return json_string(id); }) + '}';
}

std::string input_json_v3(const PhaseInputV3& input) {
    return "{\"producer_node_id\":" + json_string(input.producer_node_id) +
        ",\"producer_adapter_id\":" + json_string(input.producer_adapter_id) +
        ",\"producer_adapter_version\":" + json_string(input.producer_adapter_version) +
        ",\"producer_status\":" + json_string(input.producer_status) +
        ",\"producer_output_digest\":" + json_string(input.producer_output_digest) +
        ",\"producer_evidence_digest\":" + json_string(input.producer_evidence_digest) +
        ",\"producer_payload_digest\":" + json_string(input.producer_payload_digest) +
        ",\"payload_size\":" + std::to_string(input.producer_payload_bytes.size()) + '}';
}

std::string output_json_v3(const PhaseOutputRefV3& output) {
    // Only the replay-verified references are presented; payloads stay private.
    return "{\"output_version\":" + json_string(output.output_version) +
        ",\"status\":" + json_string(output_status_name_v3(output.status)) +
        ",\"output_digest\":" + json_string(output.output_digest) +
        ",\"evidence_digest\":" + json_string(output.evidence_digest) +
        ",\"payload_digest\":" + json_string(output.payload_digest) +
        ",\"payload_size\":" + std::to_string(output.payload_bytes.size()) +
        ",\"claim_scope\":" + json_string(output.claim_scope) + '}';
}

std::string receipt_json_v3(const PhaseReceiptV3& receipt) {
    return "{\"node_id\":" + json_string(receipt.node_id) +
        ",\"adapter_id\":" + json_string(receipt.adapter_id) +
        ",\"adapter_version\":" + json_string(receipt.adapter_version) +
        ",\"request_digest\":" + json_string(receipt.request_digest) +
        ",\"input_refs\":" + array_json(receipt.input_refs, input_json_v3) +
        ",\"outcome\":" + json_string(receipt_outcome_name_v3(receipt.outcome)) +
        ",\"output\":" + (receipt.output ? output_json_v3(*receipt.output) : "null") +
        ",\"failure_code\":" + json_string(receipt.failure_code) + '}';
}

std::string trace_json_v3(const PhaseTraceEntryV3& entry) {
    return "{\"event\":" + json_string(trace_event_name_v3(entry.event)) +
        ",\"node_id\":" + json_string(entry.node_id) + '}';
}

const PhaseReceiptV3& receipt_for_node_v3(const PhaseDagRunV3& run, std::string_view node_id) {
    const auto receipt = std::find_if(run.receipts.begin(), run.receipts.end(), [&](const auto& value) {
        return value.node_id == node_id;
    });
    if (receipt == run.receipts.end()) throw Error("MissingPhaseDagReceipt");
    return *receipt;
}

std::string scheduler_explanation_json_v3(const PhaseDagRunV3& run) {
    // Replay guarantees complete receipts; join by ID because receipt order is logical execution order.
    return "{\"trace_model\":\"canonical-logical-v1\",\"parallelism_recorded\":false,\"nodes\":" +
        array_json(run.spec.nodes, [&](const auto& node) {
            const auto& receipt = receipt_for_node_v3(run, node.node_id);
            std::vector<const PhaseReceiptV3*> blockers;
            if (receipt.outcome == PhaseReceiptOutcomeV3::blocked) {
                for (const auto& dependency : node.dependencies) {
                    const auto& predecessor = receipt_for_node_v3(run, dependency);
                    if (predecessor.outcome != PhaseReceiptOutcomeV3::ready) blockers.push_back(&predecessor);
                }
            }
            return "{\"node_id\":" + json_string(node.node_id) +
                ",\"outcome\":" + json_string(receipt_outcome_name_v3(receipt.outcome)) +
                ",\"failure_code\":" + json_string(receipt.failure_code) +
                ",\"dependencies\":" + array_json(node.dependencies, [](const auto& id) { return json_string(id); }) +
                ",\"blocking_predecessors\":" + array_json(blockers, [](const auto* predecessor) {
                    return "{\"node_id\":" + json_string(predecessor->node_id) +
                        ",\"outcome\":" + json_string(receipt_outcome_name_v3(predecessor->outcome)) +
                        ",\"failure_code\":" + json_string(predecessor->failure_code) + '}';
                }) + '}';
        }) + '}';
}

std::string run_json_v3(const PhaseDagRunV3& run) {
    return "{\"status\":\"VALID\",\"code\":\"VALID\",\"dag_version\":" + json_string(run.spec.dag_version) +
        ",\"dag_id\":" + json_string(run.spec.dag_id) +
        ",\"nodes\":" + array_json(run.spec.nodes, node_json_v3) +
        ",\"receipts\":" + array_json(run.receipts, receipt_json_v3) +
        ",\"trace\":" + array_json(run.trace, trace_json_v3) +
        ",\"scheduler_explanation\":" + scheduler_explanation_json_v3(run) + '}';
}


std::string_view scheduler_outcome_name_v4(DagSchedulerReceiptOutcomeV2 outcome) {
    switch (outcome) {
    case DagSchedulerReceiptOutcomeV2::ready: return "READY";
    case DagSchedulerReceiptOutcomeV2::failed: return "FAILED";
    case DagSchedulerReceiptOutcomeV2::blocked: return "BLOCKED";
    case DagSchedulerReceiptOutcomeV2::cancelled: return "CANCELLED";
    }
    throw Error("InvalidDagSchedulerReceiptOutcome");
}

std::string_view scheduler_event_name_v4(DagSchedulerTraceEventV2 event) {
    switch (event) {
    case DagSchedulerTraceEventV2::validated: return "VALIDATED";
    case DagSchedulerTraceEventV2::started: return "STARTED";
    case DagSchedulerTraceEventV2::ready: return "READY";
    case DagSchedulerTraceEventV2::failed: return "FAILED";
    case DagSchedulerTraceEventV2::blocked: return "BLOCKED";
    case DagSchedulerTraceEventV2::cancelled: return "CANCELLED";
    case DagSchedulerTraceEventV2::completed: return "COMPLETED";
    }
    throw Error("InvalidDagSchedulerTraceEvent");
}

std::string scheduler_node_json_v4(const DagSchedulerNodeSpecV2& node) {
    return "{\"node_id\":" + json_string(node.node_id) +
        ",\"callback_id\":" + json_string(node.callback_id) +
        ",\"dependencies\":" + array_json(node.dependencies, [](const auto& id) { return json_string(id); }) +
        ",\"request_size\":" + std::to_string(node.request_bytes.size()) +
        ",\"request_digest\":" + json_string(node.request_digest) + '}';
}

std::string scheduler_input_json_v4(const DagSchedulerInputV2& input) {
    return "{\"producer_node_id\":" + json_string(input.producer_node_id) +
        ",\"result_code\":" + json_string(input.result_code) +
        ",\"payload_size\":" + std::to_string(input.payload_bytes.size()) +
        ",\"payload_digest\":" + json_string(input.payload_digest) + '}';
}

std::string scheduler_output_json_v4(const DagSchedulerOutputRefV2& output) {
    return "{\"output_version\":" + json_string(output.output_version) +
        ",\"result_code\":" + json_string(output.result_code) +
        ",\"payload_size\":" + std::to_string(output.payload_bytes.size()) +
        ",\"payload_digest\":" + json_string(output.payload_digest) + '}';
}

std::string scheduler_receipt_json_v4(const DagSchedulerReceiptV2& receipt, const DagSchedulerSpecV2& spec) {
    const auto node = std::find_if(spec.nodes.begin(), spec.nodes.end(), [&](const auto& value) {
        return value.node_id == receipt.node_id;
    });
    if (node == spec.nodes.end()) throw Error("MissingDagSchedulerNode");
    return "{\"node_id\":" + json_string(receipt.node_id) +
        ",\"callback_id\":" + json_string(receipt.callback_id) +
        ",\"request_size\":" + std::to_string(node->request_bytes.size()) +
        ",\"request_digest\":" + json_string(receipt.request_digest) +
        ",\"input_refs\":" + array_json(receipt.input_refs, scheduler_input_json_v4) +
        ",\"outcome\":" + json_string(scheduler_outcome_name_v4(receipt.outcome)) +
        ",\"output\":" + (receipt.output ? scheduler_output_json_v4(*receipt.output) : "null") +
        ",\"failure_code\":" + json_string(receipt.failure_code) + '}';
}

std::string scheduler_run_json_v4(const DagSchedulerRunV2& run) {
    // Only recorded, structurally validated metadata is public. No replay claim.
    return "{\"status\":\"VALID\",\"code\":\"VALID\",\"scheduler_version\":" + json_string(run.spec.scheduler_version) +
        ",\"dag_id\":" + json_string(run.spec.dag_id) +
        ",\"max_started_nodes\":" + std::to_string(run.spec.max_started_nodes) +
        ",\"max_total_receipt_bytes\":" + std::to_string(run.spec.max_total_receipt_bytes) +
        ",\"nodes\":" + array_json(run.spec.nodes, scheduler_node_json_v4) +
        ",\"receipts\":" + array_json(run.receipts, [&](const auto& receipt) {
            return scheduler_receipt_json_v4(receipt, run.spec);
        }) +
        ",\"trace\":" + array_json(run.trace, [](const auto& entry) {
            return "{\"event\":" + json_string(scheduler_event_name_v4(entry.event)) +
                ",\"node_id\":" + json_string(entry.node_id) + '}';
        }) + '}';
}

McpInspectionResultV1 rejected() {
    return {McpInspectionStatusV1::rejected, "REJECTED", "{\"status\":\"REJECTED\",\"code\":\"REJECTED\"}"};
}

} // namespace

std::string mcp_inspection_status_name(McpInspectionStatusV1 status) {
    switch (status) {
    case McpInspectionStatusV1::valid: return "VALID";
    case McpInspectionStatusV1::rejected: return "REJECTED";
    }
    throw Error("InvalidMcpInspectionStatus");
}

McpInspectionResultV1 inspect_phase_dag_v2_bundle(std::string_view bytes) {
    if (bytes.size() > mcp_max_inspect_bytes_v1) return rejected();
    try {
        const auto run = replay_phase_dag_run_v2(bytes, phase_dag_nanocell_catalog_v2());
        return {McpInspectionStatusV1::valid, "VALID", run_json(run)};
    } catch (const Error&) {
        return rejected();
    }
}

McpInspectionResultV1 inspect_phase_dag_v3_bundle(std::string_view bytes) {
    if (bytes.size() > mcp_max_inspect_bytes_v3) return rejected();
    try {
        const auto run = replay_phase_dag_run_v3(bytes, phase_dag_nanocell_catalog_v3());
        return {McpInspectionStatusV1::valid, "VALID", run_json_v3(run)};
    } catch (const Error&) {
        return rejected();
    }
}

McpInspectionResultV1 mcp_inspector_detail::inspect_dag_scheduler_v4_bundle(
        std::string_view bytes, DagSchedulerCallbackRegistryV2 registry) {
    if (bytes.size() > dag_scheduler_v4_max_bundle_bytes) return rejected();
    try {
        const auto run = inspect_dag_scheduler_run_v4(bytes, registry);
        return {McpInspectionStatusV1::valid, "VALID", scheduler_run_json_v4(run)};
    } catch (const Error&) {
        return rejected();
    }
}

McpInspectionResultV1 inspect_dag_scheduler_v4_bundle(std::string_view bytes) {
    return mcp_inspector_detail::inspect_dag_scheduler_v4_bundle(bytes, dag_scheduler_v3_cli_detail::registry);
}

} // namespace bruce
