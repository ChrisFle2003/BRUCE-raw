#include "bruce/mods/nanocell/mod.hpp"
#include "bruce/mods/nanocell/nanocell.hpp"
#include "nanocell_wire.hpp"

#include "bruce/evidence/bundle.hpp"

namespace bruce {
namespace {

constexpr std::string_view input_codec = "bruce-nanocell-request-v1";
constexpr std::string_view payload_codec = "bruce-nanocell-run-v1";

void require_nanocell_envelope(const ModRunEnvelopeV1& envelope) {
    require_matching_mod_manifest(envelope.manifest, nanocell_mod_manifest_v1());
    if (envelope.input_codec != input_codec || envelope.payload_codec != payload_codec) {
        throw Error("InvalidNanoCellModCodec");
    }
}

} // namespace

const ModManifestV1& nanocell_mod_manifest_v1() {
    static const ModManifestV1 manifest{
        .id = "nanocell",
        .version = "nanocell-mod-v1",
        .api_version = std::string(mod_api_version_v1),
        .capabilities = {ModCapabilityV1::nanocell_execution},
    };
    return manifest;
}

NanoCellModRunV1 execute_nanocell_mod_v1(const NanoCellRequestV1& request,
                                          const NanoCellControlV1& control) {
    return {execute_nanocell_v1(request, control)};
}

std::string pack_nanocell_mod_run_v1(const NanoCellModRunV1& run) {
    const auto input = nanocell_detail::pack_nanocell_request_v1(run.nanocell.request);
    const auto payload = pack_nanocell_run_v1(run.nanocell);
    return pack_mod_run_envelope({
        .manifest = nanocell_mod_manifest_v1(),
        .input_codec = std::string(input_codec),
        .input_bytes = input,
        .input_digest = sha256(input),
        .payload_codec = std::string(payload_codec),
        .payload_bytes = payload,
        .payload_digest = sha256(payload),
    });
}

NanoCellModRunV1 replay_nanocell_mod_run_v1(const std::string_view bundle) {
    const auto envelope = unpack_mod_run_envelope(bundle);
    require_nanocell_envelope(envelope);
    const auto request = nanocell_detail::unpack_nanocell_request_v1(envelope.input_bytes);
    const auto run = replay_nanocell_run_v1(envelope.payload_bytes);
    if (nanocell_detail::pack_nanocell_request_v1(run.request) != envelope.input_bytes ||
        nanocell_detail::pack_nanocell_request_v1(request) != envelope.input_bytes) {
        throw Error("NanoCellModRequestRunMismatch");
    }
    NanoCellModRunV1 result{run};
    if (pack_nanocell_mod_run_v1(result) != bundle) throw Error("NoncanonicalNanoCellModRun");
    return result;
}

} // namespace bruce
