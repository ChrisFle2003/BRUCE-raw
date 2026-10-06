#include "bruce/application/execution_session.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/mod/mod_api.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

namespace {

using bruce::ApplicationTask;
using bruce::GridTask;
using bruce::ModCapabilityV1;
using bruce::ModCoreRunRequestV1;
using bruce::ModManifestV1;
using bruce::ModRunEnvelopeV1;

[[nodiscard]] ModManifestV1 language_manifest() {
    return ModManifestV1{
        .id = "language",
        .version = "language-mod-v1",
        .api_version = std::string(bruce::mod_api_version_v1),
        .capabilities = {ModCapabilityV1::language_interpretation,
                         ModCapabilityV1::core_task_submission},
    };
}

[[nodiscard]] bool rejects(const auto& action) {
    try {
        action();
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

void require(const bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    const auto manifest = language_manifest();
    const ModRunEnvelopeV1 envelope{
        .manifest = manifest,
        .input_codec = "BRUCE_LANGUAGE_DRAFT_V1",
        .input_bytes = "draft-bytes",
        .input_digest = bruce::sha256("draft-bytes"),
        .payload_codec = "BRUCE_LANGUAGE_RUN_V1",
        .payload_bytes = "run-bytes",
        .payload_digest = bruce::sha256("run-bytes"),
    };

    const auto packed = bruce::pack_mod_run_envelope(envelope);
    const auto unpacked = bruce::unpack_mod_run_envelope(packed);
    require(unpacked == envelope, "canonical mod envelope round-trips");
    bruce::require_matching_mod_manifest(unpacked.manifest, manifest);

    auto tampered = packed;
    tampered.back() = tampered.back() == 'x' ? 'y' : 'x';
    require(rejects([&] { static_cast<void>(bruce::unpack_mod_run_envelope(tampered)); }),
            "tampered envelope bytes are rejected");

    auto noncanonical = packed;
    constexpr std::string_view header = "BRUCE_MOD_RUN_V1\n";
    const auto body_offset = header.size() + 65;
    noncanonical.insert(body_offset, "0");
    const auto noncanonical_body = std::string_view(noncanonical).substr(body_offset);
    noncanonical.replace(header.size(), 64, bruce::sha256(noncanonical_body));
    require(rejects([&] { static_cast<void>(bruce::unpack_mod_run_envelope(noncanonical)); }),
            "a renewed digest cannot make a noncanonical field length valid");

    auto forged = envelope;
    forged.manifest.version = "language-mod-v2";
    const auto forged_bytes = bruce::pack_mod_run_envelope(forged);
    const auto forged_envelope = bruce::unpack_mod_run_envelope(forged_bytes);
    require(rejects([&] { bruce::require_matching_mod_manifest(forged_envelope.manifest, manifest); }),
            "a renewed digest cannot substitute a different manifest");

    auto invalid = manifest;
    invalid.capabilities.push_back(ModCapabilityV1::core_task_submission);
    require(rejects([&] { bruce::validate_mod_manifest(invalid); }),
            "duplicate capabilities are rejected");

    auto extended = manifest;
    extended.capabilities.push_back(ModCapabilityV1::presentation);
    extended.capabilities.push_back(ModCapabilityV1::phase_execution);
    bruce::validate_mod_manifest(extended);

    invalid = manifest;
    invalid.id = std::string(65, 'a');
    require(rejects([&] { bruce::validate_mod_manifest(invalid); }),
            "manifest identifiers longer than 64 bytes are rejected");

    auto bad_digest = envelope;
    bad_digest.input_digest = "not-the-input-digest";
    require(rejects([&] { static_cast<void>(bruce::pack_mod_run_envelope(bad_digest)); }),
            "mismatched input digests are rejected before packaging");

    GridTask grid;
    grid.path = "2,6";
    const bruce::SessionBudget session_budget{.max_checkpoints = 100};
    const auto direct = bruce::execute_checked_application(
        ApplicationTask{grid}, {}, {}, {}, session_budget, {});
    const auto through_mod = bruce::execute_mod_core_run(ModCoreRunRequestV1{
        .manifest = manifest,
        .task = ApplicationTask{grid},
        .control = {},
        .verification_budget = {},
        .verification = {},
        .session_budget = session_budget,
        .serialization_budget = {},
    });
    require(through_mod.bundle == direct.bundle,
            "mod core bridge preserves the checked application bundle");
    require(bruce::render_execution_session(through_mod) == bruce::render_execution_session(direct),
            "mod core bridge preserves the checked application session");

    invalid = manifest;
    invalid.id.clear();
    require(rejects([&] {
                static_cast<void>(bruce::execute_mod_core_run(
                    ModCoreRunRequestV1{
                        .manifest = invalid,
                        .task = ApplicationTask{grid},
                        .control = {},
                        .verification_budget = {},
                        .verification = {},
                        .session_budget = {},
                        .serialization_budget = {},
                    }));
            }),
            "invalid manifests cannot submit tasks to the core");

    return 0;
}
