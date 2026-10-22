#include "bruce/phase_dag/phase_dag_v3.hpp"

#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

int adapter_calls = 0;
std::vector<bruce::PhaseInputV3> received_inputs;
std::vector<std::string> adapter_inputs;
std::vector<std::string> adapter_invocations;

bruce::PhaseOutputRefV3 test_adapter(std::string_view request, std::span<const bruce::PhaseInputV3> inputs) {
    ++adapter_calls;
    adapter_invocations.emplace_back(request);
    received_inputs.assign(inputs.begin(), inputs.end());
    adapter_inputs.clear();
    for (const auto& input : inputs) adapter_inputs.push_back(input.producer_node_id);
    if (request == "F") throw bruce::Error("TestPhaseAdapterFailure");
    if (request == "F:std") throw std::runtime_error("ordinary exception");
    if (request == "F:unknown") throw 42;
    if (request == "F:large") throw bruce::Error(std::string(100000, 'e'));
    bruce::PhaseOutputStatusV3 status;
    if (request.starts_with("C:")) status = bruce::PhaseOutputStatusV3::complete;
    else if (request.starts_with("I:")) status = bruce::PhaseOutputStatusV3::incomplete;
    else if (request.starts_with("R:")) status = bruce::PhaseOutputStatusV3::rejected;
    else throw bruce::Error("InvalidTestRequest");
    const std::string output(request.substr(2));
    std::string payload = "payload:" + output;
    if (output == "boundary") payload.assign(65536, 'p');
    else if (output == "oversize") payload.assign(65537, 'p');
    else if (output == "empty") payload.clear();
    else if (output == "binary") payload = std::string("a\0:b\n", 6);
    const std::string evidence = "test-evidence:" + output;
    return {"phase-output-v3", status, output, bruce::sha256(output), evidence, bruce::sha256(evidence), payload, bruce::sha256(payload), "test-scope-v1"};
}

constexpr std::array<bruce::PhaseAdapterV3, 1> catalog{{{"test-adapter", "v1", test_adapter}}};

bruce::PhaseOutputRefV3 malformed_adapter(std::string_view request,
    std::span<const bruce::PhaseInputV3>) {
    const std::string output = "value";
    const std::string evidence = "evidence";
    bruce::PhaseOutputRefV3 value{"phase-output-v3", bruce::PhaseOutputStatusV3::complete,
        output, bruce::sha256(output), evidence, bruce::sha256(evidence), "payload:value", bruce::sha256("payload:value"), "test-scope-v1"};
    if (request == "version") value.output_version = "phase-output-v2";
    else if (request == "output-digest") value.output_digest = std::string(64, '0');
    else if (request == "evidence-digest") value.evidence_digest = std::string(64, '0');
    else if (request == "payload-digest") value.payload_digest = std::string(64, '0');
    else if (request == "scope") value.claim_scope = "invalid/scope";
    return value;
}

constexpr std::array<bruce::PhaseAdapterV3, 2> malformed_catalog{{
    {"malformed", "v1", malformed_adapter}, {"test-adapter", "v1", test_adapter}}};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class Action>
void rejected(Action action, const char* message) {
    bool caught = false;
    const int calls_before = adapter_calls;
    try {
        action();
    } catch (const bruce::Error&) {
        caught = true;
    }
    require(caught, message);
    require(adapter_calls == calls_before, "Validation invoked an adapter");
}

template<class Action>
void run_rejected(Action action, const char* message) {
    bool caught = false;
    try { action(); } catch (const bruce::Error&) { caught = true; }
    require(caught, message);
}

std::string rehashed(std::string bundle) {
    constexpr std::string_view header = "BRUCE_PHASE_DAG_RUN_V3\n";
    const auto offset = header.size() + 65;
    require(bundle.starts_with(header) && bundle.size() >= offset, "Invalid test bundle framing");
    bundle.replace(header.size(), 64, bruce::sha256(bundle.substr(offset)));
    return bundle;
}

std::string changed(std::string bundle, std::string_view before, std::string_view after,
    std::size_t occurrence = 1) {
    auto position = std::string::npos;
    for (std::size_t index = 0; index < occurrence; ++index) {
        position = bundle.find(before, position == std::string::npos ? 0 : position + before.size());
        require(position != std::string::npos, "Mutation field not found");
    }
    bundle.replace(position, before.size(), after);
    return rehashed(std::move(bundle));
}

