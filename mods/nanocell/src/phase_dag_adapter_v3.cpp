#include "bruce/phase_dag/nanocell_adapter_v3.hpp"
#include "bruce/phase_dag/payload_gate_adapter_v3.hpp"

#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/mods/nanocell/nanocell.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <vector>

namespace bruce {
namespace {
constexpr std::size_t max_inputs = 15;
void field(std::string& out, std::string_view value) {
    out += std::to_string(value.size());
    out += ':';
    out.append(value);
}
void number(std::string& out, std::uint64_t value) { field(out, std::to_string(value)); }
class Fields {
public:
    explicit Fields(std::string_view bytes) : remaining_(bytes) {
        if (bytes.size() > 65536) throw Error("PhasePayloadCodecTooLarge");
    }
    std::string field() {
        const auto colon = remaining_.find(':');
        if (colon == std::string_view::npos) throw Error("PhasePayloadFieldMissing");
        const auto length = parse_number(remaining_.substr(0, colon), 65536);
        remaining_.remove_prefix(colon + 1);
        if (length > remaining_.size()) throw Error("PhasePayloadFieldTruncated");
        const auto result = std::string(remaining_.substr(0, length));
        remaining_.remove_prefix(length);
        return result;
    }
    std::uint64_t number(std::uint64_t maximum) { return parse_number(field(), maximum); }
    void finish() const {
        if (!remaining_.empty()) throw Error("PhasePayloadTrailingBytes");
    }
private:
    static std::uint64_t parse_number(std::string_view value, std::uint64_t maximum) {
        if (value.empty() || (value.size() > 1 && value.front() == '0') ||
            !std::all_of(value.begin(), value.end(), [](char c) { return c >= '0' && c <= '9'; }))
            throw Error("InvalidPhasePayloadNumber");
        std::uint64_t result{};
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
        if (error != std::errc{} || end != value.data() + value.size() || result > maximum)
            throw Error("InvalidPhasePayloadNumber");
        return result;
    }
    std::string_view remaining_;
};
bool identifier(std::string_view value) {
    return !value.empty() && value.size() <= 64 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
    });
}
bool digest(std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
void validate_decision(std::string_view decision, std::uint64_t count) {
    if (count > max_inputs || (decision != "COMPLETE" && decision != "REJECTED") ||
        (decision == "COMPLETE" && count == 0)) throw Error("InvalidPayloadGateDecision");
}
void validate_nanocell_payload(const NanoCellPhasePayloadV1& value) {
    if (value.payload_version != "nanocell-phase-payload-v1" ||
        (value.nanocell_status != "MATCHED" && value.nanocell_status != "NO_MATCH" &&
         value.nanocell_status != "INCOMPLETE" && value.nanocell_status != "CONTEXT_REJECTED") ||
        value.final_state.has_value() != (value.nanocell_status != "CONTEXT_REJECTED"))
        throw Error("InvalidNanoCellPhasePayload");
}
PhaseOutputRefV3 output(PhaseOutputStatusV3 status, std::string metadata, std::string evidence,
    std::string payload, std::string scope) {
    PhaseOutputRefV3 value;
    value.status = status;
    value.output_bytes = std::move(metadata); value.output_digest = sha256(value.output_bytes);
    value.evidence_bytes = std::move(evidence); value.evidence_digest = sha256(value.evidence_bytes);
    value.payload_bytes = std::move(payload); value.payload_digest = sha256(value.payload_bytes);
    value.claim_scope = std::move(scope);
    return value;
}
PhaseOutputRefV3 execute_nanocell_adapter(std::string_view bytes, std::span<const PhaseInputV3>) {
    const auto phase = unpack_nanocell_phase_request_v1(bytes);
    NanoCellRequestV1 request;
    request.core.core_layout_version = phase.core_layout_version;
    request.core.core_bytes = phase.core_bytes;
    request.core.active_index = phase.active_index;
    request.context = {phase.context_id, phase.context_bytes, phase.context_digest};
    NanoCellReferenceTechniqueV1 technique;
    if (phase.technique_id == "masked-match-v1") technique = NanoCellReferenceTechniqueV1::masked_match;
    else if (phase.technique_id == "rotate-match-v1") technique = NanoCellReferenceTechniqueV1::rotate_match;
    else if (phase.technique_id == "context-conflict-v1") technique = NanoCellReferenceTechniqueV1::context_conflict;
    else throw Error("UnknownNanoCellPhaseTechnique");
    request.technique = nanocell_reference_technique_v1(technique);
    request.max_operations = phase.max_operations;
    request.max_trace_entries = phase.max_trace_entries;
    const auto run = execute_nanocell_mod_v1(request);
    const auto& answer = run.nanocell.answer;
    NanoCellPhasePayloadV1 payload;
    PhaseOutputStatusV3 status;
    switch (answer.status) {
    case NanoCellStatusV1::matched: payload.nanocell_status = "MATCHED"; status = PhaseOutputStatusV3::complete; break;
    case NanoCellStatusV1::no_match: payload.nanocell_status = "NO_MATCH"; status = PhaseOutputStatusV3::complete; break;
    case NanoCellStatusV1::incomplete: payload.nanocell_status = "INCOMPLETE"; status = PhaseOutputStatusV3::incomplete; break;
    case NanoCellStatusV1::context_rejected: payload.nanocell_status = "CONTEXT_REJECTED"; status = PhaseOutputStatusV3::rejected; break;
    default: throw Error("InvalidNanoCellPhaseStatus");
    }
    payload.final_state = answer.final_state;
    NanoCellPhaseMetadataV1 metadata;
    metadata.nanocell_status = payload.nanocell_status;
    metadata.technique_id = answer.technique_id;
    metadata.technique_version = answer.technique_version;
    metadata.core_layout_version = answer.core_layout_version;
    metadata.active_index = answer.active_index;
    metadata.core_digest = answer.core_digest;
    metadata.context_id = answer.context_id;
    metadata.context_digest = answer.context_digest;
    metadata.technique_digest = answer.technique_digest;
    metadata.operations_used = answer.operations_used;
    metadata.trace_entries = answer.trace.size();
    const auto metadata_bytes = pack_nanocell_phase_metadata_v1(metadata);
    const auto payload_bytes = pack_nanocell_phase_payload_v1(payload);
    // Replay reruns the request stored by V3; evidence binds its references,
    // metadata and payload without duplicating any raw context/state bytes.
    std::string evidence;
    field(evidence, "nanocell-phase-evidence-v1");
    field(evidence, sha256(bytes));
    field(evidence, sha256(metadata_bytes));
    field(evidence, answer.core_layout_version);
    field(evidence, answer.core_digest);
    field(evidence, answer.context_id);
    field(evidence, answer.context_digest);
    field(evidence, answer.technique_id);
    field(evidence, answer.technique_version);
    field(evidence, answer.technique_digest);
    field(evidence, sha256(payload_bytes));
    return output(status, metadata_bytes, evidence, payload_bytes, "nanocell-phase-v3");
}
PhaseOutputRefV3 execute_gate_adapter(std::string_view bytes, std::span<const PhaseInputV3> inputs) {
    unpack_payload_gate_request_v1(bytes);
    if (inputs.size() > max_inputs) throw Error("InvalidPayloadGateInputCount");
    std::vector<const PhaseInputV3*> ordered;
    for (const auto& input : inputs) {
        if (!identifier(input.producer_node_id) || !identifier(input.producer_adapter_id) ||
            !identifier(input.producer_adapter_version) || !digest(input.producer_output_digest) ||
            !digest(input.producer_evidence_digest) || !digest(input.producer_payload_digest) ||
            input.producer_payload_bytes.size() > 65536 ||
            (input.producer_status != "COMPLETE" && input.producer_status != "INCOMPLETE" && input.producer_status != "REJECTED"))
            throw Error("InvalidPayloadGateInput");
        ordered.push_back(&input);
    }
    std::sort(ordered.begin(), ordered.end(), [](const auto* left, const auto* right) {
        return left->producer_node_id < right->producer_node_id;
    });
    PayloadGatePayloadV1 manifest;
    manifest.decision = inputs.empty() ? "REJECTED" : "COMPLETE";
    manifest.input_count = inputs.size();
    std::string evidence;
    field(evidence, "payload-gate-evidence-v1");
    number(evidence, inputs.size());
    std::string_view previous;
    for (const auto* input : ordered) {
        if (input->producer_node_id == previous) throw Error("DuplicatePayloadGateInput");
        previous = input->producer_node_id;
        if (input->producer_status != "COMPLETE" || input->producer_payload_bytes.empty() ||
            input->producer_payload_digest != sha256(input->producer_payload_bytes)) manifest.decision = "REJECTED";
        manifest.input_payload_digests.push_back(input->producer_payload_digest);
        field(evidence, input->producer_node_id);
        field(evidence, input->producer_adapter_id);
        field(evidence, input->producer_adapter_version);
        field(evidence, input->producer_status);
        field(evidence, input->producer_output_digest);
        field(evidence, input->producer_evidence_digest);
        field(evidence, input->producer_payload_digest);
    }
    PayloadGateMetadataV1 metadata;
    metadata.decision = manifest.decision;
    metadata.input_count = manifest.input_count;
    return output(manifest.decision == "COMPLETE" ? PhaseOutputStatusV3::complete : PhaseOutputStatusV3::rejected,
        pack_payload_gate_metadata_v1(metadata), evidence, pack_payload_gate_payload_v1(manifest), "payload-gate-v1");
}
} // namespace

