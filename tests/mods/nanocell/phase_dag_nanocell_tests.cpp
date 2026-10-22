#include "bruce/phase_dag/nanocell_adapter_v2.hpp"
#include "bruce/phase_dag/evidence_gate_adapter_v2.hpp"
#include "bruce/mods/nanocell/nanocell.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include "../../../apps/phase_dag_v2_cli.hpp"

#include <array>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

// Faults affect only this test executable; all other file operations stay real.
std::string blocked_cleanup_target;
bool fail_directory_sync = false;
bool directory_sync_failure_observed = false;
extern "C" int unlink(const char* path) {
    if (!blocked_cleanup_target.empty() &&
        std::string_view(path).starts_with(blocked_cleanup_target + ".tmp.") &&
        std::filesystem::exists(blocked_cleanup_target)) {
        errno = EACCES;
        return -1;
    }
    return ::unlinkat(AT_FDCWD, path, 0);
}
extern "C" int fsync(int fd) {
    struct stat status{};
    if (fail_directory_sync && ::fstat(fd, &status) == 0 && S_ISDIR(status.st_mode)) {
        directory_sync_failure_observed = true;
        errno = EIO;
        return -1;
    }
    return static_cast<int>(::syscall(SYS_fsync, fd));
}

namespace {

void require(bool value, std::string_view message) {
    if (!value) throw std::runtime_error(std::string(message));
}

template<class Action> void rejected(Action action, std::string_view message) {
    try { action(); } catch (const bruce::Error&) { return; }
    throw std::runtime_error(std::string(message));
}

bruce::NanoCellPhaseRequestV1 request(std::string technique = "masked-match-v1", std::uint8_t active = 0xA5) {
    bruce::NanoCellPhaseRequestV1 value;
    value.core_layout_version = "nanoneuron-core-v1";
    value.core_bytes = {0x10, 0x20, active, 0x40, 0x50, 0x60, 0x70, 0x80};
    value.active_index = 2;
    value.context_id = "reference.context_1";
    value.context_bytes = "nanocell-reference-context-v1";
    value.context_digest = bruce::sha256(value.context_bytes);
    value.technique_id = std::move(technique);
    value.max_operations = 16;
    value.max_trace_entries = 18;
    return value;
}

bruce::PhaseNodeSpecV2 node(std::string id, std::string bytes, std::vector<std::string> dependencies = {}) {
    return {std::move(id), "nanocell-phase-v1", "v1", std::move(dependencies), bytes, bruce::sha256(bytes)};
}

bruce::PhaseNodeSpecV2 evidence_gate_node(std::string id, std::vector<std::string> dependencies = {}) {
    const auto bytes = bruce::pack_evidence_gate_request_v1();
    return {std::move(id), "evidence-gate-v1", "v1", std::move(dependencies), bytes, bruce::sha256(bytes)};
}

bruce::PhaseDagSpecV2 spec(std::vector<bruce::PhaseNodeSpecV2> nodes) {
    bruce::PhaseDagSpecV2 value;
    value.dag_id = "nanocell-adapter-test";
    value.nodes = std::move(nodes);
    value.max_started_nodes = value.nodes.size();
    value.max_receipt_bytes = 65536;
    value.max_trace_entries = 32;
    return value;
}

std::string rehash(std::string bundle) {
    constexpr std::string_view header = "BRUCE_PHASE_DAG_RUN_V2\n";
    require(bundle.starts_with(header), "test bundle header");
    bundle.replace(header.size(), 64, bruce::sha256(bundle.substr(header.size() + 65)));
    return bundle;
}

std::string mutate(std::string bundle, std::string_view before, std::string_view after) {
    require(before.size() == after.size(), "same-sized test mutation");
    const auto offset = bundle.find(before, std::string_view("BRUCE_PHASE_DAG_RUN_V2\n").size() + 65);
    require(offset != std::string::npos, "test mutation target present");
    bundle.replace(offset, before.size(), after);
    return rehash(std::move(bundle));
}

void terminal_case(bruce::NanoCellPhaseRequestV1 value, std::string_view status,
    bruce::PhaseOutputStatusV2 output_status, std::uint64_t operations, std::uint64_t trace) {
    using namespace bruce;
    const auto bytes = pack_nanocell_phase_request_v1(value);
    require(unpack_nanocell_phase_request_v1(bytes).technique_id == value.technique_id, "request codec roundtrip");
    const auto run = execute_phase_dag_v2(spec({node("a", bytes)}), phase_dag_nanocell_catalog_v2());
    require(run.receipts.size() == 1 && run.receipts[0].outcome == PhaseReceiptOutcomeV2::ready,
        "terminal NanoCell run is READY");
    require(run.receipts[0].output.has_value(), "READY has output");
    const auto& output = *run.receipts[0].output;
    require(output.status == output_status && output.claim_scope == "nanocell-phase-v1", "V2 status/scope");
    require(output.output_digest == sha256(output.output_bytes), "metadata digest");
    require(output.evidence_digest == sha256(output.evidence_bytes), "evidence digest");
    const auto metadata = unpack_nanocell_phase_metadata_v1(output.output_bytes);
    require(pack_nanocell_phase_metadata_v1(metadata) == output.output_bytes, "metadata canonical roundtrip");
    require(metadata.metadata_version == "nanocell-phase-output-v1" && metadata.nanocell_status == status,
        "NanoCell status metadata");
    require(metadata.technique_id == value.technique_id && metadata.technique_version == "nanocell-technique-v1",
        "technique metadata");
    require(metadata.core_layout_version == value.core_layout_version && metadata.active_index == value.active_index,
        "core metadata");
    require(metadata.context_id == value.context_id && metadata.context_digest == value.context_digest,
        "context metadata");
    require(metadata.core_digest.size() == 64 && metadata.technique_digest.size() == 64,
        "digest metadata");
    require(metadata.operations_used == operations && metadata.trace_entries == trace, "budget metadata");
    const auto replayed_mod = replay_nanocell_mod_run_v1(output.evidence_bytes);
    require(pack_nanocell_mod_run_v1(replayed_mod) == output.evidence_bytes, "exact Mod envelope evidence");
    const auto& answer = replayed_mod.nanocell.answer;
    require(metadata.core_digest == answer.core_digest && metadata.context_digest == answer.context_digest &&
            metadata.technique_digest == answer.technique_digest, "metadata digests bind to Mod evidence");
    require(metadata.technique_id == answer.technique_id && metadata.technique_version == answer.technique_version &&
            metadata.context_id == answer.context_id && metadata.active_index == answer.active_index,
        "metadata IDs bind to Mod evidence");
    if (value.core_bytes[2] == 0xA5 && value.technique_id == "masked-match-v1") {
        require(metadata.core_digest == "df6c4838075217518af6bd509dba5f4dfef027f3fcf767557f47c9111062f024",
            "fixed core digest vector");
        require(metadata.technique_digest == "8520761a8f085f128fd6d484133f3b9ef5325d9d7af3186bd190e1061b7c12bb",
            "fixed technique digest vector");
    }
    require(replayed_mod.nanocell.answer.operations_used == operations &&
            replayed_mod.nanocell.answer.trace.size() == trace, "evidence accounting");
    const auto bundle = pack_phase_dag_run_v2(run);
    require(pack_phase_dag_run_v2(replay_phase_dag_run_v2(bundle, phase_dag_nanocell_catalog_v2())) == bundle,
        "V2 pack/replay terminal state");
    rejected([&] { (void)replay_phase_dag_run_v2(mutate(bundle, "nanocell-phase-request-v1",
        "nanocell-phase-request-v2"), phase_dag_nanocell_catalog_v2()); }, "rehashed request mutation accepted");
    rejected([&] { (void)replay_phase_dag_run_v2(mutate(bundle, "nanocell-phase-output-v1",
        "nanocell-phase-output-v2"), phase_dag_nanocell_catalog_v2()); }, "rehashed metadata mutation accepted");
    rejected([&] { (void)replay_phase_dag_run_v2(mutate(bundle, "BRUCE_MOD_RUN_V1",
        "BRUCE_MOD_RUN_V2"), phase_dag_nanocell_catalog_v2()); }, "rehashed Mod envelope mutation accepted");
}

void terminal_states() {
    using namespace bruce;
    terminal_case(request(), "MATCHED", PhaseOutputStatusV2::complete, 1, 3);
    terminal_case(request("masked-match-v1", 0xB5), "NO_MATCH", PhaseOutputStatusV2::complete, 1, 3);
    auto operation_limited = request("rotate-match-v1", 0x82);
    operation_limited.max_operations = 1;
    terminal_case(operation_limited, "INCOMPLETE", PhaseOutputStatusV2::incomplete, 1, 3);
    auto trace_limited = request("rotate-match-v1", 0x82);
    trace_limited.max_trace_entries = 2;
    terminal_case(trace_limited, "INCOMPLETE", PhaseOutputStatusV2::incomplete, 0, 2);
    terminal_case(request("context-conflict-v1"), "CONTEXT_REJECTED", PhaseOutputStatusV2::rejected, 0, 2);
}

void invalid_requests() {
    using namespace bruce;
    const auto catalog = phase_dag_nanocell_catalog_v2();
    require(catalog.size() == 2 && catalog[0].adapter_id == "evidence-gate-v1" &&
        catalog[0].adapter_version == "v1" && catalog[1].adapter_id == "nanocell-phase-v1" &&
        catalog[1].adapter_version == "v1", "static phase adapter catalog");
    const auto valid = pack_nanocell_phase_request_v1(request());
    rejected([&] { (void)unpack_nanocell_phase_request_v1("01:" + valid); }, "leading zero length accepted");
    rejected([&] { (void)unpack_nanocell_phase_request_v1("x:" + valid); }, "malformed length accepted");
    rejected([&] { (void)unpack_nanocell_phase_request_v1(valid + "1:x"); }, "program injection accepted");
    for (const std::string& context_id : {std::string{}, std::string{"invalid context"}, std::string(65, 'a')}) {
        auto malformed = request();
        malformed.context_id = context_id;
        const auto bytes = pack_nanocell_phase_request_v1(malformed);
        rejected([&] { (void)unpack_nanocell_phase_request_v1(bytes); },
            "invalid context ID reached past the phase request decoder");
        const auto run = execute_phase_dag_v2(spec({node("a", bytes)}), catalog);
        require(run.receipts.size() == 1 && run.receipts[0].outcome == PhaseReceiptOutcomeV2::failed &&
            !run.receipts[0].output, "invalid context ID did not yield FAILED without output");
    }
    auto value = request();
    value.active_index = 8;
    rejected([&] { (void)catalog[0].execute(pack_nanocell_phase_request_v1(value), {}); }, "invalid index accepted");
    value = request(); value.context_digest[0] = '0';
    rejected([&] { (void)catalog[0].execute(pack_nanocell_phase_request_v1(value), {}); }, "altered context digest accepted");
    value = request("unrecognised-v1");
    rejected([&] { (void)catalog[0].execute(pack_nanocell_phase_request_v1(value), {}); }, "unknown technique accepted");
    value = request(); value.context_bytes.assign(4097, 'x'); value.context_digest = sha256(value.context_bytes);
    rejected([&] { (void)catalog[0].execute(pack_nanocell_phase_request_v1(value), {}); }, "large context accepted");
    value = request(); value.max_operations = 17;
    rejected([&] { (void)catalog[0].execute(pack_nanocell_phase_request_v1(value), {}); }, "large operation budget accepted");
    value = request(); value.max_trace_entries = 19;
    rejected([&] { (void)catalog[0].execute(pack_nanocell_phase_request_v1(value), {}); }, "large trace budget accepted");
    value.max_trace_entries = 1;
    rejected([&] { (void)catalog[0].execute(pack_nanocell_phase_request_v1(value), {}); }, "small trace budget accepted");
    auto malformed_core = valid;
    const auto core_field = malformed_core.find("8:");
    require(core_field != std::string::npos, "test core field present");
    malformed_core.replace(core_field, 2, "7:");
    rejected([&] { (void)unpack_nanocell_phase_request_v1(malformed_core); }, "non-eight-byte core accepted");
    auto leading_zero_number = valid;
    const auto index = leading_zero_number.find("1:2");
    require(index != std::string::npos, "test index field present");
    leading_zero_number.replace(index, 3, "2:02");
    rejected([&] { (void)unpack_nanocell_phase_request_v1(leading_zero_number); }, "leading-zero index accepted");
    leading_zero_number = valid;
    const auto operation_field = leading_zero_number.find("2:16");
    require(operation_field != std::string::npos, "test operation field present");
    leading_zero_number.replace(operation_field, 4, "3:016");
    rejected([&] { (void)unpack_nanocell_phase_request_v1(leading_zero_number); }, "leading-zero operation budget accepted");
    leading_zero_number = valid;
    const auto trace_field = leading_zero_number.find("2:18");
    require(trace_field != std::string::npos, "test trace field present");
    leading_zero_number.replace(trace_field, 4, "3:018");
    rejected([&] { (void)unpack_nanocell_phase_request_v1(leading_zero_number); }, "leading-zero trace budget accepted");
    auto changed_digest = node("a", valid);
    changed_digest.request_digest[0] = changed_digest.request_digest[0] == '0' ? '1' : '0';
    rejected([&] { (void)execute_phase_dag_v2(spec({changed_digest}), catalog); }, "changed spec digest accepted");
}

void evidence_gate_states() {
    using namespace bruce;
    const auto complete_bytes = pack_nanocell_phase_request_v1(request());
    const auto accepted = execute_phase_dag_v2(spec({node("a", complete_bytes),
        evidence_gate_node("gate", {"a"})}), phase_dag_nanocell_catalog_v2());
    require(accepted.receipts.size() == 2 && accepted.receipts[1].outcome == PhaseReceiptOutcomeV2::ready &&
        accepted.receipts[1].output && accepted.receipts[1].output->status == PhaseOutputStatusV2::complete,
        "gate accepts complete predecessor");
    const auto accepted_metadata = unpack_evidence_gate_metadata_v1(accepted.receipts[1].output->output_bytes);
    require(accepted_metadata.decision == "COMPLETE" && accepted_metadata.input_count == 1 &&
        accepted_metadata.complete_count == 1 && accepted_metadata.incomplete_count == 0 &&
        accepted_metadata.rejected_count == 0, "gate complete metadata");
    require(accepted.receipts[1].output->evidence_bytes.find("evidence-gate-evidence-v1") != std::string::npos,
        "gate emits canonical input evidence");
    const auto rejected_bytes = pack_nanocell_phase_request_v1(request("context-conflict-v1"));
    const auto rejected_run = execute_phase_dag_v2(spec({node("a", rejected_bytes),
        evidence_gate_node("gate", {"a"})}), phase_dag_nanocell_catalog_v2());
    require(rejected_run.receipts[1].outcome == PhaseReceiptOutcomeV2::ready && rejected_run.receipts[1].output &&
        rejected_run.receipts[1].output->status == PhaseOutputStatusV2::rejected,
        "gate rejects rejected predecessor without failing the DAG");
    const auto rejected_metadata = unpack_evidence_gate_metadata_v1(rejected_run.receipts[1].output->output_bytes);
    require(rejected_metadata.decision == "REJECTED" && rejected_metadata.rejected_count == 1,
        "gate rejected metadata");
    const auto empty = execute_phase_dag_v2(spec({evidence_gate_node("gate")}), phase_dag_nanocell_catalog_v2());
    require(empty.receipts[0].outcome == PhaseReceiptOutcomeV2::ready && empty.receipts[0].output &&
        empty.receipts[0].output->status == PhaseOutputStatusV2::rejected,
        "gate rejects missing evidence");
    const auto bundle = pack_phase_dag_run_v2(rejected_run);
    require(pack_phase_dag_run_v2(replay_phase_dag_run_v2(bundle, phase_dag_nanocell_catalog_v2())) == bundle,
        "gate bundle replays canonically");
}

void failure_blocks_only_dependents() {
    using namespace bruce;
    auto bad = request(); bad.technique_id = "unknown";
    const auto good_bytes = pack_nanocell_phase_request_v1(request());
    const auto run = execute_phase_dag_v2(spec({node("a", pack_nanocell_phase_request_v1(bad)),
        node("b", good_bytes, {"a"}), node("c", good_bytes)}), phase_dag_nanocell_catalog_v2());
    require(run.receipts.size() == 3, "three receipts");
    require(run.receipts[0].outcome == PhaseReceiptOutcomeV2::failed && !run.receipts[0].output,
        "invalid adapter request FAILED");
    require(run.receipts[1].outcome == PhaseReceiptOutcomeV2::blocked && !run.receipts[1].output,
        "dependent BLOCKED");
    require(run.receipts[2].outcome == PhaseReceiptOutcomeV2::ready && run.receipts[2].output,
        "independent READY");
}

void cli_publication_contracts() {
    using namespace bruce;
    struct TemporaryRoot {
        std::filesystem::path path;
        TemporaryRoot() {
            char directory[] = "/tmp/bruce-phase-cli-publication-XXXXXX";
            require(::mkdtemp(directory) != nullptr, "publication test directory");
            path = directory;
        }
        ~TemporaryRoot() {
            blocked_cleanup_target.clear();
            fail_directory_sync = false;
            std::filesystem::remove_all(path);
        }
    } root;
    std::ofstream(root.path / "context.bin", std::ios::binary) << "nanocell-reference-context-v1";
    std::ofstream(root.path / "graph.spec") <<
        "BRUCE_PHASE_DAG_SPEC_V1\ndag-id=publication-test\nmax-started-nodes=1\n"
        "max-receipt-bytes=65536\nmax-trace-entries=32\n\n"
        "node=a\ndepends=\ncore=1020a54050607080\nactive-index=2\n"
        "context-id=reference.context_1\ncontext-file=context.bin\n"
        "technique=masked-match-v1\nmax-operations=16\nmax-trace-entries=18\n";
    std::ostringstream report;
    const auto execute = [&](const std::filesystem::path& output) {
        std::array<std::string, 6> args{"bruce", "phase-dag-v2", "execute",
            (root.path / "graph.spec").string(), "--output", output.string()};
        std::array<char*, 6> argv{};
        for (std::size_t i = 0; i < args.size(); ++i) argv[i] = args[i].data();
        struct Capture {
            std::streambuf* previous;
            explicit Capture(std::ostream& output): previous(std::cout.rdbuf(output.rdbuf())) {}
            ~Capture() { std::cout.rdbuf(previous); }
        } capture(report);
        return phase_dag_v2_cli(static_cast<int>(argv.size()), argv.data());
    };
    const auto verify_bundle = [&](const std::filesystem::path& output) {
        const auto bytes = LocalFiles::read(output, 5 * 1024 * 1024);
        const auto replay = replay_phase_dag_run_v2(bytes, phase_dag_nanocell_catalog_v2());
        require(replay.spec.dag_id == "publication-test" && replay.receipts.size() == 1 &&
            replay.receipts[0].outcome == PhaseReceiptOutcomeV2::ready, "published CLI bundle replays");
        for (const auto& item : std::filesystem::directory_iterator(root.path))
            require(item.path().filename().string().find(".tmp.") == std::string::npos,
                "CLI publication leaves no temporary hard link");
        return bytes;
    };

    // Reverting execute to publish_new must fail with ExportTemporaryCleanupFailed.
    const auto output = root.path / "run.bundle";
    blocked_cleanup_target = output.string();
    require(execute(output) == 0, "CLI succeeds without post-publication unlink");
    blocked_cleanup_target.clear();
    const auto bytes = verify_bundle(output);
    require(report.str().starts_with("{\"contract\":\"phase-dag-v2-cli-v1\",\"action\":\"execute\"") &&
        report.str().ends_with("}\n") && report.str().find(sha256(bytes)) != std::string::npos,
        "success report retains contract and full bundle digest");

    report.str("");
    const auto uncertain = root.path / "uncertain.bundle";
    fail_directory_sync = true;
    std::string error;
    try { (void)execute(uncertain); } catch (const Error& failure) { error = failure.what(); }
    fail_directory_sync = false;
    require(directory_sync_failure_observed && error == "ExportCommitDurabilityUnknown",
        "directory sync uncertainty uses the established error");
    require(report.str().empty(), "uncertain publication has no success report");
    require(verify_bundle(uncertain) == bytes, "uncertain publication preserves the complete bundle");

    error.clear();
    try { (void)execute(uncertain); } catch (const Error& failure) { error = failure.what(); }
    require(error.starts_with("ExportExistsOrUnavailable"), "retry cannot replace a visible bundle");
    require(report.str().empty() && verify_bundle(uncertain) == bytes,
        "collision preserves bundle and emits no success report");
}

} // namespace

int main() {
    try {
        terminal_states();
        invalid_requests();
        evidence_gate_states();
        failure_blocks_only_dependents();
        cli_publication_contracts();
        std::cout << "phase DAG NanoCell adapter contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