bruce::PhaseNodeSpecV3 node(std::string id, std::vector<std::string> dependencies = {}) {
    bruce::PhaseNodeSpecV3 value;
    value.node_id = std::move(id);
    value.adapter_id = "test-adapter";
    value.adapter_version = "v1";
    value.dependencies = std::move(dependencies);
    value.request_bytes = "request";
    value.request_digest = bruce::sha256(value.request_bytes);
    return value;
}

bruce::PhaseDagSpecV3 valid_spec() {
    bruce::PhaseDagSpecV3 value;
    value.dag_id = "test-dag";
    value.nodes = {node("a")};
    value.max_started_nodes = 1;
    value.max_receipt_bytes = 4096;
    value.max_trace_entries = 8;
    return value;
}

bruce::PhaseNodeSpecV3 source(std::string id, std::string request, std::vector<std::string> dependencies = {}) {
    auto value = node(std::move(id), std::move(dependencies));
    value.request_bytes = std::move(request);
    value.request_digest = bruce::sha256(value.request_bytes);
    return value;
}

bruce::PhaseDagSpecV3 executable_spec(std::vector<bruce::PhaseNodeSpecV3> nodes,
    std::uint64_t starts = 16, std::uint64_t receipt_bytes = 4096) {
    auto value = valid_spec();
    value.nodes = std::move(nodes);
    value.max_started_nodes = starts;
    value.max_receipt_bytes = receipt_bytes;
    value.max_trace_entries = 64;
    return value;
}

// The instrumented adapter only shares synchronized observation state. Its
// output depends exclusively on the request and direct predecessor payloads.
std::mutex parallel_mutex;
std::condition_variable parallel_changed;
int parallel_active = 0, parallel_peak = 0, parallel_target = 1;
bool parallel_gate = false;
std::vector<std::string> parallel_calls;
std::vector<std::string> parallel_finished;

bruce::PhaseOutputRefV3 parallel_adapter(std::string_view request,
    std::span<const bruce::PhaseInputV3> inputs) {
    {
        std::unique_lock lock(parallel_mutex);
        ++parallel_active;
        parallel_peak = std::max(parallel_peak, parallel_active);
        parallel_calls.emplace_back(request);
        if (parallel_active >= parallel_target) parallel_gate = true;
        parallel_changed.notify_all();
        parallel_changed.wait_for(lock, std::chrono::seconds(2), [] { return parallel_gate; });
    }
    // Require a later ID to finish first, independently of thread timing.
    if (request == "a" || request == "fail") {
        std::unique_lock lock(parallel_mutex);
        if (parallel_target > 1) parallel_changed.wait_for(lock, std::chrono::seconds(2),
            [] { return !parallel_finished.empty(); });
    }
    std::string payload(request);
    for (const auto& input : inputs) payload += ":" + input.producer_payload_bytes;
    {
        std::lock_guard lock(parallel_mutex);
        --parallel_active;
        parallel_finished.emplace_back(request);
        parallel_changed.notify_all();
    }
    if (request == "fail") throw bruce::Error("ParallelTestFailure");
    return {"phase-output-v3", bruce::PhaseOutputStatusV3::complete,
        payload, bruce::sha256(payload), "evidence", bruce::sha256("evidence"),
        payload, bruce::sha256(payload), "test-scope-v1"};
}

constexpr std::array<bruce::PhaseAdapterV3, 1> parallel_catalog{{
    {"test-adapter", "v1", parallel_adapter}}};

void reset_parallel(int target = 1) {
    std::lock_guard lock(parallel_mutex);
    parallel_active = parallel_peak = 0;
    parallel_target = target;
    parallel_gate = false;
    parallel_calls.clear();
    parallel_finished.clear();
}

bruce::PhaseDagControlV3 parallel_control(unsigned count) {
    bruce::PhaseDagControlV3 control;
    control.max_parallel_nodes = count;
    return control;
}

void blocked_classification_does_not_check_cancellation() {
    const auto chain = executable_spec({source("a", "fail"),
        source("b", "b", {"a"}), source("c", "c", {"b"})}, 3);
    for (bool cancel_on_second_check : {false, true}) {
        reset_parallel();
        auto control = parallel_control(4);
        int checks = 0;
        control.cancelled = [&] { return ++checks > 1 && cancel_on_second_check; };
        const auto run = bruce::execute_phase_dag_v3(chain, parallel_catalog, control);
        require(checks == 1 && parallel_calls == std::vector<std::string>{"fail"},
            "BLOCKED classification consumed a cancellation check without a physical wave");
        require(run.receipts.size() == 3 &&
            run.receipts[0].outcome == bruce::PhaseReceiptOutcomeV3::failed &&
            run.receipts[1].outcome == bruce::PhaseReceiptOutcomeV3::blocked &&
            run.receipts[2].outcome == bruce::PhaseReceiptOutcomeV3::blocked,
            "Cancellation without a new physical wave changed BLOCKED descendants");
    }
}

