#include "bruce/mod/logic_mod_result_inspector_v1.hpp"
#include "bruce/mod/logic_mod_worker_v1.hpp"
#include "bruce/evidence/bundle.hpp"
#include "../../apps/logic_mod_result_inspector_v1_cli.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cerrno>
#include <csignal>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
void check(bool value, const char* message) { if (!value) throw bruce::Error(message); }
void write(const std::filesystem::path& path, std::string_view bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc); out << bytes; out.close();
    check(bool(out), "fixture write failed");
}
struct Fixture {
    std::filesystem::path dir, root;
    bruce::LogicModRunV1 run;
    bruce::artifact_v1::ArtifactManifestV1 manifest;
    Fixture() {
        char pattern[] = "/tmp/bruce-result-inspect-XXXXXX";
        check(::mkdtemp(pattern), "mkdtemp failed"); dir = pattern; root = dir / "store";
        auto store = bruce::artifact_v1::ArtifactStoreV1::open(root);
        const auto object = store.put_bytes("9/16");
        manifest = {"result", "result", "result", "READY", "ACCEPTED",
            {{object, "logic-mod-result-text-v1", "logic-mod-result-v1"}}};
        store.publish_manifest("result", manifest);
        run.accepted = true; run.status = "ACCEPTED"; run.result = "9/16";
        run.binding_identity = "opaque-binding"; run.response_frame = "private-response-frame";
        run.normalized_input = "private-input"; run.output_present = true;
        run.output_manifest_id = "result"; run.output_sha256 = object.sha256;
        run.output_size_bytes = 4; run.output_codec = "logic-mod-result-text-v1";
    }
    ~Fixture() { std::filesystem::remove_all(dir); }
    auto inspect() { return bruce::inspect_logic_mod_result_v1(bruce::logic_mod_run_record_v1(run), root); }
    auto manifest_path() { return root / "manifests/result.manifest"; }
    auto object_path() { return root / "objects" / run.output_sha256.substr(0, 2) / (run.output_sha256 + ".blob"); }
    void manifest_write() { write(manifest_path(), bruce::artifact_v1::encode_manifest_v1(manifest)); }
};

// Catch bypassed Store verification, exposed transcript fields, and renderer drift.
void ready_contract() {
    Fixture f;
    const auto result = f.inspect();
    check(result.status == "READY" && result.accepted && !result.input_present && result.output_present,
        "READY inspection metadata wrong");
    check(bruce::render_logic_mod_result_inspection_v1(result) ==
        "{\"contract\":\"logic-mod-result-inspect-v1\",\"action\":\"inspect\",\"status\":\"READY\",\"accepted\":true,\"input_present\":false,\"output_present\":true,\"binding_identity\":\"e98361b90d3a8fcfad5bfe02876d0d064e72025db806b832db4bc545b0c4565a\",\"output_manifest_id\":\"result\",\"output_sha256\":\"31540b43fa20cb275095247f6784794cecee19c0117b76238fa153964a52af5b\",\"output_size_bytes\":4,\"output_codec\":\"logic-mod-result-text-v1\",\"replay_readiness\":\"READY\"}\n",
        "READY public JSON wrong or private data exposed");
}

// Catch accidental Store access for legacy/input-only records and invented output.
void no_output_contracts() {
    bruce::LogicModRunV1 run;
    run.status = "WORKER_EXIT_1"; run.binding_identity = "binding\"with\nquotes";
    const auto legacy = bruce::inspect_logic_mod_result_v1(bruce::logic_mod_run_record_v1(run), "/does/not/exist");
    check(bruce::render_logic_mod_result_inspection_v1(legacy) ==
        "{\"contract\":\"logic-mod-result-inspect-v1\",\"action\":\"inspect\",\"status\":\"NO_OUTPUT\",\"accepted\":false,\"input_present\":false,\"output_present\":false,\"binding_identity\":\"b357a22990c9692071b192072b314338bd2c7106cbaab8d6768553f9a2f39573\",\"output_manifest_id\":null,\"output_sha256\":null,\"output_size_bytes\":null,\"output_codec\":null,\"replay_readiness\":\"NO_OUTPUT\"}\n",
        "legacy no-output JSON wrong");
    run.input_present = true; run.input_manifest_id = "input"; run.input_sha256 = std::string(64, 'a');
    run.input_size_bytes = 1; run.input_codec = "artifact-bytes-v1";
    const auto input = bruce::inspect_logic_mod_result_v1(bruce::logic_mod_run_record_v1(run), "/does/not/exist");
    check(input.status == "NO_OUTPUT" && input.input_present && !input.output_present, "input-only inspected Store or invented output");
    run.binding_identity = "/tmp/private-binding-path payload-marker";
    const auto malicious = bruce::render_logic_mod_result_inspection_v1(
        bruce::inspect_logic_mod_result_v1(bruce::logic_mod_run_record_v1(run), "/does/not/exist"));
    check(malicious.find("/tmp/private-binding-path") == std::string::npos &&
        malicious.find("payload-marker") == std::string::npos, "raw binding path or payload reflected");
}

