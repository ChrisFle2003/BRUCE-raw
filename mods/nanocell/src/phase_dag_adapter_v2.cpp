#include "bruce/phase_dag/nanocell_adapter_v2.hpp"
#include "bruce/phase_dag/evidence_gate_adapter_v2.hpp"

#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/mods/nanocell/nanocell.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {
namespace {

void append_field(std::string& bytes, std::string_view value) {
    bytes += std::to_string(value.size());
    bytes += ':';
    bytes.append(value);
}

void append_number(std::string& bytes, std::uint64_t value) {
    append_field(bytes, std::to_string(value));
}

class Fields {
public:
    explicit Fields(std::string_view bytes) : bytes_(bytes) {
        if (bytes.size() > 65536) throw Error("NanoCellPhaseCodecTooLarge");
    }

    std::string field() {
        if (offset_ == bytes_.size()) throw Error("NanoCellPhaseFieldMissing");
        const auto first = offset_;
        std::size_t length = 0;
        while (offset_ < bytes_.size() && bytes_[offset_] != ':') {
            const char c = bytes_[offset_++];
            if (c < '0' || c > '9') throw Error("InvalidNanoCellPhaseLength");
            const auto digit = static_cast<std::size_t>(c - '0');
            if (length > (std::numeric_limits<std::size_t>::max() - digit) / 10)
                throw Error("InvalidNanoCellPhaseLength");
            length = length * 10 + digit;
        }
        if (offset_ == first || offset_ == bytes_.size() ||
            (offset_ - first > 1 && bytes_[first] == '0'))
            throw Error("InvalidNanoCellPhaseLength");
        ++offset_;
        if (length > bytes_.size() - offset_) throw Error("NanoCellPhaseFieldTruncated");
        const auto result = std::string(bytes_.substr(offset_, length));
        offset_ += length;
        return result;
    }

    std::uint64_t number(std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max()) {
        const auto value = field();
        if (value.empty() || (value.size() > 1 && value.front() == '0'))
            throw Error("InvalidNanoCellPhaseNumber");
        std::uint64_t result{};
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
        if (error != std::errc{} || end != value.data() + value.size() || result > maximum)
            throw Error("InvalidNanoCellPhaseNumber");
        return result;
    }

    void finish() const {
        if (offset_ != bytes_.size()) throw Error("NanoCellPhaseTrailingBytes");
    }

private:
    std::string_view bytes_;
    std::size_t offset_{};
};

NanoCellReferenceTechniqueV1 reference_technique(std::string_view id) {
    if (id == "masked-match-v1") return NanoCellReferenceTechniqueV1::masked_match;
    if (id == "rotate-match-v1") return NanoCellReferenceTechniqueV1::rotate_match;
    if (id == "context-conflict-v1") return NanoCellReferenceTechniqueV1::context_conflict;
    throw Error("UnknownNanoCellPhaseTechnique");
}

std::string_view status_name(NanoCellStatusV1 status) {
    switch (status) {
    case NanoCellStatusV1::matched: return "MATCHED";
    case NanoCellStatusV1::no_match: return "NO_MATCH";
    case NanoCellStatusV1::context_rejected: return "CONTEXT_REJECTED";
    case NanoCellStatusV1::incomplete: return "INCOMPLETE";
    }
    throw Error("InvalidNanoCellPhaseStatus");
}

PhaseOutputStatusV2 phase_status(NanoCellStatusV1 status) {
    switch (status) {
    case NanoCellStatusV1::matched:
    case NanoCellStatusV1::no_match: return PhaseOutputStatusV2::complete;
    case NanoCellStatusV1::incomplete: return PhaseOutputStatusV2::incomplete;
    case NanoCellStatusV1::context_rejected: return PhaseOutputStatusV2::rejected;
    }
    throw Error("InvalidNanoCellPhaseStatus");
}

bool digest(std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
    });
}

std::string pack_evidence_gate_evidence_v1(std::span<const PhaseInputRefV2> inputs) {
    std::vector<const PhaseInputRefV2*> ordered;
    ordered.reserve(inputs.size());
    for (const auto& input : inputs) ordered.push_back(&input);
    std::sort(ordered.begin(), ordered.end(), [](const auto* left, const auto* right) {
        return left->producer_node_id < right->producer_node_id;
    });
    std::string bytes;
    append_field(bytes, "evidence-gate-evidence-v1");
    append_number(bytes, ordered.size());
    for (const auto* input : ordered) {
        append_field(bytes, input->producer_node_id);
        append_field(bytes, input->producer_adapter_id);
        append_field(bytes, input->producer_adapter_version);
        append_field(bytes, input->producer_status);
        append_field(bytes, input->producer_output_digest);
        append_field(bytes, input->producer_evidence_digest);
    }
    return bytes;
}

