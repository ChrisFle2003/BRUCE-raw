#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bruce::worker_v1 {

enum class FrameType { REQUEST, RESULT, STATUS_QUERY, STATUS, CANCEL, ACK, EXIT };

struct Frame {
    FrameType type;
    std::string payload;
};

struct Budget {
    std::uint64_t max_wall_ms{};
    std::uint64_t max_input_bytes{};
    std::uint64_t max_output_bytes{};
    std::uint64_t max_operations{};
    std::uint64_t max_memory_bytes{};
};

struct ArtifactRef {
    std::string reference;
    std::uint64_t size_bytes{};
    std::string sha256;
};

struct WorkerRequest {
    std::string run_id;
    std::string attempt_id;
    std::string node_id;
    std::string adapter_id;
    std::string adapter_version;
    std::string request_digest;
    std::vector<ArtifactRef> input_artifact_refs;
    Budget budget;
};

enum class WorkerStatus { READY, FAILED, BLOCKED, CANCELLED };

struct WorkerUsage {
    std::uint64_t wall_ms{};
    std::uint64_t input_bytes{};
    std::uint64_t output_bytes{};
    std::uint64_t operations{};
    std::uint64_t peak_memory_bytes{};
};

enum class FailureCode {
    NONE,
    READY,
    FAILED,
    BLOCKED,
    INVALID_REQUEST,
    PROTOCOL_ERROR,
    PROCESS_START_FAILED,
    PROCESS_CRASHED,
    TIMEOUT,
    CANCELLED,
    INPUT_LIMIT_EXCEEDED,
    OUTPUT_LIMIT_EXCEEDED,
    OPERATION_BUDGET_EXCEEDED,
    MEMORY_LIMIT_EXCEEDED,
    DIGEST_MISMATCH
};

struct WorkerResult {
    WorkerStatus status{WorkerStatus::FAILED};
    std::string result_code;
    std::optional<ArtifactRef> output_artifact_ref;
    std::optional<std::string> payload_digest;
    std::uint64_t payload_size{};
    WorkerUsage usage;
    FailureCode failure_code{FailureCode::NONE};
};

[[nodiscard]] std::string encode_frame(FrameType type, std::string_view payload);
[[nodiscard]] Frame decode_frame(std::string_view bytes, std::size_t max_payload_bytes);

} // namespace bruce::worker_v1
