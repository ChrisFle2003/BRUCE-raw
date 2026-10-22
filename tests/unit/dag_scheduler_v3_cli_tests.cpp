#include "../../apps/dag_scheduler_v3_cli.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unistd.h>

namespace {
using namespace bruce;
using namespace bruce::dag_scheduler_v3_cli_detail;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Fixture {
    std::filesystem::path path;
    Fixture() {
        std::string pattern = (std::filesystem::temp_directory_path() /
            "bruce-v3-unit-XXXXXX").string();
        const int fd = ::mkstemp(pattern.data());
        if (fd < 0) throw std::runtime_error("cannot create fixture");
        ::close(fd);
        path = pattern;
    }
    ~Fixture() { std::error_code error; std::filesystem::remove(path, error); }
    void write(std::string_view bytes) const {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!out) throw std::runtime_error("cannot write fixture");
    }
};

const std::string small_spec =
    "BRUCE_DAG_SCHEDULER_SPEC_V3\n"
    "dag-id=small\nmax-started-nodes=2\nmax-total-receipt-bytes=4096\n\n"
    "node=a\ndepends=\ncallback=literal-v1\nrequest-hex=00\n\n"
    "node=b\ndepends=a\ncallback=append-v1\nrequest-hex=\n";

void parse_and_execute_binary_request() {
    Fixture fixture;
    fixture.write(small_spec);
    const auto parsed = parse_spec(fixture.path);
    require(parsed.raw_bytes == small_spec, "raw spec bytes changed");
    require(parsed.spec.scheduler_version == "dag-scheduler-v2", "wrong core version");
    require(parsed.spec.dag_id == "small" && parsed.spec.max_started_nodes == 2 &&
            parsed.spec.max_total_receipt_bytes == 4096, "global mapping changed");
    require(parsed.spec.nodes.size() == 2 && parsed.spec.nodes[0].node_id == "a" &&
            parsed.spec.nodes[1].node_id == "b", "node order changed");
    require(parsed.spec.nodes[0].callback_id == "literal-v1" &&
            parsed.spec.nodes[1].callback_id == "append-v1" &&
            parsed.spec.nodes[1].dependencies == std::vector<std::string>{"a"},
            "callback/dependency mapping changed");
    require(parsed.spec.nodes[0].request_bytes == std::string(1, '\0') &&
            parsed.spec.nodes[0].request_digest ==
            "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d",
            "NUL decoding or digest changed");
    require(parsed.spec.nodes[1].request_bytes.empty() &&
            parsed.spec.nodes[1].request_digest ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "empty decoding or digest changed");
    const auto run = execute(parsed);
    require(run.receipts.size() == 2 && run.receipts[1].output &&
            run.receipts[1].output->payload_bytes == std::string(1, '\0') &&
            run.receipts[1].output->result_code == "APPENDED_READY",
            "binary append output changed");
}

DagSchedulerNodeSpecV2 node(std::string id, std::string callback,
                           std::vector<std::string> deps, std::string request) {
    auto digest = sha256(request);
    return {std::move(id), std::move(callback), std::move(deps),
            std::move(request), std::move(digest)};
}

ParsedDagSchedulerSpecV3 append_spec(std::size_t suffix_size) {
    return {{"dag-scheduler-v2", "append-limit", {
        node("a", "literal-v1", {}, std::string(65536, 'a')),
        node("b", "append-v1", {"a"}, std::string(suffix_size, 'b'))},
        2, 4194304}, ""};
}

ParsedDagSchedulerSpecV3 join_spec(std::size_t second_size, std::size_t separator_size) {
    return {{"dag-scheduler-v2", "join-limit", {
        node("a", "literal-v1", {}, std::string(32768, 'a')),
        node("b", "literal-v1", {}, std::string(second_size, 'b')),
        node("c", "join-v1", {"a", "b"}, std::string(separator_size, '|'))},
        3, 4194304}, ""};
}

void rejects_oversized_result(const ParsedDagSchedulerSpecV3& parsed) {
    std::optional<DagSchedulerRunV2> returned;
    bool rejected = false;
    try { returned = execute(parsed); }
    catch (const Error& error) {
        require(std::string_view(error.what()) == "DagSchedulerV3PayloadLimitExceeded",
                "overflow returned wrong error");
        rejected = true;
    }
    require(rejected && !returned, "overflow returned a partial or oversized run");
}


