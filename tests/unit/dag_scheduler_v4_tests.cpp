#include "bruce/dag_scheduler/dag_scheduler_v4.hpp"
#include "../../apps/dag_scheduler_v3_cli.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
using namespace bruce;
namespace v3 = dag_scheduler_v3_cli_detail;
constexpr std::string_view header = "BRUCE_DAG_SCHEDULER_RUN_V4\n";
constexpr std::string_view empty_digest = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class F> void rejects(F action, const char* message) {
    try { action(); } catch (const Error&) { return; }
    throw std::runtime_error(message);
}
DagSchedulerNodeSpecV2 node(std::string id, std::string callback,
        std::vector<std::string> deps = {}, std::string request = {}) {
    auto digest = sha256(request);
    return {std::move(id), std::move(callback), std::move(deps), std::move(request), std::move(digest)};
}
DagSchedulerRunV2 run(std::vector<DagSchedulerNodeSpecV2> nodes, std::uint64_t starts = 64) {
    return execute_dag_scheduler_v2({"dag-scheduler-v2", "tiny", std::move(nodes), starts, 4194304}, v3::registry);
}
DagSchedulerRunV2 single() { return run({node("a", "literal-v1")}); }
DagSchedulerRunV2 flow() {
    return run({node("a", "literal-v1", {}, std::string("A\0\xff", 3)),
                node("b", "append-v1", {"a"}, "\n:"),
                node("c", "literal-v1", {}, "C"),
                node("d", "join-v1", {"b", "c"}, "|")});
}
std::string envelope(std::string_view payload) {
    return std::string(header) + sha256(payload) + '\n' + std::string(payload);
}
// Independent unchecked fixture encoder: enables digest-correct malformed bundles
// without relying on the production packer's validation.
std::string fixture(const DagSchedulerRunV2& value) {
    std::string p;
    auto n = [&](std::uint64_t v) { p += std::to_string(v) + '\n'; };
    auto s = [&](std::string_view v) { p += std::to_string(v.size()) + ':'; p += v; };
    s(value.spec.scheduler_version); s(value.spec.dag_id);
    n(value.spec.max_started_nodes); n(value.spec.max_total_receipt_bytes);
    n(value.spec.nodes.size());
    for (const auto& x : value.spec.nodes) {
        s(x.node_id); s(x.callback_id); n(x.dependencies.size());
        for (const auto& d : x.dependencies) s(d);
        s(x.request_bytes); s(x.request_digest);
    }
    n(value.receipts.size());
    for (const auto& x : value.receipts) {
        s(x.node_id); s(x.callback_id); s(x.request_digest); n(x.input_refs.size());
        for (const auto& i : x.input_refs) {
            s(i.producer_node_id); s(i.result_code); s(i.payload_bytes); s(i.payload_digest);
        }
        n(static_cast<unsigned>(x.outcome)); n(x.output ? 1 : 0);
        if (x.output) {
            s(x.output->output_version); s(x.output->result_code);
            s(x.output->payload_bytes); s(x.output->payload_digest);
        }
        s(x.failure_code);
    }
    n(value.trace.size());
    for (const auto& x : value.trace) { n(static_cast<unsigned>(x.event)); s(x.node_id); }
    return envelope(p);
}
void reject_bundle(const std::string& bytes) {
    rejects([&] { (void)replay_dag_scheduler_run_v4(bytes, v3::registry); }, "accepted malformed bundle");
}
void reject_encoding(const std::string& bytes) {
    rejects([&] { (void)inspect_dag_scheduler_run_v4(bytes, v3::registry); }, "inspect accepted malformed encoding");
    reject_bundle(bytes);
}
void canonical_roundtrip() {
    const auto original = flow();
    const auto bytes = pack_dag_scheduler_run_v4(original);
    const auto decoded = inspect_dag_scheduler_run_v4(bytes, v3::registry);
    const auto replayed = replay_dag_scheduler_run_v4(bytes, v3::registry);
    require(replayed.receipts.size() == 4 && replayed.trace.size() == 10, "lost receipts/trace");
    require(replayed.receipts[3].output->payload_bytes == std::string("A\0\xff\n:|C", 7), "binary flow changed");
    require(decoded.spec.nodes[0].request_bytes == std::string("A\0\xff", 3), "request bytes changed");
    require(pack_dag_scheduler_run_v4(decoded) == bytes && pack_dag_scheduler_run_v4(replayed) == bytes,
            "non-identical repack");
    const auto limited = run({node("a", "literal-v1"), node("b", "append-v1", {"a"})}, 1);
    const auto replay = replay_dag_scheduler_run_v4(pack_dag_scheduler_run_v4(limited), v3::registry);
    require(replay.receipts[1].outcome == DagSchedulerReceiptOutcomeV2::cancelled &&
            replay.receipts[1].failure_code == "START_BUDGET_EXHAUSTED", "lost budget cancellation");
}
void golden_bytes() {
    const std::string h(empty_digest);
    const std::string payload = "16:dag-scheduler-v24:tiny64\n4194304\n1\n1:a10:literal-v10\n0:64:" + h +
        "1\n1:a10:literal-v164:" + h + "0\n0\n1\n23:dag-scheduler-output-v213:LITERAL_READY0:64:" + h +
        "0:4\n0\n0:1\n1:a2\n1:a6\n0:";
    require(pack_dag_scheduler_run_v4(single()) == envelope(payload), "canonical fixture differs");
}
void corrupt_envelope_and_lengths() {
    const auto bytes = pack_dag_scheduler_run_v4(single());
    auto changed = bytes; changed[0] = 'X'; reject_encoding(changed);
    changed = bytes; changed[header.size()] = changed[header.size()] == 'a' ? 'b' : 'a'; reject_encoding(changed);
    changed = bytes; changed[header.size() + 64] = ':'; reject_encoding(changed);
    reject_encoding(bytes + 'x');
    const auto payload = bytes.substr(header.size() + 65);
    for (const auto prefix : {"017:", "+16:", "-1:", "18446744073709551616:", "9999999999999999999:", "65:", ":"})
        reject_encoding(envelope(std::string(prefix) + payload.substr(3)));
    changed = payload;
    const auto flag = changed.find("0\n0\n1\n23:");
    require(flag != std::string::npos, "bad optional output marker");
    changed[flag + 4] = '2'; reject_encoding(envelope(changed));
    // Valid hashes must not turn truncated fields into accepted input.
    for (std::size_t i = 0; i < payload.size(); ++i) reject_encoding(envelope(payload.substr(0, i)));
    reject_encoding(envelope(payload + "0:"));
    reject_encoding(std::string(dag_scheduler_v4_max_bundle_bytes + 1, 'x'));
    const auto marker = payload.find("64\n4194304\n1\n");
    require(marker != std::string::npos, "bad fixture marker");
    for (const auto count : {"0", "65", "18446744073709551615", "01"}) {
        changed = payload;
        changed.replace(marker + 11, 1, count);
        reject_encoding(envelope(changed));
    }
}
void corrupt_fields_and_counts() {
    const auto good = flow();
    const std::vector<std::function<void(DagSchedulerRunV2&)>> mutations{
        [](auto& r) { r.spec.scheduler_version = "dag-scheduler-v1"; },
        [](auto& r) { r.spec.dag_id = std::string(65, 'a'); },
        [](auto& r) { r.spec.max_started_nodes = 0; },
        [](auto& r) { r.spec.max_started_nodes = 65; },
        [](auto& r) { r.spec.max_total_receipt_bytes = 4194305; },
        [](auto& r) { r.spec.max_total_receipt_bytes = 1; },
        [](auto& r) { r.spec.nodes[0].request_bytes += 'x'; },
        [](auto& r) { r.spec.nodes[0].request_digest[0] = 'x'; },
        [](auto& r) { r.spec.nodes[0].request_bytes = std::string(65537, 'x'); r.spec.nodes[0].request_digest = sha256(r.spec.nodes[0].request_bytes); },
        [](auto& r) { r.spec.nodes[0].callback_id = "custom"; },
        [](auto& r) { r.spec.nodes[0].callback_id = "append-v1"; },
        [](auto& r) { std::swap(r.spec.nodes[0], r.spec.nodes[1]); },
        [](auto& r) { std::swap(r.spec.nodes[3].dependencies[0], r.spec.nodes[3].dependencies[1]); },
        [](auto& r) { r.spec.nodes[3].dependencies.resize(16, "a"); },
        [](auto& r) { r.receipts.pop_back(); },
        [](auto& r) { r.receipts.resize(65); },
        [](auto& r) { std::swap(r.receipts[0], r.receipts[1]); },
        [](auto& r) { r.receipts[0].callback_id = "append-v1"; },
        [](auto& r) { r.receipts[0].request_digest = std::string(64, '0'); },
        [](auto& r) { r.receipts[0].outcome = static_cast<DagSchedulerReceiptOutcomeV2>(4); },
        [](auto& r) { r.receipts[0].output.reset(); },
        [](auto& r) { r.receipts[0].output->output_version = "wrong"; },
        [](auto& r) { r.receipts[0].output->payload_bytes += 'x'; },
        [](auto& r) { r.receipts[0].output->payload_bytes = std::string(65537, 'x'); },
        [](auto& r) { r.receipts[0].output->payload_digest = std::string(64, '0'); },
        [](auto& r) { r.receipts[0].output->result_code = std::string(65, 'x'); },
        [](auto& r) { r.receipts[0].failure_code = "UNEXPECTED"; },
        [](auto& r) { r.receipts[3].input_refs.resize(16); },
        [](auto& r) { std::swap(r.receipts[3].input_refs[0], r.receipts[3].input_refs[1]); },
        [](auto& r) { r.receipts[3].input_refs[0].payload_bytes += 'x'; },
        [](auto& r) { r.receipts[3].input_refs[0].payload_digest = std::string(64, '0'); },
        [](auto& r) { r.trace.resize(131); },
        [](auto& r) { r.trace[0].event = static_cast<DagSchedulerTraceEventV2>(7); },
        [](auto& r) { r.trace[0].node_id = "a"; },
        [](auto& r) { r.trace[1].node_id = "missing"; },
    };
    for (const auto& mutate : mutations) {
        auto value = good; mutate(value); reject_encoding(fixture(value));
        rejects([&] { (void)pack_dag_scheduler_run_v4(value); }, "packer accepted invalid run");
    }
}
int callback_calls = 0;
DagSchedulerCallbackResultV2 unexpected(std::string_view, std::span<const DagSchedulerInputV2>) {
    ++callback_calls;
    throw Error("inspect executed callback");
}
void replay_equality_and_inspect_boundary() {
    const auto bytes = pack_dag_scheduler_run_v4(flow());
    const std::array<DagSchedulerCallbackEntryV2, 3> watched{{
        {"append-v1", unexpected}, {"join-v1", unexpected}, {"literal-v1", unexpected}}};
    (void)inspect_dag_scheduler_run_v4(bytes, watched);
    require(callback_calls == 0, "inspect executed code");
    auto altered = single();
    altered.receipts[0].output->payload_bytes = "forged";
    altered.receipts[0].output->payload_digest = sha256("forged");
    const auto forged = fixture(altered);
    (void)inspect_dag_scheduler_run_v4(forged, v3::registry);
    reject_bundle(forged);
    altered = single(); altered.spec.nodes[0].request_bytes = "changed";
    altered.spec.nodes[0].request_digest = sha256("changed");
    altered.receipts[0].request_digest = altered.spec.nodes[0].request_digest;
    reject_bundle(fixture(altered));
    altered = flow(); std::swap(altered.trace[1], altered.trace[2]); reject_bundle(fixture(altered));
    altered = single(); altered.receipts[0].output->result_code = "FORGED"; reject_bundle(fixture(altered));
    const std::array<DagSchedulerCallbackEntryV2, 1> custom{{{"custom", unexpected}}};
    rejects([&] { (void)replay_dag_scheduler_run_v4(bytes, custom); }, "accepted arbitrary registry");
    require(callback_calls == 0, "invalid input reached callbacks");
}
void maximum_boundaries() {
    const auto exact_field = run({node(std::string(64, 'a'), "literal-v1", {}, std::string(65536, '\0'))});
    require(replay_dag_scheduler_run_v4(pack_dag_scheduler_run_v4(exact_field), v3::registry)
        .receipts[0].output->payload_bytes.size() == 65536, "rejected field limit");
    std::vector<DagSchedulerNodeSpecV2> nodes;
    for (int i = 0; i < 64; ++i) nodes.push_back(node("n" + std::to_string(100 + i), "literal-v1"));
    auto max_nodes = run(nodes);
    require(replay_dag_scheduler_run_v4(pack_dag_scheduler_run_v4(max_nodes), v3::registry).trace.size() == 130,
        "rejected maximum nodes or trace");
    for (int i = 0; i < 15; ++i) nodes.back().dependencies.push_back(nodes[i].node_id);
    nodes.back().callback_id = "join-v1";
    require(replay_dag_scheduler_run_v4(pack_dag_scheduler_run_v4(run(nodes)), v3::registry)
        .receipts.back().input_refs.size() == 15, "rejected dependency/input limit");
    nodes.resize(40);
    for (auto& n : nodes) { n.request_bytes = std::string(65536, 'x'); n.request_digest = sha256(n.request_bytes); }
    auto large = run(nodes);
    const auto oversized = fixture(large);
    require(oversized.size() > dag_scheduler_v4_max_bundle_bytes, "bad oversized fixture");
    rejects([&] { (void)pack_dag_scheduler_run_v4(large); }, "packer exceeded total cap");
    reject_bundle(oversized);
    const auto excess = oversized.size() - dag_scheduler_v4_max_bundle_bytes;
    nodes.back().request_bytes.resize(65536 - (excess + 1) / 2);
    nodes.back().request_digest = sha256(nodes.back().request_bytes);
    large = run(nodes);
    // Changing one DAG-ID byte reaches the exact cap if the paired payload fields left one byte.
    if (fixture(large).size() < dag_scheduler_v4_max_bundle_bytes) large.spec.dag_id += 'x';
    const auto exact = pack_dag_scheduler_run_v4(large);
    require(exact.size() == dag_scheduler_v4_max_bundle_bytes, "fixture missed exact file limit");
    require(pack_dag_scheduler_run_v4(replay_dag_scheduler_run_v4(exact, v3::registry)) == exact,
        "exact total cap rejected");
    large.spec.dag_id += 'x';
    rejects([&] { (void)pack_dag_scheduler_run_v4(large); }, "accepted cap plus one");
}
}
int main() {
    try {
        canonical_roundtrip(); golden_bytes(); corrupt_envelope_and_lengths();
        corrupt_fields_and_counts(); replay_equality_and_inspect_boundary(); maximum_boundaries();
        std::cout << "DAG scheduler V4 contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
