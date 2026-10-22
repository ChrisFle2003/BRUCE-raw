#include "bruce/artifact/artifact_manifest_v1.hpp"

#include <charconv>
#include <limits>
#include <sstream>

namespace bruce::artifact_v1 {
namespace {
[[noreturn]] void invalid() { throw Error("ArtifactManifestInvalid"); }

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

bool valid_controlled_token(std::string_view value, bool optional) {
    if (optional && value.empty()) return true;
    if (value == "-") return optional;
    if (value.empty() || value.size() > artifact_manifest_token_limit_v1) return false;
    if (value.front() == '/' || value.front() == '\\') return false;
    if (value.find("..") != std::string_view::npos) return false;
    for (const unsigned char c : value) {
        if (ascii_alnum(c) || c == '_' || c == '-' || c == '.' || c == '/' ||
            c == ':' || c == '+' || c == '=') continue;
        return false;
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

void validate(const ArtifactManifestV1& manifest) {
    if (!valid_id(manifest.run_id) || !valid_id(manifest.node_id) ||
        !valid_id(manifest.attempt_id) || !valid_id(manifest.status) ||
        !valid_id(manifest.result_code) ||
        manifest.objects.size() > artifact_manifest_object_limit_v1) invalid();
    for (const auto& object : manifest.objects) {
        if (!valid_digest(object.id.sha256) || object.id.size_bytes > max_file_bytes ||
            !valid_controlled_token(object.media_type, true) ||
            !valid_controlled_token(object.origin, true)) invalid();
    }
}

std::string field(std::string_view key, std::string_view value) {
    return std::string(key) + std::string(value) + "\n";
}

std::uint64_t number(std::string_view value) {
    std::uint64_t result{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        throw Error("ArtifactManifestInvalidNumber");
    return result;
}

std::vector<std::string_view> lines(std::string_view bytes) {
    if (bytes.size() > max_file_bytes || bytes.empty() || bytes.back() != '\n')
        throw Error("ArtifactManifestTrailingBytes");
    std::vector<std::string_view> result;
    std::size_t start = 0;
    while (start < bytes.size()) {
        const auto end = bytes.find('\n', start);
        if (end == std::string_view::npos) throw Error("ArtifactManifestTrailingBytes");
        if (end == start) throw Error("ArtifactManifestTrailingBytes");
        result.push_back(bytes.substr(start, end - start));
        start = end + 1;
    }
    return result;
}

std::string_view expect_field(std::string_view line, std::string_view key) {
    if (!line.starts_with(key)) throw Error("ArtifactManifestUnknownField");
    return line.substr(key.size());
}

} // namespace

std::string encode_manifest_v1(const ArtifactManifestV1& manifest) {
    validate(manifest);
    std::string result;
    result.reserve(256 + manifest.objects.size() * 256);
    result += std::string(artifact_manifest_contract_v1) + "\n";
    result += field("run_id=", manifest.run_id);
    result += field("node_id=", manifest.node_id);
    result += field("attempt_id=", manifest.attempt_id);
    result += field("status=", manifest.status);
    result += field("result_code=", manifest.result_code);
    result += field("object_count=", std::to_string(manifest.objects.size()));
    for (std::size_t index = 0; index < manifest.objects.size(); ++index) {
        const auto& object = manifest.objects[index];
        const auto prefix = "object_" + std::to_string(index) + "_";
        result += field(prefix + "sha256=", object.id.sha256);
        result += field(prefix + "size_bytes=", std::to_string(object.id.size_bytes));
        result += field(prefix + "media_type=", object.media_type.empty() ? "-" : object.media_type);
        result += field(prefix + "origin=", object.origin.empty() ? "-" : object.origin);
    }
    if (result.size() > max_file_bytes) throw Error("ArtifactManifestTooLarge");
    return result;
}

ArtifactManifestV1 decode_manifest_v1(std::string_view bytes) {
    const auto entries = lines(bytes);
    std::size_t cursor = 0;
    if (entries.empty() || entries[cursor++] != artifact_manifest_contract_v1)
        throw Error("ArtifactManifestContractMismatch");
    ArtifactManifestV1 result;
    if (cursor >= entries.size()) throw Error("ArtifactManifestTruncated");
    result.run_id = std::string(expect_field(entries[cursor++], "run_id="));
    if (cursor >= entries.size()) throw Error("ArtifactManifestTruncated");
    result.node_id = std::string(expect_field(entries[cursor++], "node_id="));
    if (cursor >= entries.size()) throw Error("ArtifactManifestTruncated");
    result.attempt_id = std::string(expect_field(entries[cursor++], "attempt_id="));
    if (cursor >= entries.size()) throw Error("ArtifactManifestTruncated");
    result.status = std::string(expect_field(entries[cursor++], "status="));
    if (cursor >= entries.size()) throw Error("ArtifactManifestTruncated");
    result.result_code = std::string(expect_field(entries[cursor++], "result_code="));
    if (cursor >= entries.size()) throw Error("ArtifactManifestTruncated");
    const auto count = number(expect_field(entries[cursor++], "object_count="));
    if (count > artifact_manifest_object_limit_v1) throw Error("ArtifactManifestObjectLimit");
    result.objects.reserve(static_cast<std::size_t>(count));
    for (std::size_t index = 0; index < count; ++index) {
        const auto prefix = "object_" + std::to_string(index) + "_";
        if (cursor + 3 >= entries.size()) throw Error("ArtifactManifestTruncated");
        ArtifactManifestObjectV1 object;
        object.id.sha256 = std::string(expect_field(entries[cursor++], prefix + "sha256="));
        object.id.size_bytes = number(expect_field(entries[cursor++], prefix + "size_bytes="));
        const auto media = expect_field(entries[cursor++], prefix + "media_type=");
        const auto origin = expect_field(entries[cursor++], prefix + "origin=");
        if (media != "-") object.media_type = std::string(media);
        if (origin != "-") object.origin = std::string(origin);
        result.objects.push_back(std::move(object));
    }
    if (cursor != entries.size()) throw Error("ArtifactManifestUnknownField");
    validate(result);
    if (encode_manifest_v1(result) != bytes) throw Error("ArtifactManifestNonCanonical");
    return result;
}

} // namespace bruce::artifact_v1
