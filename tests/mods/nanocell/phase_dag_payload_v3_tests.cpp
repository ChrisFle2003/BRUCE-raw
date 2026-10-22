#include "bruce/phase_dag/nanocell_adapter_v3.hpp"
#include "bruce/phase_dag/payload_gate_adapter_v3.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace bruce;
void require(bool value, std::string_view message) {
    if (!value) throw std::runtime_error(std::string(message));
}
template<class Action> void rejected(Action action, std::string_view message) {
    try { action(); } catch (const Error&) { return; }
    throw std::runtime_error(std::string(message));
}
std::string field(std::string_view value) { return std::to_string(value.size()) + ":" + std::string(value); }
NanoCellPhaseRequestV1 request(std::string technique = "masked-match-v1", std::uint8_t active = 0xA5) {
    NanoCellPhaseRequestV1 value;
    value.core_layout_version = "nanoneuron-core-v1";
    value.core_bytes = {0x10, 0x20, active, 0x40, 0x50, 0x60, 0x70, 0x80};
    value.active_index = 2;
    value.context_id = "reference.context_1";
    value.context_bytes = "nanocell-reference-context-v1";
    value.context_digest = sha256(value.context_bytes);
    value.technique_id = std::move(technique);
    value.max_operations = 16;
    value.max_trace_entries = 18;
    return value;
}
PhaseNodeSpecV3 node(std::string id, const NanoCellPhaseRequestV1& value) {
    const auto bytes = pack_nanocell_phase_request_v1(value);
    return {std::move(id), "nanocell-phase-v3", "v1", {}, bytes, sha256(bytes)};
}
PhaseNodeSpecV3 gate(std::string id, std::vector<std::string> dependencies = {}) {
    const auto bytes = pack_payload_gate_request_v1();
    return {std::move(id), "payload-gate-v1", "v1", std::move(dependencies), bytes, sha256(bytes)};
}
PhaseDagSpecV3 spec(std::vector<PhaseNodeSpecV3> nodes) {
    PhaseDagSpecV3 value;
    value.dag_id = "payload-adapter-test";
    value.nodes = std::move(nodes);
    value.max_started_nodes = value.nodes.size();
    value.max_receipt_bytes = 65536;
    value.max_trace_entries = 32;
    return value;
}
const PhaseOutputRefV3& ready(const PhaseReceiptV3& receipt, PhaseOutputStatusV3 status) {
    require(receipt.outcome == PhaseReceiptOutcomeV3::ready && receipt.output.has_value(), "expected READY output");
    require(receipt.output->status == status, "expected output status");
    require(receipt.output->payload_digest == sha256(receipt.output->payload_bytes), "payload digest mismatch");
    require(receipt.output->output_digest == sha256(receipt.output->output_bytes), "output digest mismatch");
    require(receipt.output->evidence_digest == sha256(receipt.output->evidence_bytes), "evidence digest mismatch");
    return *receipt.output;
}
void replay(const PhaseDagRunV3& run, std::span<const PhaseAdapterV3> catalog) {
    const auto bytes = pack_phase_dag_run_v3(run);
    require(pack_phase_dag_run_v3(replay_phase_dag_run_v3(bytes, catalog)) == bytes, "V3 replay differs");
}
void nanocell_payloads() {
    struct Case { NanoCellPhaseRequestV1 request; std::string status; PhaseOutputStatusV3 phase; std::optional<std::uint8_t> state; };
    auto limited = request("rotate-match-v1", 0x82); limited.max_operations = 1;
    auto trace_limited = request("rotate-match-v1", 0x82); trace_limited.max_trace_entries = 2;
    const std::array cases{
        Case{request(), "MATCHED", PhaseOutputStatusV3::complete, 0xA0},
        Case{request("masked-match-v1", 0xB5), "NO_MATCH", PhaseOutputStatusV3::complete, 0xB0},
        Case{request("rotate-match-v1", 0x82), "MATCHED", PhaseOutputStatusV3::complete, 0x05},
        Case{limited, "INCOMPLETE", PhaseOutputStatusV3::incomplete, 0x05},
        Case{trace_limited, "INCOMPLETE", PhaseOutputStatusV3::incomplete, 0x82},
        Case{request("context-conflict-v1"), "CONTEXT_REJECTED", PhaseOutputStatusV3::rejected, std::nullopt}
    };
    const auto catalog = phase_dag_nanocell_catalog_v3();
    require(catalog.size() == 2 && catalog[0].adapter_id == "nanocell-phase-v3" && catalog[0].adapter_version == "v1" &&
        catalog[1].adapter_id == "payload-gate-v1" && catalog[1].adapter_version == "v1", "fixed sorted V3 catalog");
    for (const auto& value : cases) {
        const auto run = execute_phase_dag_v3(spec({node("a", value.request), gate("gate", {"a"})}), catalog);
        const auto& output = ready(run.receipts[0], value.phase);
        const auto payload = unpack_nanocell_phase_payload_v1(output.payload_bytes);
        require(payload.payload_version == "nanocell-phase-payload-v1" && payload.nanocell_status == value.status &&
            payload.final_state == value.state, "NanoCell status/final-state payload");
        auto literal = field("nanocell-phase-payload-v1") + field(value.status) + field(value.state ? "1" : "0");
        if (value.state) literal += field(std::string(1, static_cast<char>(*value.state)));
        require(output.payload_bytes == literal && pack_nanocell_phase_payload_v1(payload) == literal, "canonical NanoCell payload");
        require(output.evidence_bytes.find(value.request.context_bytes) == std::string::npos &&
            output.evidence_bytes.find(output.payload_bytes) == std::string::npos, "NanoCell evidence leaks raw bytes");
        ready(run.receipts[1], value.phase == PhaseOutputStatusV3::complete ? PhaseOutputStatusV3::complete : PhaseOutputStatusV3::rejected);
        replay(run, catalog);
    }
}
PhaseInputV3 input(std::string id = "a", std::string status = "COMPLETE", std::string payload = "secret raw payload") {
    return {std::move(id), "producer-v1", "v1", std::move(status), sha256("output"), sha256("evidence"), payload, sha256(payload)};
}
void gate_direct_inputs() {
    const auto catalog = phase_dag_nanocell_catalog_v3();
    const auto bytes = pack_payload_gate_request_v1();
    require(bytes == field("payload-gate-request-v1"), "parameterless request form");
    unpack_payload_gate_request_v1(bytes);
    for (const auto& bad : {std::string{}, bytes + field("injected"), std::string("023:payload-gate-request-v1")})
        rejected([&] { unpack_payload_gate_request_v1(bad); }, "noncanonical gate request accepted");
    auto valid = input();
    const auto check = [&](const std::vector<PhaseInputV3>& inputs, PhaseOutputStatusV3 expected) {
        const auto output = catalog[1].execute(bytes, inputs);
        require(output.status == expected, "gate decision");
        require(output.payload_digest == sha256(output.payload_bytes), "manifest digest");
        const auto metadata = unpack_payload_gate_metadata_v1(output.output_bytes);
        const auto manifest = unpack_payload_gate_payload_v1(output.payload_bytes);
        const auto decision = expected == PhaseOutputStatusV3::complete ? "COMPLETE" : "REJECTED";
        require(metadata.decision == decision && metadata.input_count == inputs.size(), "gate metadata");
        require(manifest.decision == decision && manifest.input_count == inputs.size() &&
            manifest.input_payload_digests.size() == inputs.size(), "gate manifest");
        require(pack_payload_gate_metadata_v1(metadata) == output.output_bytes &&
            pack_payload_gate_payload_v1(manifest) == output.payload_bytes, "gate codec roundtrip");
        for (const auto& value : inputs) if (!value.producer_payload_bytes.empty())
            require(output.evidence_bytes.find(value.producer_payload_bytes) == std::string::npos, "gate evidence leaks raw payload");
        return output;
    };
    check({valid}, PhaseOutputStatusV3::complete);
    check({}, PhaseOutputStatusV3::rejected);
    check({input("a", "COMPLETE", "")}, PhaseOutputStatusV3::rejected);
    check({input("a", "INCOMPLETE")}, PhaseOutputStatusV3::rejected);
    check({input("a", "REJECTED")}, PhaseOutputStatusV3::rejected);
    auto inconsistent = valid; inconsistent.producer_payload_digest = sha256("wrong");
    check({inconsistent}, PhaseOutputStatusV3::rejected);
    check({valid, input("b", "REJECTED")}, PhaseOutputStatusV3::rejected);
    const auto ordered = check({input("z", "COMPLETE", "second"), valid}, PhaseOutputStatusV3::complete);
    const auto manifest = unpack_payload_gate_payload_v1(ordered.payload_bytes);
    require(manifest.input_payload_digests == std::vector<std::string>{valid.producer_payload_digest, sha256("second")},
        "manifest producer order");
    const auto literal = field("payload-gate-payload-v1") + field("COMPLETE") + field("2") +
        field(valid.producer_payload_digest) + field(sha256("second"));
    require(ordered.payload_bytes == literal && ordered.payload_digest == sha256(literal), "manifest exact digest binding");
    require(ordered.evidence_bytes.find(field("a")) < ordered.evidence_bytes.find(field("z")), "evidence producer order");
    for (int mutation = 0; mutation < 7; ++mutation) {
        auto bad = valid;
        if (mutation == 0) bad.producer_node_id.clear();
        if (mutation == 1) bad.producer_adapter_id = "bad id";
        if (mutation == 2) bad.producer_adapter_version.clear();
        if (mutation == 3) bad.producer_status = "UNKNOWN";
        if (mutation == 4) bad.producer_output_digest = "not-a-digest";
        if (mutation == 5) bad.producer_evidence_digest[0] = 'G';
        if (mutation == 6) bad.producer_payload_digest.clear();
        const std::array inputs{bad};
        rejected([&] { (void)catalog[1].execute(bytes, inputs); }, "malformed input accepted");
    }
    const std::array duplicated{valid, valid};
    rejected([&] { (void)catalog[1].execute(bytes, duplicated); }, "duplicate producer accepted");
}
PhaseOutputRefV3 empty_producer(std::string_view, std::span<const PhaseInputV3>) {
    PhaseOutputRefV3 output;
    output.status = PhaseOutputStatusV3::complete;
    output.output_bytes = "empty-producer"; output.output_digest = sha256(output.output_bytes);
    output.evidence_bytes = "empty-evidence"; output.evidence_digest = sha256(output.evidence_bytes);
    output.payload_digest = sha256(""); output.claim_scope = "empty-producer-v1";
    return output;
}
PhaseOutputRefV3 malformed_input_gate(std::string_view bytes, std::span<const PhaseInputV3> inputs) {
    auto corrupted = std::vector<PhaseInputV3>(inputs.begin(), inputs.end());
    corrupted.at(0).producer_status = "UNKNOWN";
    return phase_dag_nanocell_catalog_v3()[1].execute(bytes, corrupted);
}
void scheduler_and_chain() {
    const auto catalog = phase_dag_nanocell_catalog_v3();
    const auto run = execute_phase_dag_v3(spec({node("a", request()), gate("gate1", {"a"}), gate("gate2", {"gate1"})}), catalog);
    const auto& first = ready(run.receipts[1], PhaseOutputStatusV3::complete);
    const auto& second = ready(run.receipts[2], PhaseOutputStatusV3::complete);
    require(run.receipts[2].input_refs.size() == 1 && run.receipts[2].input_refs[0].producer_node_id == "gate1" &&
        run.receipts[2].input_refs[0].producer_payload_bytes == first.payload_bytes, "gate2 consumes gate1 manifest only");
    const auto manifest = unpack_payload_gate_payload_v1(second.payload_bytes);
    require(manifest.input_payload_digests == std::vector<std::string>{first.payload_digest}, "second manifest binds first payload");
    replay(run, catalog);
    auto producer = node("a", request()); producer.adapter_id = "empty-producer-v1";
    const std::array custom{PhaseAdapterV3{"empty-producer-v1", "v1", empty_producer}, catalog[1]};
    const auto empty = execute_phase_dag_v3(spec({producer, gate("gate", {"a"})}), custom);
    ready(empty.receipts[1], PhaseOutputStatusV3::rejected);
    replay(empty, custom);
    const std::array corrupted{catalog[0], PhaseAdapterV3{"payload-gate-v1", "v1", malformed_input_gate}};
    const auto failed = execute_phase_dag_v3(spec({node("a", request()), gate("gate", {"a"})}), corrupted);
    require(failed.receipts[1].outcome == PhaseReceiptOutcomeV3::failed && !failed.receipts[1].output, "malformed input causes FAILED");
}
void invalid_payload_codecs() {
    const auto output = phase_dag_nanocell_catalog_v3()[0].execute(pack_nanocell_phase_request_v1(request()), {});
    rejected([&] { (void)unpack_nanocell_phase_payload_v1(output.payload_bytes + field("extra")); }, "NanoCell trailing field accepted");
    rejected([&] { (void)unpack_nanocell_phase_payload_v1(field("nanocell-phase-payload-v1") + field("MATCHED") + field("0")); }, "missing final state accepted");
    rejected([&] { (void)unpack_nanocell_phase_payload_v1(field("nanocell-phase-payload-v1") + field("CONTEXT_REJECTED") + field("1") + field("x")); }, "context rejected final state accepted");
    rejected([&] { (void)unpack_nanocell_phase_payload_v1(field("nanocell-phase-payload-v1") + field("MATCHED") + field("1") + field("xx")); }, "wide final state accepted");
    const auto bad = field("payload-gate-payload-v1") + field("COMPLETE") + field("0");
    rejected([&] { (void)unpack_payload_gate_payload_v1(bad); }, "COMPLETE empty manifest accepted");
    rejected([&] { (void)unpack_payload_gate_payload_v1(field("payload-gate-payload-v1") + field("REJECTED") + field("1") + field("bad")); }, "invalid manifest digest accepted");
}
}
int main() {
    try {
        nanocell_payloads(); gate_direct_inputs(); scheduler_and_chain(); invalid_payload_codecs();
        std::cout << "phase DAG V3 payload adapter contracts passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
