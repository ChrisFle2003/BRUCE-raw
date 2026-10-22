#include "bruce/mod/logic_mod_artifact_dag_v5.hpp"
#include "logic_mod_artifact_dag_execution.hpp"
#include <chrono>
namespace bruce {
LogicModArtifactDagRunV5 run_logic_mod_artifact_dag_v5(const std::vector<LogicModDagNodeV2>& nodes,
    const std::filesystem::path& store, std::string_view spec, LogicModArtifactDagControlV5 control,
    const artifact_v4::HttpsTransportV4& transport) {
    if (!control.remote_restore) return run_logic_mod_artifact_dag_v4(nodes,store,spec,control);
    // Validate the explicit host policy before admitting any root workers.
    (void)artifact_v3::https_request_v3(*control.remote_restore,"/manifests/check.mirror",
        std::chrono::steady_clock::now()+std::chrono::seconds(30));
    return detail::run_logic_mod_artifact_dag_with_predecessor_v4(nodes,store,spec,control,
        [&](const LogicModPublishedOutputV1& expected) {
            auto local=artifact_v1::ArtifactStoreV1::open_existing(store);
            // fstatat(NO_FOLLOW) distinguishes absence from an unreadable,
            // corrupt, directory or dangling-symlink manifest. Existing entries
            // go through the engine's normal verification without networking.
            if (local.manifest_entry_exists(expected.manifest_id)) return;
            // A missing manifest must not turn an unsafe existing object into
            // permission to use the network. Only actual object absence may skip
            // the local digest and size verification.
            try {
                if (local.inspect(expected.sha256).id != artifact_v1::ArtifactIdV1{expected.sha256,expected.size_bytes})
                    throw Error("LogicModOutputReplayMismatch");
            } catch (const Error& error) {
                if (std::string_view(error.what())!="ArtifactObjectMissing") throw;
            }
            (void)artifact_v4::restore_from_remote_v4(expected.manifest_id,store,*control.remote_restore,transport);
            // The shared engine now checks the full expected manifest, codec,
            // digest, size and result bytes before publishing the FD-6 input.
        });
}
std::string logic_mod_artifact_dag_record_v5(const LogicModArtifactDagRunV5& run) { return logic_mod_artifact_dag_record_v4(run); }
LogicModArtifactDagRunV5 read_logic_mod_artifact_dag_record_v5(std::string_view bytes) { return read_logic_mod_artifact_dag_record_v4(bytes); }
void verify_logic_mod_artifact_dag_replay_v5(const std::vector<LogicModDagNodeV2>& nodes,
    const std::filesystem::path& store, std::string_view spec, const LogicModArtifactDagRunV5& run) {
    verify_logic_mod_artifact_dag_replay_v4(nodes,store,spec,run);
}
}
