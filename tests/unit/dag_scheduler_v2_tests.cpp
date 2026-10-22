#include "bruce/dag_scheduler/dag_scheduler_v2.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
using namespace bruce;
using Status = DagSchedulerCallbackStatusV2;
using Outcome = DagSchedulerReceiptOutcomeV2;
using Event = DagSchedulerTraceEventV2;
int calls = 0, fail_calls = 0, forbidden_calls = 0, result_calls = 0;
bool cancellation = false;
DagSchedulerCallbackResultV2 next_result;
std::vector<std::string> requests;
std::vector<std::vector<DagSchedulerInputV2>> observations;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void reset() {
    calls = fail_calls = forbidden_calls = result_calls = 0;
    cancellation = false; requests.clear(); observations.clear();
}
DagSchedulerCallbackResultV2 success(std::string code, std::string payload) {
    auto digest = sha256(payload);
    return {Status::ready, std::move(code), std::move(payload), std::move(digest)};
}
std::string inputs(std::span<const DagSchedulerInputV2> refs) {
    std::ostringstream out;
    for (const auto& r : refs)
        out << r.producer_node_id << '/' << r.result_code << '/' << r.payload_bytes << '/' << r.payload_digest << ';';
    return out.str();
}
DagSchedulerCallbackResultV2 ready(std::string_view request, std::span<const DagSchedulerInputV2> refs) {
    ++calls; requests.emplace_back(request); observations.emplace_back(refs.begin(), refs.end());
    return success("OK", std::string(request));
}
DagSchedulerCallbackResultV2 flow(std::string_view request, std::span<const DagSchedulerInputV2> refs) {
    ++calls; requests.emplace_back(request); observations.emplace_back(refs.begin(), refs.end());
    if (request == "source-request") {
        require(refs.empty(), "source got inputs"); return success("SOURCE_READY", "alpha");
    }
    if (request == "transform-request") {
        require(refs.size() == 1 && refs[0].producer_node_id == "a-source" &&
                refs[0].result_code == "SOURCE_READY" && refs[0].payload_bytes == "alpha" &&
                refs[0].payload_digest == sha256("alpha"), "transform got wrong direct input");
        return success("TRANSFORM_READY", refs[0].payload_bytes + ":beta");
    }
    if (request == "independent-request") {
        require(refs.empty(), "independent got inputs"); return success("INDEPENDENT_READY", "gamma");
    }
    require(request == "join-request" && refs.size() == 2 &&
            refs[0].producer_node_id == "b-transform" && refs[1].producer_node_id == "c-independent" &&
            refs[0].result_code == "TRANSFORM_READY" && refs[1].result_code == "INDEPENDENT_READY" &&
            refs[0].payload_digest == sha256("alpha:beta") && refs[1].payload_digest == sha256("gamma"),
            "join got unsorted, transitive or corrupt inputs");
    return success("JOIN_READY", refs[0].payload_bytes + '|' + refs[1].payload_bytes);
}
DagSchedulerCallbackResultV2 fail(std::string_view, std::span<const DagSchedulerInputV2>) {
    ++fail_calls; return {Status::failed, "EXPECTED_FAILURE", "", ""};
}
DagSchedulerCallbackResultV2 forbidden(std::string_view, std::span<const DagSchedulerInputV2>) {
    ++forbidden_calls; return success("FORBIDDEN", "");
}
DagSchedulerCallbackResultV2 cancel(std::string_view request, std::span<const DagSchedulerInputV2> refs) {
    cancellation = true; return ready(request, refs);
}
DagSchedulerCallbackResultV2 result(std::string_view, std::span<const DagSchedulerInputV2>) {
    ++result_calls; return next_result;
}
DagSchedulerCallbackResultV2 throwing(std::string_view, std::span<const DagSchedulerInputV2>) {
    ++result_calls; throw std::runtime_error("callback exploded");
}
constexpr std::array<DagSchedulerCallbackEntryV2, 7> registry{{
    {"cancel", cancel}, {"fail", fail}, {"flow", flow}, {"forbidden", forbidden},
    {"ready", ready}, {"result", result}, {"throw", throwing}}};
