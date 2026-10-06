#include "bruce/evidence/bundle.hpp"
#include "bruce/mod/mod_api.hpp"
#include "bruce/mods/nanocell/mod.hpp"
#include "bruce/mods/nanocell/nanocell.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

[[nodiscard]] bool rejects(const auto& action) {
    try {
        action();
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

bruce::NanoCellRequestV1 masked_match_request(const std::uint8_t active = 0xA5) {
    bruce::NanoCellRequestV1 request;
    request.core.core_bytes = {0x10, 0x20, active, 0x40, 0x50, 0x60, 0x70, 0x80};
    request.core.active_index = 2;
    request.context.context_id = "reference";
    request.context.context_bytes = "nanocell-reference-context-v1";
    request.context.context_digest = bruce::sha256(request.context.context_bytes);
    request.technique = bruce::nanocell_reference_technique_v1(
        bruce::NanoCellReferenceTechniqueV1::masked_match);
    request.max_operations = 16;
    request.max_trace_entries = 18;
    return request;
}

} // namespace

int main() {
    const auto& manifest = bruce::nanocell_mod_manifest_v1();
    require(manifest.id == "nanocell" && manifest.version == "nanocell-mod-v1" &&
                manifest.api_version == bruce::mod_api_version_v1,
            "NanoCell Mod manifest identity");
    require(manifest.capabilities == std::vector{bruce::ModCapabilityV1::nanocell_execution},
            "NanoCell Mod capability");

    const auto request = masked_match_request();
    const auto direct = bruce::execute_nanocell_v1(request);
    const auto direct_bytes = bruce::pack_nanocell_run_v1(direct);
    require(bruce::pack_nanocell_run_v1(bruce::replay_nanocell_run_v1(direct_bytes)) == direct_bytes,
            "direct NanoCell V1 replay remains canonical");

    const auto mod_run = bruce::execute_nanocell_mod_v1(request);
    require(mod_run.nanocell.answer.status == bruce::NanoCellStatusV1::matched,
            "outer Mod run preserves MATCHED");
    const auto outer = bruce::pack_nanocell_mod_run_v1(mod_run);
    require(bruce::pack_nanocell_mod_run_v1(mod_run) == outer, "outer Mod run is deterministic");

    const auto envelope = bruce::unpack_mod_run_envelope(outer);
    require(envelope.manifest == manifest, "outer manifest exactly matches NanoCell Mod");
    require(envelope.input_codec == "bruce-nanocell-request-v1" &&
                envelope.payload_codec == "bruce-nanocell-run-v1",
            "outer codecs identify NanoCell request and run");
    require(envelope.input_digest == bruce::sha256(envelope.input_bytes) &&
                envelope.payload_digest == bruce::sha256(envelope.payload_bytes),
            "outer content digests bind both fields");
    require(envelope.payload_bytes == direct_bytes, "outer payload preserves direct V1 bytes");

    const auto replayed = bruce::replay_nanocell_mod_run_v1(outer);
    require(replayed.nanocell.answer.status == bruce::NanoCellStatusV1::matched,
            "outer replay preserves technical status");
    require(bruce::pack_nanocell_mod_run_v1(replayed) == outer,
            "outer replay remains canonical");
    require(rejects([&] { static_cast<void>(bruce::replay_nanocell_mod_run_v1(outer + "x")); }),
            "outer replay rejects trailing bytes");

    auto changed_codec = envelope;
    changed_codec.payload_codec = "other-nanocell-run-v1";
    require(rejects([&] {
                static_cast<void>(bruce::replay_nanocell_mod_run_v1(
                    bruce::pack_mod_run_envelope(changed_codec)));
            }),
            "outer replay rejects a valid but wrong payload codec");

    auto changed_manifest = envelope;
    changed_manifest.manifest.version = "nanocell-mod-v2";
    require(rejects([&] {
                static_cast<void>(bruce::replay_nanocell_mod_run_v1(
                    bruce::pack_mod_run_envelope(changed_manifest)));
            }),
            "outer replay rejects a rehashed manifest change");

    const auto alternate = bruce::execute_nanocell_mod_v1(masked_match_request(0xB5));
    const auto alternate_envelope = bruce::unpack_mod_run_envelope(
        bruce::pack_nanocell_mod_run_v1(alternate));

    auto changed_payload = envelope;
    changed_payload.payload_bytes = alternate_envelope.payload_bytes;
    changed_payload.payload_digest = bruce::sha256(changed_payload.payload_bytes);
    require(rejects([&] {
                static_cast<void>(bruce::replay_nanocell_mod_run_v1(
                    bruce::pack_mod_run_envelope(changed_payload)));
            }),
            "outer replay rejects a rehashed payload/request mismatch");

    auto changed_request = envelope;
    changed_request.input_bytes = alternate_envelope.input_bytes;
    changed_request.input_digest = bruce::sha256(changed_request.input_bytes);
    require(rejects([&] {
                static_cast<void>(bruce::replay_nanocell_mod_run_v1(
                    bruce::pack_mod_run_envelope(changed_request)));
            }),
            "outer replay rejects a rehashed request/payload mismatch");

    return 0;
}
