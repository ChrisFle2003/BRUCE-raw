#include "bruce/application/common_runtime_compatibility_v1.hpp"
#include "bruce/core/types.hpp"

#include <array>

namespace bruce {
namespace {

bool valid_identifier(std::string_view value) {
    if (value.empty() || value.size() > common_runtime_compatibility_v1_max_identifier_bytes) {
        return false;
    }
    const auto lower = [](char c) { return c >= 'a' && c <= 'z'; };
    const auto digit = [](char c) { return c >= '0' && c <= '9'; };
    if (!lower(value.front()) && !digit(value.front())) return false;
    for (const char c : value) {
        if (!lower(c) && !digit(c) && c != '-' && c != '_' && c != '.' &&
            c != ':' && c != '/' && c != '+') {
            return false;
        }
    }
    return true;
}

bool valid_family(std::string_view value) {
    constexpr std::array<std::string_view, 4> families{
        "session", "gate", "transfer", "management"};
    for (const auto family : families) {
        if (value == family) return true;
    }
    return false;
}

void validate(const CommonRuntimeCompatibilityV1& value) {
    if (!valid_family(value.family) || !valid_identifier(value.budget_unit) ||
        !valid_identifier(value.checkpoint_order) || !valid_identifier(value.error_envelope) ||
        !valid_identifier(value.publication_boundary) || !valid_identifier(value.legacy_route)) {
        throw Error("InvalidCommonRuntimeCompatibilityV1");
    }
}

void consume(std::string_view& input, std::string_view prefix) {
    if (!input.starts_with(prefix)) throw Error("InvalidCommonRuntimeCompatibilityV1Json");
    input.remove_prefix(prefix.size());
}

std::string read_identifier(std::string_view& input) {
    const auto end = input.find('"');
    if (end == std::string_view::npos ||
        end > common_runtime_compatibility_v1_max_identifier_bytes ||
        !valid_identifier(input.substr(0, end))) {
        throw Error("InvalidCommonRuntimeCompatibilityV1Json");
    }
    std::string value(input.substr(0, end));
    input.remove_prefix(end + 1);
    return value;
}

}

std::string encode_common_runtime_compatibility_v1(const CommonRuntimeCompatibilityV1& value) {
    validate(value);
    std::string output;
    output.reserve(common_runtime_compatibility_v1_max_json_bytes);
    output += "{\"contract\":\"";
    output += common_runtime_compatibility_v1_contract;
    output += "\",\"family\":\"";
    output += value.family;
    output += "\",\"budget_unit\":\"";
    output += value.budget_unit;
    output += "\",\"checkpoint_order\":\"";
    output += value.checkpoint_order;
    output += "\",\"error_envelope\":\"";
    output += value.error_envelope;
    output += "\",\"publication_boundary\":\"";
    output += value.publication_boundary;
    output += "\",\"legacy_route\":\"";
    output += value.legacy_route;
    output += "\"}\n";
    if (output.size() > common_runtime_compatibility_v1_max_json_bytes) {
        throw Error("CommonRuntimeCompatibilityV1TooLarge");
    }
    return output;
}

CommonRuntimeCompatibilityV1 decode_common_runtime_compatibility_v1(std::string_view json) {
    if (json.size() > common_runtime_compatibility_v1_max_json_bytes) {
        throw Error("CommonRuntimeCompatibilityV1TooLarge");
    }
    consume(json, "{\"contract\":\"bruce-common-runtime-compatibility-v1\",\"family\":\"");
    CommonRuntimeCompatibilityV1 value;
    value.family = read_identifier(json);
    consume(json, ",\"budget_unit\":\"");
    value.budget_unit = read_identifier(json);
    consume(json, ",\"checkpoint_order\":\"");
    value.checkpoint_order = read_identifier(json);
    consume(json, ",\"error_envelope\":\"");
    value.error_envelope = read_identifier(json);
    consume(json, ",\"publication_boundary\":\"");
    value.publication_boundary = read_identifier(json);
    consume(json, ",\"legacy_route\":\"");
    value.legacy_route = read_identifier(json);
    consume(json, "}\n");
    if (!json.empty()) throw Error("InvalidCommonRuntimeCompatibilityV1Json");
    validate(value);
    return value;
}

CommonRuntimeCompatibilityV1 control_family_compatibility_v1(std::string_view family) {
    if (family == "session") {
        return {"session", "session-checkpoints-v1", "path-dependent",
                "application-execution-failure-v1-v3", "final-export-outside", "execute-cli"};
    }
    if (family == "gate") {
        return {"gate", "session-checkpoints-v1", "signal-before-consumption",
                "simple-error-json", "no-export", "gate-run"};
    }
    if (family == "transfer") {
        return {"transfer", "transfer-checkpoints-v1", "signal-budget-consume",
                "simple-error-json", "export-preparation", "transfer-cli"};
    }
    if (family == "management") {
        return {"management", "management-checkpoints-v1", "signal-budget-consume",
                "simple-error-json", "read-only", "experience-inspect+rules-status"};
    }
    throw Error("UnknownCommonRuntimeCompatibilityV1Family");
}

void require_control_family_compatibility_v1(const CommonRuntimeCompatibilityV1& value) {
    if (value != control_family_compatibility_v1(value.family)) {
        throw Error("CommonRuntimeCompatibilityV1FamilyMismatch");
    }
}

}
