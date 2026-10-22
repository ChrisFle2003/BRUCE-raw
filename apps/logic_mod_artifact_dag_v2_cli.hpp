#pragma once

#include "logic_mod_artifact_chain_v1_cli.hpp"
#include "bruce/mod/logic_mod_artifact_dag_v2.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/tree/normalizer.hpp"
#include <memory>

namespace bruce {
namespace logic_mod_dag_cli_detail {
inline constexpr std::size_t spec_limit = 65536;
[[noreturn]] inline void invalid() { throw Error("LogicModDagSpecInvalid"); }
struct Json {
    enum Kind { object, array, string, number } kind;
    std::map<std::string, Json> fields;
    std::vector<Json> elements;
    std::string text;
    explicit Json(Kind k) : kind(k) {}
};
// This bounded grammar accepts only the types used by the Spec. It decodes
// escapes before duplicate-key checks and rejects invalid UTF-8/surrogates.
class Parser {
    std::string_view bytes;
    std::size_t pos = 0;
    void space() { while (pos < bytes.size() && std::string_view(" \t\r\n").find(bytes[pos]) != std::string_view::npos) ++pos; }
    bool eat(char c) { space(); if (pos < bytes.size() && bytes[pos] == c) { ++pos; return true; } return false; }
    void require(char c) { if (!eat(c)) invalid(); }
    unsigned hex() {
        if (pos + 4 > bytes.size()) invalid();
        unsigned n = 0;
        for (unsigned i = 0; i < 4; ++i) {
            const char c = bytes[pos++]; n *= 16;
            if (c >= '0' && c <= '9') n += c - '0';
            else if (c >= 'a' && c <= 'f') n += c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') n += c - 'A' + 10;
            else invalid();
        }
        return n;
    }
    static void utf8(std::string& out, unsigned cp) {
        if (!cp || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) invalid();
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xc0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 63)); }
        else if (cp < 0x10000) { out += static_cast<char>(0xe0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 63)); out += static_cast<char>(0x80 | (cp & 63)); }
        else { out += static_cast<char>(0xf0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 63)); out += static_cast<char>(0x80 | ((cp >> 6) & 63)); out += static_cast<char>(0x80 | (cp & 63)); }
    }
    std::string text() {
        require('"'); std::string out;
        while (pos < bytes.size()) {
            unsigned char c = bytes[pos++];
            if (c == '"') return out;
            if (c < 32) invalid();
            if (c == '\\') {
                if (pos == bytes.size()) invalid();
                switch (bytes[pos++]) {
                case '"': out += '"'; break; case '\\': out += '\\'; break; case '/': out += '/'; break;
                case 'b': out += '\b'; break; case 'f': out += '\f'; break; case 'n': out += '\n'; break;
                case 'r': out += '\r'; break; case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = hex();
                    if (cp >= 0xd800 && cp <= 0xdbff) {
                        if (pos + 2 > bytes.size() || bytes.substr(pos, 2) != "\\u") invalid();
                        pos += 2; const unsigned low = hex();
                        if (low < 0xdc00 || low > 0xdfff) invalid();
                        cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
                    }
                    utf8(out, cp); break;
                }
                default: invalid();
                }
            } else if (c < 0x80) out += static_cast<char>(c);
            else {
                unsigned cp = 0, count = 0, minimum = 0;
                if (c >= 0xc2 && c <= 0xdf) { cp = c & 31; count = 1; minimum = 0x80; }
                else if (c >= 0xe0 && c <= 0xef) { cp = c & 15; count = 2; minimum = 0x800; }
                else if (c >= 0xf0 && c <= 0xf4) { cp = c & 7; count = 3; minimum = 0x10000; }
                else invalid();
                for (unsigned i = 0; i < count; ++i) {
                    if (pos == bytes.size()) invalid();
                    const unsigned char next = bytes[pos++]; if ((next & 0xc0) != 0x80) invalid();
                    cp = (cp << 6) | (next & 63);
                }
                if (cp < minimum) invalid();
                utf8(out, cp);
            }
        }
        invalid();
    }
    Json value(unsigned depth) {
        if (depth > 6) invalid();
        space(); if (pos == bytes.size()) invalid();
        if (bytes[pos] == '"') { Json out(Json::string); out.text = text(); return out; }
        if (eat('{')) {
            Json out(Json::object);
            if (eat('}')) return out;
            do { auto key = text(); require(':'); auto entry = value(depth + 1);
                if (!out.fields.emplace(std::move(key), std::move(entry)).second) invalid();
            } while (eat(','));
            require('}'); return out;
        }
        if (eat('[')) {
            Json out(Json::array);
            if (eat(']')) return out;
            do { if (out.elements.size() >= 16) invalid(); out.elements.push_back(value(depth + 1)); } while (eat(','));
            require(']'); return out;
        }
        const auto start = pos;
        while (pos < bytes.size() && bytes[pos] >= '0' && bytes[pos] <= '9') ++pos;
        if (pos == start || (pos - start > 1 && bytes[start] == '0')) invalid();
        Json out(Json::number); out.text = bytes.substr(start, pos - start); return out;
    }
