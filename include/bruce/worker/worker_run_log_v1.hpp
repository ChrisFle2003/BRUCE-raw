#pragma once

#include "bruce/worker/worker_boundary_v1.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace bruce::worker_v1 {

struct WorkerLogEvent {
    std::string schema{"worker-run-log-v1"};
    std::optional<std::string> run_id;
    std::optional<std::string> attempt_id;
    std::uint64_t seq{};
    std::uint64_t ts_unix_ms{};
    std::string event;
    std::string status;
    std::optional<std::string> digest;
    std::optional<std::string> reference;
    std::optional<std::uint64_t> size_bytes;
    std::optional<WorkerUsage> usage;
};

class WorkerRunLog {
public:
    [[nodiscard]] static WorkerRunLog create(const std::filesystem::path& run_root,
                                             std::string run_id);
    void append(const WorkerLogEvent& event);

private:
    WorkerRunLog(std::filesystem::path run_root, std::string run_id,
                 std::filesystem::path log_path, std::uint64_t last_seq);
    std::filesystem::path run_root_;
    std::string run_id_;
    std::filesystem::path log_path_;
    std::uint64_t last_seq_{};
};

} // namespace bruce::worker_v1