// Pin deterministic classification using existing read-only Store APIs only.
void failure_contracts() {
    Fixture f;
    auto expect = [&](std::string_view status) {
        const auto report = f.inspect();
        check(report.status == status, "wrong failure classification");
        const auto text = bruce::render_logic_mod_result_inspection_v1(report);
        check(text.find(f.root.string()) == std::string::npos && text.find("9/16") == std::string::npos &&
            text.find("private-response-frame") == std::string::npos && text.find("private-input") == std::string::npos,
            "path or payload leaked");
        check(text.find("\"replay_readiness\":\"READY\"") == std::string::npos, "unverified output marked replay ready");
    };
    std::filesystem::rename(f.root, f.dir / "moved-store");
    // Existing ArtifactStoreOpenFailed conflates absence with invalid/open failures.
    expect("STORE_ERROR");
    check(!std::filesystem::exists(f.root), "inspection created missing Store");
    std::filesystem::rename(f.dir / "moved-store", f.root);
    std::filesystem::rename(f.manifest_path(), f.dir / "saved-manifest"); expect("MISSING");
    std::filesystem::rename(f.dir / "saved-manifest", f.manifest_path());
    f.manifest.status = "FAILED"; f.manifest_write(); expect("NOT_READY");
    f.manifest.status = "READY"; f.manifest.node_id = "different"; f.manifest_write(); expect("IDENTITY_MISMATCH");
    f.manifest.node_id = "result"; f.manifest.objects[0].media_type = "wrong-codec";
    f.manifest_write(); expect("IDENTITY_MISMATCH");
    f.manifest.objects[0].media_type = "logic-mod-result-text-v1"; f.manifest_write();
    write(f.object_path(), "1/16"); expect("IDENTITY_MISMATCH");
    write(f.object_path(), "9/16");
    f.run.result = "1/16"; expect("IDENTITY_MISMATCH"); f.run.result = "9/16";
    std::filesystem::rename(f.object_path(), f.dir / "saved-object"); expect("MISSING");
    std::filesystem::rename(f.dir / "saved-object", f.object_path());
    write(f.manifest_path(), "untrusted malformed manifest with private diagnostics"); expect("STORE_ERROR");
    f.manifest_write();
    check(::chmod((f.root / "store.lock").c_str(), 0644) == 0, "chmod failed"); expect("STORE_ERROR");
    check(::chmod((f.root / "store.lock").c_str(), 0600) == 0, "chmod failed");
    for (const std::string& bytes : {std::string{}, std::string("private malformed /tmp/path record"),
            bruce::logic_mod_run_record_v1(f.run) + "trailing", std::string(131073, 'x')}) {
        const auto invalid = bruce::inspect_logic_mod_result_v1(bytes, f.root);
        check(invalid.status == "INVALID_RECORD" && !invalid.accepted && !invalid.input_present && !invalid.output_present,
            "malformed record accepted or partial metadata retained");
        check(bruce::render_logic_mod_result_inspection_v1(invalid).find("private") == std::string::npos,
            "malformed record leaked");
    }
}

// Catch option parsing which reads files before validating the complete grammar.
void cli_grammar_contracts() {
    using bruce::logic_mod_result_inspector_cli_detail::parse;
    const std::vector<std::string_view> valid{"record", "--store", "store"};
    const auto good = parse(valid);
    check(good.record == "record" && good.store == "store", "valid inspector grammar rejected");
    for (const std::vector<std::string_view>& args : std::vector<std::vector<std::string_view>>{
            {}, {"record"}, {"record", "--store"}, {"record", "--store", ""}, {"", "--store", "store"},
            {" \t", "--store", "store"}, {"record", "--store", "\n"},
            {"--store", "store", "record"}, {"record", "--other", "store"},
            {"record", "--store", "store", "--store", "duplicate"}, {"record", "--store", "store", "extra"},
            {"--store", "--store", "store"}, {"record", "--store", "--record"}}) {
        bool rejected = false;
        try { (void)parse(args); }
        catch (const bruce::Error& error) { rejected = std::string_view(error.what()) == "InvalidLogicModResultInspectArguments"; }
        check(rejected, "invalid inspector grammar accepted or arguments leaked");
    }
}

