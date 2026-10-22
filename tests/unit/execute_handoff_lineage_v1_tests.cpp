#include "bruce/artifact/execute_handoff_lineage_v1.hpp"

#include <iostream>
#include <string>

using namespace bruce;
using namespace bruce::artifact_v1;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw Error(message);
}

template<class F>
void rejects(F&& action, const char* message) {
    try { action(); } catch (const Error&) { return; }
    throw Error(message);
}

const std::string a(64, 'a');
const std::string b(64, 'b');
const std::string c(64, 'c');

ExecuteHandoffLineageV1 root() {
    return {"run-1", a, b, {{"root", "literal-v1", c, {}}},
            {{"root", "root-1", {}}}};
}

ExecuteHandoffLineageV1 fan_in() {
    auto value = root();
    value.nodes = {{"left", "literal-v1", c, {}},
                   {"right", "literal-v1", c, {}},
                   {"join", "join-v1", c, {"left", "right"}}};
    value.attempts = {{"left", "left-1", {}}, {"right", "right-1", {}},
                      {"join", "join-1", {
                          {0, "left", "left-1", "attempts/left-1/output/result.bin", a, 4},
                          {1, "right", "right-1", "attempts/right-1/output/result.bin", b, 5}}}};
    return value;
}

void replace_once(std::string& bytes, const std::string& from, const std::string& to) {
    const auto at = bytes.find(from);
    require(at != std::string::npos, "test fixture replacement missing");
    bytes.replace(at, from.size(), to);
}

void canonical_round_trip() {
    const auto value = root();
    const std::string expected =
        "{\"contract\":\"bruce-execute-handoff-v1\",\"run_id\":\"run-1\","
        "\"spec_digest\":\"" + a + "\",\"report_digest\":\"" + b +
        "\",\"nodes\":[{\"node_id\":\"root\",\"callback_id\":\"literal-v1\","
        "\"request_digest\":\"" + c + "\",\"dependencies\":[]}],\"attempts\":["
        "{\"consumer_node_id\":\"root\",\"consumer_attempt_id\":\"root-1\",\"inputs\":[]}]}\n";
    require(encode_execute_handoff_lineage_v1(value) == expected,
            "root sidecar key order, empty inputs or LF changed");
    require(decode_execute_handoff_lineage_v1(expected) == value,
            "root sidecar failed round trip");

    const auto joined = fan_in();
    const auto bytes = encode_execute_handoff_lineage_v1(joined);
    require(bytes.find("\"input_index\":0,\"producer_node_id\":\"left\","
                       "\"producer_attempt_id\":\"left-1\",\"reference\":"
                       "\"attempts/left-1/output/result.bin\",\"sha256\":\"" + a +
                       "\",\"size_bytes\":4") != std::string::npos,
            "first fan-in binding bytes or field order changed");
    require(bytes.find("\"input_index\":1,\"producer_node_id\":\"right\"") >
                bytes.find("\"input_index\":0,\"producer_node_id\":\"left\""),
            "fan-in dependency order changed");
    require(decode_execute_handoff_lineage_v1(bytes) == joined,
            "fan-in sidecar failed round trip");

    auto retry = joined;
    retry.attempts.push_back({"join", "join-2", retry.attempts.back().inputs});
    require(decode_execute_handoff_lineage_v1(encode_execute_handoff_lineage_v1(retry)) == retry,
            "retry attempt was lost");
}

