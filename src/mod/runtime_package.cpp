#include "bruce/mod/runtime_package.hpp"
#include "bruce/mod/logic_mod_plugin_v1.hpp"
#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include "runtime_package_builder.hpp"
#include <openssl/evp.h>
#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <set>

namespace bruce {
namespace {
constexpr std::string_view package_header = "BRUCE_RUNTIME_DATA_MOD_V1\n";
constexpr std::string_view trust_header = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n";
[[noreturn]] void invalid() { throw Error("InvalidRuntimeModSchema"); }
bool safe_id(std::string_view text) {
    return !text.empty() && text.size() <= 128 && text.front() != '.' &&
        std::all_of(text.begin(), text.end(), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
        });
}
bool hex_string(std::string_view text, std::size_t bytes) {
    return text.size() == bytes * 2 && std::all_of(text.begin(), text.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
std::vector<unsigned char> decode_hex(std::string_view text) {
    std::vector<unsigned char> bytes;
    bytes.reserve(text.size() / 2);
    const auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    for (std::size_t i = 0; i < text.size(); i += 2)
        bytes.push_back(static_cast<unsigned char>((nibble(text[i]) << 4) | nibble(text[i + 1])));
    return bytes;
}
// A bounded, deliberately small JSON subset: objects, arrays, strings, uint32.
// Canonical serialization is checked after parsing, so whitespace, alternate
// escapes/number spellings, unsorted fields and invalid UTF-8 are rejected.
struct Json {
    enum class Kind { object, array, string, number } kind;
    std::map<std::string, Json> object;
    std::vector<Json> array;
    std::string text;
    std::uint32_t number = 0;
    explicit Json(Kind k) : kind(k) {}
};
class JsonParser {
public:
    explicit JsonParser(std::string_view text) : text_(text) {}
    Json parse() {
        auto result = value(0);
        if (offset_ != text_.size()) invalid();
        return result;
    }
private:
    std::string_view text_;
    std::size_t offset_ = 0, values_ = 0;
    char next() { if (offset_ == text_.size()) invalid(); return text_[offset_++]; }
    char peek() const { if (offset_ == text_.size()) invalid(); return text_[offset_]; }
    std::string string() {
        if (next() != '"') invalid();
        std::string out;
        while (true) {
            if (out.size() > 65536) invalid();
            const auto c = static_cast<unsigned char>(next());
            if (c == '"') return out;
            if (c < 32) invalid();
            if (c != '\\') { out += static_cast<char>(c); continue; }
            const auto escaped = next();
            if (escaped == '"' || escaped == '\\') out += escaped;
            else if (escaped == 'u') {
                // json_string emits only lowercase \u00xx for control bytes.
                if (next() != '0' || next() != '0') invalid();
                std::string hex; hex += next(); hex += next();
                if (!hex_string(hex, 1)) invalid();
                const auto control_byte = decode_hex(hex)[0];
                if (control_byte >= 32) invalid();
                out += static_cast<char>(control_byte);
            } else invalid();
        }
    }
    Json value(unsigned depth) {
        if (depth > 32 || ++values_ > 8192) invalid();
        const auto c = peek();
        if (c == '"') { Json result(Json::Kind::string); result.text = string(); return result; }
        if (c == '[') {
            next(); Json result(Json::Kind::array);
            if (peek() == ']') { next(); return result; }
            do {
                result.array.push_back(value(depth + 1));
                const auto delimiter = next();
                if (delimiter == ']') return result;
                if (delimiter != ',') invalid();
            } while (true);
        }
        if (c == '{') {
            next(); Json result(Json::Kind::object);
            if (peek() == '}') { next(); return result; }
            do {
                auto key = string();
                if (next() != ':') invalid();
                auto child = value(depth + 1);
                if (!result.object.emplace(std::move(key), std::move(child)).second) throw Error("DuplicateRuntimeModJsonKey");
                const auto delimiter = next();
                if (delimiter == '}') return result;
                if (delimiter != ',') invalid();
            } while (true);
        }
        if (c < '0' || c > '9') invalid();
        Json result(Json::Kind::number);
        const auto start = offset_;
        while (offset_ < text_.size() && text_[offset_] >= '0' && text_[offset_] <= '9') {
            const auto digit = static_cast<unsigned>(next() - '0');
            if (result.number > (std::numeric_limits<std::uint32_t>::max() - digit) / 10) invalid();
            result.number = result.number * 10 + digit;
        }
        if (offset_ - start > 1 && c == '0') invalid();
        return result;
    }
};
std::string canonical(const Json& value) {
    if (value.kind == Json::Kind::string) return json_string(value.text);
    if (value.kind == Json::Kind::number) return std::to_string(value.number);
    std::string result = value.kind == Json::Kind::array ? "[" : "{";
    bool first = true;
    if (value.kind == Json::Kind::array) {
        for (const auto& child : value.array) { if (!first) result += ','; first = false; result += canonical(child); }
        return result + ']';
    }
    for (const auto& [key, child] : value.object) {
        if (!first) result += ',';
        first = false; result += json_string(key) + ':' + canonical(child);
    }
    return result + '}';
}
Json parse_json(std::string_view bytes) {
    auto result = JsonParser(bytes).parse();
    if (canonical(result) != bytes) throw Error("NoncanonicalRuntimeModJson");
    return result;
}
void fields(const Json& value, std::initializer_list<std::string_view> names) {
    if (value.kind != Json::Kind::object || value.object.size() != names.size()) invalid();
    for (const auto name : names) if (!value.object.contains(std::string(name))) invalid();
}
const Json& get(const Json& object, const char* key) {
    const auto found = object.object.find(key);
    if (found == object.object.end()) invalid();
    return found->second;
}
std::string text(const Json& value) { if (value.kind != Json::Kind::string) invalid(); return value.text; }
std::uint32_t number(const Json& value) { if (value.kind != Json::Kind::number) invalid(); return value.number; }
const std::vector<Json>& array(const Json& value) { if (value.kind != Json::Kind::array) invalid(); return value.array; }
std::vector<std::string> strings(const Json& value) {
    std::vector<std::string> result;
    for (const auto& child : array(value)) { auto item = text(child); if (item.empty() || item.size() > 4096) invalid(); result.push_back(std::move(item)); }
    return result;
}
TreeAddressV1 address(const Json& value, bool allow_root) {
    TreeAddressV1 result;
    for (const auto& child : array(value)) result.push_back(number(child));
    if (result.empty() || result.size() > 32) invalid();
    if (allow_root && result == TreeAddressV1{0}) return result;
    if (std::find(result.begin(), result.end(), 0) != result.end()) invalid();
    return result;
}
HostClaimScopeV1 scope(std::string_view name) {
    constexpr std::array<std::string_view, 8> names = {"unknown", "candidate_only", "verified_within_supported_scope", "verified_exact_result", "explanation_only", "research_only", "requires_review", "denied"};
    const auto found = std::find(names.begin(), names.end(), name);
    if (found == names.end()) invalid();
    return static_cast<HostClaimScopeV1>(found - names.begin());
}
TreeFragmentV1 parse_tree(std::string_view bytes) {
    const auto root = parse_json(bytes);
    fields(root, {"mount", "nodes"});
    TreeFragmentV1 result;
    result.mount = address(get(root, "mount"), true);
    for (const auto& item : array(get(root, "nodes"))) {
        fields(item, {"id", "key", "template_id", "aliases", "claim_scope", "route_pattern"});
        TreeNodeV1 node;
        node.id = address(get(item, "id"), false);
        node.key = text(get(item, "key"));
        node.template_id = text(get(item, "template_id"));
        if (!safe_id(node.key) || (!node.template_id.empty() && !safe_id(node.template_id))) invalid();
        node.aliases = strings(get(item, "aliases"));
        node.claim_scope = scope(text(get(item, "claim_scope")));
        node.route_pattern = strings(get(item, "route_pattern"));
        result.nodes.push_back(std::move(node));
    }
    if (result.nodes.empty()) invalid();
    return result;
}
std::vector<RuntimeModGoldenCaseV1> parse_golden(std::string_view bytes) {
    const auto root = parse_json(bytes);
    std::vector<RuntimeModGoldenCaseV1> result;
    for (const auto& item : array(root)) {
        fields(item, {"input", "status", "template_id"});
        RuntimeModGoldenCaseV1 test;
        test.input = text(get(item, "input"));
        const auto status = text(get(item, "status"));
        if (status == "MATCHED") test.status = RouteStatusV1::matched;
        else if (status == "UNKNOWN") test.status = RouteStatusV1::unknown;
        else if (status == "AMBIGUOUS") test.status = RouteStatusV1::ambiguous;
        else invalid();
        test.template_id = text(get(item, "template_id"));
        if (test.input.size() > 4096 || (test.status == RouteStatusV1::matched ? !safe_id(test.template_id) : !test.template_id.empty())) invalid();
        result.push_back(std::move(test));
    }
    if (result.empty()) invalid();
    return result;
}
RuntimeModManifestV1 parse_manifest(std::string_view bytes) {
    const auto root = parse_json(bytes);
    fields(root, {"schema_version", "host_abi_version", "tree_schema_version", "token_normalization_version", "mod_id", "version", "owner_scope", "signing_key_id", "payload_sha256", "capabilities"});
    RuntimeModManifestV1 result;
    result.schema_version = number(get(root, "schema_version"));
    result.host_abi_version = number(get(root, "host_abi_version"));
    result.tree_schema_version = number(get(root, "tree_schema_version"));
    result.token_normalization_version = number(get(root, "token_normalization_version"));
    if (result.schema_version != 1 || result.host_abi_version != 1 || result.tree_schema_version != 1 || result.token_normalization_version != 1) throw Error("UnsupportedRuntimeModVersion");
    result.mod_id = text(get(root, "mod_id"));
    result.version = text(get(root, "version"));
    result.owner_scope = text(get(root, "owner_scope"));
    result.signing_key_id = text(get(root, "signing_key_id"));
    result.payload_sha256 = text(get(root, "payload_sha256"));
    if (!safe_id(result.mod_id) || result.mod_id == "core" || !safe_id(result.version) || !safe_id(result.signing_key_id) || result.owner_scope != "user" || !hex_string(result.payload_sha256, 32)) invalid();
    result.capabilities = strings(get(root, "capabilities"));
    const auto expected = std::vector<std::string>{"dataset.read", "tree.extend"};
    if (result.capabilities != expected) throw Error("UnsupportedRuntimeModCapability");
    return result;
}
std::string_view take_field(std::string_view bytes, std::size_t& offset) {
    const auto start = offset;
    std::size_t size = 0;
    while (offset < bytes.size() && bytes[offset] >= '0' && bytes[offset] <= '9') {
        const auto digit = static_cast<unsigned>(bytes[offset++] - '0');
        if (size > (max_runtime_mod_package_bytes_v1 - digit) / 10) throw Error("InvalidRuntimeModContainer");
        size = size * 10 + digit;
    }
    if (offset == start || (offset - start > 1 && bytes[start] == '0') || offset == bytes.size() || bytes[offset++] != ':' || size > bytes.size() - offset) throw Error("InvalidRuntimeModContainer");
    const auto result = bytes.substr(offset, size);
    offset += size;
    return result;
}
}
namespace runtime_package_builder_detail {
std::string unsigned_bytes(std::string_view metadata, std::string_view tree, std::string_view golden) {
    // Validate all public source bytes before any output is published.
    auto manifest = parse_json(metadata);
    fields(manifest, {"mod_id", "version", "signing_key_id"});
    (void)parse_tree(tree);
    (void)parse_golden(golden);
    const auto field = [](std::string_view value) { return std::to_string(value.size()) + ':' + std::string(value); };
    const auto member = [&](std::string_view name, std::string_view value) { return field(name) + field(value); };
    const auto payload = member("tree.json", tree) + member("tests/golden-cases.json", golden);
    for (const auto name : {"schema_version", "host_abi_version", "tree_schema_version", "token_normalization_version"}) {
        Json version(Json::Kind::number); version.number = 1;
        manifest.object.emplace(name, std::move(version));
    }
    Json owner(Json::Kind::string); owner.text = "user";
    manifest.object.emplace("owner_scope", std::move(owner));
    Json digest(Json::Kind::string); digest.text = sha256(payload);
    manifest.object.emplace("payload_sha256", std::move(digest));
    Json capabilities(Json::Kind::array);
    for (const auto name : {"dataset.read", "tree.extend"}) {
        Json capability(Json::Kind::string); capability.text = name;
        capabilities.array.push_back(std::move(capability));
    }
    manifest.object.emplace("capabilities", std::move(capabilities));
    const auto manifest_bytes = canonical(manifest);
    (void)parse_manifest(manifest_bytes);
    const auto bytes = std::string(package_header) + member("manifest.json", manifest_bytes) + payload;
    // Reserve enough space for the final canonical signature member and LF.
    if (bytes.size() + member("signature.ed25519", std::string(128, '0')).size() + 1 > max_runtime_mod_package_bytes_v1)
        throw Error("RuntimeModPackageTooLarge");
    return bytes;
}
}
RuntimeDataModPackageV1 unpack_runtime_data_mod_package_v1(std::string_view bytes) {
    if (bytes.size() > max_runtime_mod_package_bytes_v1) throw Error("RuntimeModPackageTooLarge");
    if (!bytes.starts_with(package_header)) throw Error("InvalidRuntimeModContainer");
    std::size_t offset = package_header.size();
    std::array<std::string_view, 4> contents;
    constexpr std::array<std::string_view, 4> names = {"manifest.json", "tree.json", "tests/golden-cases.json", "signature.ed25519"};
    std::size_t payload_start = 0, unsigned_end = 0;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i == 1) payload_start = offset;
        if (i == 3) unsigned_end = offset;
        if (take_field(bytes, offset) != names[i]) throw Error("InvalidRuntimeModMember");
        contents[i] = take_field(bytes, offset);
    }
    if (offset + 1 != bytes.size() || bytes[offset] != '\n') throw Error("UnexpectedRuntimeModMember");
    if (!hex_string(contents[3], 64)) throw Error("InvalidRuntimeModSignatureEncoding");
    RuntimeDataModPackageV1 result;
    result.manifest_ = parse_manifest(contents[0]);
    if (result.manifest_.payload_sha256 != sha256(bytes.substr(payload_start, unsigned_end - payload_start))) throw Error("RuntimeModPayloadDigestMismatch");
    result.bytes_ = bytes;
    result.package_digest_ = sha256(bytes);
    result.unsigned_bytes_ = bytes.substr(0, unsigned_end);
    result.tree_bytes_ = contents[1]; result.golden_bytes_ = contents[2]; result.signature_hex_ = contents[3];
    return result;
}
RuntimeModTrustStoreV1 read_runtime_mod_truststore_v1(std::string_view bytes) {
    if (bytes.size() > 65536 || !bytes.starts_with(trust_header)) throw Error("InvalidRuntimeModTrustStore");
    bytes.remove_prefix(trust_header.size());
    RuntimeModTrustStoreV1 result;
    std::string previous;
    while (!bytes.empty()) {
        const auto newline = bytes.find('\n');
        if (newline == std::string_view::npos) throw Error("InvalidRuntimeModTrustStore");
        const auto line = bytes.substr(0, newline);
        const auto colon = line.find(':');
        if (colon == std::string_view::npos || !safe_id(line.substr(0, colon)) || !hex_string(line.substr(colon + 1), 32)) throw Error("InvalidRuntimeModTrustStore");
        const std::string id(line.substr(0, colon));
        if (id <= previous || !result.public_keys_.emplace(id, line.substr(colon + 1)).second) throw Error("InvalidRuntimeModTrustStore");
        previous = id;
        bytes.remove_prefix(newline + 1);
    }
    return result;
}
void verify_runtime_data_mod_package_v1(const RuntimeDataModPackageV1& package, const RuntimeModTrustStoreV1& trust) {
    const auto found = trust.public_keys().find(package.manifest_.signing_key_id);
    if (found == trust.public_keys().end()) throw Error("RuntimeModSigningKeyUntrusted");
    const auto public_bytes = decode_hex(found->second);
    const auto signature = decode_hex(package.signature_hex_);
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, public_bytes.data(), public_bytes.size()), EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!key || !ctx || EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) != 1) throw Error("RuntimeModCryptoFailure");
    if (EVP_DigestVerify(ctx.get(), signature.data(), signature.size(), reinterpret_cast<const unsigned char*>(package.unsigned_bytes_.data()), package.unsigned_bytes_.size()) != 1) throw Error("RuntimeModSignatureInvalid");
    (void)parse_tree(package.tree_bytes_);
    (void)parse_golden(package.golden_bytes_);
}
TreeFragmentV1 RuntimeDataModPackageV1::tree_fragment(const RuntimeModTrustStoreV1& trust) const {
    verify_runtime_data_mod_package_v1(*this, trust);
    return parse_tree(tree_bytes_);
}
std::vector<RuntimeModGoldenCaseV1> RuntimeDataModPackageV1::golden_cases(const RuntimeModTrustStoreV1& trust) const {
    verify_runtime_data_mod_package_v1(*this, trust);
    return parse_golden(golden_bytes_);
}
LogicModPackageV1 unpack_logic_mod_package_v1(std::string_view bytes) {
    constexpr std::string_view header = "BRUCE_LOGIC_MOD_PLUGIN_V1\n";
    if (bytes.size() > max_runtime_mod_package_bytes_v1) throw Error("RuntimeModPackageTooLarge");
    if (!bytes.starts_with(header)) throw Error("InvalidLogicModContainer");
    std::size_t offset = header.size();
    if (take_field(bytes, offset) != "data.brucemod") throw Error("InvalidLogicModMember");
    LogicModPackageV1 result(unpack_runtime_data_mod_package_v1(take_field(bytes, offset)));
    if (take_field(bytes, offset) != "logic.json") throw Error("InvalidLogicModMember");
    const auto config = parse_json(take_field(bytes, offset));
    fields(config, {"capabilities", "template_ids", "worker_path", "worker_sha256"});
    result.capabilities_ = strings(get(config, "capabilities"));
    if (result.capabilities_ != std::vector<std::string>{"worker.execute"} &&
        result.capabilities_ != std::vector<std::string>{"artifact.input.v1", "worker.execute"} &&
        result.capabilities_ != std::vector<std::string>{"artifact.output.v1", "worker.execute"} &&
        result.capabilities_ != std::vector<std::string>{"artifact.input.v1", "artifact.output.v1", "worker.execute"})
        throw Error("UnsupportedLogicModCapability");
    result.template_ids_ = strings(get(config, "template_ids"));
    if (result.template_ids_.empty() || result.template_ids_.size() > 128) throw Error("InvalidLogicModTemplates");
    std::string previous;
    for (const auto& id : result.template_ids_) {
        if (!safe_id(id) || id <= previous) throw Error("InvalidLogicModTemplates");
        previous = id;
    }
    result.worker_path_ = text(get(config, "worker_path"));
    result.worker_sha256_ = text(get(config, "worker_sha256"));
    // One direct filename, no subdirectories, external paths or reserved members.
    if (!safe_id(result.worker_path_) || result.worker_path_ == "data.brucemod" ||
        result.worker_path_ == "logic.json" || result.worker_path_ == "signature.ed25519" ||
        !hex_string(result.worker_sha256_, 32)) throw Error("InvalidLogicModWorker");
    if (take_field(bytes, offset) != result.worker_path_) throw Error("InvalidLogicModMember");
    result.worker_bytes_ = take_field(bytes, offset);
    if (result.worker_bytes_.empty() || sha256(result.worker_bytes_) != result.worker_sha256_) throw Error("LogicModWorkerDigestMismatch");
    result.unsigned_bytes_ = bytes.substr(0, offset);
    if (take_field(bytes, offset) != "signature.ed25519") throw Error("InvalidLogicModMember");
    result.signature_hex_ = take_field(bytes, offset);
    if (!hex_string(result.signature_hex_, 64)) throw Error("InvalidRuntimeModSignatureEncoding");
    if (offset + 1 != bytes.size() || bytes[offset] != '\n') throw Error("UnexpectedLogicModMember");
    result.bytes_ = bytes;
    result.package_digest_ = sha256(bytes);
    return result;
}
void verify_logic_mod_package_v1(const LogicModPackageV1& package, const RuntimeModTrustStoreV1& trust) {
    const auto found = trust.public_keys().find(package.manifest().signing_key_id);
    if (found == trust.public_keys().end()) throw Error("RuntimeModSigningKeyUntrusted");
    const auto public_bytes = decode_hex(found->second);
    const auto signature = decode_hex(package.signature_hex_);
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, public_bytes.data(), public_bytes.size()), EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!key || !ctx || EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) != 1) throw Error("RuntimeModCryptoFailure");
    if (EVP_DigestVerify(ctx.get(), signature.data(), signature.size(), reinterpret_cast<const unsigned char*>(package.unsigned_bytes_.data()), package.unsigned_bytes_.size()) != 1) throw Error("RuntimeModSignatureInvalid");
    verify_runtime_data_mod_package_v1(package.data_, trust);
    std::set<std::string> templates;
    for (const auto& node : package.data_.tree_fragment(trust).nodes)
        if (!node.template_id.empty()) templates.insert(node.template_id);
    if (std::vector<std::string>(templates.begin(), templates.end()) != package.template_ids_) throw Error("LogicModTemplateMismatch");
}
std::string LogicModPackageV1::worker_bytes(const RuntimeModTrustStoreV1& trust) const {
    verify_logic_mod_package_v1(*this, trust);
    return worker_bytes_;
}
TreeFragmentV1 LogicModPackageV1::tree_fragment(const RuntimeModTrustStoreV1& trust) const {
    verify_logic_mod_package_v1(*this, trust);
    return data_.tree_fragment(trust);
}
std::vector<RuntimeModGoldenCaseV1> LogicModPackageV1::golden_cases(const RuntimeModTrustStoreV1& trust) const {
    verify_logic_mod_package_v1(*this, trust);
    return data_.golden_cases(trust);
}
}