void parallel_contracts() {
    const auto independent = executable_spec({source("a", "a"), source("b", "b"),
        source("c", "c"), source("d", "d"), source("e", "e")});
    reset_parallel();
    const auto default_bundle = bruce::pack_phase_dag_run_v3(
        bruce::execute_phase_dag_v3(independent, parallel_catalog));
    require(parallel_peak == 1, "Default scheduler ran concurrent adapters");
    reset_parallel();
    require(bruce::pack_phase_dag_run_v3(bruce::execute_phase_dag_v3(independent,
        parallel_catalog, parallel_control(1))) == default_bundle,
        "Explicit one changed default bytes");
    for (unsigned count : {2u, 4u}) {
        reset_parallel(static_cast<int>(count));
        const auto run = bruce::execute_phase_dag_v3(independent, parallel_catalog,
            parallel_control(count));
        require(parallel_peak == static_cast<int>(count) && parallel_active == 0,
            "Scheduler did not reach the configured bounded concurrency");
        require(parallel_finished.front() != "a", "Completion order fixture did not exercise reordered results");
        require(bruce::pack_phase_dag_run_v3(run) == default_bundle,
            "Parallel completion timing changed bundle bytes");
        reset_parallel();
        require(bruce::pack_phase_dag_run_v3(bruce::replay_phase_dag_run_v3(
            bruce::pack_phase_dag_run_v3(run), parallel_catalog)) == default_bundle,
            "Parallel run failed serial replay");
    }
    for (unsigned count : {0u, 5u}) {
        reset_parallel();
        run_rejected([&] { (void)bruce::execute_phase_dag_v3(independent, parallel_catalog,
            parallel_control(count)); }, "Invalid parallel limit accepted");
        require(parallel_calls.empty(), "Invalid parallel limit invoked adapters");
    }
    // The legacy logical order is b,a,c,d, even when b and c physically overlap.
    const auto multiwave = executable_spec({source("a", "a", {"b"}), source("b", "b"),
        source("c", "c"), source("d", "d", {"a", "c"})});
    reset_parallel();
    const auto serial = bruce::execute_phase_dag_v3(multiwave, parallel_catalog);
    std::vector<std::string> serial_starts;
    for (const auto& entry : serial.trace)
        if (entry.event == bruce::PhaseTraceEventV3::started) serial_starts.push_back(entry.node_id);
    require(serial_starts == std::vector<std::string>({"b", "a", "c", "d"}),
        "Serial scheduler changed the canonical logical start order");
    for (unsigned count : {2u, 4u}) {
        reset_parallel(2);
        const auto parallel = bruce::execute_phase_dag_v3(multiwave, parallel_catalog,
            parallel_control(count));
        require(parallel_peak == 2 && bruce::pack_phase_dag_run_v3(parallel) == bruce::pack_phase_dag_run_v3(serial),
            "Multiwave dependencies changed canonical receipts or trace");
    }
    for (unsigned budget : {0u, 1u, 2u, 3u}) {
        auto limited = multiwave;
        limited.max_started_nodes = budget;
        reset_parallel();
        const auto serial_limited = bruce::pack_phase_dag_run_v3(bruce::execute_phase_dag_v3(limited, parallel_catalog));
        for (unsigned count : {2u, 4u}) {
            reset_parallel();
            require(bruce::pack_phase_dag_run_v3(bruce::execute_phase_dag_v3(limited, parallel_catalog,
                parallel_control(count))) == serial_limited,
                "Parallel prefetch changed start-budget semantics");
        }
    }
    const auto failing = executable_spec({source("a", "fail"), source("b", "b", {"a"}),
        source("c", "c"), source("d", "d", {"c"})});
    reset_parallel();
    const auto serial_failure = bruce::pack_phase_dag_run_v3(bruce::execute_phase_dag_v3(failing, parallel_catalog));
    reset_parallel(2);
    const auto failure = bruce::execute_phase_dag_v3(failing, parallel_catalog,
        parallel_control(4));
    require(failure.receipts[0].outcome == bruce::PhaseReceiptOutcomeV3::failed &&
        failure.receipts[1].outcome == bruce::PhaseReceiptOutcomeV3::blocked &&
        failure.receipts[2].outcome == bruce::PhaseReceiptOutcomeV3::ready &&
        failure.receipts[3].outcome == bruce::PhaseReceiptOutcomeV3::ready &&
        bruce::pack_phase_dag_run_v3(failure) == serial_failure,
        "Parallel failure stopped an independent node or changed canonical bytes");
    reset_parallel(2);
    auto control = parallel_control(2);
    int checks = 0;
    control.cancelled = [&] { return ++checks > 1; };
    const auto cancelled = bruce::execute_phase_dag_v3(independent, parallel_catalog, control);
    require(checks == 2 && parallel_calls.size() == 2 && parallel_active == 0 &&
        cancelled.receipts[0].outcome == bruce::PhaseReceiptOutcomeV3::ready &&
        cancelled.receipts[1].outcome == bruce::PhaseReceiptOutcomeV3::ready &&
        cancelled.receipts[2].outcome == bruce::PhaseReceiptOutcomeV3::cancelled,
        "Cancellation interrupted a started wave or started a new one");
    reset_parallel();
    control.cancelled = [] { return true; };
    const auto pre_cancelled = bruce::execute_phase_dag_v3(independent, parallel_catalog, control);
    require(parallel_calls.empty() && pre_cancelled.receipts.back().outcome == bruce::PhaseReceiptOutcomeV3::cancelled,
        "Cancellation before the first wave invoked adapters");
}

} // namespace

