#pragma once

#include "bruce/application/execution_session.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace bruce {

inline constexpr std::string_view mod_api_version_v1 = "bruce-mod-api-v1";

enum class ModCapabilityV1 {
    language_interpretation,
    core_task_submission,
    presentation,
    phase_execution,
    nanocell_execution,
};

struct ModManifestV1 {
    std::string id;
    std::string version;
    std::string api_version;
    std::vector<ModCapabilityV1> capabilities;

    bool operator==(const ModManifestV1&) const = default;
};

struct ModRunEnvelopeV1 {
    ModManifestV1 manifest;
    std::string input_codec;
    std::string input_bytes;
    std::string input_digest;
    std::string payload_codec;
    std::string payload_bytes;
    std::string payload_digest;

    bool operator==(const ModRunEnvelopeV1&) const = default;
};

struct ModCoreRunRequestV1 {
    ModManifestV1 manifest;
    ApplicationTask task;
    ExecutionControl control;
    VerificationBudget verification_budget;
    VerificationControl verification;
    SessionBudget session_budget;
    SerializationBudget serialization_budget;
};

void validate_mod_manifest(const ModManifestV1& manifest);
void require_matching_mod_manifest(const ModManifestV1& actual, const ModManifestV1& expected);
std::string pack_mod_run_envelope(const ModRunEnvelopeV1& envelope);
ModRunEnvelopeV1 unpack_mod_run_envelope(std::string_view bytes);
ExecutionSession execute_mod_core_run(const ModCoreRunRequestV1& request);

} // namespace bruce
