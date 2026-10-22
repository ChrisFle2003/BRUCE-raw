#include "bruce/mod/logic_mod_worker_v1.hpp"
#include "bruce/mod/logic_mod_payload_v1.hpp"
#include "logic_mod_worker_v1_internal.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/tree/normalizer.hpp"
#include <charconv>
#include <gmpxx.h>

namespace bruce {
namespace {
constexpr std::size_t frame_limit = 65536;
std::string field(std::string_view s) { return std::to_string(s.size()) + ':' + std::string(s); }
class Fields {
    std::string_view rest;
public:
    explicit Fields(std::string_view s) : rest(s) {}
    void literal(std::string_view s) { if (!rest.starts_with(s)) throw Error("LogicFrameInvalid"); rest.remove_prefix(s.size()); }
    std::string take() {
        const auto colon = rest.find(':');
        if (colon == std::string_view::npos || colon == 0 || colon > 5 || (colon > 1 && rest[0] == '0')) throw Error("LogicFrameInvalid");
        std::size_t n = 0;
        for (std::size_t i = 0; i < colon; ++i) { if (rest[i] < '0' || rest[i] > '9') throw Error("LogicFrameInvalid"); n = n*10 + rest[i]-'0'; }
        rest.remove_prefix(colon + 1);
        if (n > frame_limit || n > rest.size()) throw Error("LogicFrameInvalid");
        std::string out(rest.substr(0, n)); rest.remove_prefix(n); return out;
    }
    void end() { if (!rest.empty()) throw Error("LogicFrameTrailingBytes"); }
    bool empty() const { return rest.empty(); }
};
std::uint64_t natural(std::string_view value) {
    std::uint64_t n = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), n);
    if (value.empty() || (value.size() > 1 && value.front() == '0') || result.ec != std::errc{} || result.ptr != value.data() + value.size()) throw Error("LogicRecordInvalid");
    return n;
}
std::string payload_fields(const LogicModRunV1& r) {
    if (!r.input_present) {
        if (!r.input_manifest_id.empty() || !r.input_sha256.empty() || !r.input_codec.empty() || r.input_object_index || r.input_size_bytes) throw Error("LogicRecordInvalid");
        return {};
    }
    if (r.input_manifest_id.empty() || r.input_sha256.size() != 64 || r.input_sha256.find_first_not_of("0123456789abcdef") != std::string::npos ||
        r.input_size_bytes > logic_mod_payload_limit_v1 || r.input_codec != "artifact-bytes-v1") throw Error("LogicRecordInvalid");
    return field("1") + field(r.input_manifest_id) + field(std::to_string(r.input_object_index)) + field(r.input_sha256) + field(std::to_string(r.input_size_bytes)) + field(r.input_codec);
}
std::string output_fields(const LogicModRunV1& r) {
    if (!r.output_present) {
        if (!r.output_manifest_id.empty() || !r.output_sha256.empty() || !r.output_codec.empty() || r.output_object_index || r.output_size_bytes) throw Error("LogicRecordInvalid");
        return {};
    }
    if (!r.accepted || r.status != "ACCEPTED" || !artifact_v1::is_valid_manifest_id_v1(r.output_manifest_id) || r.output_object_index != 0 ||
        r.output_sha256.size() != 64 || r.output_sha256.find_first_not_of("0123456789abcdef") != std::string::npos ||
        r.output_size_bytes > logic_mod_result_limit_v1 || r.output_codec != "logic-mod-result-text-v1") throw Error("LogicRecordInvalid");
    return field("output.v1") + field("1") + field(r.output_manifest_id) + field(std::to_string(r.output_object_index)) +
        field(r.output_sha256) + field(std::to_string(r.output_size_bytes)) + field(r.output_codec);
}
void output_evidence(LogicModRunV1& r, const LogicModPublishedOutputV1& published) {
    r.output_present = true; r.output_manifest_id = published.manifest_id; r.output_object_index = published.object_index;
    r.output_sha256 = published.sha256; r.output_size_bytes = published.size_bytes; r.output_codec = published.codec;
}
std::string budget_text(LogicModBudgetsV1 b) {
    if (!b.wall_ms || b.wall_ms > 5000 || b.memory_bytes < 8*1024*1024 || b.memory_bytes > 256*1024*1024 ||
        !b.input_bytes || b.input_bytes > 4096 || !b.output_bytes || b.output_bytes > frame_limit) throw Error("LogicBudgetDenied");
    return field(std::to_string(b.wall_ms)) + field(std::to_string(b.memory_bytes)) + field(std::to_string(b.input_bytes)) + field(std::to_string(b.output_bytes));
}
LogicModRunV1 request(const LogicModBindingV1& b, std::string_view input, LogicModBudgetsV1 limits, const LogicModPayloadV1* payload = nullptr) {
    LogicModRunV1 r; r.budget_decision = budget_text(limits);
    if (input.size() > limits.input_bytes) throw Error("LogicInputBudgetExceeded");
    r.normalized_input = normalize_route_text_v1(input).normalized_text;
    r.binding_identity = b.canonical_identity();
    if (payload) {
        r.input_present = true; r.input_manifest_id = payload->manifest_id(); r.input_object_index = payload->object_index();
        r.input_sha256 = payload->sha256(); r.input_size_bytes = payload->size_bytes(); r.input_codec = payload->codec();
    }
    r.request_digest = sha256("BRUCE_LOGIC_REQUEST_DIGEST_V1\n" + field(r.binding_identity) + field(r.normalized_input) + field(r.budget_decision) + payload_fields(r));
    return r;
}
std::string request_frame(const LogicModRunV1& r) {
    return field("BRUCE_LOGIC_REQUEST_V1\n" + field(r.binding_identity) + field(r.request_digest) + field(r.normalized_input) + field(r.budget_decision) + payload_fields(r));
}
std::optional<LogicModPayloadV1> resolve_input(const LogicModRegistryV1& registry, std::string_view mod_id, std::string_view version,
    std::string_view template_id, const std::optional<LogicModInputV1>& input) {
    if (!input) return std::nullopt;
    const auto binding = registry.bind(mod_id, version, template_id, "artifact.input.v1");
    return resolve_logic_mod_payload_v1(binding, input->store, input->manifest_id, input->object_index);
}
LogicModBindingV1 bind_execution(const LogicModRegistryV1& registry, std::string_view mod_id, std::string_view version,
    std::string_view template_id, std::string_view capability, const std::optional<LogicModPayloadV1>& payload) {
    std::optional<LogicModBindingV1> input_binding;
    if (payload) {
        input_binding = registry.bind(mod_id, version, template_id, "artifact.input.v1");
        if (input_binding->canonical_identity() != payload->authorization_identity()) throw Error("LogicPayloadAuthorizationChanged");
    }
    auto execution = registry.bind(mod_id, version, template_id, capability);
    if (input_binding && (capability != "worker.execute" || input_binding->mod_id() != execution.mod_id() || input_binding->version() != execution.version() ||
        input_binding->package_digest() != execution.package_digest() || input_binding->worker_sha256() != execution.worker_sha256() ||
        input_binding->template_id() != execution.template_id() || input_binding->registry_digest() != execution.registry_digest())) throw Error("LogicPayloadAuthorizationChanged");
    return execution;
}
LogicModBindingV1 bind_output(const LogicModRegistryV1& registry, std::string_view mod_id, std::string_view version,
    std::string_view template_id, const LogicModBindingV1& execution) {
    auto output = registry.bind(mod_id, version, template_id, "artifact.output.v1");
    if (execution.capability() != "worker.execute" || output.mod_id() != execution.mod_id() || output.version() != execution.version() ||
        output.package_digest() != execution.package_digest() || output.worker_sha256() != execution.worker_sha256() ||
        output.template_id() != execution.template_id() || output.registry_digest() != execution.registry_digest()) throw Error("LogicOutputAuthorizationChanged");
    return output;
}
bool hex_digest(std::string_view s) { return s.size() == 64 && s.find_first_not_of("0123456789abcdef") == std::string_view::npos; }
bool validate(std::string_view template_id, std::string_view input, std::string_view codec, std::string_view result) {
    if (template_id != "math.rational.power.v1" || codec != "rational-v1") return false;
    const auto tokens = normalize_route_text_v1(input).tokens;
    if (tokens.size() != 3 || tokens[1] != "hoch" || tokens[0].size() > 64 || tokens[2].empty() || tokens[2].size() > 2 || tokens[2].find_first_not_of("0123456789") != std::string::npos) return false;
    const auto power = std::stoul(tokens[2]); if (power > 16) return false;
    // Accept only integer/rational decimal syntax; no GMP expression language.
    const auto slash = tokens[0].find('/');
    const auto numeric = [](std::string_view s) { if (s.starts_with('-')) s.remove_prefix(1); return !s.empty() && s.find_first_not_of("0123456789") == std::string_view::npos; };
    if (!numeric(std::string_view(tokens[0]).substr(0, slash)) || (slash != std::string::npos && !numeric(std::string_view(tokens[0]).substr(slash+1)))) return false;
    mpq_class base;
    if (base.set_str(tokens[0], 10) != 0 || base.get_den() == 0) return false;
    base.canonicalize(); mpq_class answer = 1;
    for (unsigned long i = 0; i < power; ++i) answer *= base;
    return result == answer.get_str();
}
void accept_response(LogicModRunV1& r, std::string_view template_id) {
    Fields wire(r.response_frame); const auto payload = wire.take(); wire.end(); Fields fields(payload);
    fields.literal("BRUCE_LOGIC_RESULT_V1\n");
    if (fields.take() != r.binding_identity || fields.take() != r.request_digest) throw Error("LogicResponseIdentityMismatch");
    if (fields.take() != "candidate") throw Error("LogicCandidateStatusInvalid");
    const auto codec = fields.take(); const auto value = fields.take(); const auto digest = fields.take(); const auto trace = fields.take(); fields.end();
    if (digest != sha256(value) || (!trace.empty() && !hex_digest(trace))) throw Error("LogicResultDigestInvalid");
    if (!validate(template_id, r.normalized_input, codec, value)) throw Error("LogicHostValidationFailed");
    r.accepted = true; r.status = "ACCEPTED"; r.result = value;
}
}

