#pragma once

#include "bruce/application/management_inspection_control.hpp"
#include "bruce/application/common_runtime_compatibility_v1.hpp"
#include "bruce/core/json.hpp"

#include <csignal>
#include <string>
#include <string_view>

namespace bruce {
namespace management_inspection_detail {
inline volatile std::sig_atomic_t interrupted = 0;
inline void on_interrupt(int) { interrupted = 1; }
}

struct ManagementInspectionCliControl {
    bool enabled = false;
    ManagementInspectionControl control;

    ReplayCheckpoint checkpoint() {
        return enabled ? control.callback() : ReplayCheckpoint{};
    }

    std::string render(std::string_view report) const {
        if (!enabled) return std::string(report);
        return "{\"contract\":\"management-inspection-v1\",\"result\":" +
               std::string(report) + ",\"usage\":" + control.report() + "}";
    }
};

inline ManagementInspectionCliControl parse_management_inspection_options(
    int argc, char** argv, int first_option, std::string_view legacy_usage) {
    ManagementInspectionCliControl result;
    bool has_maximum = false;
    bool has_cancellation = false;
    bool has_compatibility_family = false;
    for (int index = first_option; index < argc; index += 2) {
        if (index + 1 >= argc) throw Error(std::string(legacy_usage));
        const std::string key = argv[index];
        const std::string value = argv[index + 1];
        if (key == "--max-management-checkpoints") {
            if (has_maximum) throw Error("DuplicateOption: " + key);
            result.control.maximum = integer<std::uint64_t>(value);
            has_maximum = true;
            result.enabled = true;
        } else if (key == "--management-cancellation") {
            if (has_cancellation) throw Error("DuplicateOption: " + key);
            if (value != "none" && value != "checkpoints") {
                throw Error("UnknownManagementCancellation");
            }
            if (value == "checkpoints") {
                std::signal(SIGINT, management_inspection_detail::on_interrupt);
                std::signal(SIGTERM, management_inspection_detail::on_interrupt);
                result.control.cancelled = [] {
                    return management_inspection_detail::interrupted != 0;
                };
            }
            has_cancellation = true;
            result.enabled = true;
        } else if (key == "--compatibility-family") {
            if (has_compatibility_family) throw Error("DuplicateOption: " + key);
            const auto selected = control_family_compatibility_v1(value);
            require_control_family_compatibility_v1(selected);
            if (selected != control_family_compatibility_v1("management")) {
                throw Error("CommonRuntimeCompatibilityV1FamilyMismatch");
            }
            has_compatibility_family = true;
        } else {
            throw Error(std::string(legacy_usage));
        }
    }
    return result;
}

}
