#include "bruce/dag_scheduler/dag_scheduler_v1.hpp"
#include "bruce/core/types.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
using namespace bruce;
using Status = DagSchedulerCallbackStatusV1;
using Outcome = DagSchedulerReceiptOutcomeV1;
using Event = DagSchedulerTraceEventV1;
int ready_calls = 0, fail_calls = 0, forbidden_calls = 0, result_calls = 0;
bool cancellation = false;
DagSchedulerCallbackResultV1 next_result;
std::vector<std::vector<DagSchedulerInputV1>> observations;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void reset() {
    ready_calls = fail_calls = forbidden_calls = result_calls = 0;
    cancellation = false;
    observations.clear();
}
DagSchedulerCallbackResultV1 ready(std::span<const DagSchedulerInputV1> inputs) {
    ++ready_calls;
    observations.emplace_back(inputs.begin(), inputs.end());
    return {Status::ready, "OK"};
}
DagSchedulerCallbackResultV1 fail(std::span<const DagSchedulerInputV1> inputs) {
    ++fail_calls;
    observations.emplace_back(inputs.begin(), inputs.end());
    return {Status::failed, "EXPECTED_FAILURE"};
}
DagSchedulerCallbackResultV1 forbidden(std::span<const DagSchedulerInputV1>) {
    ++forbidden_calls;
    return {Status::ready, "FORBIDDEN"};
}
DagSchedulerCallbackResultV1 cancel(std::span<const DagSchedulerInputV1> inputs) {
    cancellation = true;
    return ready(inputs);
}
DagSchedulerCallbackResultV1 result(std::span<const DagSchedulerInputV1>) {
    ++result_calls;
    return next_result;
}
DagSchedulerCallbackResultV1 throwing(std::span<const DagSchedulerInputV1>) {
    ++result_calls;
    throw std::runtime_error("callback exploded");
}
constexpr std::array<DagSchedulerCallbackEntryV1, 6> registry{{
    {"cancel", cancel}, {"fail", fail}, {"forbidden", forbidden},
    {"ready", ready}, {"result", result}, {"throw", throwing}}};

DagSchedulerSpecV1 spec(std::vector<DagSchedulerNodeSpecV1> nodes,
                        std::uint64_t budget = 64) {
    return {"dag-scheduler-v1", "test-dag", std::move(nodes), budget};
}
DagSchedulerSpecV1 single() { return spec({{"a", "ready", {}}}); }
std::string inputs(const std::vector<DagSchedulerInputV1>& refs) {
    std::ostringstream out;
    for (const auto& ref : refs) out << ref.producer_node_id << '/' << ref.result_code << ',';
    return out.str();
}
std::string trace(const DagSchedulerRunV1& run) {
    std::ostringstream out;
    for (const auto& entry : run.trace) {
        const char* name = "INVALID";
        switch (entry.event) {
        case Event::validated: name = "VALIDATED"; break;
        case Event::started: name = "STARTED"; break;
        case Event::ready: name = "READY"; break;
        case Event::failed: name = "FAILED"; break;
        case Event::blocked: name = "BLOCKED"; break;
        case Event::cancelled: name = "CANCELLED"; break;
        case Event::completed: name = "COMPLETED"; break;
        }
        out << name << ':' << entry.node_id << ';';
    }
    return out.str();
}
std::string receipts(const DagSchedulerRunV1& run) {
    std::ostringstream out;
    for (const auto& receipt : run.receipts) {
        const char* name = "INVALID";
        switch (receipt.outcome) {
        case Outcome::ready: name = "READY"; break;
        case Outcome::failed: name = "FAILED"; break;
        case Outcome::blocked: name = "BLOCKED"; break;
        case Outcome::cancelled: name = "CANCELLED"; break;
        }
        out << receipt.node_id << '/' << receipt.callback_id << '/' << name << '/'
            << receipt.terminal_code << '[' << inputs(receipt.input_refs) << "];";
    }
    return out.str();
}
std::string spec_projection(const DagSchedulerSpecV1& value) {
    std::ostringstream out;
    out << value.scheduler_version << '/' << value.dag_id << '/' << value.max_started_nodes;
    for (const auto& node : value.nodes) {
        out << ';' << node.node_id << '/' << node.callback_id;
        for (const auto& dep : node.dependencies) out << '/' << dep;
    }
    return out.str();
}
std::string projection(const DagSchedulerRunV1& run) {
    return spec_projection(run.spec) + '|' + receipts(run) + '|' + trace(run);
}
void rejected(const DagSchedulerSpecV1& value, DagSchedulerCallbackRegistryV1 catalog,
              const char* code, std::string_view affected = {}) {
    reset();
    auto sentinel = execute_dag_scheduler_v1(single(), registry);
    const auto before = projection(sentinel), original = spec_projection(value);
    reset();
    try { sentinel = execute_dag_scheduler_v1(value, catalog); }
    catch (const Error& error) {
        const std::string message = error.what();
        require(message == code || message.starts_with(std::string(code) + ":"),
                "wrong error category");
        require(affected.empty() || message.find(affected) != std::string::npos,
                "error omitted affected valid ID");
        require(projection(sentinel) == before, "invalid spec published a partial run");
        require(spec_projection(value) == original, "validation mutated the spec");
        require(ready_calls + fail_calls + forbidden_calls + result_calls == 0,
                "invalid spec called a callback");
        bool validation_rejected = false;
        try { validate_dag_scheduler_spec_v1(value, catalog); }
        catch (const Error& validation_error) {
            validation_rejected = std::string(validation_error.what()) == message;
        }
        require(validation_rejected, "standalone validation disagrees with execution");
        return;
    }
    throw std::runtime_error("invalid spec accepted");
}