int main() {
    try {
        blocked_classification_does_not_check_cancellation();
        parallel_contracts();
        auto spec = valid_spec();
        bruce::validate_phase_dag_spec_v3(spec, catalog);
        require(adapter_calls == 0, "Validating a valid spec invoked an adapter");

        spec.dag_version = "phase-dag-v2";
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Wrong DAG version accepted");
        spec = valid_spec();
        spec.dag_id = "invalid id";
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Invalid DAG ID accepted");
        spec = valid_spec();
        spec.nodes[0].node_id = "bad/id";
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Invalid node ID accepted");
        spec = valid_spec();
        spec.nodes[0].adapter_id = "bad/id";
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Invalid adapter ID accepted");
        spec = valid_spec();
        spec.nodes[0].adapter_version = "bad/version";
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Invalid adapter version accepted");
        spec = valid_spec();
        spec.nodes = {node("b"), node("a")};
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Unsorted node IDs accepted");
        spec.nodes = {node("a"), node("a")};
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Duplicate node IDs accepted");
        spec.nodes = {node("a"), node("b", {"a", "a"})};
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Duplicate dependencies accepted");
        spec.nodes = {node("a"), node("b"), node("c", {"b", "a"})};
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Unsorted dependencies accepted");
        spec.nodes = {node("a", {"bad/id"})};
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Invalid dependency ID accepted");
        spec.nodes = {node("a", {"a"})};
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Self edge accepted");
        spec.nodes = {node("a", {"missing"})};
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Unknown predecessor accepted");
        spec.nodes = {node("a", {"b"}), node("b", {"a"})};
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Cycle accepted");
        spec = valid_spec();
        spec.nodes[0].request_digest = std::string(64, '0');
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Wrong request digest accepted");
        spec = valid_spec();
        spec.nodes[0].request_bytes.assign(65537, 'x');
        spec.nodes[0].request_digest = bruce::sha256(spec.nodes[0].request_bytes);
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Oversized request accepted");
        spec = valid_spec();
        spec.nodes.clear();
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Empty graph accepted");
        spec = valid_spec();
        spec.nodes.clear();
        for (int index = 0; index < 17; ++index) spec.nodes.push_back(node(std::string(1, 'a' + index)));
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "17 nodes accepted");
        spec = valid_spec();
        spec.nodes.clear();
        for (int index = 0; index < 12; ++index) {
            std::vector<std::string> dependencies;
            for (int predecessor = 0; predecessor < index; ++predecessor)
                dependencies.push_back(std::string(1, 'a' + predecessor));
            spec.nodes.push_back(node(std::string(1, 'a' + index), dependencies));
        }
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "66 edges accepted");
        spec = valid_spec();
        spec.max_started_nodes = 17;
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Excess start budget accepted");
        spec = valid_spec();
        spec.max_receipt_bytes = 0;
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Zero receipt budget accepted");
        spec.max_receipt_bytes = 262145;
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Excess receipt budget accepted");
        spec = valid_spec();
        spec.max_trace_entries = 1;
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Too small trace budget accepted");
        spec.max_trace_entries = 513;
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Excess trace budget accepted");
        spec = valid_spec();
        spec.nodes[0].adapter_id = "unknown";
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Unknown adapter ID accepted");
        spec.nodes[0].adapter_id = "test-adapter";
        spec.nodes[0].adapter_version = "v2";
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, catalog); }, "Wrong adapter version accepted");
        spec = valid_spec();
        constexpr std::array duplicate_catalog{catalog[0], catalog[0]};
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, duplicate_catalog); }, "Duplicate catalog key accepted");
        constexpr std::array unsorted_catalog{bruce::PhaseAdapterV3{"z", "v1", test_adapter}, catalog[0]};
        rejected([&] { bruce::validate_phase_dag_spec_v3(spec, unsorted_catalog); }, "Unsorted catalog accepted");
        constexpr std::array sorted_catalog{bruce::PhaseAdapterV3{"a", "v1", test_adapter}, catalog[0],
            bruce::PhaseAdapterV3{"z", "v2", test_adapter}};
        bruce::validate_phase_dag_spec_v3(spec, sorted_catalog);
        const auto joined = bruce::execute_phase_dag_v3(executable_spec({
            source("a", "C:left"), source("b", "I:right"), source("c", "R:third"),
            source("d", "C:joined", {"a", "b", "c"})}), catalog);
        require(joined.receipts.size() == 4 && joined.receipts[0].node_id == "a" &&
            joined.receipts[1].node_id == "b" && joined.receipts[2].node_id == "c" &&
            joined.receipts[3].node_id == "d", "Scheduler did not start lexicographically or omitted a receipt");
        require(joined.receipts[1].outcome == bruce::PhaseReceiptOutcomeV3::ready &&
            joined.receipts[1].output->status == bruce::PhaseOutputStatusV3::incomplete &&
            joined.receipts[2].outcome == bruce::PhaseReceiptOutcomeV3::ready &&
            joined.receipts[2].output->status == bruce::PhaseOutputStatusV3::rejected,
            "Adapter status changed receipt outcome");
        require(adapter_inputs == std::vector<std::string>({"a", "b", "c"}) &&
            joined.receipts[3].input_refs[0].producer_node_id == "a" &&
            joined.receipts[3].input_refs[1].producer_node_id == "b" &&
            joined.receipts[3].input_refs[2].producer_node_id == "c",
            "Predecessor refs were not passed in dependency order");
        adapter_invocations.clear();
        const auto newly_ready = bruce::execute_phase_dag_v3(executable_spec({
            source("a", "C:a", {"b"}), source("b", "C:b"), source("c", "C:c")}), catalog);
        require(adapter_invocations == std::vector<std::string>({"C:b", "C:a", "C:c"}),
            "Newly ready a did not start before independent c");
        std::vector<std::string> started_nodes;
        for (const auto& entry : newly_ready.trace)
            if (entry.event == bruce::PhaseTraceEventV3::started) started_nodes.push_back(entry.node_id);
        require(started_nodes == std::vector<std::string>({"b", "a", "c"}),
            "STARTED trace did not rescan lexicographic readiness");
        const auto failure = bruce::execute_phase_dag_v3(executable_spec({
            source("a", "F"), source("b", "C:blocked", {"a"}), source("c", "C:independent")}), catalog);
        require(failure.receipts[0].outcome == bruce::PhaseReceiptOutcomeV3::failed &&
            failure.receipts[0].failure_code == "TestPhaseAdapterFailure" && !failure.receipts[0].output &&
            failure.receipts[1].outcome == bruce::PhaseReceiptOutcomeV3::blocked &&
            failure.receipts[2].outcome == bruce::PhaseReceiptOutcomeV3::ready,
            "Failure did not remain local to descendants");
        bool stop_rules = true;
        for (const bool use_control : {false, true}) {
            adapter_invocations.clear();
            const auto stopped = bruce::execute_phase_dag_v3(executable_spec({
                source("a", "F"), source("b", "C:blocked", {"a"}), source("c", "C:independent")},
                use_control ? 16 : 1), catalog,
                bruce::PhaseDagControlV3{[&] { return use_control && !adapter_invocations.empty(); }});
            require(adapter_invocations == std::vector<std::string>({"F"}),
                "Global stop started another adapter after failed a");
            const bool correct_remaining_outcomes = stopped.receipts.size() == 3 &&
                stopped.receipts[0].node_id == "a" &&
                stopped.receipts[0].outcome == bruce::PhaseReceiptOutcomeV3::failed &&
                stopped.receipts[1].node_id == "b" &&
                stopped.receipts[1].outcome == (use_control ? bruce::PhaseReceiptOutcomeV3::blocked :
                    bruce::PhaseReceiptOutcomeV3::cancelled) &&
                stopped.receipts[2].node_id == "c" &&
                stopped.receipts[2].outcome == bruce::PhaseReceiptOutcomeV3::cancelled;
            if (!correct_remaining_outcomes) {
                std::cerr << (use_control ? "Control" : "Start budget")
                    << " did not preserve dependency/budget stop outcomes\n";
                stop_rules = false;
            }
        }
        require(stop_rules, "Cancellation or start budget violated dependency stop rules");
        const auto overflow = bruce::execute_phase_dag_v3(executable_spec({
            source("a", "C:large"), source("b", "C:blocked", {"a"})}, 16, 1), catalog);
        require(overflow.receipts[0].outcome == bruce::PhaseReceiptOutcomeV3::failed &&
            !overflow.receipts[0].output && overflow.receipts[1].outcome == bruce::PhaseReceiptOutcomeV3::blocked,
            "Receipt limit exposed output or failed to block descendants");
        for (const std::string request : {"version", "output-digest", "evidence-digest", "payload-digest", "scope"}) {
            auto malformed = source("a", request);
            malformed.adapter_id = "malformed";
            const auto invalid_output = bruce::execute_phase_dag_v3(executable_spec({malformed}), malformed_catalog);
            require(invalid_output.receipts[0].outcome == bruce::PhaseReceiptOutcomeV3::failed &&
                !invalid_output.receipts[0].output &&
                invalid_output.receipts[0].failure_code == "InvalidPhaseAdapterOutput",
                "Invalid adapter output was published as ready");
        }
        const auto budget = bruce::execute_phase_dag_v3(executable_spec({
            source("a", "C:x"), source("b", "C:y")}, 0), catalog);
        require(budget.receipts[0].outcome == bruce::PhaseReceiptOutcomeV3::cancelled &&
            budget.receipts[1].outcome == bruce::PhaseReceiptOutcomeV3::cancelled &&
            budget.receipts[0].failure_code == "DagBudgetExhausted",
            "Zero start budget did not cancel nodes");
        int control_checks = 0;
        const auto controlled = bruce::execute_phase_dag_v3(executable_spec({
            source("a", "C:x"), source("b", "C:y")}), catalog,
            bruce::PhaseDagControlV3{[&] { return ++control_checks > 1; }});
        require(controlled.receipts[0].outcome == bruce::PhaseReceiptOutcomeV3::ready &&
            controlled.receipts[1].outcome == bruce::PhaseReceiptOutcomeV3::cancelled &&
            controlled.receipts[1].failure_code.empty(),
            "Control did not cancel before the second start");
        const auto controlled_bundle = bruce::pack_phase_dag_run_v3(controlled);
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v3(controlled_bundle, catalog); },
            "Externally controlled run replayed without control");
        const auto budget_bundle = bruce::pack_phase_dag_run_v3(budget);
        require(bruce::pack_phase_dag_run_v3(bruce::replay_phase_dag_run_v3(budget_bundle, catalog)) == budget_bundle,
            "Start-budget cancellation was not replayable");
        const auto replay_run = bruce::execute_phase_dag_v3(executable_spec({
            source("a", "C:left"), source("b", "C:right", {"a"})}), catalog);
        const auto bundle = bruce::pack_phase_dag_run_v3(replay_run);
        require(replay_run.receipts[1].input_refs[0].producer_payload_bytes == "payload:left" &&
            received_inputs.size() == 1 && received_inputs[0].producer_node_id == "a" &&
            received_inputs[0].producer_payload_bytes == "payload:left" &&
            received_inputs[0].producer_payload_digest == bruce::sha256("payload:left"),
            "Direct dependency did not transport exact named payload bytes");
        require(bundle.find("12:payload:left") != std::string::npos,
            "Bundle omitted full output/input payload bytes");
        require(bruce::pack_phase_dag_run_v3(bruce::execute_phase_dag_v3(replay_run.spec, catalog)) == bundle,
            "Repeated execution did not pack deterministically");
        const auto indirect = bruce::execute_phase_dag_v3(executable_spec({
            source("a", "C:left"), source("b", "C:right", {"a"}), source("c", "C:last", {"b"})}), catalog);
        require(received_inputs.size() == 1 && received_inputs[0].producer_node_id == "b" &&
            indirect.receipts.back().input_refs.size() == 1,
            "Adapter received an indirect predecessor payload");
        for (const auto& request : {"C:boundary", "C:empty", "C:binary"}) {
            const auto run = bruce::execute_phase_dag_v3(executable_spec({
                source("a", request), source("b", "C:consumer", {"a"})}, 16, 262144), catalog);
            require(run.receipts[0].outcome == bruce::PhaseReceiptOutcomeV3::ready &&
                run.receipts[1].outcome == bruce::PhaseReceiptOutcomeV3::ready,
                "Allowed payload size or binary/empty payload was rejected");
            if (std::string_view(request) == "C:boundary")
                require(run.receipts[0].output->payload_bytes.size() == 65536 &&
                    run.receipts[1].input_refs[0].producer_payload_bytes.size() == 65536,
                    "Boundary payload was truncated");
            const auto packed = bruce::pack_phase_dag_run_v3(run);
            require(bruce::pack_phase_dag_run_v3(bruce::replay_phase_dag_run_v3(packed, catalog)) == packed,
                "Allowed boundary/binary/empty payload failed replay");
        }
        adapter_invocations.clear();
        const auto oversized = bruce::execute_phase_dag_v3(executable_spec({
            source("a", "C:oversize"), source("b", "C:blocked", {"a"})}, 16, 262144), catalog);
        require(oversized.receipts[0].outcome == bruce::PhaseReceiptOutcomeV3::failed &&
            !oversized.receipts[0].output && oversized.receipts[1].outcome == bruce::PhaseReceiptOutcomeV3::blocked &&
            adapter_invocations == std::vector<std::string>{"C:oversize"},
            "65537-byte payload published or dependent adapter executed");
        const auto payload_budget = bruce::execute_phase_dag_v3(executable_spec({
            source("a", "C:boundary"), source("b", "C:blocked", {"a"})}, 16, 65000), catalog);
        require(payload_budget.receipts[0].outcome == bruce::PhaseReceiptOutcomeV3::failed &&
            payload_budget.receipts[0].failure_code == "DagReceiptBudgetExceeded" &&
            payload_budget.receipts[1].outcome == bruce::PhaseReceiptOutcomeV3::blocked,
            "Payload bytes did not count toward receipt budget");
        const auto input_budget = bruce::execute_phase_dag_v3(executable_spec({
            source("a", "C:boundary"), source("b", "C:boundary", {"a"})}, 16, 100000), catalog);
        require(input_budget.receipts[0].outcome == bruce::PhaseReceiptOutcomeV3::ready &&
            input_budget.receipts[1].outcome == bruce::PhaseReceiptOutcomeV3::failed &&
            input_budget.receipts[1].failure_code == "DagReceiptBudgetExceeded",
            "Input payload bytes did not count toward receipt budget");
        // Failure receipts must obey the same payload budget as successful receipts.
        for (const std::string failure_request : {"F", "F:std", "F:unknown", "F:large", "payload-digest"}) {
            auto failing_node = source("z", failure_request, {"a", "b"});
            if (failure_request == "payload-digest") failing_node.adapter_id = "malformed";
            const auto failure_run = bruce::execute_phase_dag_v3(executable_spec({
                source("a", "C:boundary"), source("b", "C:boundary"), failing_node}, 16, 70000), malformed_catalog);
            require(failure_run.receipts[0].outcome == bruce::PhaseReceiptOutcomeV3::ready &&
                failure_run.receipts[1].outcome == bruce::PhaseReceiptOutcomeV3::ready,
                "Failure-budget test producers did not become READY");
            const auto& failed_receipt = failure_run.receipts.back();
            require(failed_receipt.outcome == bruce::PhaseReceiptOutcomeV3::failed &&
                failed_receipt.input_refs.empty() && !failed_receipt.output &&
                failed_receipt.failure_code == "DagReceiptBudgetExceeded",
                "Adapter failure retained input payloads above the receipt budget");
            const auto packed_failure = bruce::pack_phase_dag_run_v3(failure_run);
            require(bruce::pack_phase_dag_run_v3(bruce::replay_phase_dag_run_v3(packed_failure, malformed_catalog)) == packed_failure,
                "Bounded failure receipt did not replay deterministically");
        }
        // Construct the old oversized failure representation with a larger legal budget,
        // then lower only the budget so pack/replay must independently reject it.
        const auto large_budget_run = bruce::execute_phase_dag_v3(executable_spec({
            source("a", "C:boundary"), source("b", "C:boundary"), source("z", "F", {"a", "b"})},
            16, 200000), catalog);
        require(large_budget_run.receipts.back().input_refs.size() == 2,
            "Within-budget failure lost valid input references");
        auto lowered_budget_run = large_budget_run;
        lowered_budget_run.spec.max_receipt_bytes = 70000;
        run_rejected([&] { (void)bruce::pack_phase_dag_run_v3(lowered_budget_run); },
            "Packing accepted an oversized failed receipt");
        const auto lowered_bundle = changed(bruce::pack_phase_dag_run_v3(large_budget_run), "6:200000", "5:70000");
        const auto replay_calls_before = adapter_calls;
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v3(lowered_bundle, catalog); },
            "Replay accepted an oversized failed receipt after rehash");
        require(adapter_calls == replay_calls_before,
            "Replay executed adapters before rejecting an oversized stored receipt");
        const auto minimal_budget_run = bruce::execute_phase_dag_v3(executable_spec({source("a", "C:value")}, 16, 1), catalog);
        const auto minimal_budget_bundle = bruce::pack_phase_dag_run_v3(minimal_budget_run);
        require(bruce::pack_phase_dag_run_v3(bruce::replay_phase_dag_run_v3(minimal_budget_bundle, catalog)) == minimal_budget_bundle,
            "Minimal budget fallback was no longer replayable");
        auto oversized_fallback = minimal_budget_run;
        oversized_fallback.receipts[0].node_id.assign(70001, 'z');
        run_rejected([&] { (void)bruce::pack_phase_dag_run_v3(oversized_fallback); },
            "Packing treated oversized arbitrary metadata as a minimal budget fallback");
        for (const std::size_t occurrence : {1, 2}) {
            run_rejected([&] { (void)bruce::replay_phase_dag_run_v3(
                changed(bundle, "12:payload:left", "12:payload:lift", occurrence), catalog); },
                "Rehashed output/input payload byte mutation accepted");
            run_rejected([&] { (void)bruce::replay_phase_dag_run_v3(
                changed(bundle, bruce::sha256("payload:left"), std::string(64, '0'), occurrence), catalog); },
                "Rehashed output/input payload digest mutation accepted");
        }
        auto consistently_forged = changed(bundle, "12:payload:left", "12:payload:lift");
        consistently_forged = changed(consistently_forged, bruce::sha256("payload:left"), bruce::sha256("payload:lift"));
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v3(consistently_forged, catalog); },
            "Internally rehashed forged payload bypassed execution comparison");
        auto v2_header = bundle;
        v2_header.replace(0, std::string_view("BRUCE_PHASE_DAG_RUN_V3").size(), "BRUCE_PHASE_DAG_RUN_V2");
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v3(v2_header, catalog); },
            "V2 header accepted by V3 replay");

        require(bundle.starts_with("BRUCE_PHASE_DAG_RUN_V3\n") &&
            bruce::pack_phase_dag_run_v3(bruce::replay_phase_dag_run_v3(bundle, catalog)) == bundle,
            "V3 replay did not preserve canonical bundle bytes");
        auto bad_header = bundle;
        bad_header.front() = 'X';
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v3(bad_header, catalog); },
            "Changed V3 header accepted");
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v3(bundle + "x", catalog); },
            "Trailing V3 bundle bytes accepted");
        const auto request = changed(bundle, "6:C:left", "6:C:lift");
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v3(request, catalog); },
            "Rehashed node request mutation accepted");
        const auto output = changed(bundle, "4:left", "4:lift");
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v3(output, catalog); },
            "Rehashed output byte mutation accepted");
        const auto outcome = changed(bundle, "5:READY", "6:FAILED");
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v3(outcome, catalog); },
            "Rehashed receipt outcome mutation accepted");
        const auto trace = changed(bundle, "9:VALIDATED", "9:CANCELLED");
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v3(trace, catalog); },
            "Rehashed trace event mutation accepted");
        const auto version = changed(bundle, "2:v1", "2:v2");
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v3(version, catalog); },
            "Unknown adapter version mutation accepted");
        const auto input_digest = changed(bundle, "64:" + bruce::sha256("left"),
            "64:" + std::string(64, '0'), 2);
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v3(input_digest, catalog); },
            "Rehashed input-ref digest mutation accepted");
        std::cout << "Phase DAG V3 validation contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
