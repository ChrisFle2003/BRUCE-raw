#include "bruce/artifact/execute_handoff_lineage_v1.hpp"

#include <algorithm>
#include <charconv>
#include <set>

namespace bruce::artifact_v1 {
namespace {

[[noreturn]] void invalid() { throw Error("ExecuteHandoffLineageV1Invalid"); }

bool alnum(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9');
}

bool identifier(std::string_view value, std::size_t maximum) {
    if (value.empty() || value.size() > maximum) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return alnum(c) || c == '_' || c == '-';
    });
}

bool node_identifier(std::string_view value) { return identifier(value, 64); }
bool run_identifier(std::string_view value) { return identifier(value, 128); }
bool attempt_identifier(std::string_view value) { return identifier(value, 128); }

bool digest(std::string_view value) {
    return value.size() == 64 &&
        std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        });
}

bool reference(std::string_view value, std::string_view producer_attempt_id) {
    if (value.empty() || value.size() > execute_handoff_lineage_v1_max_reference_bytes ||
        !attempt_identifier(producer_attempt_id)) return false;
    return value == "attempts/" + std::string(producer_attempt_id) + "/output/result.bin";
}

void validate_value(const ExecuteHandoffLineageV1& lineage) {
    if (!run_identifier(lineage.run_id) || !digest(lineage.spec_digest) ||
        !digest(lineage.report_digest) || lineage.nodes.empty() ||
        lineage.nodes.size() > execute_handoff_lineage_v1_max_nodes ||
        lineage.attempts.size() > execute_handoff_lineage_v1_max_attempts) invalid();

    std::set<std::string> node_ids;
    for (const auto& node : lineage.nodes) {
        if (!node_identifier(node.node_id) || !node_identifier(node.callback_id) ||
            !digest(node.request_digest) ||
            node.dependencies.size() > execute_handoff_lineage_v1_max_dependencies ||
            !node_ids.insert(node.node_id).second) invalid();
        std::set<std::string> dependencies;
        for (const auto& dependency : node.dependencies) {
            if (!node_identifier(dependency) || dependency == node.node_id ||
                !dependencies.insert(dependency).second) invalid();
        }
    }
    for (const auto& node : lineage.nodes)
        for (const auto& dependency : node.dependencies)
            if (!node_ids.contains(dependency)) invalid();

    std::set<std::string> attempt_ids;
    for (const auto& attempt : lineage.attempts) {
        if (!node_identifier(attempt.consumer_node_id) ||
            !attempt_identifier(attempt.consumer_attempt_id) ||
            !node_ids.contains(attempt.consumer_node_id) ||
            !attempt_ids.insert(attempt.consumer_attempt_id).second ||
            attempt.inputs.size() > execute_handoff_lineage_v1_max_inputs) invalid();
        const auto node = std::find_if(lineage.nodes.begin(), lineage.nodes.end(),
            [&](const auto& candidate) { return candidate.node_id == attempt.consumer_node_id; });
        if (node == lineage.nodes.end() || attempt.inputs.size() != node->dependencies.size())
            invalid();
        for (std::size_t position = 0; position < attempt.inputs.size(); ++position) {
            const auto& input = attempt.inputs[position];
            if (input.input_index != position ||
                !node_identifier(input.producer_node_id) ||
                !attempt_identifier(input.producer_attempt_id) ||
                input.producer_node_id != node->dependencies[position] ||
                !reference(input.reference, input.producer_attempt_id) ||
                !digest(input.sha256) || input.size_bytes > max_file_bytes)
                invalid();
        }
    }
}

class Reader {
public:
    explicit Reader(std::string_view bytes) : bytes_(bytes) {
        if (bytes.empty() || bytes.size() > max_file_bytes || bytes.back() != '\n') invalid();
        limit_ = bytes.size() - 1;
    }

    char peek() const { return position_ < limit_ ? bytes_[position_] : '\0'; }

