#pragma once

#include "bruce/worker/worker_boundary_v1.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace bruce::worker_v1 {

struct WorkerAttemptInspectionV1 {
    std::string attempt_id;
    std::vector<std::string> statuses;
    bool accepted{};
    std::uint64_t event_count{};
    std::optional<std::string> digest;
    std::optional<std::uint64_t> size_bytes;
    // Internal, already verified source binding for Artifact Store import.
    // The public inspector renderer intentionally does not expose it.
    std::optional<std::string> artifact_reference;
    std::optional<WorkerUsage> usage;
    bool artifact_verified{};
};

struct WorkerRunInspectionV1 {
    std::string run_id;
    std::string status;
    bool run_finished{};
    std::uint64_t last_seq{};
    std::vector<WorkerAttemptInspectionV1> attempts;
};

[[nodiscard]] WorkerRunInspectionV1 inspect_worker_run_v1(
    const std::filesystem::path& run_root);

} // namespace bruce::worker_v1
