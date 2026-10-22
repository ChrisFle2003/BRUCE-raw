#include "bruce/application/common_runtime_host_v1.hpp"
#include "bruce/application/common_runtime_compatibility_v1.hpp"
#include "bruce/core/types.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
int assertions = 0;

void require(bool condition, const char* message) {
    ++assertions;
    if (!condition) throw bruce::Error(message);
}

template<class Function>
void require_error(Function&& function, std::string_view expected, const char* message) {
    ++assertions;
    try {
        function();
    } catch (const bruce::Error& error) {
        if (error.what() == expected) return;
        throw bruce::Error(message);
    }
    throw bruce::Error(message);
}

bruce::CommonRuntimeHostV1Request canonical_request() {
    return {"compare", bruce::control_family_compatibility_v1("session")};
}

bruce::CommonRuntimeHostV1Request canonical_grid_request() {
    return {"grid", bruce::control_family_compatibility_v1("session")};
}

bruce::CommonRuntimeHostV1Request canonical_grid_batch_request() {
    return {"grid-batch", bruce::control_family_compatibility_v1("session")};
}
}

int main() {
    try {
        using namespace bruce;

        unsigned calls = 0;
        const auto request = canonical_request();
        delegate_common_runtime_host_v1(request, [&] { ++calls; });
        require(calls == 1, "Canonical host request was not delegated exactly once");

        calls = 0;
        const auto grid_request = canonical_grid_request();
        delegate_common_runtime_host_v1(grid_request, [&] { ++calls; });
        require(calls == 1, "Canonical grid host request was not delegated exactly once");

        calls = 0;
        const auto grid_batch_request = canonical_grid_batch_request();
        delegate_common_runtime_host_v1(grid_batch_request, [&] { ++calls; });
        require(calls == 1, "Canonical grid-batch host request was not delegated exactly once");

        calls = 0;
        const CommonRuntimeHostV1Request resume_request{
            "grid-resume", control_family_compatibility_v1("session")};
        require(resume_request.compatibility.legacy_route == "execute-cli",
                "Resume must retain the execute-cli route");
        validate_common_runtime_host_v1(resume_request);
        require(calls == 0, "Validation invoked a delegate");
        delegate_common_runtime_host_v1(resume_request, [&] { ++calls; });
        require(calls == 1, "Canonical grid-resume was not delegated exactly once");

        for (const auto entrypoint : {"", "research", "compatibility", "replay",
                                      "grid-resume-other", "GRID-RESUME"}) {
            calls = 0;
            auto wrong_entrypoint = resume_request;
            wrong_entrypoint.entrypoint = entrypoint;
            require_error([&] {
                delegate_common_runtime_host_v1(wrong_entrypoint, [&] { ++calls; });
            }, "CommonRuntimeHostV1EntrypointMismatch", "Wrong entrypoint was accepted");
            require(calls == 0, "Wrong entrypoint invoked the delegate");
        }

        for (const auto entrypoint : {"compare", "grid", "grid-batch", "grid-resume"}) {
            for (const auto family : {"gate", "transfer", "management"}) {
                calls = 0;
                auto wrong_family = request;
                wrong_family.entrypoint = entrypoint;
                wrong_family.compatibility = control_family_compatibility_v1(family);
                require_error([&] {
                    delegate_common_runtime_host_v1(wrong_family, [&] { ++calls; });
                }, "CommonRuntimeCompatibilityV1FamilyMismatch", "Wrong family was accepted");
                require(calls == 0, "Wrong family invoked the delegate");
            }
        }

        for (const auto entrypoint : {"compare", "grid", "grid-batch", "grid-resume"}) {
            for (const auto mutate : {
                     0, 1, 2, 3, 4, 5
                 }) {
                calls = 0;
                auto changed_descriptor = canonical_grid_request();
                changed_descriptor.entrypoint = entrypoint;
                switch (mutate) {
                    case 0: changed_descriptor.compatibility.family = "gate"; break;
                    case 1: changed_descriptor.compatibility.budget_unit = "other-budget"; break;
                    case 2: changed_descriptor.compatibility.checkpoint_order = "other-order"; break;
                    case 3: changed_descriptor.compatibility.error_envelope = "other-envelope"; break;
                    case 4: changed_descriptor.compatibility.publication_boundary = "other-boundary"; break;
                    case 5: changed_descriptor.compatibility.legacy_route = "other-route"; break;
                }
                require_error([&] {
                    delegate_common_runtime_host_v1(changed_descriptor, [&] { ++calls; });
                }, "CommonRuntimeCompatibilityV1FamilyMismatch",
                              "Mutated descriptor was accepted");
                require(calls == 0, "Mutated descriptor invoked the delegate");
            }
        }

        for (const auto entrypoint : {"compare", "grid", "grid-batch", "grid-resume"}) {
            auto selected = request;
            selected.entrypoint = entrypoint;
            require_error([&] {
                delegate_common_runtime_host_v1(selected, {});
            }, "EmptyCommonRuntimeHostV1Delegate", "Empty delegate was accepted");

            bool preserved = false;
            int cause = 0;
            calls = 0;
            try {
                delegate_common_runtime_host_v1(selected, [&] { ++calls; throw &cause; });
            } catch (int* error) {
                preserved = error == &cause;
            }
            require(preserved, "Delegate exception identity was translated");
            require(calls == 1, "Throwing delegate was not invoked exactly once");

            calls = 0;
            selected.compatibility.family = "unknown";
            require_error([&] {
                delegate_common_runtime_host_v1(selected, [&] { ++calls; });
            }, "UnknownCommonRuntimeCompatibilityV1Family", "Unknown family was accepted");
            require(calls == 0, "Unknown family invoked the delegate");
        }

        // The request is descriptive only. Delegation carries no budget,
        // output, bundle, or descriptor-rendering state.
        calls = 0;
        validate_common_runtime_host_v1(request);
        delegate_common_runtime_host_v1(request, [&] { ++calls; });
        require(calls == 1, "Descriptive host request changed delegation semantics");

        std::cout << assertions << " common runtime host assertions passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