    void expect(char c) {
        if (position_ >= limit_ || bytes_[position_] != c) invalid();
        ++position_;
    }

    void key(std::string_view name) {
        expect('"');
        if (name.size() > limit_ - position_ || bytes_.substr(position_, name.size()) != name)
            invalid();
        position_ += name.size();
        expect('"');
        expect(':');
    }

    std::string_view string(std::size_t maximum = execute_handoff_lineage_v1_max_token_bytes) {
        expect('"');
        const auto start = position_;
        while (position_ < limit_ && bytes_[position_] != '"') {
            const auto c = static_cast<unsigned char>(bytes_[position_]);
            if (c < 0x20 || c > 0x7e || c == '\\' || position_ - start >= maximum)
                invalid();
            ++position_;
        }
        if (position_ >= limit_) invalid();
        const auto result = bytes_.substr(start, position_ - start);
        ++position_;
        return result;
    }

    std::uint64_t number() {
        const auto start = position_;
        if (peek() == '0') {
            ++position_;
            if (peek() >= '0' && peek() <= '9') invalid();
        } else {
            if (peek() < '1' || peek() > '9') invalid();
            do { ++position_; } while (peek() >= '0' && peek() <= '9');
        }
        std::uint64_t value{};
        const auto digits = bytes_.substr(start, position_ - start);
        const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size()) invalid();
        return value;
    }

    void finish() const { if (position_ != limit_) invalid(); }

private:
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

ExecuteHandoffNodeV1 parse_node(Reader& reader) {
    ExecuteHandoffNodeV1 node;
    reader.expect('{');
    reader.key("node_id"); node.node_id = reader.string(64); reader.expect(',');
    reader.key("callback_id"); node.callback_id = reader.string(64); reader.expect(',');
    reader.key("request_digest"); node.request_digest = reader.string(); reader.expect(',');
    reader.key("dependencies");
    node.dependencies = array<std::string>(reader, execute_handoff_lineage_v1_max_dependencies,
        [&] { return std::string(reader.string(64)); });
    reader.expect('}');
    return node;
}

ExecuteHandoffInputBindingV1 parse_input(Reader& reader) {
    ExecuteHandoffInputBindingV1 input;
    reader.expect('{');
    reader.key("input_index"); input.input_index = reader.number(); reader.expect(',');
    reader.key("producer_node_id"); input.producer_node_id = reader.string(64); reader.expect(',');
    reader.key("producer_attempt_id"); input.producer_attempt_id = reader.string(); reader.expect(',');
    reader.key("reference"); input.reference = reader.string(execute_handoff_lineage_v1_max_reference_bytes); reader.expect(',');
    reader.key("sha256"); input.sha256 = reader.string(); reader.expect(',');
    reader.key("size_bytes"); input.size_bytes = reader.number();
    reader.expect('}');
    return input;
}

ExecuteHandoffAttemptV1 parse_attempt(Reader& reader) {
    ExecuteHandoffAttemptV1 attempt;
    reader.expect('{');
    reader.key("consumer_node_id"); attempt.consumer_node_id = reader.string(64); reader.expect(',');
    reader.key("consumer_attempt_id"); attempt.consumer_attempt_id = reader.string(); reader.expect(',');
    reader.key("inputs");
    attempt.inputs = array<ExecuteHandoffInputBindingV1>(reader,
        execute_handoff_lineage_v1_max_inputs, [&] { return parse_input(reader); });
    reader.expect('}');
    return attempt;
}

class Writer {
public:
    void append(std::string_view value) {
        if (value.size() > max_file_bytes - bytes_.size()) invalid();
        bytes_.append(value);
    }
    void quoted(std::string_view value) {
        append("\""); append(value); append("\"");
    }
    void number(std::uint64_t value) { append(std::to_string(value)); }
    std::string finish() && { return std::move(bytes_); }
private:
    std::string bytes_;
};

} // namespace