std::string pack_nanocell_phase_payload_v1(const NanoCellPhasePayloadV1& value) {
    validate_nanocell_payload(value);
    std::string bytes;
    field(bytes, value.payload_version);
    field(bytes, value.nanocell_status);
    number(bytes, value.final_state.has_value() ? 1 : 0);
    if (value.final_state) field(bytes, std::string(1, static_cast<char>(*value.final_state)));
    return bytes;
}
NanoCellPhasePayloadV1 unpack_nanocell_phase_payload_v1(std::string_view bytes) {
    Fields fields(bytes);
    NanoCellPhasePayloadV1 value;
    value.payload_version = fields.field();
    value.nanocell_status = fields.field();
    if (fields.number(1)) {
        const auto state = fields.field();
        if (state.size() != 1) throw Error("InvalidNanoCellPhaseFinalState");
        value.final_state = static_cast<std::uint8_t>(static_cast<unsigned char>(state[0]));
    }
    fields.finish();
    validate_nanocell_payload(value);
    if (pack_nanocell_phase_payload_v1(value) != bytes) throw Error("NoncanonicalNanoCellPhasePayload");
    return value;
}
std::string pack_payload_gate_request_v1() {
    std::string bytes;
    field(bytes, "payload-gate-request-v1");
    return bytes;
}
void unpack_payload_gate_request_v1(std::string_view bytes) {
    Fields fields(bytes);
    if (fields.field() != "payload-gate-request-v1") throw Error("InvalidPayloadGateRequestVersion");
    fields.finish();
    if (pack_payload_gate_request_v1() != bytes) throw Error("NoncanonicalPayloadGateRequest");
}
std::string pack_payload_gate_metadata_v1(const PayloadGateMetadataV1& value) {
    validate_decision(value.decision, value.input_count);
    if (value.metadata_version != "payload-gate-output-v1") throw Error("InvalidPayloadGateMetadataVersion");
    std::string bytes;
    field(bytes, value.metadata_version);
    field(bytes, value.decision);
    number(bytes, value.input_count);
    return bytes;
}
PayloadGateMetadataV1 unpack_payload_gate_metadata_v1(std::string_view bytes) {
    Fields fields(bytes);
    PayloadGateMetadataV1 value;
    value.metadata_version = fields.field();
    value.decision = fields.field();
    value.input_count = fields.number(max_inputs);
    fields.finish();
    if (pack_payload_gate_metadata_v1(value) != bytes) throw Error("NoncanonicalPayloadGateMetadata");
    return value;
}
std::string pack_payload_gate_payload_v1(const PayloadGatePayloadV1& value) {
    validate_decision(value.decision, value.input_count);
    if (value.payload_version != "payload-gate-payload-v1" || value.input_payload_digests.size() != value.input_count ||
        !std::all_of(value.input_payload_digests.begin(), value.input_payload_digests.end(), digest))
        throw Error("InvalidPayloadGatePayload");
    std::string bytes;
    field(bytes, value.payload_version);
    field(bytes, value.decision);
    number(bytes, value.input_count);
    for (const auto& value_digest : value.input_payload_digests) field(bytes, value_digest);
    return bytes;
}
PayloadGatePayloadV1 unpack_payload_gate_payload_v1(std::string_view bytes) {
    Fields fields(bytes);
    PayloadGatePayloadV1 value;
    value.payload_version = fields.field();
    value.decision = fields.field();
    value.input_count = fields.number(max_inputs);
    for (std::uint64_t index = 0; index < value.input_count; ++index) value.input_payload_digests.push_back(fields.field());
    fields.finish();
    if (pack_payload_gate_payload_v1(value) != bytes) throw Error("NoncanonicalPayloadGatePayload");
    return value;
}
std::span<const PhaseAdapterV3> phase_dag_nanocell_catalog_v3() {
    static constexpr std::array<PhaseAdapterV3, 2> catalog{{
        {"nanocell-phase-v3", "v1", execute_nanocell_adapter},
        {"payload-gate-v1", "v1", execute_gate_adapter}
    }};
    return catalog;
}
} // namespace bruce
