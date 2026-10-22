#include "bruce/phase_dag/phase_dag.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw bruce::Error(message);
}

template<class Callback>
void rejected(Callback callback, const char* message) {
    bool caught = false;
    try {
        callback();
    } catch (const bruce::Error&) {
        caught = true;
    }
    require(caught, message);
}

bruce::PhaseNodeSpecV1 node(std::string id, std::vector<std::string> dependencies = {}) {
    bruce::PhaseNodeSpecV1 value;
    value.node_id = std::move(id);
    value.adapter_id = "fixture-source-v1";
    value.adapter_version = "v1";
    value.dependencies = std::move(dependencies);
    value.request_bytes = "C:value";
    value.request_digest = bruce::sha256(value.request_bytes);
    return value;
}

bruce::PhaseNodeSpecV1 source(std::string id, std::string request, std::vector<std::string> dependencies = {}) {
    auto value = node(std::move(id), std::move(dependencies));
    value.request_bytes = std::move(request);
    value.request_digest = bruce::sha256(value.request_bytes);
    return value;
}

bruce::PhaseNodeSpecV1 join(std::string id, std::vector<std::string> dependencies) {
    auto value = node(std::move(id), std::move(dependencies));
    value.adapter_id = "fixture-join-v1";
    value.request_bytes.clear();
    value.request_digest = bruce::sha256(value.request_bytes);
    return value;
}

bruce::PhaseDagSpecV1 valid_spec() {
    bruce::PhaseDagSpecV1 value;
    value.dag_id = "validation";
    value.nodes = {node("a"), node("b", {"a"})};
    value.max_started_nodes = 2;
    value.max_receipt_bytes = 4096;
    value.max_trace_entries = 8;
    return value;
}

bruce::PhaseDagSpecV1 executable_spec(std::vector<bruce::PhaseNodeSpecV1> nodes, std::uint64_t max_started = 16,
    std::uint64_t max_receipt_bytes = 4096, std::uint64_t max_trace_entries = 64) {
    bruce::PhaseDagSpecV1 value;
    value.dag_id = "scheduler";
    value.nodes = std::move(nodes);
    value.max_started_nodes = max_started;
    value.max_receipt_bytes = max_receipt_bytes;
    value.max_trace_entries = max_trace_entries;
    return value;
}

std::string rehashed(std::string bundle) {
    constexpr std::string_view header = "BRUCE_PHASE_DAG_RUN_V1\n";
    const auto payload_offset = header.size() + 65;
    require(bundle.starts_with(header) && bundle.size() >= payload_offset, "Invalid test bundle framing");
    bundle.replace(header.size(), 64, bruce::sha256(bundle.substr(payload_offset)));
    return bundle;
}
}