PhaseOutputRefV2 execute_evidence_gate_adapter(std::string_view bytes,
    std::span<const PhaseInputRefV2> inputs) {
    unpack_evidence_gate_request_v1(bytes);
    EvidenceGateMetadataV1 metadata;
    metadata.input_count = inputs.size();
    metadata.decision = "COMPLETE";
    if (inputs.empty()) metadata.decision = "REJECTED";
    for (const auto& input : inputs) {
        if (input.producer_node_id.empty() || input.producer_adapter_id.empty() ||
            input.producer_adapter_version.empty() || !digest(input.producer_output_digest) ||
            !digest(input.producer_evidence_digest))
            throw Error("InvalidEvidenceGateInput");
        if (input.producer_status == "COMPLETE") ++metadata.complete_count;
        else if (input.producer_status == "INCOMPLETE") {
            ++metadata.incomplete_count;
            metadata.decision = "REJECTED";
        } else if (input.producer_status == "REJECTED") {
            ++metadata.rejected_count;
            metadata.decision = "REJECTED";
        } else {
            throw Error("InvalidEvidenceGateInput");
        }
    }
    const auto output_bytes = pack_evidence_gate_metadata_v1(metadata);
    const auto evidence_bytes = pack_evidence_gate_evidence_v1(inputs);
    return {"phase-output-v2", metadata.decision == "COMPLETE" ? PhaseOutputStatusV2::complete :
        PhaseOutputStatusV2::rejected, output_bytes, sha256(output_bytes), evidence_bytes,
        sha256(evidence_bytes), "evidence-gate-v1"};
}

PhaseOutputRefV2 execute_adapter(std::string_view bytes, std::span<const PhaseInputRefV2>) {
    const auto phase_request = unpack_nanocell_phase_request_v1(bytes);
    NanoCellRequestV1 request;
    request.core.core_layout_version = phase_request.core_layout_version;
    request.core.core_bytes = phase_request.core_bytes;
    request.core.active_index = phase_request.active_index;
    request.context.context_id = phase_request.context_id;
    request.context.context_bytes = phase_request.context_bytes;
    request.context.context_digest = phase_request.context_digest;
    request.technique = nanocell_reference_technique_v1(reference_technique(phase_request.technique_id));
    request.max_operations = phase_request.max_operations;
    request.max_trace_entries = phase_request.max_trace_entries;
    const auto run = execute_nanocell_mod_v1(request);
    const auto& answer = run.nanocell.answer;
    NanoCellPhaseMetadataV1 metadata;
    metadata.nanocell_status = status_name(answer.status);
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
    const auto output_bytes = pack_nanocell_phase_metadata_v1(metadata);
    const auto evidence_bytes = pack_nanocell_mod_run_v1(run);
    return {"phase-output-v2", phase_status(answer.status), output_bytes, sha256(output_bytes),
        evidence_bytes, sha256(evidence_bytes), "nanocell-phase-v1"};
}

} // namespace

std::string pack_nanocell_phase_request_v1(const NanoCellPhaseRequestV1& value) {
    std::string bytes;
    append_field(bytes, "nanocell-phase-request-v1");
    append_field(bytes, value.core_layout_version);
    append_field(bytes, std::string_view(reinterpret_cast<const char*>(value.core_bytes.data()), value.core_bytes.size()));
    append_number(bytes, value.active_index);
    append_field(bytes, value.context_id);
    append_field(bytes, value.context_bytes);
    append_field(bytes, value.context_digest);
    append_field(bytes, value.technique_id);
    append_number(bytes, value.max_operations);
    append_number(bytes, value.max_trace_entries);
    return bytes;
}

std::string pack_evidence_gate_request_v1() {
    std::string bytes;
    append_field(bytes, "evidence-gate-request-v1");
    return bytes;
}

void unpack_evidence_gate_request_v1(std::string_view bytes) {
    Fields fields(bytes);
    if (fields.field() != "evidence-gate-request-v1") throw Error("InvalidEvidenceGateRequestVersion");
    fields.finish();
}

std::string pack_evidence_gate_metadata_v1(const EvidenceGateMetadataV1& value) {
    std::string bytes;
    append_field(bytes, value.metadata_version);
    append_field(bytes, value.decision);
    append_number(bytes, value.input_count);
    append_number(bytes, value.complete_count);
    append_number(bytes, value.incomplete_count);
    append_number(bytes, value.rejected_count);
    return bytes;
}

EvidenceGateMetadataV1 unpack_evidence_gate_metadata_v1(std::string_view bytes) {
    Fields fields(bytes);
    EvidenceGateMetadataV1 value;
    value.metadata_version = fields.field();
    value.decision = fields.field();
    value.input_count = fields.number();
    value.complete_count = fields.number();
    value.incomplete_count = fields.number();
    value.rejected_count = fields.number();
    fields.finish();
    if (value.metadata_version != "evidence-gate-output-v1" ||
        (value.decision != "COMPLETE" && value.decision != "REJECTED") ||
        value.input_count != value.complete_count + value.incomplete_count + value.rejected_count ||
        (value.decision == "COMPLETE" && (value.input_count == 0 || value.incomplete_count != 0 ||
            value.rejected_count != 0)) ||
        (value.decision == "REJECTED" && value.input_count != 0 && value.incomplete_count == 0 &&
            value.rejected_count == 0))
        throw Error("InvalidEvidenceGateMetadata");
    if (pack_evidence_gate_metadata_v1(value) != bytes) throw Error("NoncanonicalEvidenceGateMetadata");
    return value;
}