void malformed_json_rejected() {
    const auto bytes = encode_execute_handoff_lineage_v1(root());
    auto unknown = bytes;
    replace_once(unknown, "\"run_id\"", "\"run_xx\"");
    rejects([&] { (void)decode_execute_handoff_lineage_v1(unknown); }, "unknown key accepted");
    auto duplicate = bytes;
    replace_once(duplicate, ",\"spec_digest\"", ",\"run_id\":\"run-1\",\"spec_digest\"");
    rejects([&] { (void)decode_execute_handoff_lineage_v1(duplicate); }, "duplicate key accepted");
    rejects([&] { (void)decode_execute_handoff_lineage_v1(bytes.substr(0, bytes.size() - 1)); },
            "missing LF accepted");
    rejects([&] { (void)decode_execute_handoff_lineage_v1(bytes.substr(0, bytes.size() - 3)); },
            "truncated object accepted");
    rejects([&] { (void)decode_execute_handoff_lineage_v1(bytes + "x"); },
            "trailing bytes accepted");
    auto deep = bytes;
    replace_once(deep, "\"dependencies\":[]", "\"dependencies\":[[[[]]]]");
    rejects([&] { (void)decode_execute_handoff_lineage_v1(deep); }, "deep JSON accepted");
    auto escaped = bytes;
    replace_once(escaped, "run-1", "run\\u002d1");
    rejects([&] { (void)decode_execute_handoff_lineage_v1(escaped); },
            "noncanonical escaped identifier accepted");
    auto number = encode_execute_handoff_lineage_v1(fan_in());
    replace_once(number, "\"size_bytes\":4", "\"size_bytes\":04");
    rejects([&] { (void)decode_execute_handoff_lineage_v1(number); },
            "leading-zero number accepted");
    replace_once(number, "\"size_bytes\":04", "\"size_bytes\":-4");
    rejects([&] { (void)decode_execute_handoff_lineage_v1(number); },
            "negative number accepted");
}

void invalid_values_rejected() {
    auto leading_underscore = root();
    leading_underscore.nodes.front().node_id = "_root";
    leading_underscore.attempts.front().consumer_node_id = "_root";
    require(decode_execute_handoff_lineage_v1(
                encode_execute_handoff_lineage_v1(leading_underscore)) == leading_underscore,
            "valid V5 leading underscore rejected");
    auto leading_dash = root();
    leading_dash.nodes.front().node_id = "-root";
    leading_dash.attempts.front().consumer_node_id = "-root";
    require(decode_execute_handoff_lineage_v1(
                encode_execute_handoff_lineage_v1(leading_dash)) == leading_dash,
            "valid V5 leading dash rejected");
    auto invalid_colon = root();
    invalid_colon.nodes.front().node_id = "root:child";
    invalid_colon.attempts.front().consumer_node_id = "root:child";
    rejects([&] { (void)encode_execute_handoff_lineage_v1(invalid_colon); },
            "node colon outside V5 grammar accepted");
    invalid_colon = root();
    invalid_colon.nodes.front().callback_id = "literal:v1";
    rejects([&] { (void)encode_execute_handoff_lineage_v1(invalid_colon); },
            "callback colon outside V5 grammar accepted");
    invalid_colon = root();
    invalid_colon.nodes.front().node_id = std::string(65, 'n');
    invalid_colon.attempts.front().consumer_node_id = invalid_colon.nodes.front().node_id;
    rejects([&] { (void)encode_execute_handoff_lineage_v1(invalid_colon); },
            "65-byte V5 node ID accepted");

    auto value = fan_in();
    value.attempts.back().inputs[0].input_index = 1;
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "input index gap accepted");
    value = fan_in();
    std::swap(value.attempts.back().inputs[0], value.attempts.back().inputs[1]);
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "swapped fan-in accepted");
    value = fan_in();
    value.attempts.back().inputs[0].producer_node_id = "right";
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "wrong producer accepted");
    value = fan_in();
    value.attempts.back().inputs[0].reference = "../outside";
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "unsafe reference accepted");
    value = fan_in();
    value.attempts.back().inputs[0].reference = "/tmp/outside";
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "absolute reference accepted");
    value = fan_in();
    value.attempts.back().inputs[0].size_bytes = max_file_bytes + 1;
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "oversized payload accepted");
    value = fan_in();
    value.spec_digest[0] = 'A';
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "uppercase digest accepted");
    value = fan_in();
    value.run_id = "../run";
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "unsafe run ID accepted");
    value = fan_in();
    value.nodes.back().dependencies = {"missing"};
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "unknown dependency accepted");
    value = fan_in();
    value.attempts.push_back(value.attempts.back());
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "duplicate attempt accepted");
    value = fan_in();
    value.attempts[1].consumer_attempt_id = "left-1";
    value.attempts[2].inputs[1].producer_attempt_id = "left-1";
    value.attempts[2].inputs[1].reference = "attempts/left-1/output/result.bin";
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); },
            "globally duplicate V5 attempt path accepted");
}

