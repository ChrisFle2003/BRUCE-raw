#include "bruce/phase_dag/phase_dag_v2.hpp"

#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

int adapter_calls = 0;
std::vector<std::string> adapter_inputs;
std::vector<std::string> adapter_invocations;

bruce::PhaseOutputRefV2 test_adapter(std::string_view request, std::span<const bruce::PhaseInputRefV2> inputs) {
    ++adapter_calls;
    adapter_invocations.emplace_back(request);
    adapter_inputs.clear();
    for (const auto& input : inputs) adapter_inputs.push_back(input.producer_node_id);
    if (request == "F") throw bruce::Error("TestPhaseAdapterFailure");
    bruce::PhaseOutputStatusV2 status;
    if (request.starts_with("C:")) status = bruce::PhaseOutputStatusV2::complete;
    else if (request.starts_with("I:")) status = bruce::PhaseOutputStatusV2::incomplete;
    else if (request.starts_with("R:")) status = bruce::PhaseOutputStatusV2::rejected;
    else throw bruce::Error("InvalidTestRequest");
    const std::string output(request.substr(2));
    const std::string evidence = "test-evidence:" + output;
    return {"phase-output-v2", status, output, bruce::sha256(output), evidence, bruce::sha256(evidence), "test-scope-v1"};
}

constexpr std::array<bruce::PhaseAdapterV2, 1> catalog{{{"test-adapter", "v1", test_adapter}}};

bruce::PhaseOutputRefV2 malformed_adapter(std::string_view request,
    std::span<const bruce::PhaseInputRefV2>) {
    const std::string output = "value";
    const std::string evidence = "evidence";
    bruce::PhaseOutputRefV2 value{"phase-output-v2", bruce::PhaseOutputStatusV2::complete,
        output, bruce::sha256(output), evidence, bruce::sha256(evidence), "test-scope-v1"};
    if (request == "version") value.output_version = "phase-output-v1";
    else if (request == "output-digest") value.output_digest = std::string(64, '0');
    else if (request == "evidence-digest") value.evidence_digest = std::string(64, '0');
    else if (request == "scope") value.claim_scope = "invalid/scope";
    return value;
}

