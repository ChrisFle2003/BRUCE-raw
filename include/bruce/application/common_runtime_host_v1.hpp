#pragma once

#include "bruce/application/common_runtime_compatibility_v1.hpp"

#include <functional>
#include <string>

namespace bruce {

struct CommonRuntimeHostV1Request {
    // Stage-2 host probes: `compare`, `grid`, `grid-batch`, and `grid-resume`.
    std::string entrypoint;
    CommonRuntimeCompatibilityV1 compatibility;
};

using CommonRuntimeHostV1Delegate = std::function<void()>;

void validate_common_runtime_host_v1(const CommonRuntimeHostV1Request& request);

void delegate_common_runtime_host_v1(const CommonRuntimeHostV1Request& request,
                                     CommonRuntimeHostV1Delegate delegate);

}