namespace logic_mod_worker_v1_internal {

struct LogicModPreparedExecutionV6::State {
    LogicModRunV1 result;
    std::string template_id;
    std::optional<PreparedLogicModOutputV1> prepared_output;
    std::optional<LogicModBindingV1> output_binding;
    LogicModPreparedWorkerV6 worker;
    bool started{};
};

LogicModPreparedExecutionV6::LogicModPreparedExecutionV6() = default;
LogicModPreparedExecutionV6::LogicModPreparedExecutionV6(std::unique_ptr<State> state) : state_(std::move(state)) {}
LogicModPreparedExecutionV6::~LogicModPreparedExecutionV6() = default;
LogicModPreparedExecutionV6::LogicModPreparedExecutionV6(LogicModPreparedExecutionV6&&) noexcept = default;
LogicModPreparedExecutionV6& LogicModPreparedExecutionV6::operator=(LogicModPreparedExecutionV6&&) noexcept = default;

LogicModPreparedExecutionV6 prepare_logic_mod_execution_v6(const LogicModRegistryV1& registry,
    std::string_view mod_id, std::string_view version, std::string_view template_id,
    std::string_view capability, std::string_view input, LogicModBudgetsV1 budgets,
    const std::optional<LogicModInputV1>& payload_input, const std::optional<LogicModOutputV1>& output) {
    (void)budget_text(budgets);
    if (input.size() > budgets.input_bytes) throw Error("LogicInputBudgetExceeded");
    auto state = std::make_unique<LogicModPreparedExecutionV6::State>();
    state->template_id = template_id;
    if (output) {
        // Preserve V1's denial-before-Store ordering.  The fresh binding used
        // for publication is intentionally issued beside the execution bind.
        (void)registry.bind(mod_id, version, template_id, "artifact.output.v1");
        state->prepared_output.emplace(prepare_logic_mod_output_v1(*output));
    }
    const auto payload = resolve_input(registry, mod_id, version, template_id, payload_input);
    const auto binding = bind_execution(registry, mod_id, version, template_id, capability, payload);
    if (output) state->output_binding.emplace(bind_output(registry, mod_id, version, template_id, binding));
    preflight_static_worker_v1(binding.worker_bytes());
    state->result = request(binding, input, budgets, payload ? &*payload : nullptr);
    state->worker = prepare_logic_mod_worker_v6(binding.worker_bytes(), request_frame(state->result), budgets,
        payload.has_value(), payload ? payload->fd() : -1);
    return LogicModPreparedExecutionV6(std::move(state));
}

LogicModPreparedExecutionV6 prepare_logic_mod_execution_v6(
    const LogicModBindingV1& binding, const std::string_view input,
    const LogicModBudgetsV1 budgets) {
    (void)budget_text(budgets);
    if (input.size() > budgets.input_bytes) throw Error("LogicInputBudgetExceeded");
    auto state = std::make_unique<LogicModPreparedExecutionV6::State>();
    state->template_id = binding.template_id();
    preflight_static_worker_v1(binding.worker_bytes());
    state->result = request(binding, input, budgets);
    state->worker = prepare_logic_mod_worker_v6(binding.worker_bytes(), request_frame(state->result),
        budgets, false, -1);
    return LogicModPreparedExecutionV6(std::move(state));
}

LogicModWorkerAttemptV6 start_logic_mod_execution_v6(LogicModPreparedExecutionV6& execution) {
    if (!execution.state_ || execution.state_->started) throw Error("InvalidLogicModPreparedExecution");
    execution.state_->started = true;
    return start_logic_mod_worker_attempt_v6(std::move(execution.state_->worker));
}

LogicModRunV1 finalize_logic_mod_execution_v6(LogicModPreparedExecutionV6&& execution,
    const LogicModAttemptOutcomeV6& outcome, bool accept_ready) {
    if (!execution.state_ || !execution.state_->started || !outcome.reaped)
        throw Error("InvalidLogicModPreparedExecution");
    auto& state = *execution.state_;
    state.result.response_frame = outcome.response_frame;
    state.result.response_digest = outcome.response_digest;
    if (outcome.terminal_reason == "WALL_TIME_LIMIT") state.result.status = "TIMEOUT";
    else if (outcome.terminal_reason == "OUTPUT_LIMIT") state.result.status = "OUTPUT_LIMIT";
    else if (outcome.terminal_reason == "EXEC_DENIED") state.result.status = "EXEC_DENIED";
    else if (outcome.terminal_reason != "READY")
        state.result.status = outcome.worker_status.empty() ? outcome.terminal_reason : outcome.worker_status;
    if (!accept_ready && outcome.terminal_reason == "READY") {
        // A host cancel observed before acceptance wins the race.  Keep the
        // raw transcript as diagnostics, but never parse it into accepted
        // domain truth or publish an output artifact.
        state.result.status = "HOST_CANCELLED";
    } else if (state.result.status.empty()) {
        try { accept_response(state.result, state.template_id); }
        catch (const Error& error) { state.result.status = error.what(); state.result.result.clear(); state.result.accepted = false; }
    }
    if (state.result.accepted && state.prepared_output)
        output_evidence(state.result, publish_logic_mod_result_artifact_v1(*state.prepared_output,
            *state.output_binding, state.result.result));
    return std::move(state.result);
}

} // namespace logic_mod_worker_v1_internal