constexpr std::array<bruce::PhaseAdapterV2, 2> malformed_catalog{{
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
    constexpr std::string_view header = "BRUCE_PHASE_DAG_RUN_V2\n";
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

bruce::PhaseNodeSpecV2 node(std::string id, std::vector<std::string> dependencies = {}) {
    bruce::PhaseNodeSpecV2 value;
    value.node_id = std::move(id);
    value.adapter_id = "test-adapter";
    value.adapter_version = "v1";
    value.dependencies = std::move(dependencies);
    value.request_bytes = "request";
    value.request_digest = bruce::sha256(value.request_bytes);
    return value;
}

bruce::PhaseDagSpecV2 valid_spec() {
    bruce::PhaseDagSpecV2 value;
    value.dag_id = "test-dag";
    value.nodes = {node("a")};
    value.max_started_nodes = 1;
    value.max_receipt_bytes = 4096;
    value.max_trace_entries = 8;
    return value;
}

bruce::PhaseNodeSpecV2 source(std::string id, std::string request, std::vector<std::string> dependencies = {}) {
    auto value = node(std::move(id), std::move(dependencies));
    value.request_bytes = std::move(request);
    value.request_digest = bruce::sha256(value.request_bytes);
    return value;
}

bruce::PhaseDagSpecV2 executable_spec(std::vector<bruce::PhaseNodeSpecV2> nodes,
    std::uint64_t starts = 16, std::uint64_t receipt_bytes = 4096) {
    auto value = valid_spec();
    value.nodes = std::move(nodes);
    value.max_started_nodes = starts;
    value.max_receipt_bytes = receipt_bytes;
    value.max_trace_entries = 64;
    return value;
}

} // namespace

int main() {
    try {
        auto spec = valid_spec();
        bruce::validate_phase_dag_spec_v2(spec, catalog);
        require(adapter_calls == 0, "Validating a valid spec invoked an adapter");

        spec.dag_version = "phase-dag-v1";
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Wrong DAG version accepted");
        spec = valid_spec();
        spec.dag_id = "invalid id";
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Invalid DAG ID accepted");
        spec = valid_spec();
        spec.nodes[0].node_id = "bad/id";
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Invalid node ID accepted");
        spec = valid_spec();
        spec.nodes[0].adapter_id = "bad/id";
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Invalid adapter ID accepted");
        spec = valid_spec();
        spec.nodes[0].adapter_version = "bad/version";
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Invalid adapter version accepted");
        spec = valid_spec();
        spec.nodes = {node("b"), node("a")};
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Unsorted node IDs accepted");
        spec.nodes = {node("a"), node("a")};
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Duplicate node IDs accepted");
        spec.nodes = {node("a"), node("b", {"a", "a"})};
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Duplicate dependencies accepted");
        spec.nodes = {node("a"), node("b"), node("c", {"b", "a"})};
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Unsorted dependencies accepted");
        spec.nodes = {node("a", {"bad/id"})};
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Invalid dependency ID accepted");
        spec.nodes = {node("a", {"a"})};
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Self edge accepted");
        spec.nodes = {node("a", {"missing"})};
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Unknown predecessor accepted");
        spec.nodes = {node("a", {"b"}), node("b", {"a"})};
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Cycle accepted");
        spec = valid_spec();
        spec.nodes[0].request_digest = std::string(64, '0');
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Wrong request digest accepted");
        spec = valid_spec();
        spec.nodes[0].request_bytes.assign(65537, 'x');
        spec.nodes[0].request_digest = bruce::sha256(spec.nodes[0].request_bytes);
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Oversized request accepted");
        spec = valid_spec();
        spec.nodes.clear();
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Empty graph accepted");
        spec = valid_spec();
        spec.nodes.clear();
        for (int index = 0; index < 17; ++index) spec.nodes.push_back(node(std::string(1, 'a' + index)));
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "17 nodes accepted");
        spec = valid_spec();
        spec.nodes.clear();
        for (int index = 0; index < 12; ++index) {
            std::vector<std::string> dependencies;
            for (int predecessor = 0; predecessor < index; ++predecessor)
                dependencies.push_back(std::string(1, 'a' + predecessor));
            spec.nodes.push_back(node(std::string(1, 'a' + index), dependencies));
        }
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "66 edges accepted");
        spec = valid_spec();
        spec.max_started_nodes = 17;
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Excess start budget accepted");
        spec = valid_spec();
        spec.max_receipt_bytes = 0;
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Zero receipt budget accepted");
        spec.max_receipt_bytes = 262145;
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Excess receipt budget accepted");
        spec = valid_spec();
        spec.max_trace_entries = 1;
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Too small trace budget accepted");
        spec.max_trace_entries = 513;
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Excess trace budget accepted");
        spec = valid_spec();
        spec.nodes[0].adapter_id = "unknown";
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Unknown adapter ID accepted");
        spec.nodes[0].adapter_id = "test-adapter";
        spec.nodes[0].adapter_version = "v2";
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, catalog); }, "Wrong adapter version accepted");
        spec = valid_spec();
        constexpr std::array duplicate_catalog{catalog[0], catalog[0]};
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, duplicate_catalog); }, "Duplicate catalog key accepted");
        constexpr std::array unsorted_catalog{bruce::PhaseAdapterV2{"z", "v1", test_adapter}, catalog[0]};
        rejected([&] { bruce::validate_phase_dag_spec_v2(spec, unsorted_catalog); }, "Unsorted catalog accepted");
        constexpr std::array sorted_catalog{bruce::PhaseAdapterV2{"a", "v1", test_adapter}, catalog[0],
            bruce::PhaseAdapterV2{"z", "v2", test_adapter}};
        bruce::validate_phase_dag_spec_v2(spec, sorted_catalog);
        const auto joined = bruce::execute_phase_dag_v2(executable_spec({
            source("a", "C:left"), source("b", "I:right"), source("c", "R:third"),
            source("d", "C:joined", {"a", "b", "c"})}), catalog);
        require(joined.receipts.size() == 4 && joined.receipts[0].node_id == "a" &&
            joined.receipts[1].node_id == "b" && joined.receipts[2].node_id == "c" &&
            joined.receipts[3].node_id == "d", "Scheduler did not start lexicographically or omitted a receipt");
        require(joined.receipts[1].outcome == bruce::PhaseReceiptOutcomeV2::ready &&
            joined.receipts[1].output->status == bruce::PhaseOutputStatusV2::incomplete &&
            joined.receipts[2].outcome == bruce::PhaseReceiptOutcomeV2::ready &&
            joined.receipts[2].output->status == bruce::PhaseOutputStatusV2::rejected,
            "Adapter status changed receipt outcome");
        require(adapter_inputs == std::vector<std::string>({"a", "b", "c"}) &&
            joined.receipts[3].input_refs[0].producer_node_id == "a" &&
            joined.receipts[3].input_refs[1].producer_node_id == "b" &&
            joined.receipts[3].input_refs[2].producer_node_id == "c",
            "Predecessor refs were not passed in dependency order");
        adapter_invocations.clear();
        const auto newly_ready = bruce::execute_phase_dag_v2(executable_spec({
            source("a", "C:a", {"b"}), source("b", "C:b"), source("c", "C:c")}), catalog);
        require(adapter_invocations == std::vector<std::string>({"C:b", "C:a", "C:c"}),
            "Newly ready a did not start before independent c");
        std::vector<std::string> started_nodes;
        for (const auto& entry : newly_ready.trace)
            if (entry.event == bruce::PhaseTraceEventV2::started) started_nodes.push_back(entry.node_id);
        require(started_nodes == std::vector<std::string>({"b", "a", "c"}),
            "STARTED trace did not rescan lexicographic readiness");
        const auto failure = bruce::execute_phase_dag_v2(executable_spec({
            source("a", "F"), source("b", "C:blocked", {"a"}), source("c", "C:independent")}), catalog);
        require(failure.receipts[0].outcome == bruce::PhaseReceiptOutcomeV2::failed &&
            failure.receipts[0].failure_code == "TestPhaseAdapterFailure" && !failure.receipts[0].output &&
            failure.receipts[1].outcome == bruce::PhaseReceiptOutcomeV2::blocked &&
            failure.receipts[2].outcome == bruce::PhaseReceiptOutcomeV2::ready,
            "Failure did not remain local to descendants");
        bool global_stop_precedence = true;
        for (const bool use_control : {false, true}) {
            adapter_invocations.clear();
            const auto stopped = bruce::execute_phase_dag_v2(executable_spec({
                source("a", "F"), source("b", "C:blocked", {"a"}), source("c", "C:independent")},
                use_control ? 16 : 1), catalog,
                bruce::PhaseDagControlV2{[&] { return use_control && !adapter_invocations.empty(); }});
            require(adapter_invocations == std::vector<std::string>({"F"}),
                "Global stop started another adapter after failed a");
            const bool cancelled_remaining = stopped.receipts.size() == 3 &&
                stopped.receipts[0].node_id == "a" &&
                stopped.receipts[0].outcome == bruce::PhaseReceiptOutcomeV2::failed &&
                stopped.receipts[1].node_id == "b" &&
                stopped.receipts[1].outcome == bruce::PhaseReceiptOutcomeV2::cancelled &&
                stopped.receipts[2].node_id == "c" &&
                stopped.receipts[2].outcome == bruce::PhaseReceiptOutcomeV2::cancelled;
            if (!cancelled_remaining) {
                std::cerr << (use_control ? "Control" : "Start budget")
                    << " did not cancel failed a's dependent b and independent c\n";
                global_stop_precedence = false;
            }
        }
        require(global_stop_precedence, "Global stop did not precede dependency blocking");
        const auto overflow = bruce::execute_phase_dag_v2(executable_spec({
            source("a", "C:large"), source("b", "C:blocked", {"a"})}, 16, 1), catalog);
        require(overflow.receipts[0].outcome == bruce::PhaseReceiptOutcomeV2::failed &&
            !overflow.receipts[0].output && overflow.receipts[1].outcome == bruce::PhaseReceiptOutcomeV2::blocked,
            "Receipt limit exposed output or failed to block descendants");
        for (const std::string request : {"version", "output-digest", "evidence-digest", "scope"}) {
            auto malformed = source("a", request);
            malformed.adapter_id = "malformed";
            const auto invalid_output = bruce::execute_phase_dag_v2(executable_spec({malformed}), malformed_catalog);
            require(invalid_output.receipts[0].outcome == bruce::PhaseReceiptOutcomeV2::failed &&
                !invalid_output.receipts[0].output &&
                invalid_output.receipts[0].failure_code == "InvalidPhaseAdapterOutput",
                "Invalid adapter output was published as ready");
        }
        const auto budget = bruce::execute_phase_dag_v2(executable_spec({
            source("a", "C:x"), source("b", "C:y")}, 0), catalog);
        require(budget.receipts[0].outcome == bruce::PhaseReceiptOutcomeV2::cancelled &&
            budget.receipts[1].outcome == bruce::PhaseReceiptOutcomeV2::cancelled &&
            budget.receipts[0].failure_code == "DagBudgetExhausted",
            "Zero start budget did not cancel nodes");
        int control_checks = 0;
        const auto controlled = bruce::execute_phase_dag_v2(executable_spec({
            source("a", "C:x"), source("b", "C:y")}), catalog,
            bruce::PhaseDagControlV2{[&] { return ++control_checks > 1; }});
        require(controlled.receipts[0].outcome == bruce::PhaseReceiptOutcomeV2::ready &&
            controlled.receipts[1].outcome == bruce::PhaseReceiptOutcomeV2::cancelled &&
            controlled.receipts[1].failure_code.empty(),
            "Control did not cancel before the second start");
        const auto controlled_bundle = bruce::pack_phase_dag_run_v2(controlled);
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v2(controlled_bundle, catalog); },
            "Externally controlled run replayed without control");
        const auto budget_bundle = bruce::pack_phase_dag_run_v2(budget);
        require(bruce::pack_phase_dag_run_v2(bruce::replay_phase_dag_run_v2(budget_bundle, catalog)) == budget_bundle,
            "Start-budget cancellation was not replayable");
        const auto replay_run = bruce::execute_phase_dag_v2(executable_spec({
            source("a", "C:left"), source("b", "C:right", {"a"})}), catalog);
        const auto bundle = bruce::pack_phase_dag_run_v2(replay_run);
        require(bundle.starts_with("BRUCE_PHASE_DAG_RUN_V2\n") &&
            bruce::pack_phase_dag_run_v2(bruce::replay_phase_dag_run_v2(bundle, catalog)) == bundle,
            "V2 replay did not preserve canonical bundle bytes");
        auto bad_header = bundle;
        bad_header.front() = 'X';
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v2(bad_header, catalog); },
            "Changed V2 header accepted");
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v2(bundle + "x", catalog); },
            "Trailing V2 bundle bytes accepted");
        const auto request = changed(bundle, "6:C:left", "6:C:lift");
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v2(request, catalog); },
            "Rehashed node request mutation accepted");
        const auto output = changed(bundle, "4:left", "4:lift");
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v2(output, catalog); },
            "Rehashed output byte mutation accepted");
        const auto outcome = changed(bundle, "5:READY", "6:FAILED");
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v2(outcome, catalog); },
            "Rehashed receipt outcome mutation accepted");
        const auto trace = changed(bundle, "9:VALIDATED", "9:CANCELLED");
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v2(trace, catalog); },
            "Rehashed trace event mutation accepted");
        const auto version = changed(bundle, "2:v1", "2:v2");
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v2(version, catalog); },
            "Unknown adapter version mutation accepted");
        const auto input_digest = changed(bundle, "64:" + bruce::sha256("left"),
            "64:" + std::string(64, '0'), 2);
        run_rejected([&] { (void)bruce::replay_phase_dag_run_v2(input_digest, catalog); },
            "Rehashed input-ref digest mutation accepted");
        std::cout << "Phase DAG V2 validation contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
