#pragma once

#include "bruce/mod/logic_mod_artifact_dag_v6.hpp"

#include <filesystem>
#include <string>

namespace bruce {
// Bounded descriptor-relative no-follow reader for the V6 canonical record.
// It neither opens a report/store/spec nor invokes replay or workers.
[[nodiscard]] LogicModArtifactDagRunV6 read_controller_observability_v6_record_v1(
    const std::filesystem::path& record);
[[nodiscard]] std::string render_controller_observability_error_v1();
} // namespace bruce
