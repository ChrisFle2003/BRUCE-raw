#include "bruce/artifact/payload_flow_v2.hpp"

#include "bruce/artifact/artifact_manifest_v1.hpp"
#include "bruce/artifact/v5_manifest_id_v1.hpp"
#include "bruce/core/json.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <set>

namespace bruce::artifact_v1 {
namespace {

[[noreturn]] void invalid() { throw Error("PayloadFlowV2Invalid"); }

bool ascii_alnum(unsigned char value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9');
}

bool valid_id(std::string_view value) {
    if (value.empty() || value.size() > artifact_manifest_token_limit_v1) return false;
    if (!ascii_alnum(static_cast<unsigned char>(value.front()))) return false;
    for (const unsigned char c : value) {
        if (!(ascii_alnum(c) || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

bool valid_digest(std::string_view value) {
    if (value.size() != 64) return false;
    for (const unsigned char c : value) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

void validate_value(const PayloadFlowV2& flow) {
    if (!valid_id(flow.run_id) || !valid_digest(flow.spec_digest) ||
        !valid_digest(flow.report_digest) || flow.nodes.empty() ||
        flow.nodes.size() > payload_flow_v2_max_nodes ||
        flow.attempts.size() > payload_flow_v2_max_attempts) invalid();

    std::set<std::string> node_ids;
    for (const auto& node : flow.nodes) {
        if (!valid_id(node.node_id) || !valid_id(node.callback_id) ||
            !valid_digest(node.request_digest) ||
            node.dependencies.size() > payload_flow_v2_max_dependencies ||
            !node_ids.insert(node.node_id).second) invalid();
        std::set<std::string> dependencies;
        for (const auto& dependency : node.dependencies) {
            if (!valid_id(dependency) || dependency == node.node_id ||
                !dependencies.insert(dependency).second) invalid();
        }
    }
    for (const auto& node : flow.nodes) {
        for (const auto& dependency : node.dependencies) {
            if (!node_ids.contains(dependency)) invalid();
        }
    }

    std::set<std::pair<std::string, std::string>> attempts;
    for (const auto& attempt : flow.attempts) {
        if (!valid_id(attempt.consumer_node_id) || !valid_id(attempt.consumer_attempt_id) ||
            !node_ids.contains(attempt.consumer_node_id) ||
            !attempts.emplace(attempt.consumer_node_id, attempt.consumer_attempt_id).second ||
            attempt.inputs.size() > payload_flow_v2_max_inputs)
            invalid();
        const auto node = std::find_if(flow.nodes.begin(), flow.nodes.end(), [&](const auto& value) {
            return value.node_id == attempt.consumer_node_id;
        });
        if (node == flow.nodes.end()) invalid();
        std::set<std::uint64_t> indices;
        for (std::size_t position = 0; position < attempt.inputs.size(); ++position) {
            const auto& input = attempt.inputs[position];
            if (input.input_index != position || !indices.insert(input.input_index).second ||
                !valid_id(input.producer_node_id) || !valid_id(input.producer_attempt_id) ||
                !valid_id(input.manifest_id) || !valid_digest(input.sha256) ||
                input.object_index != 0 || input.size_bytes > max_file_bytes ||
                input.manifest_id != v5_manifest_id_v1(flow.run_id, input.producer_node_id,
                                                       input.producer_attempt_id) ||
                (position >= node->dependencies.size() ||
                 input.producer_node_id != node->dependencies[position]))
                invalid();
        }
        if (node->dependencies.size() != attempt.inputs.size()) {
            // Direct V5 attempts contain one input for every direct producer.
            // Root attempts are the only valid empty-input case.
            invalid();
        }
    }
}

class Reader {
public:
    explicit Reader(std::string_view bytes) : bytes_(bytes) {
        if (bytes_.empty() || bytes_.size() > max_file_bytes || bytes_.back() != '\n') invalid();
        limit_ = bytes_.size() - 1;
    }

    char peek() const { return position_ < limit_ ? bytes_[position_] : '\0'; }

    void expect(char value) {
        if (position_ >= limit_ || bytes_[position_] != value) invalid();
        ++position_;
    }

    std::string key(std::string_view expected) {
        const auto actual = string(artifact_manifest_token_limit_v1);
        if (actual != expected) invalid();
        expect(':');
        return actual;
    }

    std::string string(std::size_t maximum = artifact_manifest_token_limit_v1) {
        expect('"');
        std::string result;
        const auto append = [&](char value) {
            if (result.size() >= maximum) invalid();
            result.push_back(value);
        };
        while (position_ < limit_) {
            const unsigned char value = static_cast<unsigned char>(bytes_[position_++]);
            if (value == '"') return result;
            if (value < 0x20) invalid();
            if (value != '\\') {
                append(static_cast<char>(value));
                continue;
            }
            if (position_ >= limit_) invalid();
            const char escape = bytes_[position_++];
            switch (escape) {
            case '"': append('"'); break;
            case '\\': append('\\'); break;
            case '/': append('/'); break;
            case 'b': append('\b'); break;
            case 'f': append('\f'); break;
            case 'n': append('\n'); break;
            case 'r': append('\r'); break;
            case 't': append('\t'); break;
            case 'u': {
                const auto code = hex4();
                if (code >= 0xd800 && code <= 0xdbff) {
                    if (position_ + 6 > limit_ || bytes_[position_] != '\\' ||
                        bytes_[position_ + 1] != 'u') invalid();
                    position_ += 2;
                    const auto low = hex4();
                    if (low < 0xdc00 || low > 0xdfff) invalid();
                    append_utf8(static_cast<std::uint32_t>(0x10000 +
                        ((code - 0xd800) << 10) + (low - 0xdc00)), result, maximum);
                } else if (code >= 0xdc00 && code <= 0xdfff) {
                    invalid();
                } else {
                    append_utf8(static_cast<std::uint32_t>(code), result, maximum);
                }
                break;
            }
            default: invalid();
            }
        }
        invalid();
    }

    std::uint64_t number() {
        const auto start = position_;
        if (peek() == '0') {
            ++position_;
            if (position_ < limit_ && bytes_[position_] >= '0' && bytes_[position_] <= '9') invalid();
        } else {
            if (peek() < '1' || peek() > '9') invalid();
            while (position_ < limit_ && bytes_[position_] >= '0' && bytes_[position_] <= '9') ++position_;
        }
        std::uint64_t value{};
        const auto text = bytes_.substr(start, position_ - start);
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) invalid();
        return value;
    }

    bool boolean() {
        if (bytes_.substr(position_, 4) == "true") { position_ += 4; return true; }
        if (bytes_.substr(position_, 5) == "false") { position_ += 5; return false; }
        invalid();
    }

    void finish() {
        if (position_ != limit_) invalid();
    }

private:
    static void append_utf8(std::uint32_t value, std::string& output, std::size_t maximum) {
        const auto append = [&](char byte) {
            if (output.size() >= maximum) invalid();
            output.push_back(byte);
        };
        if (value <= 0x7f) append(static_cast<char>(value));
        else if (value <= 0x7ff) {
            append(static_cast<char>(0xc0 | (value >> 6)));
            append(static_cast<char>(0x80 | (value & 0x3f)));
        } else if (value <= 0xffff) {
            append(static_cast<char>(0xe0 | (value >> 12)));
            append(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
            append(static_cast<char>(0x80 | (value & 0x3f)));
        } else {
            append(static_cast<char>(0xf0 | (value >> 18)));
            append(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
            append(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
            append(static_cast<char>(0x80 | (value & 0x3f)));
        }
    }

    std::uint16_t hex4() {
        if (position_ + 4 > limit_) invalid();
        std::uint16_t result{};
        for (unsigned index = 0; index < 4; ++index) {
            const unsigned char value = static_cast<unsigned char>(bytes_[position_++]);
            unsigned digit{};
            if (value >= '0' && value <= '9') digit = value - '0';
            else if (value >= 'a' && value <= 'f') digit = value - 'a' + 10;
            else if (value >= 'A' && value <= 'F') digit = value - 'A' + 10;
            else invalid();
            result = static_cast<std::uint16_t>((result << 4) | digit);
        }
        return result;
    }

    std::string_view bytes_;
    std::size_t position_{};
    std::size_t limit_{};
};

template<class T, class F>
std::vector<T> array(Reader& reader, std::size_t maximum, F&& parse) {
    std::vector<T> result;
    reader.expect('[');
    if (reader.peek() == ']') { reader.expect(']'); return result; }
    while (true) {
        if (result.size() >= maximum) invalid();
        result.push_back(parse());
        if (reader.peek() == ']') { reader.expect(']'); return result; }
        reader.expect(',');
    }
}

PayloadFlowNodeV2 parse_node(Reader& reader) {
    PayloadFlowNodeV2 node;
    reader.expect('{');
    reader.key("node_id"); node.node_id = reader.string(); reader.expect(',');
    reader.key("callback_id"); node.callback_id = reader.string(); reader.expect(',');
    reader.key("request_digest"); node.request_digest = reader.string(); reader.expect(',');
    reader.key("dependencies");
    node.dependencies = array<std::string>(reader, payload_flow_v2_max_dependencies,
                                           [&] { return reader.string(); });
    reader.expect('}');
    return node;
}

PayloadFlowInputBindingV2 parse_input(Reader& reader) {
    PayloadFlowInputBindingV2 input;
    reader.expect('{');
    reader.key("input_index"); input.input_index = reader.number(); reader.expect(',');
    reader.key("producer_node_id"); input.producer_node_id = reader.string(); reader.expect(',');
    reader.key("producer_attempt_id"); input.producer_attempt_id = reader.string(); reader.expect(',');
    reader.key("manifest_id"); input.manifest_id = reader.string(); reader.expect(',');
    reader.key("object_index"); input.object_index = reader.number(); reader.expect(',');
    reader.key("sha256"); input.sha256 = reader.string(); reader.expect(',');
    reader.key("size_bytes"); input.size_bytes = reader.number();
    reader.expect('}');
    return input;
}

PayloadFlowAttemptV2 parse_attempt(Reader& reader) {
    PayloadFlowAttemptV2 attempt;
    reader.expect('{');
    reader.key("consumer_node_id"); attempt.consumer_node_id = reader.string(); reader.expect(',');
    reader.key("consumer_attempt_id"); attempt.consumer_attempt_id = reader.string(); reader.expect(',');
    reader.key("inputs");
    attempt.inputs = array<PayloadFlowInputBindingV2>(reader, payload_flow_v2_max_inputs,
                                                      [&] { return parse_input(reader); });
    reader.expect('}');
    return attempt;
}

} // namespace

void validate_payload_flow_v2(const PayloadFlowV2& flow) { validate_value(flow); }

std::string encode_payload_flow_v2(const PayloadFlowV2& flow) {
    validate_value(flow);
    std::string result = "{\"contract\":" + json_string(payload_flow_v2_contract) +
        ",\"run_id\":" + json_string(flow.run_id) +
        ",\"spec_digest\":" + json_string(flow.spec_digest) +
        ",\"report_digest\":" + json_string(flow.report_digest) +
        ",\"stream_v15\":" + std::string(flow.stream_v15 ? "true" : "false") +
        ",\"nodes\":[";
    for (std::size_t index = 0; index < flow.nodes.size(); ++index) {
        if (index) result += ',';
        const auto& node = flow.nodes[index];
        result += "{\"node_id\":" + json_string(node.node_id) +
            ",\"callback_id\":" + json_string(node.callback_id) +
            ",\"request_digest\":" + json_string(node.request_digest) +
            ",\"dependencies\":[";
        for (std::size_t dependency = 0; dependency < node.dependencies.size(); ++dependency) {
            if (dependency) result += ',';
            result += json_string(node.dependencies[dependency]);
        }
        result += "]}";
    }
    result += "],\"attempts\":[";
    for (std::size_t index = 0; index < flow.attempts.size(); ++index) {
        if (index) result += ',';
        const auto& attempt = flow.attempts[index];
        result += "{\"consumer_node_id\":" + json_string(attempt.consumer_node_id) +
            ",\"consumer_attempt_id\":" + json_string(attempt.consumer_attempt_id) +
            ",\"inputs\":[";
        for (std::size_t input = 0; input < attempt.inputs.size(); ++input) {
            if (input) result += ',';
            const auto& value = attempt.inputs[input];
            result += "{\"input_index\":" + std::to_string(value.input_index) +
                ",\"producer_node_id\":" + json_string(value.producer_node_id) +
                ",\"producer_attempt_id\":" + json_string(value.producer_attempt_id) +
                ",\"manifest_id\":" + json_string(value.manifest_id) +
                ",\"object_index\":" + std::to_string(value.object_index) +
                ",\"sha256\":" + json_string(value.sha256) +
                ",\"size_bytes\":" + std::to_string(value.size_bytes) + '}';
        }
        result += "]}";
    }
    result += "]}\n";
    if (result.size() > max_file_bytes) invalid();
    return result;
}

PayloadFlowV2 decode_payload_flow_v2(std::string_view bytes) {
    Reader reader(bytes);
    PayloadFlowV2 flow;
    reader.expect('{');
    reader.key("contract");
    if (reader.string() != payload_flow_v2_contract) invalid();
    reader.expect(',');
    reader.key("run_id"); flow.run_id = reader.string(); reader.expect(',');
    reader.key("spec_digest"); flow.spec_digest = reader.string(); reader.expect(',');
    reader.key("report_digest"); flow.report_digest = reader.string(); reader.expect(',');
    reader.key("stream_v15"); flow.stream_v15 = reader.boolean(); reader.expect(',');
    reader.key("nodes");
    flow.nodes = array<PayloadFlowNodeV2>(reader, payload_flow_v2_max_nodes,
                                          [&] { return parse_node(reader); });
    reader.expect(',');
    reader.key("attempts");
    flow.attempts = array<PayloadFlowAttemptV2>(reader, payload_flow_v2_max_attempts,
                                                [&] { return parse_attempt(reader); });
    reader.expect('}');
    reader.finish();
    validate_value(flow);
    if (encode_payload_flow_v2(flow) != bytes) invalid();
    return flow;
}

} // namespace bruce::artifact_v1
