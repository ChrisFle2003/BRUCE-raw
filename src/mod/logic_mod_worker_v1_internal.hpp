#pragma once

// Private V1 implementation seam.  It is deliberately below include/ so it
// cannot become a consumer-facing contract while the V6 runtime is designed.
#include "bruce/mod/logic_mod_worker_v1.hpp"
#include "logic_mod_worker_attempt_v6.hpp"

#include <memory>
#include <optional>

namespace bruce::logic_mod_worker_v1_internal {

// V1 preflight is deliberately callable by the public adapter so its
// established error precedence remains independent of request normalization.
void preflight_static_worker_v1(std::string_view worker_bytes);

// Private bridge between the established V1 transcript/publisher and the V6
// attempt owner.  Preparation keeps the exact V1 ordering (budget, optional
// output Store preparation, payload resolution, fresh bindings, ELF preflight
// and request construction).  The caller owns the Attempt; finalization uses
// the same V1 response acceptance and publication path, so no V6-specific
// response parser can drift from canonical V1 records.
class LogicModPreparedExecutionV6 {
public:
    struct State;
    LogicModPreparedExecutionV6();
    ~LogicModPreparedExecutionV6();
    LogicModPreparedExecutionV6(LogicModPreparedExecutionV6&&) noexcept;
    LogicModPreparedExecutionV6& operator=(LogicModPreparedExecutionV6&&) noexcept;
    LogicModPreparedExecutionV6(const LogicModPreparedExecutionV6&) = delete;
    LogicModPreparedExecutionV6& operator=(const LogicModPreparedExecutionV6&) = delete;
private:
    explicit LogicModPreparedExecutionV6(std::unique_ptr<State> state);
    std::unique_ptr<State> state_;
    friend LogicModPreparedExecutionV6 prepare_logic_mod_execution_v6(
        const LogicModRegistryV1&, std::string_view, std::string_view,
        std::string_view, std::string_view, std::string_view, LogicModBudgetsV1,
        const std::optional<LogicModInputV1>&, const std::optional<LogicModOutputV1>&);
    friend LogicModPreparedExecutionV6 prepare_logic_mod_execution_v6(
        const LogicModBindingV1&, std::string_view, LogicModBudgetsV1);
    friend LogicModWorkerAttemptV6 start_logic_mod_execution_v6(LogicModPreparedExecutionV6&);
    friend LogicModRunV1 finalize_logic_mod_execution_v6(
        LogicModPreparedExecutionV6&&, const LogicModAttemptOutcomeV6&, bool);
};

[[nodiscard]] LogicModPreparedExecutionV6 prepare_logic_mod_execution_v6(
    const LogicModRegistryV1&, std::string_view mod_id, std::string_view version,
    std::string_view template_id, std::string_view capability, std::string_view input,
    LogicModBudgetsV1, const std::optional<LogicModInputV1>& payload_input = std::nullopt,
    const std::optional<LogicModOutputV1>& output = std::nullopt);
[[nodiscard]] LogicModPreparedExecutionV6 prepare_logic_mod_execution_v6(
    const LogicModBindingV1&, std::string_view input, LogicModBudgetsV1);
[[nodiscard]] LogicModWorkerAttemptV6 start_logic_mod_execution_v6(LogicModPreparedExecutionV6&);
[[nodiscard]] LogicModRunV1 finalize_logic_mod_execution_v6(
    LogicModPreparedExecutionV6&&, const LogicModAttemptOutcomeV6&, bool accept_ready = true);

} // namespace bruce::logic_mod_worker_v1_internal