DagSchedulerNodeSpecV2 node(std::string id, std::string callback = "ready",
                          std::vector<std::string> deps = {}, std::string request = "x") {
    auto digest = sha256(request);
    return {std::move(id), std::move(callback), std::move(deps), std::move(request), std::move(digest)};
}
DagSchedulerSpecV2 spec(std::vector<DagSchedulerNodeSpecV2> nodes,
                        std::uint64_t start = 64, std::uint64_t bytes = 4194304) {
    return {"dag-scheduler-v2", "test-dag", std::move(nodes), start, bytes};
}
DagSchedulerSpecV2 single() { return spec({node("a")}); }
std::string trace(const DagSchedulerRunV2& run) {
    std::ostringstream out;
    for (const auto& e : run.trace) {
        const char* name = "INVALID";
        switch (e.event) {
        case Event::validated: name = "VALIDATED"; break;
        case Event::started: name = "STARTED"; break;
        case Event::ready: name = "READY"; break;
        case Event::failed: name = "FAILED"; break;
        case Event::blocked: name = "BLOCKED"; break;
        case Event::cancelled: name = "CANCELLED"; break;
        case Event::completed: name = "COMPLETED"; break;
        }
        out << name << ':' << e.node_id << ';';
    }
    return out.str();
}
std::string spec_projection(const DagSchedulerSpecV2& value) {
    std::ostringstream out;
    out << value.scheduler_version << '/' << value.dag_id << '/' << value.max_started_nodes << '/' << value.max_total_receipt_bytes;
    for (const auto& n : value.nodes) {
        out << ';' << n.node_id << '/' << n.callback_id << '/' << n.request_bytes << '/' << n.request_digest << '[';
        for (const auto& d : n.dependencies) out << d << ',';
        out << ']';
    }
    return out.str();
}
std::string receipts(const DagSchedulerRunV2& run) {
    std::ostringstream out;
    for (const auto& r : run.receipts) {
        const char* name = "INVALID";
        switch (r.outcome) {
        case Outcome::ready: name = "READY"; break;
        case Outcome::failed: name = "FAILED"; break;
        case Outcome::blocked: name = "BLOCKED"; break;
        case Outcome::cancelled: name = "CANCELLED"; break;
        }
        out << r.node_id << '/' << r.callback_id << '/' << r.request_digest << '/' << name << '/' << r.failure_code
            << '[' << inputs(r.input_refs) << ']';
        if (r.output) out << '{' << r.output->output_version << '/' << r.output->result_code << '/'
                          << r.output->payload_bytes << '/' << r.output->payload_digest << '}';
        out << ';';
    }
    return out.str();
}
std::string projection(const DagSchedulerRunV2& run) {
    return spec_projection(run.spec) + '|' + receipts(run) + '|' + trace(run);
}
void error_matches(const Error& error, std::string_view code, std::string_view id = {}) {
    const std::string message = error.what();
    require(message == code || message.starts_with(std::string(code) + ':'), "wrong error category");
    require(id.empty() || message.find(id) != std::string::npos, "error omitted affected valid ID");
}
void rejected(const DagSchedulerSpecV2& value, DagSchedulerCallbackRegistryV2 catalog,
              std::string_view code, std::string_view id = {}) {
    reset(); auto sentinel = execute_dag_scheduler_v2(single(), registry);
    const auto before = projection(sentinel), original = spec_projection(value); reset();
    try { sentinel = execute_dag_scheduler_v2(value, catalog); }
    catch (const Error& error) {
        error_matches(error, code, id);
        require(projection(sentinel) == before, "invalid spec published partial run");
        require(spec_projection(value) == original, "validation mutated spec");
        require(calls + fail_calls + forbidden_calls + result_calls == 0, "invalid spec invoked callback");
        bool failed = false;
        try { validate_dag_scheduler_spec_v2(value, catalog); }
        catch (const Error& e) { failed = std::string(e.what()) == error.what(); }
        require(failed, "standalone validation disagrees"); return;
    }
    throw std::runtime_error("invalid spec accepted");
}
// Catches static waves/FIFO, request substitution and missing direct payload copies.
void full_flow_and_repeatability() {
    const auto value = spec({node("d-join", "flow", {"c-independent", "b-transform"}, "join-request"),
        node("c-independent", "flow", {}, "independent-request"),
        node("b-transform", "flow", {"a-source"}, "transform-request"),
        node("a-source", "flow", {}, "source-request")}, 4, 4096);
    const auto original = spec_projection(value);
    std::string first;
    for (int i = 0; i < 3; ++i) {
        reset(); validate_dag_scheduler_spec_v2(value, registry);
        require(calls == 0, "validation invoked callback");
        const auto run = execute_dag_scheduler_v2(value, registry);
        require(trace(run) == "VALIDATED:;STARTED:a-source;READY:a-source;STARTED:b-transform;READY:b-transform;STARTED:c-independent;READY:c-independent;STARTED:d-join;READY:d-join;COMPLETED:;", "dynamic order changed");
        require(calls == 4 && requests == std::vector<std::string>{"source-request", "transform-request", "independent-request", "join-request"}, "request bytes or callback counts changed");
        require(inputs(observations.at(1)) == "a-source/SOURCE_READY/alpha/8ed3f6ad685b959ead7022518e1af76cd816f8e8ec7ccdda1ed4018e8f2223f8;", "direct payload input changed");
        require(run.receipts.size() == 4 && run.receipts.at(3).output->payload_bytes == "alpha:beta|gamma", "join payload changed");
        for (std::size_t n = 0; n < 4; ++n) {
            const auto& r = run.receipts[n];
            const std::array<std::string, 4> ids{{"a-source", "b-transform", "c-independent", "d-join"}};
            const std::array<std::string, 4> codes{{"SOURCE_READY", "TRANSFORM_READY", "INDEPENDENT_READY", "JOIN_READY"}};
            const std::array<std::string, 4> payloads{{"alpha", "alpha:beta", "gamma", "alpha:beta|gamma"}};
            require(r.node_id == ids[n] && r.callback_id == "flow" && r.outcome == Outcome::ready && r.output &&
                    r.output->output_version == "dag-scheduler-output-v2" && r.output->result_code == codes[n] &&
                    r.output->payload_bytes == payloads[n] && r.output->payload_digest == sha256(payloads[n]) &&
                    r.failure_code.empty() && inputs(r.input_refs) == inputs(observations[n]), "flow receipt projection changed");
            require(r.request_digest == sha256(requests[n]), "receipt request digest changed");
        }
        require(spec_projection(run.spec) == original && spec_projection(value) == original, "spec mutated");
        if (i == 0) first = projection(run);
        else require(projection(run) == first, "full repeated projection changed");
    }
}
// Catches transitive input leakage, unsorted dependencies and byte-string truncation.
void direct_sorted_binary_inputs() {
    reset();
    const std::string binary("a\0b\xff", 4);
    const auto run = execute_dag_scheduler_v2(spec({node("z", "ready", {"c", "b"}, ""),
        node("c", "ready", {"a"}, binary), node("b"), node("a")}), registry);
    require(trace(run) == "VALIDATED:;STARTED:a;READY:a;STARTED:b;READY:b;STARTED:c;READY:c;STARTED:z;READY:z;COMPLETED:;", "direct graph order changed");
    require(observations.back().size() == 2 && observations.back()[0].producer_node_id == "b" &&
            observations.back()[1].producer_node_id == "c" && observations.back()[1].payload_bytes == binary &&
            observations.back()[1].payload_digest == sha256(binary), "direct sorted binary input changed");
    require(requests[2] == binary && run.receipts[2].output->payload_bytes == binary &&
            run.receipts.back().output->payload_digest == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "binary/empty payload corrupted");
}
// Catches failed input retention, premature control checks and descendant execution.
void failure_blocking_before_controls() {
    for (bool controlled : {false, true}) {
        reset(); int polls = 0;
        const auto run = execute_dag_scheduler_v2(spec({node("a-child", "forbidden", {"b-child"}),
            node("b-child", "forbidden", {"c-fail"}), node("c-fail", "fail", {"c-root"}),
            node("c-root"), node("z", "forbidden")}, 2), registry,
            {[&] { ++polls; return controlled && fail_calls == 1; }});
        require(trace(run) == "VALIDATED:;STARTED:c-root;READY:c-root;STARTED:c-fail;FAILED:c-fail;BLOCKED:b-child;BLOCKED:a-child;CANCELLED:z;COMPLETED:;", "blocking/control order changed");
        require(calls == 1 && fail_calls == 1 && forbidden_calls == 0 && polls == 3, "blocked/cancelled callback ran");
        require(run.receipts[0].outcome == Outcome::blocked && run.receipts[1].outcome == Outcome::blocked &&
                run.receipts[2].outcome == Outcome::failed && run.receipts[2].failure_code == "EXPECTED_FAILURE" &&
                run.receipts[4].failure_code == (controlled ? "CONTROLLED_CANCELLATION" : "START_BUDGET_EXHAUSTED"), "terminal outcome changed");
        for (const auto& r : run.receipts) if (r.outcome != Outcome::ready)
            require(!r.output && r.input_refs.empty(), "unsuccessful receipt retained payload fields");
    }
    reset();
    const auto run = execute_dag_scheduler_v2(spec({node("a", "fail"), node("b", "forbidden", {"a"}), node("c"), node("d", "ready", {"c"})}), registry);
    require(trace(run) == "VALIDATED:;STARTED:a;FAILED:a;BLOCKED:b;STARTED:c;READY:c;STARTED:d;READY:d;COMPLETED:;" && calls == 2 && fail_calls == 1 && forbidden_calls == 0, "unrelated successful branch stalled");
}
void cancellation_and_start_budget() {
    for (const bool controlled : {false, true}) {
        reset(); int polls = 0;
        const auto run = execute_dag_scheduler_v2(spec({node("c", "forbidden"), node("b", "forbidden", {"a"}), node("a", controlled ? "cancel" : "ready")}, 1), registry,
            {[&] { ++polls; return cancellation; }});
        require(trace(run) == "VALIDATED:;STARTED:a;READY:a;CANCELLED:b;CANCELLED:c;COMPLETED:;", "cancellation trace changed");
        require(calls == 1 && forbidden_calls == 0 && polls == 2, "control polling/count changed");
        for (std::size_t i = 1; i < 3; ++i)
            require(run.receipts[i].outcome == Outcome::cancelled && !run.receipts[i].output && run.receipts[i].input_refs.empty() &&
                    run.receipts[i].failure_code == (controlled ? "CONTROLLED_CANCELLATION" : "START_BUDGET_EXHAUSTED"), "cancellation precedence changed");
    }
    reset(); const auto run = execute_dag_scheduler_v2(single(), registry, {[] { return true; }});
    require(trace(run) == "VALIDATED:;CANCELLED:a;COMPLETED:;" && calls == 0 &&
            !run.receipts[0].output && run.receipts[0].input_refs.empty(), "initial cancellation ran callback");
}
void invalid_identifiers_requests_and_budgets() {
    const std::array<std::string, 7> bad{{"", std::string(65, 'a'), "bad id", "bad/id", "bad.id", std::string("a\0b", 3), std::string("a\xc3\xa4", 3)}};
    for (const auto& id : bad) {
        auto v = single(); v.dag_id = id; rejected(v, registry, "InvalidDagSchedulerV2Spec");
        v = single(); v.nodes[0].node_id = id; rejected(v, registry, "InvalidDagSchedulerV2Spec");
        v = single(); v.nodes[0].callback_id = id; rejected(v, registry, "InvalidDagSchedulerV2Spec", "a");
        v = single(); v.nodes[0].dependencies = {id}; rejected(v, registry, "InvalidDagSchedulerV2Spec", "a");
    }
    auto v = single(); v.scheduler_version = "dag-scheduler-v1"; rejected(v, registry, "InvalidDagSchedulerV2Spec");
    v = single(); v.nodes[0].request_digest = sha256("tampered"); rejected(v, registry, "InvalidDagSchedulerV2Spec", "a");
    v = single(); v.nodes[0].request_digest.clear(); rejected(v, registry, "InvalidDagSchedulerV2Spec", "a");
    v = spec({node("a", "ready", {}, std::string(65537, 'x'))}); rejected(v, registry, "InvalidDagSchedulerV2Spec", "a");
    for (auto budget : {0ULL, 65ULL, ~0ULL}) { v = single(); v.max_started_nodes = budget; rejected(v, registry, "InvalidDagSchedulerV2Spec"); }
    for (auto budget : {0ULL, 4194305ULL, ~0ULL}) { v = single(); v.max_total_receipt_bytes = budget; rejected(v, registry, "InvalidDagSchedulerV2Spec"); }
}
void invalid_registry() {
    rejected(single(), {}, "InvalidDagSchedulerV2CallbackRegistry");
    const std::array<DagSchedulerCallbackEntryV2, 2> unsorted{{{"ready", ready}, {"fail", fail}}};
    rejected(single(), unsorted, "InvalidDagSchedulerV2CallbackRegistry", "fail");
    const std::array<DagSchedulerCallbackEntryV2, 2> duplicate{{{"ready", ready}, {"ready", ready}}};
    rejected(single(), duplicate, "InvalidDagSchedulerV2CallbackRegistry", "ready");
    const std::array<DagSchedulerCallbackEntryV2, 2> unused{{{"ready", ready}, {"z", nullptr}}};
    rejected(single(), unused, "InvalidDagSchedulerV2CallbackRegistry", "z");
    const std::array<DagSchedulerCallbackEntryV2, 1> null{{{"ready", nullptr}}};
    rejected(single(), null, "InvalidDagSchedulerV2CallbackRegistry", "ready");
    for (const auto& id : {std::string{}, std::string("bad/id"), std::string(65, 'a')}) {
        const std::array<DagSchedulerCallbackEntryV2, 1> malformed{{{id, ready}}};
        rejected(single(), malformed, "InvalidDagSchedulerV2CallbackRegistry");
    }
}
void invalid_graph() {
    rejected(spec({}), registry, "InvalidDagSchedulerV2Spec");
    std::vector<DagSchedulerNodeSpecV2> nodes;
    for (int i = 0; i < 65; ++i) nodes.push_back(node("n" + std::to_string(i)));
    rejected(spec(nodes), registry, "InvalidDagSchedulerV2Spec");
    rejected(spec({node("a"), node("a")}), registry, "DuplicateDagSchedulerV2Node", "a");
    rejected(spec({node("a", "missing")}), registry, "UnknownDagSchedulerV2Callback", "missing");
    rejected(spec({node("a", "ready", {"missing"})}), registry, "UnknownDagSchedulerV2Dependency", "missing");
    rejected(spec({node("a", "ready", {"a"})}), registry, "InvalidDagSchedulerV2Dependency", "a");
    rejected(spec({node("a"), node("b", "ready", {"a", "a"})}), registry, "InvalidDagSchedulerV2Dependency", "b");
    rejected(spec({node("a", "ready", {"b"}), node("b", "ready", {"a"})}), registry, "DagSchedulerV2Cycle", "a");
    rejected(spec({node("a"), node("b", "ready", {"c"}), node("c", "ready", {"b"})}), registry, "DagSchedulerV2Cycle", "b");
    nodes.clear(); std::vector<std::string> deps;
    for (int i = 0; i < 16; ++i) { deps.push_back("p" + std::to_string(i)); nodes.push_back(node(deps.back())); }
    nodes.push_back(node("z", "ready", deps));
    rejected(spec(nodes), registry, "InvalidDagSchedulerV2Dependency", "z");
}
// Catches publishing before callback-result validation; sentinel must stay unchanged.
void invalid_results_and_exceptions_no_partial_run() {
    std::vector<DagSchedulerCallbackResultV2> invalid;
    for (const auto status : {Status::ready, Status::failed})
        for (const auto& code : {std::string{}, std::string(65, 'x'), std::string("bad id"), std::string("bad/id"), std::string("bad.id"), std::string("a\0b", 3), std::string("a\xc3\xa4", 3)})
            invalid.push_back({status, code, "", status == Status::ready ? sha256("") : ""});
    invalid.push_back({static_cast<Status>(99), "VALID", "", ""});
    invalid.push_back({Status::ready, "VALID", "x", sha256("tampered")});
    invalid.push_back({Status::ready, "VALID", "", ""});
    invalid.push_back(success("VALID", std::string(65537, 'x')));
    invalid.push_back({Status::failed, "VALID", "x", ""});
    invalid.push_back({Status::failed, "VALID", "", sha256("")});
    for (const auto& value : invalid) {
        reset(); auto sentinel = execute_dag_scheduler_v2(single(), registry); const auto before = projection(sentinel);
        reset(); next_result = value; bool rejected_result = false;
        try { sentinel = execute_dag_scheduler_v2(spec({node("a"), node("b", "result", {"a"}), node("c", "forbidden", {"b"})}), registry); }
        catch (const Error& e) { error_matches(e, "InvalidDagSchedulerV2CallbackResult", "b"); rejected_result = true; }
        require(rejected_result && projection(sentinel) == before && calls == 1 && result_calls == 1 && forbidden_calls == 0, "invalid result published run or started successor");
    }
    reset(); auto sentinel = execute_dag_scheduler_v2(single(), registry); const auto before = projection(sentinel); reset();
    bool propagated = false;
    try { sentinel = execute_dag_scheduler_v2(spec({node("a"), node("b", "throw", {"a"}), node("c", "forbidden", {"b"})}), registry); }
    catch (const std::runtime_error& e) { propagated = std::string(e.what()) == "callback exploded"; }
    require(propagated && projection(sentinel) == before && calls == 1 && result_calls == 1 && forbidden_calls == 0, "exception translated or partial run published");
}
// Hand-counted: output a=3, output b=2 + copied a=3, output c=1 + copied a=3 + b=2 => total 14.
void receipt_budget_counts_every_copy() {
    const auto graph = std::vector<DagSchedulerNodeSpecV2>{node("a", "ready", {}, "abc"), node("b", "ready", {"a"}, "de"), node("c", "ready", {"b", "a"}, "f"), node("z", "forbidden", {"c"})};
    reset(); const auto exact = execute_dag_scheduler_v2(spec({graph[0], graph[1], graph[2]}, 3, 14), registry);
    require(exact.receipts.size() == 3 && calls == 3 && exact.receipts[2].input_refs.size() == 2, "exact 14-byte budget rejected");
    reset(); auto sentinel = execute_dag_scheduler_v2(single(), registry); const auto before = projection(sentinel); reset();
    bool overflow = false;
    try { sentinel = execute_dag_scheduler_v2(spec(graph, 4, 13), registry); }
    catch (const Error& e) { error_matches(e, "DagSchedulerV2ReceiptBudgetExceeded", "c"); overflow = true; }
    require(overflow && projection(sentinel) == before && calls == 3 && forbidden_calls == 0, "input copies ignored, partial run published or successor started");
    reset(); const auto failed = execute_dag_scheduler_v2(spec({node("a", "ready", {}, "abc"), node("b", "fail", {"a"})}, 2, 3), registry);
    require(calls == 1 && fail_calls == 1 && failed.receipts[1].input_refs.empty() && !failed.receipts[1].output, "failed receipt charged discarded inputs");
    reset(); const auto empty = execute_dag_scheduler_v2(spec({node("a", "ready", {}, "")}, 1, 1), registry);
    require(empty.receipts[0].output->payload_bytes.empty(), "empty payload budget mishandled");
}
void valid_boundaries() {
    reset(); const std::string id = "AZaz09_-" + std::string(56, 'x');
    auto value = spec({node(id, id, {}, std::string(65536, 'q'))}, 1, 65536); value.dag_id = id;
    const std::array<DagSchedulerCallbackEntryV2, 1> catalog{{{id, result}}};
    next_result = success(id, std::string(65536, 'p'));
    const auto run = execute_dag_scheduler_v2(value, catalog);
    require(result_calls == 1 && run.receipts[0].output->payload_bytes.size() == 65536 && run.receipts[0].output->result_code == id, "valid byte/identifier boundary rejected");
    reset(); std::vector<DagSchedulerNodeSpecV2> nodes; std::vector<std::string> deps;
    for (int i = 0; i < 15; ++i) { deps.push_back("p" + std::to_string(i)); nodes.push_back(node(deps.back())); }
    nodes.push_back(node("z", "ready", deps));
    const auto merge = execute_dag_scheduler_v2(spec(nodes), registry);
    require(calls == 16 && merge.receipts.back().input_refs.size() == 15 && observations.back().size() == 15, "valid 15-dependency boundary rejected");
    reset(); nodes.clear();
    for (int i = 63; i >= 0; --i) {
        const auto idn = "n" + std::string(i < 10 ? "0" : "") + std::to_string(i);
        const auto parent = "n" + std::string(i - 1 < 10 ? "0" : "") + std::to_string(i - 1);
        nodes.push_back(node(idn, "ready", i == 0 ? std::vector<std::string>{} : std::vector<std::string>{parent}));
    }
    const auto chain = execute_dag_scheduler_v2(spec(nodes), registry);
    require(calls == 64 && chain.receipts.size() == 64 && chain.trace.size() == 130 && chain.receipts.front().node_id == "n00" && chain.receipts.back().node_id == "n63", "64-node chain failed");
    for (std::size_t i = 0; i < 64; ++i) {
        require(chain.receipts[i].outcome == Outcome::ready && observations[i].size() == (i == 0 ? 0U : 1U) &&
                chain.trace[1 + 2 * i].event == Event::started && chain.trace[1 + 2 * i].node_id == chain.receipts[i].node_id && chain.trace[2 + 2 * i].event == Event::ready, "chain inputs/trace changed");
    }
    reset(); nodes.clear();
    for (int i = 0; i < 64; ++i) nodes.push_back(node("n" + std::to_string(i), "ready", {}, std::string(65536, 'p')));
    const auto maximum = execute_dag_scheduler_v2(spec(nodes, 64, 4194304), registry);
    require(calls == 64 && maximum.receipts.size() == 64, "exact 4-MiB receipt budget rejected");
}
}
int main() {
    const std::pair<const char*, void (*)()> tests[] = {
        {"full_flow_and_repeatability", full_flow_and_repeatability},
        {"direct_sorted_binary_inputs", direct_sorted_binary_inputs},
        {"failure_blocking_before_controls", failure_blocking_before_controls},
        {"cancellation_and_start_budget", cancellation_and_start_budget},
        {"invalid_identifiers_requests_and_budgets", invalid_identifiers_requests_and_budgets},
        {"invalid_registry", invalid_registry}, {"invalid_graph", invalid_graph},
        {"invalid_results_and_exceptions_no_partial_run", invalid_results_and_exceptions_no_partial_run},
        {"receipt_budget_counts_every_copy", receipt_budget_counts_every_copy},
        {"valid_boundaries", valid_boundaries}};
    int failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) { ++failures; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
    }
    std::cout << std::size(tests) - failures << '/' << std::size(tests) << " contracts passed\n";
    return failures == 0 ? 0 : 1;
}
