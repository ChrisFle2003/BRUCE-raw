#include "bruce/application/common_runtime_compatibility_v1.hpp"
#include "bruce/core/types.hpp"

#include <iostream>
#include <string>
#include <string_view>

namespace {
int assertions = 0;

void require(bool condition, const char* message) {
    ++assertions;
    if (!condition) throw bruce::Error(message);
}

template<class Function>
void require_error(Function&& function, const char* message) {
    ++assertions;
    try {
        function();
    } catch (const bruce::Error&) {
        return;
    }
    throw bruce::Error(message);
}

void round_trip(const bruce::CommonRuntimeCompatibilityV1& value,
                std::string_view expected, const char* message) {
    const auto encoded = bruce::encode_common_runtime_compatibility_v1(value);
    require(encoded == expected, message);
    require(bruce::decode_common_runtime_compatibility_v1(encoded) == value,
            "Canonical round trip changed a descriptor");
}
}

int main() {
    try {
        using namespace bruce;
        const CommonRuntimeCompatibilityV1 session{
            "session", "session-checkpoints-v1", "path-dependent",
            "application-execution-failure-v1-v3", "final-export-outside",
            "execute-cli"};
        round_trip(session,
                   "{\"contract\":\"bruce-common-runtime-compatibility-v1\",\"family\":\"session\","
                   "\"budget_unit\":\"session-checkpoints-v1\",\"checkpoint_order\":\"path-dependent\","
                   "\"error_envelope\":\"application-execution-failure-v1-v3\","
                   "\"publication_boundary\":\"final-export-outside\",\"legacy_route\":\"execute-cli\"}\n",
                   "Session encoding is not canonical");
        round_trip({"gate", "gate-inspection-checkpoints", "signal-before-consumption",
                    "simple-error-json", "no-export", "gate-run"},
                   "{\"contract\":\"bruce-common-runtime-compatibility-v1\",\"family\":\"gate\","
                   "\"budget_unit\":\"gate-inspection-checkpoints\","
                   "\"checkpoint_order\":\"signal-before-consumption\","
                   "\"error_envelope\":\"simple-error-json\","
                   "\"publication_boundary\":\"no-export\",\"legacy_route\":\"gate-run\"}\n",
                   "Gate encoding is not canonical");
        round_trip({"transfer", "transfer-checkpoints-v1", "signal-budget-consume",
                    "simple-error-json", "export-preparation", "transfer-cli"},
                   "{\"contract\":\"bruce-common-runtime-compatibility-v1\",\"family\":\"transfer\","
                   "\"budget_unit\":\"transfer-checkpoints-v1\","
                   "\"checkpoint_order\":\"signal-budget-consume\","
                   "\"error_envelope\":\"simple-error-json\","
                   "\"publication_boundary\":\"export-preparation\","
                   "\"legacy_route\":\"transfer-cli\"}\n",
                   "Transfer encoding is not canonical");
        round_trip({"management", "management-checkpoints-v1", "signal-budget-consume",
                    "simple-error-json", "read-only", "management-cli"},
                   "{\"contract\":\"bruce-common-runtime-compatibility-v1\",\"family\":\"management\","
                   "\"budget_unit\":\"management-checkpoints-v1\","
                   "\"checkpoint_order\":\"signal-budget-consume\","
                   "\"error_envelope\":\"simple-error-json\","
                   "\"publication_boundary\":\"read-only\","
                   "\"legacy_route\":\"management-cli\"}\n",
                   "Management encoding is not canonical");

        const auto valid = encode_common_runtime_compatibility_v1(session);
        auto invalid = valid;
        invalid.replace(invalid.find("\"budget_unit\":\"session-checkpoints-v1\","),
                        std::string("\"budget_unit\":\"session-checkpoints-v1\",").size(), "");
        require_error([&] { decode_common_runtime_compatibility_v1(invalid); },
                      "Missing field accepted");
        invalid = valid;
        invalid.insert(invalid.find("\"checkpoint_order\""),
                       "\"budget_unit\":\"session-checkpoints-v1\",");
        require_error([&] { decode_common_runtime_compatibility_v1(invalid); },
                      "Duplicate field accepted");
        invalid = valid;
        invalid.insert(invalid.find("\"checkpoint_order\""), "\"extra\":\"x\",");
        require_error([&] { decode_common_runtime_compatibility_v1(invalid); },
                      "Unknown field accepted");
        invalid = valid;
        invalid.replace(invalid.find("\"family\":\"session\""),
                        std::string("\"family\":\"session\"").size(),
                        "\"family\":\"other\"");
        require_error([&] { decode_common_runtime_compatibility_v1(invalid); },
                      "Unknown family accepted");
        invalid = valid;
        invalid.replace(invalid.find("bruce-common-runtime-compatibility-v1"),
                        std::string("bruce-common-runtime-compatibility-v1").size(), "wrong-contract");
        require_error([&] { decode_common_runtime_compatibility_v1(invalid); },
                      "Wrong contract accepted");
        require_error([&] { decode_common_runtime_compatibility_v1(valid.substr(0, valid.size() - 1)); },
                      "Missing LF accepted");
        require_error([&] { decode_common_runtime_compatibility_v1(valid + "x"); },
                      "Trailing byte accepted");
        require_error([&] { decode_common_runtime_compatibility_v1(valid + "\n"); },
                      "Extra LF accepted");
        require_error([&] { decode_common_runtime_compatibility_v1(" " + valid); },
                      "Leading whitespace accepted");
        require_error([&] { decode_common_runtime_compatibility_v1(std::string(769, 'x')); },
                      "Oversized JSON accepted");

        auto changed = session;
        changed.family = "other";
        require_error([&] { encode_common_runtime_compatibility_v1(changed); },
                      "Unknown family encoded");
        changed = session;
        changed.budget_unit.clear();
        require_error([&] { encode_common_runtime_compatibility_v1(changed); },
                      "Empty identifier encoded");
        changed = session;
        changed.budget_unit = "-invalid";
        require_error([&] { encode_common_runtime_compatibility_v1(changed); },
                      "Invalid identifier start encoded");
        changed = session;
        changed.budget_unit = "Uppercase";
        require_error([&] { encode_common_runtime_compatibility_v1(changed); },
                      "Uppercase identifier encoded");
        changed = session;
        changed.budget_unit = std::string(65, 'a');
        require_error([&] { encode_common_runtime_compatibility_v1(changed); },
                      "Oversized identifier encoded");
        changed.budget_unit = std::string(64, 'a');
        require(decode_common_runtime_compatibility_v1(
                    encode_common_runtime_compatibility_v1(changed)) == changed,
                "Maximum identifier length rejected");
        invalid = valid;
        invalid.replace(invalid.find("session-checkpoints-v1"),
                        std::string("session-checkpoints-v1").size(), std::string(65, 'a'));
        require_error([&] { decode_common_runtime_compatibility_v1(invalid); },
                      "Oversized identifier decoded");
        invalid = valid;
        invalid.replace(invalid.find("session-checkpoints-v1"),
                        std::string("session-checkpoints-v1").size(), "bad\\nvalue");
        require_error([&] { decode_common_runtime_compatibility_v1(invalid); },
                      "Escaped control in identifier accepted");
        std::cout << assertions << " common runtime compatibility assertions passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
