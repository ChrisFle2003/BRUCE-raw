#include "bruce/artifact/payload_flow_v2.hpp"
#include "bruce/artifact/v5_manifest_id_v1.hpp"
#include "bruce/evidence/bundle.hpp"

#include <iostream>
#include <sstream>

using namespace bruce;
using namespace bruce::artifact_v1;

namespace {
void require(bool value, const char* message) {
    if (!value) throw Error(message);
}

template<class F>
void rejects(F&& action, const char* message) {
    try {
        action();
    } catch (const Error&) {
        return;
    }
    throw Error(message);
}

PayloadFlowV2 valid_flow() {
    const auto digest = sha256("request");
    PayloadFlowV2 flow;
    flow.run_id = "run-1";
    flow.spec_digest = sha256("spec");
    flow.report_digest = sha256("report");
    flow.nodes = {
        {"root", "literal-v1", digest, {}},
        {"child", "concat-v1", digest, {"root"}},
    };
    flow.attempts = {
        {"root", "root-1", {}},
        {"child", "child-1", {{0, "root", "root-1",
            v5_manifest_id_v1("run-1", "root", "root-1"), 0, sha256("payload"), 7}}},
    };
    return flow;
}

std::string node_json(std::string_view id, std::string_view dependencies = {}) {
    std::string result = "{\"node_id\":\"" + std::string(id) +
        "\",\"callback_id\":\"literal-v1\",\"request_digest\":\"" +
        sha256("request") + "\",\"dependencies\":[";
    if (!dependencies.empty()) result += "\"" + std::string(dependencies) + "\"";
    return result + "]}";
}

std::string root_attempt_json(std::string_view node, std::string_view attempt) {
    return "{\"consumer_node_id\":\"" + std::string(node) +
        "\",\"consumer_attempt_id\":\"" + std::string(attempt) + "\",\"inputs\":[]}";
}

std::string bounded_raw(std::string_view nodes, std::string_view attempts) {
    return "{\"contract\":\"bruce-payload-flow-v2\",\"run_id\":\"run-1\","
        "\"spec_digest\":\"" + sha256("spec") + "\",\"report_digest\":\"" +
        sha256("report") + "\",\"stream_v15\":false,\"nodes\":[" +
        std::string(nodes) + "],\"attempts\":[" + std::string(attempts) + "]}\n";
}

void replace_once(std::string& value, std::string_view from, std::string_view to,
                  std::size_t start = 0) {
    const auto position = value.find(from, start);
    require(position != std::string::npos, "test replacement target missing");
    value.replace(position, from.size(), to);
}
}

