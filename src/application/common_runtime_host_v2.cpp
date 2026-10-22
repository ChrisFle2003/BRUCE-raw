#include "bruce/application/common_runtime_host_v2.hpp"
#include "bruce/application/common_runtime_host_v2_acceptance.hpp"

#include "bruce/core/types.hpp"
#include "bruce/worker/worker_abort_control_v1.hpp"
#include "bruce/worker/worker_run_log_v1.hpp"
#include "bruce/worker/worker_supervisor_v1.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace bruce {
namespace {
using namespace worker_v1;

bool same_budget(const worker_v1::Budget& a, const worker_v1::Budget& b) {
    return a.max_wall_ms == b.max_wall_ms && a.max_input_bytes == b.max_input_bytes &&
        a.max_output_bytes == b.max_output_bytes && a.max_operations == b.max_operations &&
        a.max_memory_bytes == b.max_memory_bytes;
}
bool valid_budget(const worker_v1::Budget& b) {
    return b.max_wall_ms > 0 && b.max_wall_ms <= 600000 &&
        b.max_input_bytes > 0 && b.max_input_bytes <= 1048576 &&
        b.max_output_bytes > 0 && b.max_output_bytes <= 5242880 &&
        b.max_operations > 0 && b.max_operations <= 10000000 &&
        b.max_memory_bytes > 0 && b.max_memory_bytes <= 2147483648ULL;
}
bool token(std::string_view value) {
    return !value.empty() && value.size() <= 128 &&
        std::all_of(value.begin(), value.end(), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_';
        });
}
bool digest(std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
bool relative_ref(const std::filesystem::path& ref) {
    if (ref.empty() || ref.is_absolute() || ref.has_root_name()) return false;
    for (const auto& part : ref) if (part == "." || part == "..") return false;
    return true;
}
bool canonical_ready(const WorkerResult& result) {
    return result.status == WorkerStatus::READY &&
        result.failure_code == FailureCode::READY && result.output_artifact_ref &&
        result.payload_digest && result.output_artifact_ref->reference == "result" &&
        result.output_artifact_ref->size_bytes == result.payload_size &&
        result.output_artifact_ref->sha256 == *result.payload_digest;
}
bool valid_request(const WorkerRequest& r, const worker_v1::Budget& budget) {
    if (!valid_budget(budget) || !same_budget(r.budget, budget) || !token(r.run_id) ||
        !token(r.attempt_id) || !token(r.node_id) || !token(r.adapter_id) ||
        !token(r.adapter_version) || !digest(r.request_digest) ||
        r.input_artifact_refs.size() > 1024) return false;
    for (const auto& ref : r.input_artifact_refs)
        if (!relative_ref(ref.reference) || !digest(ref.sha256)) return false;
    return true;
}
std::optional<std::filesystem::path> canonical_host_executable(const std::filesystem::path& executable) {
    if (!executable.is_absolute()) return std::nullopt;
    std::error_code ec;
    const auto self = std::filesystem::canonical("/proc/self/exe", ec);
    if (ec) return std::nullopt;
    const auto candidate = std::filesystem::canonical(executable, ec);
    if (ec || candidate != self) return std::nullopt;
    return candidate;
}
bool private_root(const std::filesystem::path& root) {
    if (root.empty() || !root.is_absolute()) return false;
    struct stat st{};
    if (::lstat(root.c_str(), &st) == 0)
        return S_ISDIR(st.st_mode) && st.st_uid == ::geteuid() && (st.st_mode & 07777) == 0700;
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (ec || ::chmod(root.c_str(), 0700) != 0 || ::lstat(root.c_str(), &st) != 0)
        return false;
    return S_ISDIR(st.st_mode) && st.st_uid == ::geteuid() && (st.st_mode & 07777) == 0700;
}
std::string_view status_name(WorkerStatus status) {
    switch (status) {
    case WorkerStatus::READY: return "READY";
    case WorkerStatus::FAILED: return "FAILED";
    case WorkerStatus::BLOCKED: return "BLOCKED";
    case WorkerStatus::CANCELLED: return "CANCELLED";
    }
    throw Error("InvalidWorkerStatus");
}
std::uint64_t unix_ms() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}
CommonRuntimeHostV2Outcome failure(FailureCode code) {
    CommonRuntimeHostV2Outcome out;
    out.result.status = WorkerStatus::FAILED;
    out.result.failure_code = code;
    out.result.result_code = code == FailureCode::INVALID_REQUEST ? "INVALID_REQUEST" : "PROTOCOL_ERROR";
    out.host_exit_code = 1;
    return out;
}
std::pair<std::uint64_t, bool> existing_log_prefix(const std::filesystem::path& root) {
    const auto path = root / "control/run-log.jsonl";
    std::ifstream in(path, std::ios::binary);
    if (!in) return {0, false};
    std::string line;
    std::uint64_t seq{};
    bool created{};
    while (std::getline(in, line)) {
        const auto at = line.find("\"seq\":");
        if (at == std::string::npos) throw Error("PROTOCOL_ERROR");
        const auto begin = at + 6;
        const auto end = line.find(',', begin);
        if (end == std::string::npos) throw Error("PROTOCOL_ERROR");
        seq = integer<std::uint64_t>(std::string_view(line).substr(begin, end - begin));
        if (!created && line.find("\"event\":\"RUN_CREATED\"") != std::string::npos)
            created = true;
        else throw Error("PROTOCOL_ERROR");
    }
    return {seq, created};
}
} // namespace

