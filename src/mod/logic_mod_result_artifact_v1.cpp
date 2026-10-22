#include "bruce/mod/logic_mod_result_artifact_v1.hpp"
#include "bruce/evidence/bundle.hpp"

namespace bruce {
namespace {
constexpr std::string_view result_codec = "logic-mod-result-text-v1";
constexpr std::string_view result_origin = "logic-mod-result-v1";
std::string field(std::string_view value) {
    return std::to_string(value.size()) + ':' + std::string(value);
}
artifact_v1::ArtifactManifestV1 manifest_for(std::string_view manifest_id,
    const artifact_v1::ArtifactIdV1& object) {
    const std::string id(manifest_id);
    return {id, id, id, "READY", "ACCEPTED", {{object, std::string(result_codec), std::string(result_origin)}}};
}
[[noreturn]] void mismatch() { throw Error("LogicModOutputReplayMismatch"); }
}

std::string LogicModPublishedOutputV1::canonical_identity() const {
    return "BRUCE_LOGIC_RESULT_ARTIFACT_V1\n" + field(manifest_id) + field(std::to_string(object_index)) +
        field(sha256) + field(std::to_string(size_bytes)) + field(codec) + '\n';
}

PreparedLogicModOutputV1 prepare_logic_mod_output_v1(const LogicModOutputV1& output) {
    if (!artifact_v1::is_valid_manifest_id_v1(output.manifest_id)) throw Error("ArtifactManifestIdInvalid");
    PreparedLogicModOutputV1 prepared;
    prepared.manifest_id_ = output.manifest_id;
    prepared.store_ = artifact_v1::ArtifactStoreV1::open(output.store);
    prepared.store_.validate_manifest_target(output.manifest_id);
    return prepared;
}

LogicModPublishedOutputV1 publish_logic_mod_result_artifact_v1(PreparedLogicModOutputV1& prepared,
    const LogicModBindingV1& binding, std::string_view result) {
    if (binding.capability() != "artifact.output.v1") throw Error("LogicModOutputCapabilityDenied");
    if (result.size() > logic_mod_result_limit_v1) throw Error("LogicModOutputLimitExceeded");
    const auto object = prepared.store_.put_bytes(result);
    prepared.store_.publish_manifest(prepared.manifest_id_, manifest_for(prepared.manifest_id_, object));
    return {prepared.manifest_id_, 0, object.sha256, object.size_bytes, std::string(result_codec)};
}

void verify_logic_mod_result_artifact_v1(const LogicModOutputV1& output,
    const LogicModPublishedOutputV1& recorded, std::string_view expected_result) {
    if (!artifact_v1::is_valid_manifest_id_v1(output.manifest_id) || output.manifest_id != recorded.manifest_id ||
        recorded.object_index != 0 || recorded.codec != result_codec || expected_result.size() > logic_mod_result_limit_v1 ||
        recorded.size_bytes != expected_result.size() || recorded.sha256 != sha256(expected_result)) mismatch();
    auto store = artifact_v1::ArtifactStoreV1::open_existing(output.store);
    const auto resolved = store.resolve_manifest(output.manifest_id);
    const artifact_v1::ArtifactIdV1 object{recorded.sha256, recorded.size_bytes};
    if (resolved.manifest_id != recorded.manifest_id || resolved.run_id != recorded.manifest_id ||
        resolved.node_id != recorded.manifest_id || resolved.attempt_id != recorded.manifest_id ||
        resolved.manifest_status != "READY" || resolved.result_code != "ACCEPTED" || resolved.objects.size() != 1 ||
        resolved.objects[0].id != object || resolved.objects[0].origin != result_origin) mismatch();
    // resolve_manifest deliberately exposes no media_type. Read the canonical
    // manifest again to pin its codec and the full fixed publication contract.
    if (store.read_manifest(output.manifest_id) != manifest_for(recorded.manifest_id, object)) mismatch();
    store.verify(object);
}
}
