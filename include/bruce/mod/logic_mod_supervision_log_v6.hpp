#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace bruce::mod {

// This is a host-only diagnostic contract.  It intentionally has no field for
// paths, PIDs, commands, frames, payload bytes, URLs, signatures or stderr.
enum class LogicModPeakMemoryAvailabilityV6 { OBSERVED, UNAVAILABLE, NOT_APPLICABLE };

struct SupervisionEventV6 {
    std::string schema{"logic-mod-supervision-v6"};
    std::uint64_t seq{};
    std::string event;
    std::string status;
    std::string attempt_id;
    std::string node_id;
    std::optional<std::string> terminal_reason;
    std::optional<std::uint64_t> elapsed_ms;
    std::optional<std::uint64_t> wall_time_limit_ms;
    std::optional<std::uint64_t> address_space_limit_bytes;
    std::optional<std::uint64_t> input_limit_bytes;
    std::optional<std::uint64_t> output_limit_bytes;
    LogicModPeakMemoryAvailabilityV6 peak_memory_availability{LogicModPeakMemoryAvailabilityV6::NOT_APPLICABLE};
    std::optional<std::uint64_t> peak_memory_bytes;

    // Test-only poison: production callers must leave this empty.  Keeping it
    // non-serializable makes the absence of data-bearing escape hatches
    // explicit and testable.
    std::string forbidden_data;
};

class LogicModSupervisionLogV6 {
public:
    [[nodiscard]] static LogicModSupervisionLogV6 create(const std::filesystem::path& report_root,
                                                          std::string run_id);
    LogicModSupervisionLogV6(LogicModSupervisionLogV6&&) noexcept;
    LogicModSupervisionLogV6& operator=(LogicModSupervisionLogV6&&) noexcept;
    LogicModSupervisionLogV6(const LogicModSupervisionLogV6&) = delete;
    LogicModSupervisionLogV6& operator=(const LogicModSupervisionLogV6&) = delete;
    ~LogicModSupervisionLogV6();

    void append(const SupervisionEventV6& event);

private:
    LogicModSupervisionLogV6(int control_fd, std::string run_id, std::uint64_t last_seq,
                             std::vector<SupervisionEventV6> events);
    int control_fd_{-1};
    std::string run_id_;
    std::uint64_t last_seq_{};
    std::vector<SupervisionEventV6> events_;
};


// Read-only and diagnostic-only.  This never repairs, starts or cleans up a
// worker and rejects noncanonical or hostile sidecars as PROTOCOL_ERROR.
[[nodiscard]] std::vector<SupervisionEventV6> validate_logic_mod_supervision_log_v6(
    const std::filesystem::path& report_root, std::string_view run_id);

} // namespace bruce::mod