NanoCellPhaseRequestV1 unpack_nanocell_phase_request_v1(std::string_view bytes) {
    Fields fields(bytes);
    if (fields.field() != "nanocell-phase-request-v1") throw Error("InvalidNanoCellPhaseRequestVersion");
    NanoCellPhaseRequestV1 value;
    value.core_layout_version = fields.field();
    const auto core = fields.field();
    if (core.size() != value.core_bytes.size()) throw Error("InvalidNanoCellPhaseCoreSize");
    std::transform(core.begin(), core.end(), value.core_bytes.begin(),
        [](unsigned char byte) { return static_cast<std::uint8_t>(byte); });
    value.active_index = static_cast<std::uint8_t>(fields.number(7));
    value.context_id = fields.field();
    if (value.context_id.empty() || value.context_id.size() > 64 ||
        !std::all_of(value.context_id.begin(), value.context_id.end(), [](unsigned char ch) {
            return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                (ch >= '0' && ch <= '9') || ch == '_' || ch == '.' || ch == '-';
        })) throw Error("InvalidNanoCellPhaseContextId");
    value.context_bytes = fields.field();
    value.context_digest = fields.field();
    value.technique_id = fields.field();
    value.max_operations = fields.number(16);
    value.max_trace_entries = fields.number(18);
    fields.finish();
    if (value.max_trace_entries < 2 || value.context_bytes.size() > 4096 ||
        value.context_digest != sha256(value.context_bytes)) throw Error("InvalidNanoCellPhaseRequest");
    (void)reference_technique(value.technique_id);
    if (value.core_layout_version != "nanoneuron-core-v1") throw Error("InvalidNanoCellPhaseCoreLayout");
    if (pack_nanocell_phase_request_v1(value) != bytes) throw Error("NoncanonicalNanoCellPhaseRequest");
    return value;
}

std::string pack_nanocell_phase_metadata_v1(const NanoCellPhaseMetadataV1& value) {
    std::string bytes;
    append_field(bytes, value.metadata_version);
    append_field(bytes, value.nanocell_status);
    append_field(bytes, value.technique_id);
    append_field(bytes, value.technique_version);
    append_field(bytes, value.core_layout_version);
    append_number(bytes, value.active_index);
    append_field(bytes, value.core_digest);
    append_field(bytes, value.context_id);
    append_field(bytes, value.context_digest);
    append_field(bytes, value.technique_digest);
    append_number(bytes, value.operations_used);
    append_number(bytes, value.trace_entries);
    return bytes;
}

NanoCellPhaseMetadataV1 unpack_nanocell_phase_metadata_v1(std::string_view bytes) {
    Fields fields(bytes);
    NanoCellPhaseMetadataV1 value;
    value.metadata_version = fields.field();
    value.nanocell_status = fields.field();
    value.technique_id = fields.field();
    value.technique_version = fields.field();
    value.core_layout_version = fields.field();
    value.active_index = static_cast<std::uint8_t>(fields.number(7));
    value.core_digest = fields.field();
    value.context_id = fields.field();
    value.context_digest = fields.field();
    value.technique_digest = fields.field();
    value.operations_used = fields.number(16);
    value.trace_entries = fields.number(18);
    fields.finish();
    if (value.metadata_version != "nanocell-phase-output-v1" ||
        (value.nanocell_status != "MATCHED" && value.nanocell_status != "NO_MATCH" &&
         value.nanocell_status != "INCOMPLETE" && value.nanocell_status != "CONTEXT_REJECTED") ||
        value.technique_version != "nanocell-technique-v1" ||
        value.core_layout_version != "nanoneuron-core-v1" ||
        value.core_digest.size() != 64 || value.context_digest.size() != 64 ||
        value.technique_digest.size() != 64 || value.trace_entries < 2)
        throw Error("InvalidNanoCellPhaseMetadata");
    (void)reference_technique(value.technique_id);
    if (pack_nanocell_phase_metadata_v1(value) != bytes) throw Error("NoncanonicalNanoCellPhaseMetadata");
    return value;
}

std::span<const PhaseAdapterV2> phase_dag_nanocell_catalog_v2() {
    static constexpr std::array<PhaseAdapterV2, 2> catalog{{
        {"evidence-gate-v1", "v1", execute_evidence_gate_adapter},
        {"nanocell-phase-v1", "v1", execute_adapter},
    }};
    return catalog;
}

} // namespace bruce