CommonRuntimeHostV2AcceptanceDecision decide_common_runtime_host_v2_acceptance(
    const std::filesystem::path& run_root, WorkerResult result) {
    CommonRuntimeHostV2AcceptanceDecision decision;
    if (!canonical_ready(result)) {
        const auto usage = result.usage;
        result = failure(FailureCode::PROTOCOL_ERROR).result;
        result.usage = usage;
        decision.result = std::move(result);
        return decision;
    }

    const auto abort = probe_abort(run_root);
    if (abort == AbortProbeResult::NOT_REQUESTED) {
        decision.disposition = CommonRuntimeHostV2AcceptanceDisposition::ACCEPT;
        decision.result = std::move(result);
        decision.host_exit_code = 0;
        decision.append_artifact_accepted = true;
        return decision;
    }

    const auto usage = result.usage;
    if (abort == AbortProbeResult::REQUESTED) {
        result = WorkerResult{};
        result.status = WorkerStatus::CANCELLED;
        result.failure_code = FailureCode::CANCELLED;
        result.result_code = "CANCELLED";
        decision.disposition = CommonRuntimeHostV2AcceptanceDisposition::CANCEL;
        decision.append_cancel_requested = true;
    } else {
        result = failure(FailureCode::PROTOCOL_ERROR).result;
    }
    result.usage = usage;
    decision.result = std::move(result);
    return decision;
}

struct CommonRuntimeHostV2Run::State {
    std::filesystem::path root;
    std::string run_id;
    std::string attempt_id;
    std::optional<WorkerRunLog> log;
    std::optional<WorkerAttemptHandle> attempt;
    std::optional<CommonRuntimeHostV2Outcome> outcome;
    std::uint64_t seq{};
    bool cancel_logged{};
    bool abort_invalid{};

    void append(std::string event, std::string status, bool attempt_event = false,
                std::optional<ArtifactRef> artifact = {}, std::optional<WorkerUsage> usage = {}) {
        WorkerLogEvent record;
        record.seq = ++seq;
        record.ts_unix_ms = unix_ms();
        record.event = std::move(event);
        record.status = std::move(status);
        if (attempt_event) record.attempt_id = attempt_id;
        if (artifact) {
            record.reference = "attempts/" + attempt_id + "/output/result.bin";
            record.digest = artifact->sha256;
            record.size_bytes = artifact->size_bytes;
        }
        record.usage = usage;
        log->append(record);
    }
    void complete(WorkerResult result) {
        if (outcome) return;
        if (abort_invalid || cancel_logged) {
            const auto usage = result.usage;
            if (abort_invalid) result = failure(FailureCode::PROTOCOL_ERROR).result;
            else {
                result = WorkerResult{};
                result.status = WorkerStatus::CANCELLED;
                result.failure_code = FailureCode::CANCELLED;
                result.result_code = "CANCELLED";
            }
            result.usage = usage;
        }
        bool ready = canonical_ready(result);
        if (result.status == WorkerStatus::READY && !ready) {
            const auto usage = result.usage;
            result = failure(FailureCode::PROTOCOL_ERROR).result;
            result.usage = usage;
        }
        if (ready) {
            auto decision = decide_common_runtime_host_v2_acceptance(root, std::move(result));
            if (decision.append_cancel_requested && !cancel_logged) {
                append("CANCEL_REQUESTED", "CANCELLED", true);
                cancel_logged = true;
                request_cancel(*attempt);
            }
            result = std::move(decision.result);
            ready = decision.append_artifact_accepted;
        }
        if (!ready) {
            result.output_artifact_ref.reset();
            result.payload_digest.reset();
            result.payload_size = 0;
        }
        const auto status = std::string(status_name(result.status));
        const auto artifact = ready ? result.output_artifact_ref : std::nullopt;
        append("PROCESS_EXITED", status, true, {}, result.usage);
        append("RESULT_RECEIVED", status, true, artifact, result.usage);
        if (ready) append("ARTIFACT_ACCEPTED", status, true, artifact, result.usage);
        append("RUN_FINISHED", status, false, {}, result.usage);
        outcome = CommonRuntimeHostV2Outcome{std::move(result), ready ? 0 : 1};
    }
};

