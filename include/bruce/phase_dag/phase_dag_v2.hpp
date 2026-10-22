#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {

struct PhaseNodeSpecV2 {
    std::string node_id, adapter_id, adapter_version;
    std::vector<std::string> dependencies;
    std::string request_bytes, request_digest;
};

struct PhaseDagSpecV2 {
    std::string dag_version{"phase-dag-v2"}, dag_id;
    std::vector<PhaseNodeSpecV2> nodes;
    std::uint64_t max_started_nodes{}, max_receipt_bytes{}, max_trace_entries{};
};

struct PhaseInputRefV2 {
    std::string producer_node_id, producer_adapter_id, producer_adapter_version;
    std::string producer_status, producer_output_digest, producer_evidence_digest;
};

enum class PhaseOutputStatusV2 { complete, incomplete, rejected };

struct PhaseOutputRefV2 {
    std::string output_version{"phase-output-v2"};
    PhaseOutputStatusV2 status{};
    std::string output_bytes, output_digest, evidence_bytes, evidence_digest, claim_scope;
};

struct PhaseAdapterV2 {
    std::string_view adapter_id, adapter_version;
    PhaseOutputRefV2 (*execute)(std::string_view, std::span<const PhaseInputRefV2>);
};

void validate_phase_dag_spec_v2(const PhaseDagSpecV2& spec, std::span<const PhaseAdapterV2> catalog);

enum class PhaseReceiptOutcomeV2 { ready, blocked, failed, cancelled };
struct PhaseReceiptV2 {
    std::string node_id, adapter_id, adapter_version, request_digest;
    std::vector<PhaseInputRefV2> input_refs;
    PhaseReceiptOutcomeV2 outcome{};
    std::optional<PhaseOutputRefV2> output;
    std::string failure_code;
};
enum class PhaseTraceEventV2 { validated, started, ready, blocked, failed, cancelled, completed };
struct PhaseTraceEntryV2 { PhaseTraceEventV2 event; std::string node_id; };
struct PhaseDagControlV2 { std::function<bool()> cancelled; };
struct PhaseDagRunV2 {
    PhaseDagSpecV2 spec;
    std::vector<PhaseReceiptV2> receipts;
    std::vector<PhaseTraceEntryV2> trace;
};
PhaseDagRunV2 execute_phase_dag_v2(const PhaseDagSpecV2&, std::span<const PhaseAdapterV2>,
                                   const PhaseDagControlV2& = {});
std::string pack_phase_dag_run_v2(const PhaseDagRunV2&);
PhaseDagRunV2 replay_phase_dag_run_v2(std::string_view, std::span<const PhaseAdapterV2>);

} // namespace bruce
