#include "bruce/artifact/execute_handoff_inspector_v1.hpp"
#include "bruce/core/types.hpp"

#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}
}

int main() {
    const auto missing = std::filesystem::temp_directory_path() /
        "bruce-execute-handoff-inspector-missing";
    bool rejected = false;
    try { (void)bruce::artifact_v1::inspect_execute_handoff_v1(missing); }
    catch (const bruce::Error& error) { rejected = std::string_view(error.what()) == "PROTOCOL_ERROR"; }
    require(rejected, "missing handoff run accepted");
    const auto open_descriptors = [] {
        std::size_t count{};
        for (const auto& entry : std::filesystem::directory_iterator("/proc/self/fd")) {
            (void)entry;
            ++count;
        }
        return count;
    };
    const auto before = open_descriptors();
    for (int index = 0; index < 32; ++index) {
        try { (void)bruce::artifact_v1::inspect_execute_handoff_v1(missing); }
        catch (const bruce::Error& error) {
            require(std::string_view(error.what()) == "PROTOCOL_ERROR", "unexpected missing-run failure");
        }
    }
    require(open_descriptors() == before, "inspector leaked parent directory descriptors");
}