const std::string inspect_spec =
    "BRUCE_DAG_SCHEDULER_SPEC_V3\n"
    "dag-id=example\nmax-started-nodes=4\nmax-total-receipt-bytes=4096\n\n"
    "node=a-source\ndepends=\ncallback=literal-v1\nrequest-hex=616c706861\n\n"
    "node=b-transform\ndepends=a-source\ncallback=append-v1\nrequest-hex=3a62657461\n\n"
    "node=c-independent\ndepends=\ncallback=literal-v1\nrequest-hex=0000\n\n"
    "node=d-join\ndepends=b-transform,c-independent\ncallback=join-v1\n"
    "request-hex=73656e74696e656c2d6a6f696e2d7633\n";

void inspector_canonical_order_and_metadata() {
    // A different ready-node policy or a leaked request must break this contract.
    Fixture fixture;
    fixture.write(inspect_spec);
    const auto parsed = parse_spec(fixture.path);
    require(planned_start_order(parsed.spec) == std::vector<std::string>{
        "a-source", "b-transform", "c-independent", "d-join"},
        "canonical inspect order changed");
    const auto report = render_inspect(parsed);
    const std::string expected = R"JSON({
  "contract": "dag-scheduler-v3-inspect-v1",
  "action": "inspect",
  "spec_version": "dag-scheduler-spec-v3",
  "spec_digest": "2152e5d27f902f8bf04113f29cdf6d3fddc625736d62232ff2ce4e3923d06d08",
  "scheduler_version": "dag-scheduler-v2",
  "dag_id": "example",
  "max_started_nodes": 4,
  "max_total_receipt_bytes": 4096,
  "nodes": [
    {"node_id": "a-source", "callback_id": "literal-v1", "dependencies": [], "request_size": 5, "request_digest": "8ed3f6ad685b959ead7022518e1af76cd816f8e8ec7ccdda1ed4018e8f2223f8"},
    {"node_id": "b-transform", "callback_id": "append-v1", "dependencies": ["a-source"], "request_size": 5, "request_digest": "93f41553e2334a65bf24fff56f11668488ed30de45b92043370854a84e79dd37"},
    {"node_id": "c-independent", "callback_id": "literal-v1", "dependencies": [], "request_size": 2, "request_digest": "96a296d224f285c67bee93c30f8a309157f0daa35dc5b87e410b78630a09cfc7"},
    {"node_id": "d-join", "callback_id": "join-v1", "dependencies": ["b-transform", "c-independent"], "request_size": 16, "request_digest": "cf3987609056cda4271e4b3c23cb6dc7ba078519d0cc9ef75db7cdaf4876ae1f"}
  ],
  "planned_start_order": ["a-source", "b-transform", "c-independent", "d-join"]
})JSON";
    require(report == expected, "inspector metadata contract changed");
    require(render_inspect(parsed) == report, "inspector bytes are not stable");
    for (const auto* forbidden : {"alpha", "beta", "sentinel-join-v3", "616c706861",
             "3a62657461", "73656e74696e656c2d6a6f696e2d7633", "request-hex",
             "request_bytes", "payload_bytes", "receipts", "trace"}) {
        require(report.find(forbidden) == std::string::npos,
                "inspector leaked request or execution data");
    }
    require(report.find('\0') == std::string::npos, "inspector leaked NUL request");
}

void inspector_empty_request_metadata() {
    Fixture fixture;
    fixture.write(small_spec);
    const auto report = render_inspect(parse_spec(fixture.path));
    require(report.find("\"request_size\": 1") != std::string::npos &&
            report.find("\"request_size\": 0") != std::string::npos &&
            report.find("6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d")
                != std::string::npos &&
            report.find("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")
                != std::string::npos && report.find('\0') == std::string::npos,
            "empty or NUL request metadata changed");
}

void planner_selects_smallest_newly_ready_node() {
    // Taking ready nodes in batches, input order, or depth-first changes this order.
    const DagSchedulerSpecV2 spec{"dag-scheduler-v2", "fan-in", {
        node("z-root", "literal-v1", {}, ""),
        node("d-join", "join-v1", {"b-left", "c-right"}, ""),
        node("c-right", "append-v1", {"a-root"}, ""),
        node("b-left", "append-v1", {"a-root"}, ""),
        node("a-root", "literal-v1", {}, ""),
        node("e-final", "join-v1", {"d-join", "z-root"}, "")}, 1, 1};
    require(planned_start_order(spec) == std::vector<std::string>{
        "a-root", "b-left", "c-right", "d-join", "z-root", "e-final"},
        "planner did not select the lexicographically smallest ready node");
}