// FIFO or static waves would start c before the newly ready b.
void dynamic_ready_order_and_spec_preservation() {
    reset();
    const auto value = spec({{"c", "ready", {}}, {"b", "ready", {"a"}},
                             {"a", "ready", {}}});
    const auto before = spec_projection(value);
    validate_dag_scheduler_spec_v1(value, registry);
    require(ready_calls == 0, "validation executed callback");
    const auto run = execute_dag_scheduler_v1(value, registry);
    require(trace(run) == "VALIDATED:;STARTED:a;READY:a;STARTED:b;READY:b;STARTED:c;READY:c;COMPLETED:;",
            "dynamic lexicographic readiness violated");
    require(receipts(run) == "a/ready/READY/OK[];b/ready/READY/OK[a/OK,];c/ready/READY/OK[];",
            "sorted receipts or direct inputs changed");
    require(ready_calls == 3 && observations.size() == 3, "callback not called once per node");
    require(spec_projection(run.spec) == before && spec_projection(value) == before,
            "spec was reordered or mutated");
}
// Transitive inputs, dependency-list ordering or failed inputs must not leak.
void sorted_direct_inputs() {
    reset();
    const auto run = execute_dag_scheduler_v1(spec({{"z", "ready", {"c", "b"}},
        {"c", "ready", {"a"}}, {"b", "ready", {}}, {"a", "ready", {}}}), registry);
    require(trace(run) == "VALIDATED:;STARTED:a;READY:a;STARTED:b;READY:b;STARTED:c;READY:c;STARTED:z;READY:z;COMPLETED:;",
            "merge execution order changed");
    require(receipts(run) == "a/ready/READY/OK[];b/ready/READY/OK[];c/ready/READY/OK[a/OK,];z/ready/READY/OK[b/OK,c/OK,];",
            "receipts contain unsorted or transitive inputs");
    require(observations.size() == 4 && inputs(observations.back()) == "b/OK,c/OK,",
            "callback did not receive sorted direct successful inputs");
}
void mixed_failure_and_transitive_blocking() {
    reset();
    const auto run = execute_dag_scheduler_v1(spec({{"e-finish", "ready", {"c-independent"}},
        {"d-blocked", "forbidden", {"b-fail"}}, {"c-independent", "ready", {}},
        {"b-fail", "fail", {"a-root"}}, {"a-root", "ready", {}}}, 5), registry);
    require(trace(run) == "VALIDATED:;STARTED:a-root;READY:a-root;STARTED:b-fail;FAILED:b-fail;BLOCKED:d-blocked;STARTED:c-independent;READY:c-independent;STARTED:e-finish;READY:e-finish;COMPLETED:;",
            "mixed chronological trace changed");
    require(receipts(run) == "a-root/ready/READY/OK[];b-fail/fail/FAILED/EXPECTED_FAILURE[a-root/OK,];c-independent/ready/READY/OK[];d-blocked/forbidden/BLOCKED/DEPENDENCY_NOT_READY[];e-finish/ready/READY/OK[c-independent/OK,];",
            "mixed receipts changed");
    require(ready_calls == 3 && fail_calls == 1 && forbidden_calls == 0,
            "mixed callback counts changed");
    reset();
    const auto deep = execute_dag_scheduler_v1(spec({{"a-child", "forbidden", {"b-child"}},
        {"b-child", "forbidden", {"c-fail"}}, {"c-fail", "fail", {}},
        {"z-independent", "ready", {}}}), registry);
    require(trace(deep) == "VALIDATED:;STARTED:c-fail;FAILED:c-fail;BLOCKED:b-child;BLOCKED:a-child;STARTED:z-independent;READY:z-independent;COMPLETED:;",
            "failure was not transitively settled before unrelated progress");
    require(receipts(deep) == "a-child/forbidden/BLOCKED/DEPENDENCY_NOT_READY[];b-child/forbidden/BLOCKED/DEPENDENCY_NOT_READY[];c-fail/fail/FAILED/EXPECTED_FAILURE[];z-independent/ready/READY/OK[];",
            "transitive failure receipts changed");
    require(forbidden_calls == 0 && fail_calls == 1 && ready_calls == 1,
            "blocked descendant callback ran");
}
DagSchedulerSpecV1 control_spec(std::string callback = "ready") {
    return spec({{"c", "forbidden", {}}, {"b", "forbidden", {"a"}},
                 {"a", std::move(callback), {}}}, 1);
}
void budget_never_starts_remaining_callbacks() {
    reset();
    const auto run = execute_dag_scheduler_v1(control_spec(), registry);
    require(trace(run) == "VALIDATED:;STARTED:a;READY:a;CANCELLED:b;CANCELLED:c;COMPLETED:;",
            "budget trace changed");
    require(receipts(run) == "a/ready/READY/OK[];b/forbidden/CANCELLED/START_BUDGET_EXHAUSTED[];c/forbidden/CANCELLED/START_BUDGET_EXHAUSTED[];",
            "budget terminal codes changed");
    require(ready_calls == 1 && forbidden_calls == 0, "budget invoked a remaining callback");
}
void cancellation_preempts_budget_and_is_polled_before_starts() {
    reset();
    int polls = 0;
    const auto run = execute_dag_scheduler_v1(control_spec("cancel"), registry,
        {[&] { ++polls; return cancellation; }});
    require(trace(run) == "VALIDATED:;STARTED:a;READY:a;CANCELLED:b;CANCELLED:c;COMPLETED:;",
            "cancellation trace changed");
    require(receipts(run) == "a/cancel/READY/OK[];b/forbidden/CANCELLED/CONTROLLED_CANCELLATION[];c/forbidden/CANCELLED/CONTROLLED_CANCELLATION[];",
            "cancellation did not take precedence over exhausted budget");
    require(polls == 2 && ready_calls == 1 && forbidden_calls == 0,
            "control was not polled immediately before further starts");
    reset();
    const auto pre = execute_dag_scheduler_v1(single(), registry, {[] { return true; }});
    require(trace(pre) == "VALIDATED:;CANCELLED:a;COMPLETED:;" && ready_calls == 0,
            "initial cancellation executed a callback");
}
void blocking_settles_before_controls() {
    for (bool controlled : {false, true}) {
        reset();
        const auto value = spec({{"a-child", "forbidden", {"b-child"}},
            {"b-child", "forbidden", {"c-fail"}}, {"c-fail", "fail", {}},
            {"z", "forbidden", {}}}, 1);
        const auto run = execute_dag_scheduler_v1(value, registry,
            {[controlled] { return controlled && fail_calls == 1; }});
        require(trace(run) == "VALIDATED:;STARTED:c-fail;FAILED:c-fail;BLOCKED:b-child;BLOCKED:a-child;CANCELLED:z;COMPLETED:;",
                "controls ran before transitive blocking settled");
        require(run.receipts[0].outcome == Outcome::blocked &&
                run.receipts[1].outcome == Outcome::blocked &&
                run.receipts[3].terminal_code == (controlled ? "CONTROLLED_CANCELLATION" : "START_BUDGET_EXHAUSTED") &&
                forbidden_calls == 0, "blocked/control callback counts or outcomes changed");
    }
}
void invalid_identifiers() {
    const std::array<std::string, 7> bad{{"", std::string(65, 'a'), "bad id", "bad/id",
        "bad.id", std::string("a\0b", 3), std::string("a\xc3\xa4", 3)}};
    for (const auto& id : bad) {
        auto value = single(); value.dag_id = id;
        rejected(value, registry, "InvalidDagSchedulerIdentifier");
        value = single(); value.nodes[0].node_id = id;
        rejected(value, registry, "InvalidDagSchedulerIdentifier");
        value = single(); value.nodes[0].callback_id = id;
        rejected(value, registry, "InvalidDagSchedulerIdentifier");
        value = single(); value.nodes[0].dependencies = {id};
        rejected(value, registry, "InvalidDagSchedulerIdentifier");
    }
    auto value = single(); value.scheduler_version = "dag-scheduler-v2";
    rejected(value, registry, "InvalidDagSchedulerIdentifier");
}
void invalid_registry() {
    rejected(single(), {}, "InvalidDagSchedulerCallbackRegistry");
    const std::array<DagSchedulerCallbackEntryV1, 2> unsorted{{{"ready", ready}, {"fail", fail}}};
    rejected(single(), unsorted, "InvalidDagSchedulerCallbackRegistry", "fail");
    const std::array<DagSchedulerCallbackEntryV1, 2> duplicate{{{"ready", ready}, {"ready", ready}}};
    rejected(single(), duplicate, "InvalidDagSchedulerCallbackRegistry", "ready");
    const std::array<DagSchedulerCallbackEntryV1, 1> null{{{"ready", nullptr}}};
    rejected(single(), null, "InvalidDagSchedulerCallbackRegistry", "ready");
    for (const std::string& id : {std::string{}, std::string("bad/id"), std::string(65, 'a')}) {
        const std::array<DagSchedulerCallbackEntryV1, 1> malformed{{{id, ready}}};
        rejected(single(), malformed, "InvalidDagSchedulerCallbackRegistry");
    }
    // Every catalog entry is validated, including unused entries.
    const std::array<DagSchedulerCallbackEntryV1, 2> unused{{{"ready", ready}, {"z-unused", nullptr}}};
    rejected(single(), unused, "InvalidDagSchedulerCallbackRegistry", "z-unused");
}
void invalid_graph_and_budget() {
    rejected(spec({}), registry, "InvalidDagSchedulerSpec");
    std::vector<DagSchedulerNodeSpecV1> nodes;
    for (int i = 0; i < 65; ++i) nodes.push_back({"n" + std::to_string(i), "ready", {}});
    rejected(spec(nodes), registry, "InvalidDagSchedulerSpec");
    for (const auto budget : {0ULL, 65ULL, ~0ULL})
        rejected(spec({{"a", "ready", {}}}, budget), registry, "InvalidDagSchedulerStartBudget");
    rejected(spec({{"a", "ready", {}}, {"a", "ready", {}}}), registry,
             "DuplicateDagSchedulerNode", "a");
    rejected(spec({{"a", "missing", {}}}), registry, "UnknownDagSchedulerCallback", "missing");
    rejected(spec({{"a", "ready", {"missing"}}}), registry, "UnknownDagSchedulerDependency", "missing");
    rejected(spec({{"a", "ready", {"a"}}}), registry, "InvalidDagSchedulerDependency", "a");
    rejected(spec({{"a", "ready", {}}, {"b", "ready", {"a", "a"}}}), registry,
             "InvalidDagSchedulerDependency", "b");
    rejected(spec({{"a", "ready", {"b"}}, {"b", "ready", {"a"}}}), registry, "DagSchedulerCycle", "a");
    rejected(spec({{"a", "ready", {}}, {"b", "ready", {"c"}}, {"c", "ready", {"b"}}}),
             registry, "DagSchedulerCycle", "b");
    std::vector<std::string> deps;
    nodes.clear();
    for (int i = 0; i < 16; ++i) {
        deps.push_back("p" + std::to_string(i));
        nodes.push_back({deps.back(), "ready", {}});
    }
    nodes.push_back({"z", "ready", deps});
    rejected(spec(nodes), registry, "InvalidDagSchedulerDependency", "z");
}
void valid_identifier_and_dependency_boundaries() {
    reset();
    const std::string id = "AZaz09_-" + std::string(56, 'x');
    auto value = spec({{id, id, {}}}, 1); value.dag_id = id;
    const std::array<DagSchedulerCallbackEntryV1, 1> catalog{{{id, result}}};
    next_result = {Status::ready, id};
    const auto run = execute_dag_scheduler_v1(value, catalog);
    require(run.receipts.size() == 1 && run.receipts[0].terminal_code == id && result_calls == 1,
            "valid 64-character identifiers were rejected");
    reset();
    std::vector<DagSchedulerNodeSpecV1> nodes;
    std::vector<std::string> deps;
    for (int i = 0; i < 15; ++i) {
        deps.push_back("p" + std::to_string(i));
        nodes.push_back({deps.back(), "ready", {}});
    }
    nodes.push_back({"z", "ready", deps});
    const auto merge = execute_dag_scheduler_v1(spec(nodes), registry);
    require(ready_calls == 16 && merge.receipts.back().input_refs.size() == 15 &&
            observations.back().size() == 15, "15-dependency boundary failed");
}
void invalid_results_publish_no_partial_run() {
    const std::array<std::string, 7> bad{{"", std::string(65, 'x'), "bad id", "bad/id",
        "bad.id", std::string("a\0b", 3), std::string("a\xc3\xa4", 3)}};
    for (const auto status : {Status::ready, Status::failed}) {
        for (const auto& code : bad) {
            reset();
            auto sentinel = execute_dag_scheduler_v1(single(), registry);
            const auto before = projection(sentinel);
            reset(); next_result = {status, code};
            try { sentinel = execute_dag_scheduler_v1(spec({{"a", "ready", {}},
                {"b", "result", {"a"}}, {"c", "forbidden", {"b"}}}), registry); }
            catch (const Error& error) {
                require(std::string(error.what()).starts_with("InvalidDagSchedulerCallbackResult"),
                        "malformed callback result had wrong error");
                require(std::string(error.what()).find("b") != std::string::npos,
                        "malformed result omitted node ID");
                require(projection(sentinel) == before && ready_calls == 1 && result_calls == 1 &&
                        forbidden_calls == 0, "invalid result leaked partial run or started successor");
                continue;
            }
            throw std::runtime_error("invalid callback result accepted");
        }
    }
    reset(); next_result = {static_cast<Status>(99), "VALID"};
    try { (void)execute_dag_scheduler_v1(spec({{"a", "result", {}}}), registry); }
    catch (const Error& error) {
        require(std::string(error.what()).starts_with("InvalidDagSchedulerCallbackResult") && result_calls == 1,
                "invalid result status had wrong error");
        return;
    }
    throw std::runtime_error("invalid callback status accepted");
}
void callback_exception_propagates_without_partial_run() {
    reset(); auto sentinel = execute_dag_scheduler_v1(single(), registry);
    const auto before = projection(sentinel);
    reset();
    try { sentinel = execute_dag_scheduler_v1(spec({{"a", "ready", {}},
        {"b", "throw", {"a"}}, {"c", "forbidden", {"b"}}}), registry); }
    catch (const std::runtime_error& error) {
        require(std::string(error.what()) == "callback exploded", "exception was translated");
        require(projection(sentinel) == before && ready_calls == 1 && result_calls == 1 &&
                forbidden_calls == 0, "callback exception leaked run or started successor");
        return;
    }
    throw std::runtime_error("callback exception swallowed");
}
void repeated_determinism_and_permuted_graph() {
    const auto value = spec({{"z", "ready", {"c", "b"}}, {"c", "ready", {"a"}},
                             {"b", "ready", {}}, {"a", "ready", {}}});
    const std::string expected_receipts = "a/ready/READY/OK[];b/ready/READY/OK[];c/ready/READY/OK[a/OK,];z/ready/READY/OK[b/OK,c/OK,];";
    const std::string expected_trace = "VALIDATED:;STARTED:a;READY:a;STARTED:b;READY:b;STARTED:c;READY:c;STARTED:z;READY:z;COMPLETED:;";
    for (int i = 0; i < 20; ++i) {
        reset(); auto permutation = value;
        if (i % 2 == 0) std::reverse(permutation.nodes.begin(), permutation.nodes.end());
        const auto run = execute_dag_scheduler_v1(permutation, registry);
        require(receipts(run) == expected_receipts && trace(run) == expected_trace && ready_calls == 4,
                "repeated execution changed deterministic semantics");
    }
}
void bounded_chain_of_64_nodes() {
    reset(); std::vector<DagSchedulerNodeSpecV1> nodes;
    for (int i = 63; i >= 0; --i) {
        const auto id = "n" + std::string(i < 10 ? "0" : "") + std::to_string(i);
        const auto parent = "n" + std::string(i - 1 < 10 ? "0" : "") + std::to_string(i - 1);
        nodes.push_back({id, "ready", i == 0 ? std::vector<std::string>{} : std::vector<std::string>{parent}});
    }
    const auto run = execute_dag_scheduler_v1(spec(nodes), registry);
    require(ready_calls == 64 && run.receipts.size() == 64 && run.trace.size() == 130 &&
            run.trace.front().event == Event::validated && run.trace.back().event == Event::completed &&
            run.trace.back().node_id.empty(), "bounded chain did not complete exactly once per node");
    require(run.receipts.front().node_id == "n00" && run.receipts.front().input_refs.empty() &&
            run.receipts.back().node_id == "n63" && inputs(run.receipts.back().input_refs) == "n62/OK,",
            "bounded chain endpoints or direct inputs changed");
    for (std::size_t i = 0; i < 64; ++i) {
        require(run.receipts[i].outcome == Outcome::ready &&
                run.trace[1 + 2 * i].event == Event::started &&
                run.trace[1 + 2 * i].node_id == run.receipts[i].node_id &&
                run.trace[2 + 2 * i].event == Event::ready,
                "bounded chain trace changed");
        require(observations[i].size() == (i == 0 ? 0U : 1U), "chain callback got transitive inputs");
    }
}
}

