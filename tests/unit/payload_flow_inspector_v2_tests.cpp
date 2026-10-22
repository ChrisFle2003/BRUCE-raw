#include "bruce/artifact/payload_flow_inspector_v2.hpp"
#include "bruce/core/types.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int main() {
    try {
        bool rejected = false;
        try {
            (void)bruce::artifact_v1::inspect_payload_flow_v2(
                std::filesystem::path("/definitely/missing/bruce-flow-bundle"));
        } catch (const bruce::Error& error) {
            rejected = std::string(error.what()) == "PROTOCOL_ERROR";
        }
        if (!rejected) return 1;
        std::cout << "payload flow inspector v2 contracts passed\n";
        return 0;
    } catch (...) {
        return 1;
    }
}
