#include "bruce/dag_scheduler/dag_scheduler_v4.hpp"
#include "../../../apps/dag_scheduler_v3_cli.hpp"
#include "bruce/mod/catalog.hpp"
#include "bruce/mods/mcp/mod.hpp"
#include "bruce/phase_dag/nanocell_adapter_v2.hpp"
#include "bruce/phase_dag/nanocell_adapter_v3.hpp"
#include "bruce/phase_dag/payload_gate_adapter_v3.hpp"
#include "bruce/evidence/bundle.hpp"
#include "../../../mods/mcp/src/mcp_inspector.hpp"
#include "../../../mods/mcp/src/mcp_inspector_assets.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}
void contains(std::string_view json, std::string_view fragment) {
    require(json.find(fragment) != std::string_view::npos, std::string("missing model field: ") + std::string(fragment));
}

bruce::NanoCellPhaseRequestV1 request(std::string technique = "masked-match-v1") {
    bruce::NanoCellPhaseRequestV1 value;
    value.core_layout_version = "nanoneuron-core-v1";
    value.core_bytes = {0x10, 0x20, 0xA5, 0x40, 0x50, 0x60, 0x70, 0x80};
    value.active_index = 2;
    value.context_id = "reference.context_1";
    value.context_bytes = "nanocell-reference-context-v1";
    value.context_digest = bruce::sha256(value.context_bytes);
    value.technique_id = std::move(technique);
    value.max_operations = 16;
    value.max_trace_entries = 18;
    return value;
}
bruce::PhaseNodeSpecV2 node(std::string id, const bruce::NanoCellPhaseRequestV1& value,
                           std::vector<std::string> dependencies = {}) {
    const auto bytes = bruce::pack_nanocell_phase_request_v1(value);
    return {std::move(id), "nanocell-phase-v1", "v1", std::move(dependencies), bytes, bruce::sha256(bytes)};
}
bruce::PhaseDagSpecV2 spec(std::vector<bruce::PhaseNodeSpecV2> nodes) {
    bruce::PhaseDagSpecV2 value;
    value.dag_id = "mcp-inspector-test";
    value.nodes = std::move(nodes);
    value.max_started_nodes = value.nodes.size();
    value.max_receipt_bytes = 65536;
    value.max_trace_entries = 32;
    return value;
}
bruce::PhaseDagRunV2 run(const bruce::PhaseDagSpecV2& value) {
    return bruce::execute_phase_dag_v2(value, bruce::phase_dag_nanocell_catalog_v2());
}
std::string valid_bundle() { return bruce::pack_phase_dag_run_v2(run(spec({node("a", request())}))); }
std::string inspect_valid(const bruce::PhaseDagRunV2& value) {
    const auto result = bruce::inspect_phase_dag_v2_bundle(bruce::pack_phase_dag_run_v2(value));
    require(result.status == bruce::McpInspectionStatusV1::valid && result.code == "VALID", "canonical bundle must be VALID");
    return result.json;
}
bruce::PhaseNodeSpecV3 node_v3(std::string id, const bruce::NanoCellPhaseRequestV1& value) {
    const auto bytes = bruce::pack_nanocell_phase_request_v1(value);
    return {std::move(id), "nanocell-phase-v3", "v1", {}, bytes, bruce::sha256(bytes)};
}
bruce::PhaseNodeSpecV3 gate_v3(std::string id, std::vector<std::string> dependencies) {
    const auto bytes = bruce::pack_payload_gate_request_v1();
    return {std::move(id), "payload-gate-v1", "v1", std::move(dependencies), bytes, bruce::sha256(bytes)};
}
bruce::PhaseDagSpecV3 spec_v3(std::vector<bruce::PhaseNodeSpecV3> nodes) {
    bruce::PhaseDagSpecV3 value;
    value.dag_id = "mcp-inspector-v3-test";
    value.nodes = std::move(nodes);
    value.max_started_nodes = value.nodes.size();
    value.max_receipt_bytes = 65536;
    value.max_trace_entries = 64;
    return value;
}
bruce::PhaseDagRunV3 run_v3(const bruce::PhaseDagSpecV3& value) {
    return bruce::execute_phase_dag_v3(value, bruce::phase_dag_nanocell_catalog_v3());
}
bruce::PhaseDagRunV3 valid_run_v3() {
    return run_v3(spec_v3({node_v3("a", request()), gate_v3("gate", {"a"})}));
}
std::string valid_bundle_v3() { return bruce::pack_phase_dag_run_v3(valid_run_v3()); }
std::string inspect_valid_v3(const bruce::PhaseDagRunV3& value) {
    const auto result = bruce::inspect_phase_dag_v3_bundle(bruce::pack_phase_dag_run_v3(value));
    require(result.status == bruce::McpInspectionStatusV1::valid && result.code == "VALID", "canonical V3 bundle must be VALID");
    return result.json;
}
void rejected_model_v3(std::string_view bytes) {
    const auto result = bruce::inspect_phase_dag_v3_bundle(bytes);
    require(result.status == bruce::McpInspectionStatusV1::rejected && result.code == "REJECTED", "bad V3 bundle must be REJECTED");
    require(result.json == "{\"status\":\"REJECTED\",\"code\":\"REJECTED\"}", "V3 rejection contains only fixed public fields");
}
// Catch missing V3 digest/size metadata and any raw-byte projection.
void valid_model_v3() {
    const auto value = valid_run_v3();
    const auto json = inspect_valid_v3(value);
    for (const auto fragment : {"\"status\":\"VALID\"", "\"code\":\"VALID\"", "\"dag_version\":\"phase-dag-v3\"",
            "\"dag_id\":\"mcp-inspector-v3-test\"", "\"nodes\":[", "\"receipts\":[", "\"trace\":[",
            "\"node_id\":\"a\"", "\"node_id\":\"gate\"", "\"adapter_id\":\"nanocell-phase-v3\"",
            "\"adapter_id\":\"payload-gate-v1\"", "\"adapter_version\":\"v1\"", "\"dependencies\":[\"a\"]",
            "\"producer_node_id\":\"a\"", "\"producer_adapter_id\":\"nanocell-phase-v3\"",
            "\"producer_adapter_version\":\"v1\"", "\"producer_status\":\"COMPLETE\"",
            "\"outcome\":\"READY\"", "\"output_version\":\"phase-output-v3\"", "\"status\":\"COMPLETE\"",
            "\"claim_scope\":\"nanocell-phase-v3\"", "\"claim_scope\":\"payload-gate-v1\"", "\"failure_code\":\"\"",
            "\"event\":\"VALIDATED\"", "\"event\":\"STARTED\"", "\"event\":\"READY\"", "\"event\":\"COMPLETED\""}) contains(json, fragment);
    for (const auto& receipt : value.receipts) {
        const auto& output = *receipt.output;
        contains(json, "\"request_digest\":\"" + receipt.request_digest + "\"");
        contains(json, "\"output_digest\":\"" + output.output_digest + "\"");
        contains(json, "\"evidence_digest\":\"" + output.evidence_digest + "\"");
        contains(json, "\"payload_digest\":\"" + output.payload_digest + "\"");
        contains(json, "\"payload_size\":" + std::to_string(output.payload_bytes.size()) + ",");
        for (const auto& secret : {output.output_bytes, output.evidence_bytes, output.payload_bytes})
            require(json.find(secret) == std::string::npos, "V3 raw output/evidence/payload is hidden");
    }
    const auto& input = value.receipts[1].input_refs.at(0);
    contains(json, "\"producer_output_digest\":\"" + input.producer_output_digest + "\"");
    contains(json, "\"producer_evidence_digest\":\"" + input.producer_evidence_digest + "\"");
    contains(json, "\"producer_payload_digest\":\"" + input.producer_payload_digest + "\",\"payload_size\":" +
        std::to_string(input.producer_payload_bytes.size()) + "}");
    for (const auto& secret : {value.spec.nodes[0].request_bytes, value.spec.nodes[1].request_bytes,
            std::string("nanocell-reference-context-v1"), std::string("BRUCE_PHASE_DAG_RUN_V3")})
        require(json.find(secret) == std::string::npos, "V3 request/context/bundle is hidden");
    for (const auto field : {"output_bytes", "evidence_bytes", "payload_bytes", "producer_payload_bytes", "request_bytes", "context_bytes"})
        require(json.find(field) == std::string::npos, "raw byte fields are absent");
}
void cross_version_rejection() {
    rejected_model_v3(valid_bundle());
    const auto result = bruce::inspect_phase_dag_v2_bundle(valid_bundle_v3());
    require(result.status == bruce::McpInspectionStatusV1::rejected &&
        result.json == "{\"status\":\"REJECTED\",\"code\":\"REJECTED\"}", "V2 rejects V3 with fixed response");
}
// Catch relying only on outer hashes instead of replay against the fixed catalog.
void mutated_bundle_v3() {
    auto bytes = valid_bundle_v3(); bytes.back() = bytes.back() == 'x' ? 'y' : 'x';
    rejected_model_v3(bytes);
    rejected_model_v3("BRUCE_PHASE_DAG_RUN_V3\n/home/private/raw-path\nsecret-request");
    rejected_model_v3("");
    for (int mutation = 0; mutation < 3; ++mutation) {
        auto value = valid_run_v3();
        auto& output = *value.receipts[0].output;
        if (mutation == 0) {
            output.payload_bytes.back() ^= 1;
            output.payload_digest = bruce::sha256(output.payload_bytes);
            value.receipts[1].input_refs[0].producer_payload_bytes = output.payload_bytes;
            value.receipts[1].input_refs[0].producer_payload_digest = output.payload_digest;
        } else if (mutation == 1) {
            auto forged = bruce::pack_phase_dag_run_v3(value);
            const auto old_digest = output.payload_digest;
            const auto replacement = bruce::sha256("private-changed-payload");
            std::size_t position = 0;
            while ((position = forged.find(old_digest, position)) != std::string::npos) {
                forged.replace(position, old_digest.size(), replacement);
                position += replacement.size();
            }
            constexpr std::string_view header = "BRUCE_PHASE_DAG_RUN_V3\n";
            forged.replace(header.size(), 64, bruce::sha256(std::string_view(forged).substr(header.size() + 65)));
            rejected_model_v3(forged);
            continue;
        } else {
            value.spec.nodes[0].adapter_id = "private-unknown-adapter";
            value.receipts[0].adapter_id = value.spec.nodes[0].adapter_id;
        }
        rejected_model_v3(bruce::pack_phase_dag_run_v3(value));
    }
}
// Catch accidentally reusing the 1 MiB guard for the independent V3 inspector.
void size_limits_v3() {
    require(bruce::mcp_max_inspect_bytes_v1 == 1024U * 1024U, "V2 retains 1 MiB");
    require(bruce::mcp_max_inspect_bytes_v3 == 5U * 1024U * 1024U, "V3 has 5 MiB");
    std::vector<bruce::PhaseNodeSpecV3> nodes;
    const std::string request_bytes(65536, 'x');
    for (int i = 0; i < 16; ++i)
        nodes.push_back({"node" + std::to_string(10 + i), "nanocell-phase-v3", "v1", {}, request_bytes, bruce::sha256(request_bytes)});
    const auto value = run_v3(spec_v3(std::move(nodes)));
    const auto bytes = bruce::pack_phase_dag_run_v3(value);
    require(bytes.size() > 1024U * 1024U && bytes.size() < 5U * 1024U * 1024U, "canonical fixture lies between route limits");
    require(bruce::inspect_phase_dag_v3_bundle(bytes).code == "VALID", "V3 accepts replayable bundle above 1 MiB");
    require(bruce::inspect_phase_dag_v2_bundle(bytes).code == "REJECTED", "V2 rejects bundle above 1 MiB");
    for (const std::size_t size : {5U * 1024U * 1024U - 1U, 5U * 1024U * 1024U, 5U * 1024U * 1024U + 1U})
        rejected_model_v3(std::string(size, 'x'));
}
void mappings_v3() {
    const auto failed = inspect_valid_v3(run_v3(spec_v3({node_v3("a", request("unknown")), gate_v3("gate", {"a"})})));
    for (const auto fragment : {"\"outcome\":\"FAILED\"", "\"outcome\":\"BLOCKED\"", "\"event\":\"FAILED\"",
            "\"event\":\"BLOCKED\"", "\"output\":null", "\"failure_code\":\"UnknownNanoCellPhaseTechnique\""}) contains(failed, fragment);
    auto cancelled = spec_v3({node_v3("a", request())}); cancelled.max_started_nodes = 0;
    const auto json = inspect_valid_v3(run_v3(cancelled));
    contains(json, "\"outcome\":\"CANCELLED\""); contains(json, "\"event\":\"CANCELLED\"");
    contains(json, "\"failure_code\":\"DagBudgetExhausted\"");
    auto limited = request("rotate-match-v1"); limited.core_bytes[2] = 0x82; limited.max_operations = 1;
    contains(inspect_valid_v3(run_v3(spec_v3({node_v3("a", limited)}))), "\"status\":\"INCOMPLETE\"");
    contains(inspect_valid_v3(run_v3(spec_v3({node_v3("a", request("context-conflict-v1"))}))), "\"status\":\"REJECTED\"");
}
// Catch recursive blockers, READY predecessors, receipt-order joins and lost dependency order.
void scheduler_blocking_predecessors_v3() {
    const auto value = run_v3(spec_v3({
        gate_v3("a", {"z"}), gate_v3("b", {"a", "c", "z"}),
        node_v3("c", request()), gate_v3("d", {"b"}), node_v3("z", request("unknown"))}));
    require(value.receipts[0].node_id == "c" && value.receipts[1].node_id == "z" &&
        value.receipts[2].node_id == "a", "fixture receipts differ from dependency order");
    const auto json = inspect_valid_v3(value);
    contains(json, R"("scheduler_explanation":{"trace_model":"canonical-logical-v1","parallelism_recorded":false,"nodes":[)"
        R"({"node_id":"a","outcome":"BLOCKED","failure_code":"","dependencies":["z"],"blocking_predecessors":[{"node_id":"z","outcome":"FAILED","failure_code":"UnknownNanoCellPhaseTechnique"}]},)"
        R"({"node_id":"b","outcome":"BLOCKED","failure_code":"","dependencies":["a","c","z"],"blocking_predecessors":[{"node_id":"a","outcome":"BLOCKED","failure_code":""},{"node_id":"z","outcome":"FAILED","failure_code":"UnknownNanoCellPhaseTechnique"}]},)"
        R"({"node_id":"c","outcome":"READY","failure_code":"","dependencies":[],"blocking_predecessors":[]},)"
        R"({"node_id":"d","outcome":"BLOCKED","failure_code":"","dependencies":["b"],"blocking_predecessors":[{"node_id":"b","outcome":"BLOCKED","failure_code":""}]},)"
        R"({"node_id":"z","outcome":"FAILED","failure_code":"UnknownNanoCellPhaseTechnique","dependencies":[],"blocking_predecessors":[]}]})");
}
// Catch applying the BLOCKED rule to READY, FAILED or CANCELLED nodes.
void scheduler_nonblocking_outcomes_v3() {
    contains(inspect_valid_v3(valid_run_v3()),
        R"("scheduler_explanation":{"trace_model":"canonical-logical-v1","parallelism_recorded":false,"nodes":[)"
        R"({"node_id":"a","outcome":"READY","failure_code":"","dependencies":[],"blocking_predecessors":[]},)"
        R"({"node_id":"gate","outcome":"READY","failure_code":"","dependencies":["a"],"blocking_predecessors":[]}]})");
    auto value = spec_v3({node_v3("a", request("unknown")), gate_v3("b", {"a"}), gate_v3("c", {"b"})});
    value.max_started_nodes = 1;
    contains(inspect_valid_v3(run_v3(value)),
        R"("scheduler_explanation":{"trace_model":"canonical-logical-v1","parallelism_recorded":false,"nodes":[)"
        R"({"node_id":"a","outcome":"FAILED","failure_code":"UnknownNanoCellPhaseTechnique","dependencies":[],"blocking_predecessors":[]},)"
        R"({"node_id":"b","outcome":"CANCELLED","failure_code":"DagBudgetExhausted","dependencies":["a"],"blocking_predecessors":[]},)"
        R"({"node_id":"c","outcome":"CANCELLED","failure_code":"DagBudgetExhausted","dependencies":["b"],"blocking_predecessors":[]}]})");
}
// Catch expanding the public projection with private bytes or recorded scheduling claims.
void scheduler_metadata_only_v3() {
    const auto value = valid_run_v3();
    const auto json = inspect_valid_v3(value);
    contains(json, R"("trace_model":"canonical-logical-v1","parallelism_recorded":false)");
    for (const auto field : {"request_bytes", "context_bytes", "output_bytes", "evidence_bytes", "payload_bytes",
            "thread_id", "thread_ids", "timestamp", "started_at", "finished_at", "duration", "elapsed",
            "wave_width", "wave_size", "performance", "max_parallel_nodes"})
        require(json.find(field) == std::string::npos, std::string("private/scheduling field is absent: ") + field);
    for (const auto& receipt : value.receipts) {
        for (const auto& secret : {receipt.output->output_bytes, receipt.output->evidence_bytes, receipt.output->payload_bytes})
            require(json.find(secret) == std::string::npos, "scheduler projection hides output/evidence/payload bytes");
    }
    for (const auto& spec_node : value.spec.nodes)
        require(json.find(spec_node.request_bytes) == std::string::npos, "scheduler projection hides request bytes");
    require(json.find("nanocell-reference-context-v1") == std::string::npos, "scheduler projection hides context bytes");
    require(inspect_valid(run(spec({node("a", request())}))).find("scheduler_explanation") == std::string::npos,
        "V2 makes no new scheduler claim");
}
void canonical_writer_bundle_v3() {
    const auto bytes = valid_bundle_v3();
    require(bruce::pack_phase_dag_run_v3(bruce::replay_phase_dag_run_v3(bytes, bruce::phase_dag_nanocell_catalog_v3())) == bytes,
        "V3 writer directly replays and repacks byte-identically");
    require(bruce::inspect_phase_dag_v3_bundle(bytes).code == "VALID", "V3 writer emits inspectable canonical bundle");
}