public:
    explicit Parser(std::string_view source) : bytes(source) { if (source.size() > spec_limit) invalid(); }
    Json parse() { auto out = value(0); space(); if (pos != bytes.size()) invalid(); return out; }
};
inline const Json& field(const Json& o, std::string_view key) {
    if (o.kind != Json::object || !o.fields.contains(std::string(key))) invalid();
    return o.fields.at(std::string(key));
}
inline void keys(const Json& o, std::initializer_list<std::string_view> required, std::initializer_list<std::string_view> optional = {}) {
    if (o.kind != Json::object) invalid();
    for (auto k : required) if (!o.fields.contains(std::string(k))) invalid();
    for (const auto& [k, v] : o.fields) {
        (void)v;
        if (std::find(required.begin(), required.end(), k) == required.end() &&
            std::find(optional.begin(), optional.end(), k) == optional.end()) invalid();
    }
}
inline std::string text(const Json& j) { if (j.kind != Json::string) invalid(); return j.text; }
inline std::uint32_t natural(const Json& j) {
    if (j.kind != Json::number) invalid();
    std::uint32_t out{}; const auto r = std::from_chars(j.text.data(), j.text.data() + j.text.size(), out);
    if (r.ec != std::errc{} || r.ptr != j.text.data() + j.text.size()) invalid();
    return out;
}
inline std::string canonical(const Json& j) {
    if (j.kind == Json::string) return json_string(j.text);
    if (j.kind == Json::number) return j.text;
    std::string out = j.kind == Json::object ? "{" : "[";
    bool first = true;
    if (j.kind == Json::object) for (const auto& [k, v] : j.fields) {
        if (!first) out += ',';
        first = false; out += json_string(k) + ':' + canonical(v);
    } else for (const auto& v : j.elements) {
        if (!first) out += ',';
        first = false; out += canonical(v);
    }
    return out + (j.kind == Json::object ? '}' : ']');
}
struct Node {
    std::string id, package, trust, input, output;
    std::vector<std::string> predecessors;
    std::optional<std::string> internal;
    LogicModBudgetsV1 budgets;
};
struct Spec { std::vector<Node> nodes; std::string digest; };
inline Spec parse_spec(std::string_view bytes) {
    auto json = Parser(bytes).parse(); keys(json, {"schema_version", "nodes"});
    if (natural(field(json, "schema_version")) != 2) invalid();
    auto& array = json.fields.at("nodes");
    if (array.kind != Json::array || array.elements.empty()) invalid();
    Spec out; std::set<std::string> ids, manifests;
    for (auto& value : array.elements) {
        keys(value, {"id", "package", "trust_store", "input", "predecessors", "output_manifest"}, {"input_manifest", "budgets"});
        Node n; n.id = text(field(value, "id")); n.package = text(field(value, "package")); n.trust = text(field(value, "trust_store"));
        n.input = text(field(value, "input")); n.output = text(field(value, "output_manifest"));
        if (n.id.empty() || n.id.size() > 64 || n.id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos || !ids.insert(n.id).second) invalid();
        for (const auto& path : {n.package, n.trust}) if (path.empty() || path.find_first_not_of(" \t\r\n\v\f") == std::string::npos || path.size() > 4096) invalid();
        const auto& parents = field(value, "predecessors");
        if (parents.kind != Json::array || parents.elements.size() > 15) invalid();
        for (const auto& p : parents.elements) n.predecessors.push_back(text(p));
        if (!std::is_sorted(n.predecessors.begin(), n.predecessors.end()) || std::adjacent_find(n.predecessors.begin(), n.predecessors.end()) != n.predecessors.end()) invalid();
        if (value.fields.contains("input_manifest")) n.internal = text(field(value, "input_manifest"));
        if (n.internal.has_value() != !n.predecessors.empty()) invalid();
        for (const auto* m : {&n.output, n.internal ? &*n.internal : nullptr})
            if (m && (!artifact_v1::is_valid_manifest_id_v1(*m) || !manifests.insert(*m).second)) invalid();
        if (value.fields.contains("budgets")) {
            const auto& b = field(value, "budgets"); keys(b, {"wall_ms", "memory_bytes", "input_bytes", "output_bytes"});
            n.budgets = {natural(field(b, "wall_ms")), natural(field(b, "memory_bytes")), natural(field(b, "input_bytes")), natural(field(b, "output_bytes"))};
        }
        const auto b = n.budgets;
        if (!b.wall_ms || b.wall_ms > 5000 || b.memory_bytes < 8*1024*1024 || b.memory_bytes > 256*1024*1024 || !b.input_bytes || b.input_bytes > 4096 || !b.output_bytes || b.output_bytes > 65536 || n.input.size() > b.input_bytes) invalid();
        n.input = normalize_route_text_v1(n.input).normalized_text;
        value.fields.at("input").text = n.input;
        // Explicit defaults and omitted defaults bind identically.
        Json budgets(Json::object);
        for (const auto& [key, number] : std::initializer_list<std::pair<std::string, std::uint32_t>>{{"wall_ms", b.wall_ms}, {"memory_bytes", b.memory_bytes}, {"input_bytes", b.input_bytes}, {"output_bytes", b.output_bytes}}) {
            Json v(Json::number); v.text = std::to_string(number); budgets.fields.emplace(key, std::move(v));
        }
        value.fields.insert_or_assign("budgets", std::move(budgets)); out.nodes.push_back(std::move(n));
    }
    // Full structure before any package, truststore, record or Store access.
    std::set<std::string> done;
    for (const auto& n : out.nodes) for (const auto& p : n.predecessors) if (p == n.id || !ids.contains(p)) invalid();
    while (done.size() < out.nodes.size()) {
        const auto before = done.size();
        for (const auto& n : out.nodes) if (std::all_of(n.predecessors.begin(), n.predecessors.end(), [&](const auto& p) { return done.contains(p); })) done.insert(n.id);
        if (done.size() == before) invalid();
    }
    std::sort(array.elements.begin(), array.elements.end(), [](const auto& a, const auto& b) { return text(field(a, "id")) < text(field(b, "id")); });
    out.digest = sha256("BRUCE_LOGIC_MOD_DAG_SPEC_V2\n" + canonical(json)); return out;
}
struct Arguments { bool replay; std::string spec, store, record; };
inline Arguments parse(std::span<const std::string_view> args) {
    const auto bad = [] { throw Error("InvalidLogicModDagArguments"); };
    if (args.size() != 6) bad();
    for (const auto value : args) if (value.empty() || value.find_first_not_of(" \t\r\n\v\f") == std::string_view::npos) bad();
    if ((args[0] != "run" && args[0] != "replay") || args[2] != "--store" || args[4] != "--record") bad();
    for (const auto i : {1u, 3u, 5u}) if (args[i].starts_with("--")) bad();
    return {args[0] == "replay", std::string(args[1]), std::string(args[3]), std::string(args[5])};
}
inline void publish_record(const std::filesystem::path& path, std::string_view bytes) {
    using namespace logic_mod_chain_cli_detail;
    if (bytes.size() > logic_mod_artifact_dag_record_limit_v2) file_error();
    Target target(path); std::array<unsigned char, 16> random{}; std::size_t offset = 0;
    while (offset < random.size()) {
        const auto n = ::getrandom(random.data() + offset, random.size() - offset, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) file_error();
        offset += static_cast<std::size_t>(n);
    }
    std::string temporary = ".bruce-dag-";
    for (auto c : random) { temporary += "0123456789abcdef"[c >> 4]; temporary += "0123456789abcdef"[c & 15]; }
    temporary += ".tmp";
    Descriptor fd(::openat(target.parent.fd, temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
    try {
        offset = 0;
        while (offset < bytes.size()) {
            const auto n = ::write(fd.fd, bytes.data() + offset, bytes.size() - offset);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) file_error();
            offset += static_cast<std::size_t>(n);
        }
        if (::fsync(fd.fd) != 0) file_error();
        const auto closed = ::close(fd.fd); fd.fd = -1;
        if (closed || ::syscall(SYS_renameat2, target.parent.fd, temporary.c_str(), target.parent.fd, target.name.c_str(), RENAME_NOREPLACE) != 0) file_error();
        if (::fsync(target.parent.fd) != 0) file_error();
    } catch (...) { ::unlinkat(target.parent.fd, temporary.c_str(), 0); throw; }
}
inline std::string output_json(const LogicModArtifactDagRunV2& run, bool replay) {
    std::string out = "{\"contract\":\"logic-mod-artifact-dag-v2\",\"action\":" + json_string(replay ? "replay" : "run") +
        ",\"accepted\":" + (run.accepted ? "true" : "false") + ",\"status\":" + json_string(run.status) +
        ",\"dag_id\":" + (run.dag_id.empty() ? "null" : json_string(run.dag_id)) + ",\"spec_digest\":" + json_string(run.spec_digest) + ",\"node_order\":[";
    bool first = true;
    for (const auto& id : run.node_order) { if (!first) out += ','; first = false; out += json_string(id); }
    out += "],\"nodes\":["; first = true;
    for (const auto& n : run.nodes) {
        if (!first) out += ',';
        first = false;
        out += "{\"id\":" + json_string(n.id) + ",\"result_artifact\":" + logic_mod_chain_cli_detail::output_json(n.result_artifact) +
            ",\"internal_input_artifact\":" + (n.internal_input_artifact ? logic_mod_chain_cli_detail::output_json(*n.internal_input_artifact) : "null") + '}';
    }
    return out + "]}\n";
}
}
inline int logic_mod_artifact_dag_v2_cli(int argc, char** argv) {
    using namespace logic_mod_dag_cli_detail;
    std::vector<std::string_view> args; for (int i = 2; i < argc; ++i) args.emplace_back(argv[i]);
    const auto options = parse(args);
    try {
        const auto spec = parse_spec(logic_mod_chain_cli_detail::read_file(options.spec, spec_limit));
        struct Session { logic_mod_cli_detail::PackageDirectory directory; LogicModRegistryV1 registry; };
        std::vector<std::unique_ptr<Session>> sessions;
        std::vector<LogicModDagNodeV2> nodes;
        for (const auto& n : spec.nodes) {
            const auto trust = read_runtime_mod_truststore_v1(logic_mod_chain_cli_detail::read_file(n.trust, 65536));
            const auto package = unpack_logic_mod_package_v1(logic_mod_chain_cli_detail::read_file(n.package, 1024*1024));
            verify_logic_mod_package_v1(package, trust);
            auto session = std::make_unique<Session>();
            logic_mod_chain_cli_detail::activate(session->registry, package, trust, session->directory);
            nodes.push_back({n.id, session->registry, package.manifest().mod_id, package.manifest().version, "math.rational.power.v1", n.input, n.predecessors,
                n.internal ? std::optional<LogicModOutputV1>{{options.store, *n.internal}} : std::nullopt, {options.store, n.output}, n.budgets});
            sessions.push_back(std::move(session));
        }
        LogicModArtifactDagRunV2 run;
        if (options.replay) {
            run = read_logic_mod_artifact_dag_record_v2(logic_mod_chain_cli_detail::read_file(options.record, logic_mod_artifact_dag_record_limit_v2, true));
            verify_logic_mod_artifact_dag_replay_v2(nodes, options.store, spec.digest, run);
        } else {
            run = run_logic_mod_artifact_dag_v2(nodes, options.store, spec.digest);
            if (run.accepted) publish_record(options.record, logic_mod_artifact_dag_record_v2(run));
        }
        std::cout << output_json(run, options.replay); return run.accepted ? 0 : 2;
    } catch (...) { throw Error(options.replay ? "DAG_REPLAY_MISMATCH" : "LogicModDagFailed"); }
}
}
