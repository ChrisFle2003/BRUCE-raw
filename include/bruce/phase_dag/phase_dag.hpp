#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {

struct PhaseNodeSpecV1 {
    std::string node_id, adapter_id, adapter_version;
    std::vector<std::string> dependencies;
    std::string request_bytes, request_digest;
};

struct PhaseDagSpecV1 {
    std::string dag_version{"phase-dag-v1"}, dag_id;
    std::vector<PhaseNodeSpecV1> nodes;
    std::uint64_t max_started_nodes{}, max_receipt_bytes{}, max_trace_entries{};
};

enum class PhaseTraceEventV1 { validated, started, ready, blocked, failed, cancelled, completed };

struct PhaseTraceEntryV1 {
    PhaseTraceEventV1 event;
    std::string node_id;
};

struct PhaseInputRefV1 {
    std::string producer_node_id, producer_adapter_id, producer_adapter_version;
    std::string producer_status, producer_output_digest, producer_evidence_digest;
};

enum class PhaseOutputStatusV1 { complete, incomplete, rejected };

struct PhaseOutputRefV1 {
    std::string output_version{"phase-output-v1"};
    PhaseOutputStatusV1 status{};
    std::string output_bytes, output_digest, evidence_bytes, evidence_digest, claim_scope;
};

enum class PhaseReceiptOutcomeV1 { ready, blocked, failed, cancelled };

struct PhaseReceiptV1 {
    std::string node_id, adapter_id, adapter_version, request_digest;
    std::vector<PhaseInputRefV1> input_refs;
    PhaseReceiptOutcomeV1 outcome{};
    std::optional<PhaseOutputRefV1> output;
    std::string failure_code;
};

struct PhaseDagControlV1 { std::function<bool()> cancelled; };

struct PhaseDagRunV1 {
    PhaseDagSpecV1 spec;
    std::vector<PhaseReceiptV1> receipts;
    std::vector<PhaseTraceEntryV1> trace;
};

void validate_phase_dag_spec_v1(const PhaseDagSpecV1& spec);
PhaseDagRunV1 execute_phase_dag_v1(const PhaseDagSpecV1& spec, const PhaseDagControlV1& control = {});
std::string pack_phase_dag_run_v1(const PhaseDagRunV1& run);
PhaseDagRunV1 replay_phase_dag_run_v1(std::string_view bundle);

} // namespace bruce