CommonRuntimeHostV2Run::CommonRuntimeHostV2Run() = default;
CommonRuntimeHostV2Run::CommonRuntimeHostV2Run(std::unique_ptr<State> state) : state_(std::move(state)) {}
CommonRuntimeHostV2Run::~CommonRuntimeHostV2Run() = default;
CommonRuntimeHostV2Run::CommonRuntimeHostV2Run(CommonRuntimeHostV2Run&&) noexcept = default;
CommonRuntimeHostV2Run& CommonRuntimeHostV2Run::operator=(CommonRuntimeHostV2Run&&) noexcept = default;

CommonRuntimeHostV2Run start_single_worker_v2(const CommonRuntimeHostV2Request& request,
                                               const CommonRuntimeHostV2Context& context) {
    auto state = std::make_unique<CommonRuntimeHostV2Run::State>();
    const auto& worker = request.worker_request;
    const auto canonical_exe = canonical_host_executable(context.worker_executable);
    if (!valid_request(worker, context.budget) || context.grace_ms != 1000 ||
        !canonical_exe || !private_root(context.run_root)) {
        state->outcome = failure(FailureCode::INVALID_REQUEST);
        return CommonRuntimeHostV2Run(std::move(state));
    }
    state->root = std::filesystem::canonical(context.run_root);
    state->run_id = worker.run_id;
    state->attempt_id = worker.attempt_id;
    state->log.emplace(WorkerRunLog::create(state->root, worker.run_id));
    const auto [seq, created] = existing_log_prefix(state->root);
    state->seq = seq;
    if (!created) state->append("RUN_CREATED", "READY");
    state->append("ATTEMPT_STARTED", "RUNNING", true);
    state->append("STATUS", "RUNNING", true);
    state->attempt.emplace(start_attempt(worker, SupervisorOptions{
        *canonical_exe, state->root, context.budget, context.grace_ms}));
    return CommonRuntimeHostV2Run(std::move(state));
}

std::optional<CommonRuntimeHostV2Outcome> poll_single_worker_v2(CommonRuntimeHostV2Run& run) {
    if (!run.state_) throw Error("InvalidCommonRuntimeHostV2Run");
    auto& state = *run.state_;
    if (state.outcome) return state.outcome;
    if (!state.cancel_logged) {
        const auto abort = probe_abort(state.root);
        if (abort == AbortProbeResult::REQUESTED) request_cancel_v2(run);
        else if (abort == AbortProbeResult::INVALID) {
            state.abort_invalid = true;
            request_cancel(*state.attempt);
            if (!state.outcome) state.complete(finish_attempt(*state.attempt).result);
            return state.outcome;
        }
    }
    if (auto outcome = poll_attempt(*state.attempt)) {
        state.complete(std::move(outcome->result));
        return state.outcome;
    }
    return state.outcome;
}

CommonRuntimeHostV2Outcome finish_single_worker_v2(CommonRuntimeHostV2Run& run) {
    for (;;) {
        if (auto outcome = poll_single_worker_v2(run)) return *outcome;
        ::usleep(1000);
    }
}

void request_cancel_v2(CommonRuntimeHostV2Run& run) {
    if (!run.state_) throw Error("InvalidCommonRuntimeHostV2Run");
    auto& state = *run.state_;
    if (state.outcome || state.cancel_logged) return;
    state.append("CANCEL_REQUESTED", "CANCELLED", true);
    state.cancel_logged = true;
    request_cancel(*state.attempt);
}

} // namespace bruce