LogicModRunV1 run_logic_mod_worker_v1(const LogicModRegistryV1& registry, std::string_view mod_id,
    std::string_view version, std::string_view template_id, std::string_view capability, std::string_view input, LogicModBudgetsV1 budgets,
    const std::optional<LogicModInputV1>& payload_input, const std::optional<LogicModOutputV1>& output) {
    auto execution = logic_mod_worker_v1_internal::prepare_logic_mod_execution_v6(
        registry, mod_id, version, template_id, capability, input, budgets, payload_input, output);
    auto attempt = logic_mod_worker_v1_internal::start_logic_mod_execution_v6(execution);
    return logic_mod_worker_v1_internal::finalize_logic_mod_execution_v6(std::move(execution),
        logic_mod_worker_v1_internal::finish_logic_mod_worker_attempt_v6(attempt));
}

LogicModRunV1 run_logic_mod_worker_v1(const LogicModBindingV1& binding,
    const std::string_view input, const LogicModBudgetsV1 budgets) {
    auto execution = logic_mod_worker_v1_internal::prepare_logic_mod_execution_v6(binding, input, budgets);
    auto attempt = logic_mod_worker_v1_internal::start_logic_mod_execution_v6(execution);
    return logic_mod_worker_v1_internal::finalize_logic_mod_execution_v6(std::move(execution),
        logic_mod_worker_v1_internal::finish_logic_mod_worker_attempt_v6(attempt));
}
std::string logic_mod_run_record_v1(const LogicModRunV1& r) {
    return "BRUCE_LOGIC_RUN_V1\n" + field(r.accepted ? "1" : "0") + field(r.status) + field(r.result) + field(r.validator) + field(r.claim_scope) +
        field(r.binding_identity) + field(r.normalized_input) + field(r.budget_decision) + field(r.request_digest) + field(r.response_digest) + field(r.response_frame) + payload_fields(r) + output_fields(r);
}
LogicModRunV1 read_logic_mod_run_record_v1(std::string_view bytes) {
    if (bytes.size() > 2*frame_limit) throw Error("LogicRecordTooLarge");
    Fields f(bytes); f.literal("BRUCE_LOGIC_RUN_V1\n"); LogicModRunV1 r;
    const auto a = f.take(); if (a != "1" && a != "0") throw Error("LogicRecordInvalid"); r.accepted = a == "1";
    r.status = f.take(); r.result = f.take(); r.validator = f.take(); r.claim_scope = f.take(); r.binding_identity = f.take(); r.normalized_input = f.take();
    r.budget_decision = f.take(); r.request_digest = f.take(); r.response_digest = f.take(); r.response_frame = f.take();
    if (!f.empty()) {
        auto tag = f.take();
        if (tag == "1") {
            r.input_present = true; r.input_manifest_id = f.take(); r.input_object_index = natural(f.take());
            r.input_sha256 = f.take(); r.input_size_bytes = natural(f.take()); r.input_codec = f.take();
            (void)payload_fields(r);
            if (f.empty()) return r;
            tag = f.take();
        }
        if (tag != "output.v1" || f.take() != "1") throw Error("LogicRecordInvalid");
        r.output_present = true; r.output_manifest_id = f.take(); r.output_object_index = natural(f.take());
        r.output_sha256 = f.take(); r.output_size_bytes = natural(f.take()); r.output_codec = f.take();
        (void)output_fields(r);
    }
    f.end(); return r;
}
void verify_logic_mod_replay_v1(const LogicModRegistryV1& registry, std::string_view mod_id, std::string_view version, std::string_view template_id,
    std::string_view input, LogicModBudgetsV1 budgets, const LogicModRunV1& record, const std::optional<LogicModInputV1>& payload_input,
    const std::optional<LogicModOutputV1>& output) {
    if (record.input_present != payload_input.has_value() || record.output_present != output.has_value()) throw Error("LogicReplayMismatch");
    const auto payload = resolve_input(registry, mod_id, version, template_id, payload_input);
    const auto binding = bind_execution(registry, mod_id, version, template_id, "worker.execute", payload);
    if (output) (void)bind_output(registry, mod_id, version, template_id, binding);
    auto expected = request(binding, input, budgets, payload ? &*payload : nullptr);
    if (record.response_frame.size() > budgets.output_bytes) throw Error("LogicReplayMismatch");
    expected.response_frame = record.response_frame; expected.response_digest = sha256(expected.response_frame);
    accept_response(expected, template_id);
    if (output) {
        const LogicModPublishedOutputV1 published{record.output_manifest_id, record.output_object_index, record.output_sha256,
            record.output_size_bytes, record.output_codec};
        verify_logic_mod_result_artifact_v1(*output, published, expected.result);
        output_evidence(expected, published);
    }
    if (logic_mod_run_record_v1(expected) != logic_mod_run_record_v1(record)) throw Error("LogicReplayMismatch");
}
}