void allocation_limits_rejected() {
    auto value = root();
    value.nodes.resize(65, value.nodes.front());
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "node limit accepted");
    value = root();
    value.attempts.resize(257, value.attempts.front());
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "attempt limit accepted");
    value = root();
    value.nodes.front().dependencies.resize(16, "root");
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "dependency limit accepted");
    value = fan_in();
    value.attempts.back().inputs.resize(16,
                                        value.attempts.back().inputs.front());
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "input limit accepted");
    value = root();
    value.run_id = std::string(129, 'a');
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "identifier limit accepted");
    value = fan_in();
    value.attempts.back().inputs[0].reference = std::string(
        execute_handoff_lineage_v1_max_reference_bytes + 1, 'a');
    rejects([&] { (void)encode_execute_handoff_lineage_v1(value); }, "reference limit accepted");

    const auto bytes = encode_execute_handoff_lineage_v1(root());
    std::string too_large(max_file_bytes + 1, ' ');
    too_large.replace(0, bytes.size(), bytes);
    rejects([&] { (void)decode_execute_handoff_lineage_v1(too_large); },
            "file limit accepted");

    const std::string node = "{\"node_id\":\"root\",\"callback_id\":\"literal-v1\","
        "\"request_digest\":\"" + c + "\",\"dependencies\":[]}";
    std::string many_nodes;
    for (std::size_t i = 0; i < 65; ++i) {
        if (i) many_nodes += ',';
        many_nodes += node;
    }
    auto raw = bytes;
    replace_once(raw, "\"nodes\":[" + node + "]", "\"nodes\":[" + many_nodes + "]");
    rejects([&] { (void)decode_execute_handoff_lineage_v1(raw); },
            "decoder accepted over-limit nodes");

    const std::string attempt = "{\"consumer_node_id\":\"root\","
        "\"consumer_attempt_id\":\"root-1\",\"inputs\":[]}";
    std::string many_attempts;
    for (std::size_t i = 0; i < 257; ++i) {
        if (i) many_attempts += ',';
        many_attempts += attempt;
    }
    raw = bytes;
    replace_once(raw, "\"attempts\":[" + attempt + "]",
                 "\"attempts\":[" + many_attempts + "]");
    rejects([&] { (void)decode_execute_handoff_lineage_v1(raw); },
            "decoder accepted over-limit attempts");

    std::string many_dependencies;
    for (std::size_t i = 0; i < 16; ++i) {
        if (i) many_dependencies += ',';
        many_dependencies += "\"root\"";
    }
    raw = bytes;
    replace_once(raw, "\"dependencies\":[]", "\"dependencies\":[" + many_dependencies + "]");
    rejects([&] { (void)decode_execute_handoff_lineage_v1(raw); },
            "decoder accepted over-limit dependencies");

    const auto joined = fan_in();
    raw = encode_execute_handoff_lineage_v1(joined);
    const std::string input = "{\"input_index\":0,\"producer_node_id\":\"left\","
        "\"producer_attempt_id\":\"left-1\",\"reference\":"
        "\"attempts/left-1/output/result.bin\",\"sha256\":\"" + a +
        "\",\"size_bytes\":4}";
    std::string many_inputs;
    for (std::size_t i = 0; i < 16; ++i) {
        if (i) many_inputs += ',';
        many_inputs += input;
    }
    replace_once(raw, "\"inputs\":[" + input + ",",
                 "\"inputs\":[" + many_inputs + ",");
    rejects([&] { (void)decode_execute_handoff_lineage_v1(raw); },
            "decoder accepted over-limit inputs");
}
} // namespace

int main() {
    try {
        canonical_round_trip();
        malformed_json_rejected();
        invalid_values_rejected();
        allocation_limits_rejected();
        std::cout << "execute handoff lineage v1 contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
