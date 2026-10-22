#include "bruce/application/common_runtime_host_v1.hpp"
#include "bruce/core/types.hpp"

namespace bruce {

void validate_common_runtime_host_v1(const CommonRuntimeHostV1Request& request) {
    if (request.entrypoint != "compare" && request.entrypoint != "grid" &&
        request.entrypoint != "grid-batch" && request.entrypoint != "grid-resume") {
        throw Error("CommonRuntimeHostV1EntrypointMismatch");
    }

    // Resolve the supplied family first so unknown values retain the existing
    // compatibility error. A known non-Session family retains the existing
    // compatibility mismatch used by the CLI adapter.
    const auto supplied_family = control_family_compatibility_v1(request.compatibility.family);
    require_control_family_compatibility_v1(request.compatibility);
    if (supplied_family.family != "session") {
        throw Error("CommonRuntimeCompatibilityV1FamilyMismatch");
    }
}

void delegate_common_runtime_host_v1(const CommonRuntimeHostV1Request& request,
                                     CommonRuntimeHostV1Delegate delegate) {
    validate_common_runtime_host_v1(request);
    if (!delegate) throw Error("EmptyCommonRuntimeHostV1Delegate");
    delegate();
}

}