// Catch dropped/wrong scheduler metadata and accidental serialization of raw bytes.
bruce::DagSchedulerRunV2 scheduler_run_v4() {
    const auto make_node = [](std::string id, std::string callback, std::vector<std::string> deps, std::string bytes) {
        auto digest = bruce::sha256(bytes);
        return bruce::DagSchedulerNodeSpecV2{std::move(id), std::move(callback), std::move(deps), std::move(bytes), std::move(digest)};
    };
    bruce::DagSchedulerSpecV2 value;
    value.dag_id = "mcp-scheduler-v4-test";
    value.max_started_nodes = 4;
    value.max_total_receipt_bytes = 4096;
    value.nodes = {
        make_node("a", "literal-v1", {}, "private-literal-a"),
        make_node("b", "append-v1", {"a"}, "private-append-b"),
        make_node("c", "literal-v1", {}, "private-literal-c"),
        make_node("d", "join-v1", {"b", "c"}, "private-join-d")};
    return bruce::execute_dag_scheduler_v2(value, bruce::dag_scheduler_v3_cli_detail::registry);
}
std::string scheduler_bundle_v4() { return bruce::pack_dag_scheduler_run_v4(scheduler_run_v4()); }
std::string inspect_scheduler_valid_v4(const bruce::DagSchedulerRunV2& run) {
    const auto result = bruce::inspect_dag_scheduler_v4_bundle(bruce::pack_dag_scheduler_run_v4(run));
    require(result.status == bruce::McpInspectionStatusV1::valid && result.code == "VALID", "V4 canonical bundle is VALID");
    return result.json;
}
void scheduler_projection_v4() {
    const auto value = scheduler_run_v4();
    const auto json = inspect_scheduler_valid_v4(value);
    contains(json, R"({"status":"VALID","code":"VALID","scheduler_version":"dag-scheduler-v2","dag_id":"mcp-scheduler-v4-test","max_started_nodes":4,"max_total_receipt_bytes":4096,)");
    // Expected sizes/payloads are hand-derived from the fixed literal/append/join flow.
    const std::vector<std::string> requests{"private-literal-a", "private-append-b", "private-literal-c", "private-join-d"};
    const std::vector<std::string> payloads{"private-literal-a", "private-literal-aprivate-append-b", "private-literal-c", "private-literal-aprivate-append-bprivate-join-dprivate-literal-c"};
    const std::vector<std::string> ids{"a", "b", "c", "d"};
    const std::vector<std::string> callbacks{"literal-v1", "append-v1", "literal-v1", "join-v1"};
    const std::vector<std::string> dependencies{"[]", "[\"a\"]", "[]", "[\"b\",\"c\"]"};
    const std::vector<std::size_t> request_sizes{17, 16, 17, 14};
    const std::vector<std::size_t> payload_sizes{17, 33, 17, 64};
    const std::vector<std::string> codes{"LITERAL_READY", "APPENDED_READY", "LITERAL_READY", "JOINED_READY"};
    for (std::size_t i = 0; i < ids.size(); ++i) {
        require(requests[i].size() == request_sizes[i] && payloads[i].size() == payload_sizes[i], "hand-derived fixture sizes");
        contains(json, "{\"node_id\":\"" + ids[i] + "\",\"callback_id\":\"" + callbacks[i] +
            "\",\"dependencies\":" + dependencies[i] + ",\"request_size\":" + std::to_string(request_sizes[i]) +
            ",\"request_digest\":\"" + bruce::sha256(requests[i]) + "\"}");
        contains(json, "{\"node_id\":\"" + ids[i] + "\",\"callback_id\":\"" + callbacks[i] +
            "\",\"request_size\":" + std::to_string(request_sizes[i]) + ",\"request_digest\":\"" + bruce::sha256(requests[i]) + "\",\"input_refs\":[");
        contains(json, "\"outcome\":\"READY\",\"output\":{\"output_version\":\"dag-scheduler-output-v2\",\"result_code\":\"" + codes[i] +
            "\",\"payload_size\":" + std::to_string(payload_sizes[i]) + ",\"payload_digest\":\"" + bruce::sha256(payloads[i]) + "\"},\"failure_code\":\"\"}");
        require(json.find(requests[i]) == std::string::npos && json.find(payloads[i]) == std::string::npos, "request and output markers stay private");
    }
    const auto input = [&](std::size_t i) {
        return "{\"producer_node_id\":\"" + ids[i] + "\",\"result_code\":\"" + codes[i] + "\",\"payload_size\":" +
            std::to_string(payload_sizes[i]) + ",\"payload_digest\":\"" + bruce::sha256(payloads[i]) + "\"}";
    };
    const std::vector<std::string> expected_inputs{"[]", "[" + input(0) + "]", "[]", "[" + input(1) + ',' + input(2) + "]"};
    for (std::size_t i = 0; i < ids.size(); ++i) {
        contains(json, "{\"node_id\":\"" + ids[i] + "\",\"callback_id\":\"" + callbacks[i] +
            "\",\"request_size\":" + std::to_string(request_sizes[i]) + ",\"request_digest\":\"" + bruce::sha256(requests[i]) +
            "\",\"input_refs\":" + expected_inputs[i] + ",\"outcome\":\"READY\",\"output\":{\"output_version\":\"dag-scheduler-output-v2\",\"result_code\":\"" + codes[i] +
            "\",\"payload_size\":" + std::to_string(payload_sizes[i]) + ",\"payload_digest\":\"" + bruce::sha256(payloads[i]) + "\"},\"failure_code\":\"\"}");
    }
    contains(json, R"("trace":[{"event":"VALIDATED","node_id":""},{"event":"STARTED","node_id":"a"},{"event":"READY","node_id":"a"},{"event":"STARTED","node_id":"b"},{"event":"READY","node_id":"b"},{"event":"STARTED","node_id":"c"},{"event":"READY","node_id":"c"},{"event":"STARTED","node_id":"d"},{"event":"READY","node_id":"d"},{"event":"COMPLETED","node_id":""}]})");
    for (const auto marker : {"BRUCE_DAG_SCHEDULER_RUN_V4", "request_bytes", "payload_bytes"})
        require(json.find(marker) == std::string::npos, "V4 raw byte field/header stays private");
}
void scheduler_rejected_v4(std::string_view bytes) {
    const auto result = bruce::inspect_dag_scheduler_v4_bundle(bytes);
    require(result.status == bruce::McpInspectionStatusV1::rejected && result.code == "REJECTED", "V4 invalid bundle is REJECTED");
    require(result.json == R"({"status":"REJECTED","code":"REJECTED"})", "V4 rejection has exactly fixed public fields");
}
void scheduler_rejections_v4() {
    auto bytes = scheduler_bundle_v4();
    auto changed = bytes; changed[0] = 'X'; scheduler_rejected_v4(changed);
    changed = bytes; constexpr std::string_view header = "BRUCE_DAG_SCHEDULER_RUN_V4\n";
    changed[header.size()] = changed[header.size()] == 'a' ? 'b' : 'a'; scheduler_rejected_v4(changed);
    changed = bytes; changed.back() = 'x'; scheduler_rejected_v4(changed);
    scheduler_rejected_v4(std::string(bruce::dag_scheduler_v4_max_bundle_bytes + 1, 'x'));
    scheduler_rejected_v4(""); scheduler_rejected_v4(valid_bundle()); scheduler_rejected_v4(valid_bundle_v3());
}
int scheduler_callback_calls_v4 = 0;
bruce::DagSchedulerCallbackResultV2 scheduler_sentinel_v4(std::string_view, std::span<const bruce::DagSchedulerInputV2>) {
    ++scheduler_callback_calls_v4;
    throw std::runtime_error("V4 Mcp must never execute a callback");
}
// Catch calling replay/execute instead of structural V4 inspect, including via the default entry.
void scheduler_no_reexecution_v4() {
    const std::array<bruce::DagSchedulerCallbackEntryV2, 3> sentinel{{
        {"append-v1", scheduler_sentinel_v4}, {"join-v1", scheduler_sentinel_v4}, {"literal-v1", scheduler_sentinel_v4}}};
    scheduler_callback_calls_v4 = 0;
    const auto result = bruce::mcp_inspector_detail::inspect_dag_scheduler_v4_bundle(scheduler_bundle_v4(), sentinel);
    require(result.status == bruce::McpInspectionStatusV1::valid && result.code == "VALID", "sentinel catalog is inspectable");
    require(scheduler_callback_calls_v4 == 0, "V4 Mcp never invokes registry callbacks");
    auto value = scheduler_run_v4();
    value.receipts[3].output->payload_bytes = "private-recorded-only";
    value.receipts[3].output->payload_digest = bruce::sha256("private-recorded-only");
    const auto json = inspect_scheduler_valid_v4(value);
    contains(json, "\"payload_size\":21,\"payload_digest\":\"" + bruce::sha256("private-recorded-only") + "\"");
    require(json.find("private-recorded-only") == std::string::npos, "recorded-only payload is private");
}
// Catch incomplete enum mapping and accidental output materialization for non-READY records.
void scheduler_outcomes_v4() {
    auto value = scheduler_run_v4();
    const std::array outcomes{bruce::DagSchedulerReceiptOutcomeV2::failed, bruce::DagSchedulerReceiptOutcomeV2::blocked, bruce::DagSchedulerReceiptOutcomeV2::cancelled};
    const std::array events{bruce::DagSchedulerTraceEventV2::failed, bruce::DagSchedulerTraceEventV2::blocked, bruce::DagSchedulerTraceEventV2::cancelled};
    const std::array<std::string_view, 3> names{"FAILED", "BLOCKED", "CANCELLED"};
    for (std::size_t i = 0; i < outcomes.size(); ++i) {
        value.receipts[3].outcome = outcomes[i]; value.receipts[3].output.reset(); value.receipts[3].input_refs.clear(); value.receipts[3].failure_code = "RECORDED_FAILURE";
        value.trace[8].event = events[i];
        const auto json = inspect_scheduler_valid_v4(value);
        contains(json, "\"outcome\":\"" + std::string(names[i]) + "\",\"output\":null,\"failure_code\":\"RECORDED_FAILURE\"");
        contains(json, "\"event\":\"" + std::string(names[i]) + "\",\"node_id\":\"d\"");
    }
}
void scheduler_writer_v4() {
    const auto bytes = scheduler_bundle_v4();
    require(bruce::pack_dag_scheduler_run_v4(bruce::inspect_dag_scheduler_run_v4(bytes, bruce::dag_scheduler_v3_cli_detail::registry)) == bytes, "V4 helper emits canonical inspectable bundle");
    require(bruce::inspect_dag_scheduler_v4_bundle(bytes).code == "VALID", "V4 helper emits Mcp-valid bundle");
}