void validate_execute_handoff_lineage_v1(const ExecuteHandoffLineageV1& lineage) {
    validate_value(lineage);
}

std::string encode_execute_handoff_lineage_v1(const ExecuteHandoffLineageV1& lineage) {
    validate_value(lineage);
    Writer writer;
    writer.append("{\"contract\":"); writer.quoted(execute_handoff_lineage_v1_contract);
    writer.append(",\"run_id\":"); writer.quoted(lineage.run_id);
    writer.append(",\"spec_digest\":"); writer.quoted(lineage.spec_digest);
    writer.append(",\"report_digest\":"); writer.quoted(lineage.report_digest);
    writer.append(",\"nodes\":[");
    for (std::size_t index = 0; index < lineage.nodes.size(); ++index) {
        if (index) writer.append(",");
        const auto& node = lineage.nodes[index];
        writer.append("{\"node_id\":"); writer.quoted(node.node_id);
        writer.append(",\"callback_id\":"); writer.quoted(node.callback_id);
        writer.append(",\"request_digest\":"); writer.quoted(node.request_digest);
        writer.append(",\"dependencies\":[");
        for (std::size_t dependency = 0; dependency < node.dependencies.size(); ++dependency) {
            if (dependency) writer.append(",");
            writer.quoted(node.dependencies[dependency]);
        }
        writer.append("]}");
    }
    writer.append("],\"attempts\":[");
    for (std::size_t index = 0; index < lineage.attempts.size(); ++index) {
        if (index) writer.append(",");
        const auto& attempt = lineage.attempts[index];
        writer.append("{\"consumer_node_id\":"); writer.quoted(attempt.consumer_node_id);
        writer.append(",\"consumer_attempt_id\":"); writer.quoted(attempt.consumer_attempt_id);
        writer.append(",\"inputs\":[");
        for (std::size_t input_index = 0; input_index < attempt.inputs.size(); ++input_index) {
            if (input_index) writer.append(",");
            const auto& input = attempt.inputs[input_index];
            writer.append("{\"input_index\":"); writer.number(input.input_index);
            writer.append(",\"producer_node_id\":"); writer.quoted(input.producer_node_id);
            writer.append(",\"producer_attempt_id\":"); writer.quoted(input.producer_attempt_id);
            writer.append(",\"reference\":"); writer.quoted(input.reference);
            writer.append(",\"sha256\":"); writer.quoted(input.sha256);
            writer.append(",\"size_bytes\":"); writer.number(input.size_bytes);
            writer.append("}");
        }
        writer.append("]}");
    }
    writer.append("]}\n");
    return std::move(writer).finish();
}

ExecuteHandoffLineageV1 decode_execute_handoff_lineage_v1(std::string_view bytes) {
    Reader reader(bytes);
    ExecuteHandoffLineageV1 lineage;
    reader.expect('{');
    reader.key("contract");
    if (reader.string() != execute_handoff_lineage_v1_contract) invalid();
    reader.expect(',');
    reader.key("run_id"); lineage.run_id = reader.string(); reader.expect(',');
    reader.key("spec_digest"); lineage.spec_digest = reader.string(); reader.expect(',');
    reader.key("report_digest"); lineage.report_digest = reader.string(); reader.expect(',');
    reader.key("nodes");
    lineage.nodes = array<ExecuteHandoffNodeV1>(reader, execute_handoff_lineage_v1_max_nodes,
        [&] { return parse_node(reader); });
    reader.expect(',');
    reader.key("attempts");
    lineage.attempts = array<ExecuteHandoffAttemptV1>(reader,
        execute_handoff_lineage_v1_max_attempts, [&] { return parse_attempt(reader); });
    reader.expect('}');
    reader.finish();
    validate_value(lineage);
    if (encode_execute_handoff_lineage_v1(lineage) != bytes) invalid();
    return lineage;
}

} // namespace bruce::artifact_v1
