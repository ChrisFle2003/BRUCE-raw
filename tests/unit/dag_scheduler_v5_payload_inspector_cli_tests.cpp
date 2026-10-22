#include "../../apps/dag_scheduler_v5_cli.hpp"
#include "bruce/artifact/payload_run_inspector_v1.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}
int main() {
    using namespace bruce::dag_scheduler_v5_cli_detail;
    try {
        const auto plain = parse_payload_inspect_v1({"payload-inspect", "bundle"});
        require(plain.bundle == "bundle" && !plain.stream_v15, "default inspection");
        const auto stream = parse_payload_inspect_v1({"payload-inspect", "bundle", "--stream-v15"});
        require(stream.bundle == "bundle" && stream.stream_v15, "explicit stream inspection");
        for (const std::vector<std::string_view>& args : std::vector<std::vector<std::string_view>>{
                {}, {"payload-inspect"}, {"inspect", "bundle"}, {"payload-inspect", ""},
                {"payload-inspect", "."}, {"payload-inspect", ".."},
                {"payload-inspect", "bundle.run"}, {"payload-inspect", "--stream-v15", "bundle"},
                {"payload-inspect", "bundle", "--stream-v15", "--stream-v15"},
                {"payload-inspect", "bundle", "--stream-v15=1"},
                {"payload-inspect", "bundle", "--store", "private-store"},
                {"payload-inspect", "bundle", "extra"}}) {
            bool rejected = false;
            try { (void)parse_payload_inspect_v1(args); }
            catch (const bruce::Error& error) { rejected = std::string(error.what()) == "PROTOCOL_ERROR"; }
            require(rejected, "invalid grammar accepted or diagnostic leaked");
        }
        bruce::artifact_v1::PayloadRunInspectionV1 value;
        value.run_finished = true;
        value.run_status = "FAILED";
        value.source_contract = "dag-scheduler-v5-payload-v14";
        value.run_id = "example";
        value.last_seq = 7; value.node_count = 2; value.attempt_count = 1;
        const auto rendered = render_payload_inspection_v1(value);
        require(rendered == "{\"contract\":\"dag-scheduler-v5-payload-inspect-v1\",\"action\":\"payload-inspect\",\"status\":\"VALID\",\"run_finished\":true,\"run_status\":\"FAILED\",\"source_contract\":\"dag-scheduler-v5-payload-v14\",\"run_id\":\"example\",\"last_seq\":7,\"node_count\":2,\"attempt_count\":1,\"accepted_artifact_count\":0,\"stream_check\":\"NOT_REQUESTED\",\"verified_stream_count\":0,\"verified_stream_bytes\":0,\"input_completeness\":\"NOT_PROVABLE\",\"external_provenance\":\"NOT_CHECKED\"}\n", "exact metadata contract");
        value.run_finished = false;
        value.streams_requested = true;
        value.verified_stream_count = 2; value.verified_stream_bytes = 65537;
        const auto incomplete = render_payload_inspection_v1(value);
        require(incomplete.find("\"status\":\"INCOMPLETE\",\"run_finished\":false,\"run_status\":\"INCOMPLETE\"") != std::string::npos, "incomplete status");
        require(incomplete.find("\"stream_check\":\"VERIFIED_PRESENT\",\"verified_stream_count\":2,\"verified_stream_bytes\":65537") != std::string::npos, "stream counters");
        std::cout << "payload inspector CLI contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