int main() {
    try {
        auto flow = valid_flow();
        const auto encoded = encode_payload_flow_v2(flow);
        const std::string expected =
            "{\"contract\":\"bruce-payload-flow-v2\",\"run_id\":\"run-1\","
            "\"spec_digest\":\"" + sha256("spec") + "\",\"report_digest\":\"" +
            sha256("report") + "\",\"stream_v15\":false,\"nodes\":["
            "{\"node_id\":\"root\",\"callback_id\":\"literal-v1\",\"request_digest\":\"" +
            sha256("request") + "\",\"dependencies\":[]},"
            "{\"node_id\":\"child\",\"callback_id\":\"concat-v1\",\"request_digest\":\"" +
            sha256("request") + "\",\"dependencies\":[\"root\"]}],\"attempts\":["
            "{\"consumer_node_id\":\"root\",\"consumer_attempt_id\":\"root-1\",\"inputs\":[]},"
            "{\"consumer_node_id\":\"child\",\"consumer_attempt_id\":\"child-1\",\"inputs\":["
            "{\"input_index\":0,\"producer_node_id\":\"root\",\"producer_attempt_id\":\"root-1\",\"manifest_id\":\"" +
            v5_manifest_id_v1("run-1", "root", "root-1") +
            "\",\"object_index\":0,\"sha256\":\"" + sha256("payload") +
            "\",\"size_bytes\":7}]}]}\n";
        require(encoded == expected, "sidecar key order or canonical bytes changed");
        require(decode_payload_flow_v2(encoded) == flow, "valid sidecar did not round trip");
        require(v5_manifest_id_v1("run-1", "root", "root-1") ==
                    "v5-da5cf8d6a84385acdc9b875006529109529f61abe6c709a471075b811e9f7ee2",
                "shared manifest helper fixture changed");

        auto no_stream = flow;
        no_stream.stream_v15 = true;
        require(decode_payload_flow_v2(encode_payload_flow_v2(no_stream)) == no_stream,
                "stream mode was not preserved");

        auto invalid_index = flow;
        invalid_index.attempts[1].inputs[0].input_index = 1;
        rejects([&] { (void)encode_payload_flow_v2(invalid_index); }, "gap input index accepted");
        auto duplicate_attempt = flow;
        duplicate_attempt.attempts.push_back(duplicate_attempt.attempts[0]);
        rejects([&] { (void)encode_payload_flow_v2(duplicate_attempt); }, "duplicate attempt accepted");
        auto unknown_dependency = flow;
        unknown_dependency.nodes[1].dependencies = {"missing"};
        rejects([&] { (void)encode_payload_flow_v2(unknown_dependency); }, "unknown dependency accepted");
        auto wrong_producer = flow;
        wrong_producer.attempts[1].inputs[0].producer_node_id = "child";
        rejects([&] { (void)encode_payload_flow_v2(wrong_producer); }, "non-dependency producer accepted");
        auto oversized = flow;
        oversized.attempts[1].inputs[0].size_bytes = max_file_bytes + 1;
        rejects([&] { (void)encode_payload_flow_v2(oversized); }, "oversized input accepted");

        auto unknown_key = encoded;
        const auto key = unknown_key.find("\"run_id\"");
        unknown_key.replace(key, 8, "\"run_xx\"");
        rejects([&] { (void)decode_payload_flow_v2(unknown_key); }, "unknown key accepted");
        auto duplicate_key = encoded;
        duplicate_key.insert(duplicate_key.find(",\"spec_digest\""), ",\"run_id\":\"run-1\"");
        rejects([&] { (void)decode_payload_flow_v2(duplicate_key); }, "duplicate key accepted");
        auto no_lf = encoded.substr(0, encoded.size() - 1);
        rejects([&] { (void)decode_payload_flow_v2(no_lf); }, "missing LF accepted");
        rejects([&] { (void)decode_payload_flow_v2(encoded + "x"); }, "trailing bytes accepted");
        auto bad_number = encoded;
        const auto size_key = bad_number.find("\"size_bytes\":7");
        bad_number.replace(size_key, 16, "\"size_bytes\":07");
        rejects([&] { (void)decode_payload_flow_v2(bad_number); }, "non-canonical number accepted");
        auto bad_digest = encoded;
        const auto digest_pos = bad_digest.find("\"spec_digest\":\"") + 16;
        bad_digest[digest_pos] = 'A';
        rejects([&] { (void)decode_payload_flow_v2(bad_digest); }, "invalid digest accepted");

        PayloadFlowV2 fan_in = flow;
        fan_in.nodes = {{"left", "literal-v1", sha256("request"), {}},
                        {"right", "literal-v1", sha256("request"), {}},
                        {"join", "concat-v1", sha256("request"), {"left", "right"}}};
        fan_in.attempts = {
            {"left", "left-1", {}}, {"right", "right-1", {}},
            {"join", "join-1", {
                {0, "left", "left-1", v5_manifest_id_v1("run-1", "left", "left-1"), 0,
                 sha256("left"), 4},
                {1, "right", "right-1", v5_manifest_id_v1("run-1", "right", "right-1"), 0,
                 sha256("right"), 5}}}};
        const auto fan_in_bytes = encode_payload_flow_v2(fan_in);
        auto swapped = fan_in_bytes;
        const auto inputs = swapped.find("\"inputs\":[", swapped.find("\"consumer_node_id\":\"join\""));
        require(inputs != std::string::npos, "fan-in input fixture missing");
        const auto left_manifest = v5_manifest_id_v1("run-1", "left", "left-1");
        const auto right_manifest = v5_manifest_id_v1("run-1", "right", "right-1");
        replace_once(swapped, "\"producer_node_id\":\"left\"", "\"producer_node_id\":\"TEMP\"", inputs);
        replace_once(swapped, "\"producer_node_id\":\"right\"", "\"producer_node_id\":\"left\"", inputs);
        replace_once(swapped, "\"producer_node_id\":\"TEMP\"", "\"producer_node_id\":\"right\"", inputs);
        replace_once(swapped, left_manifest, "TEMP_MANIFEST", inputs);
        replace_once(swapped, right_manifest, left_manifest, inputs);
        replace_once(swapped, "TEMP_MANIFEST", right_manifest, inputs);
        rejects([&] { (void)decode_payload_flow_v2(swapped); }, "swapped fan-in accepted by decoder");
        auto swapped_model = fan_in;
        std::swap(swapped_model.attempts[2].inputs[0], swapped_model.attempts[2].inputs[1]);
        rejects([&] { (void)encode_payload_flow_v2(swapped_model); }, "swapped fan-in accepted by encoder");

        std::string oversized_nodes;
        for (std::size_t index = 0; index < payload_flow_v2_max_nodes + 1; ++index) {
            if (index) oversized_nodes += ',';
            oversized_nodes += node_json("n" + std::to_string(index));
        }
        rejects([&] { (void)decode_payload_flow_v2(bounded_raw(oversized_nodes, "")); },
                "node container limit applied after materialization");
        std::string oversized_attempts;
        for (std::size_t index = 0; index < payload_flow_v2_max_attempts + 1; ++index) {
            if (index) oversized_attempts += ',';
            oversized_attempts += root_attempt_json("root", "a" + std::to_string(index));
        }
        rejects([&] { (void)decode_payload_flow_v2(bounded_raw(node_json("root"), oversized_attempts)); },
                "attempt container limit applied after materialization");
        std::string oversized_dependencies;
        for (std::size_t index = 0; index < payload_flow_v2_max_dependencies + 1; ++index) {
            if (index) oversized_dependencies += ',';
            oversized_dependencies += "\"root\"";
        }
        auto dependency_node = node_json("child");
        dependency_node.replace(dependency_node.find("[]"), 2, "[" + oversized_dependencies + "]");
        rejects([&] { (void)decode_payload_flow_v2(bounded_raw(node_json("root") + "," + dependency_node, "")); },
                "dependency container limit applied after materialization");
        std::string oversized_inputs;
        const auto input_json = "{\"input_index\":0,\"producer_node_id\":\"root\",\"producer_attempt_id\":\"root-1\",\"manifest_id\":\"" +
            v5_manifest_id_v1("run-1", "root", "root-1") + "\",\"object_index\":0,\"sha256\":\"" +
            sha256("payload") + "\",\"size_bytes\":7}";
        for (std::size_t index = 0; index < payload_flow_v2_max_inputs + 1; ++index) {
            if (index) oversized_inputs += ',';
            oversized_inputs += input_json;
        }
        const auto oversized_input_attempt = "{\"consumer_node_id\":\"child\",\"consumer_attempt_id\":\"child-1\",\"inputs\":[" +
            oversized_inputs + "]}";
        rejects([&] { (void)decode_payload_flow_v2(bounded_raw(node_json("root") + "," + node_json("child", "root"), oversized_input_attempt)); },
                "input container limit applied after materialization");
        auto overlong = encoded;
        replace_once(overlong, "run-1", std::string(129, 'r'));
        rejects([&] { (void)decode_payload_flow_v2(overlong); }, "field string limit applied after materialization");

        std::cout << "payload flow v2 contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
