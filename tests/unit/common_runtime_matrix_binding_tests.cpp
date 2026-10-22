#include "bruce/application/common_runtime_compatibility_v1.hpp"
#include "bruce/core/types.hpp"

#include <array>
#include <iostream>
#include <string>
#include <string_view>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw bruce::Error(message);
}

template<class Function>
void require_error(Function&& function, const char* message) {
    try {
        function();
    } catch (const bruce::Error&) {
        return;
    }
    throw bruce::Error(message);
}

struct Case {
    std::string_view family;
    std::string_view json;
};

constexpr std::array<Case, 4> cases{{
    {"session", "{\"contract\":\"bruce-common-runtime-compatibility-v1\",\"family\":\"session\",\"budget_unit\":\"session-checkpoints-v1\",\"checkpoint_order\":\"path-dependent\",\"error_envelope\":\"application-execution-failure-v1-v3\",\"publication_boundary\":\"final-export-outside\",\"legacy_route\":\"execute-cli\"}\n"},
    {"gate", "{\"contract\":\"bruce-common-runtime-compatibility-v1\",\"family\":\"gate\",\"budget_unit\":\"session-checkpoints-v1\",\"checkpoint_order\":\"signal-before-consumption\",\"error_envelope\":\"simple-error-json\",\"publication_boundary\":\"no-export\",\"legacy_route\":\"gate-run\"}\n"},
    {"transfer", "{\"contract\":\"bruce-common-runtime-compatibility-v1\",\"family\":\"transfer\",\"budget_unit\":\"transfer-checkpoints-v1\",\"checkpoint_order\":\"signal-budget-consume\",\"error_envelope\":\"simple-error-json\",\"publication_boundary\":\"export-preparation\",\"legacy_route\":\"transfer-cli\"}\n"},
    {"management", "{\"contract\":\"bruce-common-runtime-compatibility-v1\",\"family\":\"management\",\"budget_unit\":\"management-checkpoints-v1\",\"checkpoint_order\":\"signal-budget-consume\",\"error_envelope\":\"simple-error-json\",\"publication_boundary\":\"read-only\",\"legacy_route\":\"experience-inspect+rules-status\"}\n"},
}};
}

int main() {
    try {
        using namespace bruce;
        for (const auto& item : cases) {
            const auto expected = decode_common_runtime_compatibility_v1(item.json);
            const auto actual = control_family_compatibility_v1(item.family);
            require(actual == expected, "Control family descriptor changed");
            require(encode_common_runtime_compatibility_v1(actual) == item.json,
                    "Control family descriptor bytes changed");
            require_control_family_compatibility_v1(expected);
        }

        require_error([] { control_family_compatibility_v1("unknown"); },
                      "Unknown family selected");

        auto changed = control_family_compatibility_v1("session");
        changed.family = "gate";
        require_error([&] { require_control_family_compatibility_v1(changed); },
                      "Descriptor accepted for wrong family");
        changed = control_family_compatibility_v1("gate");
        changed.budget_unit = "gate-inspection-checkpoints";
        require_error([&] { require_control_family_compatibility_v1(changed); },
                      "Wrong gate budget unit accepted");
        changed = control_family_compatibility_v1("transfer");
        changed.checkpoint_order = "budget-signal-consume";
        require_error([&] { require_control_family_compatibility_v1(changed); },
                      "Wrong transfer checkpoint order accepted");
        changed = control_family_compatibility_v1("management");
        changed.error_envelope = "application-execution-failure-v1-v3";
        require_error([&] { require_control_family_compatibility_v1(changed); },
                      "Wrong management error envelope accepted");
        changed = control_family_compatibility_v1("transfer");
        changed.publication_boundary = "final-export-outside";
        require_error([&] { require_control_family_compatibility_v1(changed); },
                      "Wrong transfer publication boundary accepted");
        changed = control_family_compatibility_v1("management");
        changed.legacy_route = "management-cli";
        require_error([&] { require_control_family_compatibility_v1(changed); },
                      "Wrong management route accepted");
        std::cout << "Four control family bindings and six mutations verified\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
