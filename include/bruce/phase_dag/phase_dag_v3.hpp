#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {

struct PhaseNodeSpecV3 {
    std::string node_id, adapter_id, adapter_version;
    std::vector<std::string> dependencies;
    std::string request_bytes, request_digest;
};

struct PhaseDagSpecV3 {
    std::string dag_version{"phase-dag-v3"}, dag_id;
    std::vector<PhaseNodeSpecV3> nodes;
    std::uint64_t max_started_nodes{}, max_receipt_bytes{}, max_trace_entries{};
};

struct PhaseInputV3 {
    std::string producer_node_id, producer_adapter_id, producer_adapter_version;
    std::string producer_status, producer_output_digest, producer_evidence_digest;
    std::string producer_payload_bytes, producer_payload_digest;
};

enum class PhaseOutputStatusV3 { complete, incomplete, rejected };

struct PhaseOutputRefV3 {
    std::string output_version{"phase-output-v3"};
    PhaseOutputStatusV3 status{};
    std::string output_bytes, output_digest, evidence_bytes, evidence_digest;
    std::string payload_bytes, payload_digest, claim_scope;
};

using PhaseOutputV3 = PhaseOutputRefV3;

struct PhaseAdapterV3 {
    std::string_view adapter_id, adapter_version;
    PhaseOutputRefV3 (*execute)(std::string_view, std::span<const PhaseInputV3>);
};

// Registry entries are sorted uniquely by (adapter_id, adapter_version).
using PhaseAdapterRegistryV3 = std::span<const PhaseAdapterV3>;
using PhaseDagBundleV3 = std::string;

void validate_phase_dag_spec_v3(const PhaseDagSpecV3& spec, std::span<const PhaseAdapterV3> catalog);

enum class PhaseReceiptOutcomeV3 { ready, blocked, failed, cancelled };
struct PhaseReceiptV3 {
    std::string node_id, adapter_id, adapter_version, request_digest;
    std::vector<PhaseInputV3> input_refs;
    PhaseReceiptOutcomeV3 outcome{};
    std::optional<PhaseOutputRefV3> output;
    std::string failure_code;
};
enum class PhaseTraceEventV3 { validated, started, ready, blocked, failed, cancelled, completed };
struct PhaseTraceEntryV3 { PhaseTraceEventV3 event; std::string node_id; };
struct PhaseDagControlV3 {
    std::function<bool()> cancelled;
    std::uint64_t max_parallel_nodes{1};
};
struct PhaseDagRunV3 {
    PhaseDagSpecV3 spec;
    std::vector<PhaseReceiptV3> receipts;
    std::vector<PhaseTraceEntryV3> trace;
};
PhaseDagRunV3 execute_phase_dag_v3(const PhaseDagSpecV3&, std::span<const PhaseAdapterV3>,
                                   const PhaseDagControlV3& = {});
std::string pack_phase_dag_run_v3(const PhaseDagRunV3&);
PhaseDagRunV3 replay_phase_dag_run_v3(std::string_view, std::span<const PhaseAdapterV3>);

} // namespace bruce