// Catch blocking opens and symlink traversal with valid CLI grammar. An alarm
// kills only the isolated child; the parent always reaps it before asserting.
void record_path_cli_contract(std::string_view selection) {
    Fixture f;
    const auto regular = f.dir / "legacy.record";
    bruce::LogicModRunV1 legacy;
    write(regular, bruce::logic_mod_run_record_v1(legacy));
    check(::chmod(regular.c_str(), 0600) == 0, "fixture chmod failed");
    const auto fifo = f.dir / "fifo.record";
    check(::mkfifo(fifo.c_str(), 0600) == 0, "fixture mkfifo failed");
    const auto link_fifo = f.dir / "symlink-fifo.record";
    std::filesystem::create_symlink(fifo, link_fifo);
    const auto link_regular = f.dir / "symlink-regular.record";
    std::filesystem::create_symlink(regular, link_regular);
    const auto target = selection == "--record-fifo" ? fifo :
        selection == "--record-symlink-fifo" ? link_fifo : link_regular;
    int capture[2]; check(::pipe(capture) == 0, "fixture pipe failed");
    const auto child = ::fork(); check(child >= 0, "fixture fork failed");
    if (child == 0) {
        ::close(capture[0]);
        if (::dup2(capture[1], STDOUT_FILENO) < 0) ::_exit(3);
        ::close(capture[1]);
        ::alarm(1);
        std::string executable = "bruce", command = "logic-mod-result-inspect";
        std::string record = target.string(), option = "--store", store = f.root.string();
        char* args[] = {executable.data(), command.data(), record.data(), option.data(), store.data()};
        try {
            const auto code = bruce::logic_mod_result_inspector_v1_cli(5, args);
            std::cout << std::flush;
            ::_exit(code);
        } catch (...) { ::_exit(4); }
    }
    ::close(capture[1]);
    int status{}; pid_t waited;
    do { waited = ::waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    std::string output;
    char buffer[1024]; ssize_t count;
    while ((count = ::read(capture[0], buffer, sizeof(buffer))) > 0) output.append(buffer, count);
    ::close(capture[0]);
    check(waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
        "valid inspector grammar blocked on unsafe record or failed without JSON");
    check(output == bruce::render_logic_mod_result_inspection_v1({}),
        "unsafe record followed or private/partial stdout exposed");
    check(std::filesystem::is_fifo(fifo) && std::filesystem::is_symlink(link_fifo) &&
        std::filesystem::is_symlink(link_regular), "inspection mutated unsafe fixtures");
}

// Catch device/directory reads, private-file check regressions and byte-limit
// off-by-one errors at the reader boundary, independently of record parsing.
void record_reader_contracts() {
    Fixture f;
    const auto regular = f.dir / "private.record";
    const std::string exact_limit(131072, 'x');
    write(regular, exact_limit);
    check(::chmod(regular.c_str(), 0600) == 0, "fixture chmod failed");
    check(bruce::read_logic_mod_result_record_file_v1(regular) == exact_limit,
        "reader truncated or rejected exact byte limit");
    const auto rejected = [&](const std::filesystem::path& path) {
        bool failed = false;
        try { (void)bruce::read_logic_mod_result_record_file_v1(path); }
        catch (const bruce::Error& error) { failed = std::string_view(error.what()) == "LogicModResultRecordInvalid"; }
        check(failed, "unsafe record inode read or raw diagnostic leaked");
    };
    write(regular, exact_limit + 'x'); rejected(regular);
    write(regular, "private fixture bytes");
    rejected(f.dir); rejected("/dev/null"); rejected("/dev/zero"); rejected(f.dir / "missing");
    check(::chmod(regular.c_str(), 0644) == 0, "fixture chmod failed"); rejected(regular);
    check(::chmod(regular.c_str(), 0600) == 0, "fixture chmod failed");
    const auto hardlink = f.dir / "hardlink.record";
    std::filesystem::create_hard_link(regular, hardlink); rejected(hardlink);
    std::filesystem::remove(hardlink);
    const auto symlink = f.dir / "device.record";
    std::filesystem::create_symlink("/dev/null", symlink); rejected(symlink);
    const auto link_dir = f.dir / "linked-parent";
    std::filesystem::create_directory_symlink(f.dir, link_dir); rejected(link_dir / regular.filename());
    check(bruce::read_logic_mod_result_record_file_v1(regular) == "private fixture bytes", "private record read changed");
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            ready_contract(); no_output_contracts(); failure_contracts(); cli_grammar_contracts(); record_reader_contracts();
        } else if (argc == 2 && (std::string_view(argv[1]) == "--record-fifo" ||
                std::string_view(argv[1]) == "--record-symlink-fifo" || std::string_view(argv[1]) == "--record-symlink-regular")) {
            record_path_cli_contract(argv[1]);
        } else throw bruce::Error("Unknown inspector test selection");
        std::cout << "logic result inspector contracts passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
