#include "bruce/mod/mod_api.hpp"

#include "bruce/evidence/bundle.hpp"

#include <cctype>
#include <cstdint>
#include <limits>

namespace bruce {
namespace {

constexpr std::string_view header = "BRUCE_MOD_RUN_V1\n";
constexpr std::size_t max_envelope_bytes = 128 * 1024 * 1024;
constexpr std::size_t max_manifest_capabilities = 16;

[[nodiscard]] bool valid_token(const std::string_view value) {
    if (value.empty() || value.size() > 64) return false;
    for (const unsigned char ch : value) {
        if (!std::isalnum(ch) && ch != '-' && ch != '_' && ch != '.') return false;
    }
    return true;
}

[[nodiscard]] unsigned capability_value(const ModCapabilityV1 capability) {
    switch (capability) {
        case ModCapabilityV1::language_interpretation: return 0;
        case ModCapabilityV1::core_task_submission: return 1;
        case ModCapabilityV1::presentation: return 2;
        case ModCapabilityV1::phase_execution: return 3;
        case ModCapabilityV1::nanocell_execution: return 4;
    }
    throw Error("InvalidModCapability");
}

[[nodiscard]] ModCapabilityV1 parse_capability(const std::uint64_t value) {
    switch (value) {
        case 0: return ModCapabilityV1::language_interpretation;
        case 1: return ModCapabilityV1::core_task_submission;
        case 2: return ModCapabilityV1::presentation;
        case 3: return ModCapabilityV1::phase_execution;
        case 4: return ModCapabilityV1::nanocell_execution;
        default: throw Error("InvalidModCapability");
    }
}

void put_field(std::string& out, const std::string_view value) {
    out += std::to_string(value.size());
    out += ':';
    out.append(value);
}

void put_number(std::string& out, const std::uint64_t value) {
    put_field(out, std::to_string(value));
}

[[nodiscard]] std::string_view read_field(const std::string_view body, std::size_t& offset) {
    const auto start = offset;
    while (offset < body.size() && body[offset] >= '0' && body[offset] <= '9') ++offset;
    if (offset == start || offset >= body.size() || body[offset] != ':') throw Error("InvalidModRunField");
    const auto digits = body.substr(start, offset - start);
    if (digits.size() > 1 && digits.front() == '0') throw Error("NoncanonicalModRun");
    const auto count = integer<std::size_t>(digits);
    ++offset;
    if (count > body.size() - offset) throw Error("InvalidModRunField");
    const auto value = body.substr(offset, count);
    offset += count;
    return value;
}

[[nodiscard]] std::uint64_t read_number(const std::string_view body, std::size_t& offset) {
    const auto field = read_field(body, offset);
    if (field.empty() || (field.size() > 1 && field.front() == '0')) throw Error("NoncanonicalModRun");
    return integer<std::uint64_t>(field);
}

void validate_envelope(const ModRunEnvelopeV1& envelope) {
    validate_mod_manifest(envelope.manifest);
    if (!valid_token(envelope.input_codec) || !valid_token(envelope.payload_codec)) {
        throw Error("InvalidModRunCodec");
    }
    if (envelope.input_digest != sha256(envelope.input_bytes) ||
        envelope.payload_digest != sha256(envelope.payload_bytes)) {
        throw Error("ModRunContentDigestMismatch");
    }
    if (envelope.input_bytes.size() > max_envelope_bytes ||
        envelope.payload_bytes.size() > max_envelope_bytes) {
        throw Error("ModRunByteLimit");
    }
}

} // namespace

void validate_mod_manifest(const ModManifestV1& manifest) {
    if (!valid_token(manifest.id) || !valid_token(manifest.version) ||
        manifest.api_version != mod_api_version_v1) {
        throw Error("InvalidModManifest");
    }
    if (manifest.capabilities.empty() || manifest.capabilities.size() > max_manifest_capabilities) {
        throw Error("InvalidModCapabilities");
    }
    unsigned previous = 0;
    bool first = true;
    for (const auto capability : manifest.capabilities) {
        const auto current = capability_value(capability);
        if (!first && current <= previous) throw Error("NoncanonicalModCapabilities");
        previous = current;
        first = false;
    }
}

void require_matching_mod_manifest(const ModManifestV1& actual, const ModManifestV1& expected) {
    validate_mod_manifest(actual);
    validate_mod_manifest(expected);
    if (actual != expected) throw Error("ModManifestMismatch");
}

std::string pack_mod_run_envelope(const ModRunEnvelopeV1& envelope) {
    validate_envelope(envelope);
    std::string body;
    put_field(body, envelope.manifest.id);
    put_field(body, envelope.manifest.version);
    put_field(body, envelope.manifest.api_version);
    put_number(body, envelope.manifest.capabilities.size());
    for (const auto capability : envelope.manifest.capabilities) {
        put_number(body, capability_value(capability));
    }
    put_field(body, envelope.input_codec);
    put_field(body, envelope.input_bytes);
    put_field(body, envelope.input_digest);
    put_field(body, envelope.payload_codec);
    put_field(body, envelope.payload_bytes);
    put_field(body, envelope.payload_digest);
    if (body.size() > max_envelope_bytes) throw Error("ModRunByteLimit");
    return std::string(header) + sha256(body) + "\n" + body;
}

ModRunEnvelopeV1 unpack_mod_run_envelope(const std::string_view bytes) {
    if (bytes.size() > max_envelope_bytes + header.size() + 65) throw Error("ModRunByteLimit");
    if (!bytes.starts_with(header) || bytes.size() < header.size() + 65 || bytes[header.size() + 64] != '\n') {
        throw Error("InvalidModRunHeader");
    }
    const auto digest = bytes.substr(header.size(), 64);
    const auto body = bytes.substr(header.size() + 65);
    if (digest != sha256(body)) throw Error("ModRunDigestMismatch");

    std::size_t offset = 0;
    ModRunEnvelopeV1 envelope;
    envelope.manifest.id = read_field(body, offset);
    envelope.manifest.version = read_field(body, offset);
    envelope.manifest.api_version = read_field(body, offset);
    const auto capability_count = read_number(body, offset);
    if (capability_count == 0 || capability_count > max_manifest_capabilities) {
        throw Error("InvalidModCapabilities");
    }
    for (std::uint64_t i = 0; i < capability_count; ++i) {
        envelope.manifest.capabilities.push_back(parse_capability(read_number(body, offset)));
    }
    envelope.input_codec = read_field(body, offset);
    envelope.input_bytes = read_field(body, offset);
    envelope.input_digest = read_field(body, offset);
    envelope.payload_codec = read_field(body, offset);
    envelope.payload_bytes = read_field(body, offset);
    envelope.payload_digest = read_field(body, offset);
    if (offset != body.size() || pack_mod_run_envelope(envelope) != bytes) {
        throw Error("NoncanonicalModRun");
    }
    return envelope;
}

ExecutionSession execute_mod_core_run(const ModCoreRunRequestV1& request) {
    validate_mod_manifest(request.manifest);
    return execute_checked_application(request.task, request.control, request.verification_budget,
                                       request.verification, request.session_budget,
                                       request.serialization_budget);
}

} // namespace bruce
