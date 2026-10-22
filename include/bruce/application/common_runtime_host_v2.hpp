#pragma once

#include "bruce/worker/worker_boundary_v1.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

namespace bruce {

struct CommonRuntimeHostV2Request {
    worker_v1::WorkerRequest worker_request;
};

struct CommonRuntimeHostV2Context {
    std::filesystem::path run_root;
    std::filesystem::path worker_executable;
    worker_v1::Budget budget;
    std::uint64_t grace_ms{1000};
};

struct CommonRuntimeHostV2Outcome {
    worker_v1::WorkerResult result;
    int host_exit_code{};
};

class CommonRuntimeHostV2Run {
public:
    CommonRuntimeHostV2Run();
    ~CommonRuntimeHostV2Run();
    CommonRuntimeHostV2Run(CommonRuntimeHostV2Run&&) noexcept;
    CommonRuntimeHostV2Run& operator=(CommonRuntimeHostV2Run&&) noexcept;
    CommonRuntimeHostV2Run(const CommonRuntimeHostV2Run&) = delete;
    CommonRuntimeHostV2Run& operator=(const CommonRuntimeHostV2Run&) = delete;

private:
    struct State;
    explicit CommonRuntimeHostV2Run(std::unique_ptr<State> state);
    std::unique_ptr<State> state_;
    friend CommonRuntimeHostV2Run start_single_worker_v2(
        const CommonRuntimeHostV2Request&, const CommonRuntimeHostV2Context&);
    friend std::optional<CommonRuntimeHostV2Outcome> poll_single_worker_v2(CommonRuntimeHostV2Run&);
    friend CommonRuntimeHostV2Outcome finish_single_worker_v2(CommonRuntimeHostV2Run&);
    friend void request_cancel_v2(CommonRuntimeHostV2Run&);
};

[[nodiscard]] CommonRuntimeHostV2Run start_single_worker_v2(
    const CommonRuntimeHostV2Request&, const CommonRuntimeHostV2Context&);
[[nodiscard]] std::optional<CommonRuntimeHostV2Outcome> poll_single_worker_v2(CommonRuntimeHostV2Run&);
[[nodiscard]] CommonRuntimeHostV2Outcome finish_single_worker_v2(CommonRuntimeHostV2Run&);
void request_cancel_v2(CommonRuntimeHostV2Run&);

} // namespace bruce
