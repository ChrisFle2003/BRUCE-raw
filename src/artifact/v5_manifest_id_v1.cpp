#include "bruce/artifact/v5_manifest_id_v1.hpp"

#include "bruce/evidence/bundle.hpp"

namespace bruce::artifact_v1 {

std::string v5_manifest_id_v1(std::string_view run_id, std::string_view node_id,
                              std::string_view attempt_id) {
    const std::string binding = std::string("BRUCE_DAG_SCHEDULER_V5_PUBLISH_V1\nrun_id=") +
        std::string(run_id) + "\nnode_id=" + std::string(node_id) + "\nattempt_id=" +
        std::string(attempt_id) + "\n";
    return "v5-" + sha256(binding);
}

} // namespace bruce::artifact_v1