int main() {
    const std::pair<const char*, void (*)()> tests[] = {
        {"dynamic_ready_order_and_spec_preservation", dynamic_ready_order_and_spec_preservation},
        {"sorted_direct_inputs", sorted_direct_inputs},
        {"mixed_failure_and_transitive_blocking", mixed_failure_and_transitive_blocking},
        {"budget_never_starts_remaining_callbacks", budget_never_starts_remaining_callbacks},
        {"cancellation_preempts_budget_and_is_polled_before_starts", cancellation_preempts_budget_and_is_polled_before_starts},
        {"blocking_settles_before_controls", blocking_settles_before_controls},
        {"invalid_identifiers", invalid_identifiers},
        {"invalid_registry", invalid_registry},
        {"invalid_graph_and_budget", invalid_graph_and_budget},
        {"valid_identifier_and_dependency_boundaries", valid_identifier_and_dependency_boundaries},
        {"invalid_results_publish_no_partial_run", invalid_results_publish_no_partial_run},
        {"callback_exception_propagates_without_partial_run", callback_exception_propagates_without_partial_run},
        {"repeated_determinism_and_permuted_graph", repeated_determinism_and_permuted_graph},
        {"bounded_chain_of_64_nodes", bounded_chain_of_64_nodes},
    };
    int failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) {
            ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        }
    }
    std::cout << std::size(tests) - failures << '/' << std::size(tests) << " contracts passed\n";
    return failures == 0 ? 0 : 1;
}