int main() {
    try {
        using namespace bruce;
        validate_phase_dag_spec_v1(valid_spec());

        auto wrong_version = valid_spec();
        wrong_version.dag_version = "phase-dag-v0";
        rejected([&] { validate_phase_dag_spec_v1(wrong_version); }, "Wrong version accepted");

        auto invalid_id = valid_spec();
        invalid_id.nodes.front().node_id = "not valid";
        rejected([&] { validate_phase_dag_spec_v1(invalid_id); }, "Invalid node id accepted");

        auto wrong_digest = valid_spec();
        wrong_digest.nodes.front().request_digest = std::string(64, '0');
        rejected([&] { validate_phase_dag_spec_v1(wrong_digest); }, "Wrong request digest accepted");

        auto unsorted_nodes = valid_spec();
        std::swap(unsorted_nodes.nodes[0], unsorted_nodes.nodes[1]);
        rejected([&] { validate_phase_dag_spec_v1(unsorted_nodes); }, "Unsorted nodes accepted");

        auto duplicate_dependency = valid_spec();
        duplicate_dependency.nodes[1].dependencies = {"a", "a"};
        rejected([&] { validate_phase_dag_spec_v1(duplicate_dependency); }, "Duplicate dependency accepted");

        auto self_edge = valid_spec();
        self_edge.nodes[0].dependencies = {"a"};
        rejected([&] { validate_phase_dag_spec_v1(self_edge); }, "Self edge accepted");

        auto unknown_predecessor = valid_spec();
        unknown_predecessor.nodes[1].dependencies = {"missing"};
        rejected([&] { validate_phase_dag_spec_v1(unknown_predecessor); }, "Unknown predecessor accepted");

        auto cycle = valid_spec();
        cycle.nodes[0].dependencies = {"b"};
        rejected([&] { validate_phase_dag_spec_v1(cycle); }, "Cycle accepted");

        const auto joined = execute_phase_dag_v1(executable_spec({
            source("a", "C:left"), source("b", "C:right"), join("c", {"a", "b"})}));
        require(joined.receipts.size() == 3, "Scheduler did not create one receipt per node");
        require(joined.receipts[0].node_id == "a" && joined.receipts[1].node_id == "b" &&
            joined.receipts[2].node_id == "c", "Scheduler order changed");
        require(joined.receipts[2].outcome == PhaseReceiptOutcomeV1::ready,
            "Join receipt is not ready");
        require(joined.receipts[2].input_refs[0].producer_node_id == "a" &&
            joined.receipts[2].input_refs[1].producer_node_id == "b", "Join input order changed");
        require(joined.receipts[2].output->output_bytes == "left|right", "Join output changed");

        const auto partial = execute_phase_dag_v1(executable_spec({source("a", "I:x"), source("b", "R:x")}));
        require(partial.receipts[0].outcome == PhaseReceiptOutcomeV1::ready &&
            partial.receipts[0].output->status == PhaseOutputStatusV1::incomplete,
            "Incomplete output was not retained as ready");
        require(partial.receipts[1].outcome == PhaseReceiptOutcomeV1::ready &&
            partial.receipts[1].output->status == PhaseOutputStatusV1::rejected,
            "Rejected output was not retained as ready");

        const auto failure = execute_phase_dag_v1(executable_spec({
            source("a", "F"), source("b", "C:blocked", {"a"}), source("c", "C:independent")}));
        require(failure.receipts[0].outcome == PhaseReceiptOutcomeV1::failed &&
            failure.receipts[1].outcome == PhaseReceiptOutcomeV1::blocked &&
            failure.receipts[2].outcome == PhaseReceiptOutcomeV1::ready,
            "Failure did not remain local to dependent nodes");

        const auto start_budget = execute_phase_dag_v1(executable_spec({source("a", "C:x"), source("b", "C:y")}, 0));
        require(start_budget.receipts[0].outcome == PhaseReceiptOutcomeV1::cancelled &&
            start_budget.receipts[1].outcome == PhaseReceiptOutcomeV1::cancelled,
            "Start budget did not cancel unresolved nodes");

        int control_checks = 0;
        const auto controlled = execute_phase_dag_v1(executable_spec({source("a", "C:x"), source("b", "C:y")}),
            PhaseDagControlV1{[&] { return ++control_checks > 1; }});
        require(controlled.receipts[0].outcome == PhaseReceiptOutcomeV1::ready &&
            controlled.receipts[1].outcome == PhaseReceiptOutcomeV1::cancelled,
            "Cancellation control did not dominate later starts");

        const auto overflow = execute_phase_dag_v1(executable_spec({
            source("a", "C:too-large"), source("b", "C:blocked", {"a"})}, 16, 1));
        require(overflow.receipts[0].outcome == PhaseReceiptOutcomeV1::failed &&
            !overflow.receipts[0].output && overflow.receipts[0].failure_code == "DagReceiptBudgetExceeded" &&
            overflow.receipts[1].outcome == PhaseReceiptOutcomeV1::blocked,
            "Receipt overflow exposed an output or failed to block its dependent");

        const auto replay_run = execute_phase_dag_v1(executable_spec({
            source("a", "C:left"), source("b", "C:right"), join("c", {"a", "b"})}));
        const auto bundle = pack_phase_dag_run_v1(replay_run);
        require(pack_phase_dag_run_v1(replay_phase_dag_run_v1(bundle)) == bundle,
            "Replay did not preserve canonical bundle bytes");
        require(pack_phase_dag_run_v1(execute_phase_dag_v1(executable_spec({source("a", "I:x")}))).size() > 0,
            "Partial output did not pack");
        require(pack_phase_dag_run_v1(execute_phase_dag_v1(executable_spec({source("a", "F")}))).size() > 0,
            "Local failure did not pack");
        require(pack_phase_dag_run_v1(execute_phase_dag_v1(executable_spec({source("a", "C:x")}, 0))).size() > 0,
            "Budget cancellation did not pack");

        auto corrupt_header = bundle;
        corrupt_header.front() = 'X';
        rejected([&] { replay_phase_dag_run_v1(corrupt_header); }, "Corrupt bundle header accepted");
        rejected([&] { replay_phase_dag_run_v1(bundle + "x"); }, "Trailing bundle bytes accepted");
        auto changed_output = bundle;
        const auto left = changed_output.find("left");
        require(left != std::string::npos, "Test bundle has no mutable output");
        changed_output.replace(left, 4, "lift");
        rejected([&] { replay_phase_dag_run_v1(rehashed(changed_output)); },
            "Rehashed semantic output mutation accepted");

        std::cout << "Phase DAG spec validation preserved\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