// Catch route growth and any DOM rendering that interprets bundle text.
void text_safe_asset() {
    const auto& html = bruce::mcp_inspector_html_v1();
    for (const auto fragment : {"BRUCE MCP — Phase-DAG V2 Inspector", "type=\"file\"",
             "Inspect bundle", "Reset", "id=\"status\"", "id=\"overview\"", "id=\"graph\"",
             "id=\"receipts\"", "id=\"trace\"", "id=\"validation\"", "textContent",
             "id=\"version\"", "value=\"v2\"", "value=\"v3\"", "Phase-DAG V3",
             "value=\"v4\"", "BRUCE MCP — DAG-Scheduler V4 Inspector",
             "VALID: canonical bundle validation passed; not re-executed.",
             "fetch(config.route", "method:'POST'", "body:"}) contains(html, fragment);
    for (const auto marker : {"jobs", "imports", "language", "redis", "process", "run", "replay", "path",
             "/execute", "/replay", "execute(", "replay(", "execute_phase_dag", "replay_phase_dag",
             "execute_dag_scheduler", "replay_dag_scheduler",
             "innerHTML", "outerHTML", "insertAdjacentHTML", "DOMParser", "createContextualFragment", "document.write",
             "localStorage", "sessionStorage", "onclick=", "onchange=", "http://", "https://", "src=", "href="})
        require(html.find(marker) == std::string::npos, std::string("forbidden UI marker: ") + marker);
    constexpr std::string_view routes[] = {"/api/v1/inspect", "/api/v3/inspect", "/api/dag-scheduler-v4/inspect"};
    std::size_t route_counts[] = {0, 0, 0};
    std::size_t position = 0;
    while ((position = html.find("/api/", position)) != std::string::npos) {
        const auto index = html.compare(position, routes[0].size(), routes[0]) == 0 ? 0U :
            html.compare(position, routes[1].size(), routes[1]) == 0 ? 1U : 2U;
        const auto route = routes[index];
        require(html.compare(position, route.size(), route) == 0, "only V2, V3 and Scheduler V4 inspect APIs are permitted");
        ++route_counts[index];
        position += route.size();
        require(position < html.size() && (html[position] == '\'' || html[position] == '"'),
                "inspect API has no extra endpoint suffix");
    }
    require(route_counts[0] == 1 && route_counts[1] == 1 && route_counts[2] == 1,
            "exactly the three explicit versioned inspect routes appear");
}
void manifest_contract() {
    const auto& manifest = bruce::mcp_mod_manifest_v1();
    require(manifest.id == "mcp", "Mcp manifest ID");
    require(manifest.version == "mcp-mod-v1", "Mcp manifest version");
    require(manifest.api_version == "bruce-mod-api-v1", "Mcp manifest API");
    require(manifest.capabilities == std::vector{bruce::ModCapabilityV1::presentation}, "Mcp grants presentation only");
    bruce::validate_mod_manifest(manifest);
    const auto& catalog = bruce::builtin_mod_catalog_v1();
    require(catalog.find("mcp") != nullptr && *catalog.find("mcp") == manifest, "builtin catalog discovers the exact Mcp manifest");
    const auto manifests = catalog.manifests();
    for (std::size_t index = 1; index < manifests.size(); ++index)
        require(manifests[index - 1].id < manifests[index].id, "builtin manifests have strict sorted order");
}
// Catch missing replay-derived fields, accidental raw-byte exposure, and wrong enum names.
void valid_model() {
    const auto value = run(spec({node("a", request()), node("b", request(), {"a"})}));
    const auto json = inspect_valid(value);
    require(json.starts_with('{') && json.ends_with('}'), "presentation is a JSON object");
    for (const auto fragment : {"\"status\":\"VALID\"", "\"code\":\"VALID\"", "\"dag_version\":\"phase-dag-v2\"",
            "\"dag_id\":\"mcp-inspector-test\"", "\"nodes\":[", "\"receipts\":[", "\"trace\":[",
            "\"node_id\":\"a\"", "\"node_id\":\"b\"", "\"adapter_id\":\"nanocell-phase-v1\"",
            "\"adapter_version\":\"v1\"", "\"dependencies\":[\"a\"]", "\"input_refs\":[{",
            "\"producer_node_id\":\"a\"", "\"producer_adapter_id\":\"nanocell-phase-v1\"",
            "\"producer_adapter_version\":\"v1\"", "\"producer_status\":\"COMPLETE\"",
            "\"outcome\":\"READY\"", "\"output\":{", "\"output_version\":\"phase-output-v2\"",
            "\"status\":\"COMPLETE\"", "\"claim_scope\":\"nanocell-phase-v1\"", "\"failure_code\":\"\"",
            "\"event\":\"VALIDATED\"", "\"event\":\"STARTED\"", "\"event\":\"READY\"", "\"event\":\"COMPLETED\""}) contains(json, fragment);
    const auto& output = *value.receipts[0].output;
    contains(json, "\"request_digest\":\"" + value.receipts[0].request_digest + "\"");
    contains(json, "\"output_digest\":\"" + output.output_digest + "\"");
    contains(json, "\"evidence_digest\":\"" + output.evidence_digest + "\"");
    contains(json, "\"producer_output_digest\":\"" + output.output_digest + "\"");
    contains(json, "\"producer_evidence_digest\":\"" + output.evidence_digest + "\"");
    for (const auto& secret : {value.spec.nodes[0].request_bytes, output.output_bytes, output.evidence_bytes,
                              std::string("nanocell-reference-context-v1"), std::string("BRUCE_PHASE_DAG_RUN_V2")})
        require(json.find(secret) == std::string::npos, "raw request/output/evidence is hidden");
    require(bruce::mcp_inspection_status_name(bruce::McpInspectionStatusV1::valid) == "VALID", "valid status name");
}
void rejected_model(std::string_view bytes) {
    const auto result = bruce::inspect_phase_dag_v2_bundle(bytes);
    require(result.status == bruce::McpInspectionStatusV1::rejected && result.code == "REJECTED", "bad bundle must be REJECTED");
    require(result.json == "{\"status\":\"REJECTED\",\"code\":\"REJECTED\"}", "rejection must contain only fixed public fields");
    require(bruce::mcp_inspection_status_name(result.status) == "REJECTED", "rejected status name");
}
void mutated_bundle() {
    auto bytes = valid_bundle(); bytes.back() = bytes.back() == 'x' ? 'y' : 'x';
    rejected_model(bytes);
    rejected_model("BRUCE_PHASE_DAG_RUN_V2\n/home/private/raw-path\nsecret-request");
}
void non_v2_bundle() { rejected_model("BRUCE_PHASE_DAG_RUN_V1\n/tmp/private/bundle\nraw-error"); }
// A replayable fixture distinguishes the inspector size guard from decoder rejection.
std::string v2_boundary_bundle(const std::size_t target) {
    std::vector<bruce::PhaseNodeSpecV2> nodes;
    const std::string request_bytes(65536, 'x');
    for (int i = 0; i < 16; ++i)
        nodes.push_back({"node" + std::to_string(10 + i), "nanocell-phase-v1", "v1", {}, request_bytes, bruce::sha256(request_bytes)});
    auto value = spec(std::move(nodes));
    auto bytes = bruce::pack_phase_dag_run_v2(run(value));
    for (int adjustment = 0; bytes.size() != target && adjustment < 3; ++adjustment) {
        auto& last = value.nodes.back();
        const auto new_size = last.request_bytes.size() + target - bytes.size();
        require(new_size <= 65536, "boundary fixture stays inside per-request limit");
        last.request_bytes.resize(new_size, 'x');
        last.request_digest = bruce::sha256(last.request_bytes);
        bytes = bruce::pack_phase_dag_run_v2(run(value));
    }
    require(bytes.size() == target, "canonical V2 fixture reaches exact boundary");
    require(bruce::pack_phase_dag_run_v2(bruce::replay_phase_dag_run_v2(bytes, bruce::phase_dag_nanocell_catalog_v2())) == bytes,
        "V2 boundary fixture is valid in the core");
    return bytes;
}
void oversized_bundle() {
    rejected_model(std::string(1024U * 1024U + 1U, 'x'));
    require(bruce::mcp_max_inspect_bytes_v1 == 1024U * 1024U, "documented 1 MiB boundary");
    for (const std::size_t target : {1024U * 1024U, 1024U * 1024U + 1U}) {
        const auto bytes = v2_boundary_bundle(target);
        const auto result = bruce::inspect_phase_dag_v2_bundle(bytes);
        if (target == 1024U * 1024U) require(result.code == "VALID", "V2 accepts exactly 1 MiB");
        else rejected_model(bytes);
    }
}
void failure_and_blocked_mappings() {
    const auto json = inspect_valid(run(spec({node("a", request("unknown")), node("b", request(), {"a"})})));
    for (const auto fragment : {"\"outcome\":\"FAILED\"", "\"outcome\":\"BLOCKED\"",
             "\"event\":\"FAILED\"", "\"event\":\"BLOCKED\"", "\"output\":null",
             "\"failure_code\":\"UnknownNanoCellPhaseTechnique\""}) contains(json, fragment);
}
void cancelled_mapping() {
    auto value = spec({node("a", request())}); value.max_started_nodes = 0;
    const auto json = inspect_valid(run(value));
    contains(json, "\"outcome\":\"CANCELLED\""); contains(json, "\"event\":\"CANCELLED\"");
    contains(json, "\"failure_code\":\"DagBudgetExhausted\"");
}
void output_status_mappings() {
    auto limited = request("rotate-match-v1"); limited.core_bytes[2] = 0x82; limited.max_operations = 1;
    contains(inspect_valid(run(spec({node("a", limited)}))), "\"status\":\"INCOMPLETE\"");
    contains(inspect_valid(run(spec({node("a", request("context-conflict-v1"))}))), "\"status\":\"REJECTED\"");
}
void canonical_writer_bundle() {
    const auto bytes = valid_bundle();
    require(bruce::pack_phase_dag_run_v2(bruce::replay_phase_dag_run_v2(bytes, bruce::phase_dag_nanocell_catalog_v2())) == bytes,
            "writer fixture directly replays and repacks byte-identically");
    const auto result = bruce::inspect_phase_dag_v2_bundle(bytes);
    require(result.code == "VALID", "writer emits inspectable canonical bundle");
}
}
int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--write-valid-bundle") {
        const auto bytes = valid_bundle();
        require(bruce::pack_phase_dag_run_v2(bruce::replay_phase_dag_run_v2(bytes, bruce::phase_dag_nanocell_catalog_v2())) == bytes,
                "writer canonical replay");
        std::ofstream output(argv[2], std::ios::binary); output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        require(static_cast<bool>(output), "writer explicit path write");
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--write-v2-limit-bundle") {
        const auto bytes = v2_boundary_bundle(1024U * 1024U + 1U);
        std::ofstream output(argv[2], std::ios::binary); output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        require(static_cast<bool>(output), "V2 boundary writer explicit path write");
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--write-valid-v3-bundle") {
        const auto bytes = valid_bundle_v3();
        require(bruce::pack_phase_dag_run_v3(bruce::replay_phase_dag_run_v3(bytes, bruce::phase_dag_nanocell_catalog_v3())) == bytes,
                "V3 writer canonical replay");
        std::ofstream output(argv[2], std::ios::binary); output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        require(static_cast<bool>(output), "V3 writer explicit path write");
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--write-valid-dag-scheduler-v4-bundle") {
        const auto bytes = scheduler_bundle_v4();
        std::ofstream output(argv[2], std::ios::binary); output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        require(static_cast<bool>(output), "V4 writer explicit path write");
        return 0;
    }
    if (argc != 1) return 2;
    unsigned int failed = 0;
    const std::vector<std::pair<std::string_view, void(*)()>> tests{{"text safe asset", text_safe_asset}, {"manifest", manifest_contract}, {"valid model", valid_model},
        {"byte mutation", mutated_bundle}, {"non V2", non_v2_bundle}, {"oversized", oversized_bundle},
        {"failed blocked", failure_and_blocked_mappings}, {"cancelled", cancelled_mapping},
        {"output statuses", output_status_mappings}, {"canonical writer fixture", canonical_writer_bundle},
        {"V3 valid model", valid_model_v3}, {"cross version rejection", cross_version_rejection},
        {"V3 mutation", mutated_bundle_v3}, {"V3 size limits", size_limits_v3},
        {"V3 mappings", mappings_v3}, {"V3 scheduler blockers", scheduler_blocking_predecessors_v3},
        {"V3 scheduler nonblocking", scheduler_nonblocking_outcomes_v3}, {"V3 scheduler metadata", scheduler_metadata_only_v3},
        {"V3 canonical writer", canonical_writer_bundle_v3},
        {"V4 scheduler projection", scheduler_projection_v4}, {"V4 scheduler rejections", scheduler_rejections_v4},
        {"V4 scheduler no reexecution", scheduler_no_reexecution_v4}, {"V4 scheduler outcomes", scheduler_outcomes_v4},
        {"V4 scheduler canonical writer", scheduler_writer_v4}};
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS: " << name << '\n'; }
        catch (const std::exception& error) { ++failed; std::cerr << "FAIL: " << name << ": " << error.what() << '\n'; }
    }
    std::cout << tests.size() - failed << '/' << tests.size() << " contracts passed\n";
    return failed ? 1 : 0;
}