void inspector_has_no_callback_entry_path() {
    static_assert(std::is_same_v<decltype(&planned_start_order),
        std::vector<std::string> (*)(const DagSchedulerSpecV2&)>);
    static_assert(std::is_same_v<decltype(&render_inspect),
        std::string (*)(const ParsedDagSchedulerSpecV3&)>);
    // V2-valid, but its built-in join callback is a throwing sentinel with no inputs.
    // Calling execute(), or invoking the callback while planning, must fail here.
    const ParsedDagSchedulerSpecV3 parsed{{"dag-scheduler-v2", "sentinel", {
        node("a", "join-v1", {}, "")}, 1, 1}, ""};
    validate_dag_scheduler_spec_v2(parsed.spec, registry);
    bool sentinel_throws = false;
    try { (void)join("", {}); }
    catch (const Error& error) {
        require(std::string_view(error.what()) == "InvalidDagSchedulerV3SpecCallbackArity",
                "sentinel callback changed");
        sentinel_throws = true;
    }
    require(sentinel_throws, "no-callback sentinel did not throw");
    require(planned_start_order(parsed.spec) == std::vector<std::string>{"a"},
            "planner invoked sentinel callback or execution");
    require(render_inspect(parsed).find("\"planned_start_order\": [\"a\"]")
            != std::string::npos, "renderer invoked sentinel callback or execution");
}

void planner_validates_before_planning() {
    // A planner that bypasses V2 validation would accept this malformed digest/cycle.
    const DagSchedulerSpecV2 valid{"dag-scheduler-v2", "validation", {
        node("a", "append-v1", {}, "")}, 1, 1};
    for (const bool cycle : {false, true}) {
        auto invalid = valid;
        if (cycle) {
            invalid.nodes.push_back(node("b", "append-v1", {"a"}, ""));
            invalid.nodes[0].dependencies = {"b"};
        } else {
            invalid.nodes[0].request_digest = std::string(64, '0');
        }
        bool rejected = false;
        try { (void)planned_start_order(invalid); }
        catch (const Error&) { rejected = true; }
        require(rejected, "planner bypassed V2 validation");
    }
}

void concatenation_boundaries() {
    // Removing the guard or ignoring a separator must break these expectations.
    rejects_oversized_result(append_spec(1));
    rejects_oversized_result(join_spec(32769, 0));
    rejects_oversized_result(join_spec(32768, 1));
    const auto appended = execute(append_spec(0));
    require(appended.receipts.back().output->payload_bytes == std::string(65536, 'a'),
            "exact-limit append rejected or corrupted");
    const auto joined = execute(join_spec(32767, 1));
    const auto& output = *joined.receipts.back().output;
    require(output.result_code == "JOINED_READY" && output.payload_bytes.size() == 65536 &&
            output.payload_bytes == std::string(32768, 'a') + '|' + std::string(32767, 'b') &&
            output.payload_digest == sha256(output.payload_bytes),
            "exact-limit join rejected or corrupted");
}

void rejects_malformed_decimal() {
    Fixture fixture;
    for (const auto* decimal : {"", "0", "+2", "-2", " 2", "2 ", "2x", "2.0",
                                "18446744073709551616"}) {
        auto bytes = small_spec;
        bytes.replace(bytes.find("max-started-nodes=2"),
                      std::string("max-started-nodes=2").size(),
                      std::string("max-started-nodes=") + decimal);
        fixture.write(bytes);
        bool rejected = false;
        try { (void)parse_spec(fixture.path); }
        catch (const Error&) { rejected = true; }
        require(rejected, "malformed decimal accepted");
    }
}
} // namespace

int main() {
    try {
        parse_and_execute_binary_request();
        concatenation_boundaries();
        rejects_malformed_decimal();
        inspector_canonical_order_and_metadata();
        inspector_empty_request_metadata();
        planner_selects_smallest_newly_ready_node();
        inspector_has_no_callback_entry_path();
        planner_validates_before_planning();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "dag scheduler v3 CLI contracts passed\n";
    return 0;
}
