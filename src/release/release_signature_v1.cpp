#include "bruce/release/release_signature_v1.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {
namespace {
constexpr std::string_view signature_contract = "bruce-release-signature-v1";
constexpr std::string_view key_contract = "bruce-release-key-v1";
[[noreturn]] void invalid() { throw Error("InvalidReleaseSignatureV1"); }
[[noreturn]] void untrusted() { throw Error("ReleaseKeyUntrusted"); }
bool lowercase_hex(std::string_view value, std::size_t digits) {
    return value.size() == digits && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
bool safe_version(std::string_view value) {
    return !value.empty() && value.size() <= 64 &&
        ((value.front() >= '0' && value.front() <= '9') ||
         (value.front() >= 'A' && value.front() <= 'Z') ||
         (value.front() >= 'a' && value.front() <= 'z')) &&
        std::all_of(value.begin(), value.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                (c >= 'a' && c <= 'z') || c == '.' || c == '-' || c == '_' || c == '+';
        }) && value.find("..") == std::string_view::npos;
}
bool safe_archive(std::string_view value) {
    return !value.empty() && value.size() <= 255 &&
        ((value.front() >= '0' && value.front() <= '9') ||
         (value.front() >= 'A' && value.front() <= 'Z') ||
         (value.front() >= 'a' && value.front() <= 'z')) &&
        std::all_of(value.begin(), value.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                (c >= 'a' && c <= 'z') || c == '.' || c == '-' || c == '_';
        }) && value.find("..") == std::string_view::npos;
}
std::vector<unsigned char> decode_hex(std::string_view value) {
    std::vector<unsigned char> bytes;
    bytes.reserve(value.size() / 2);
    const auto nibble = [](char c) -> unsigned { return c <= '9' ? static_cast<unsigned>(c - '0') : static_cast<unsigned>(c - 'a' + 10); };
    for (std::size_t i = 0; i < value.size(); i += 2)
        bytes.push_back(static_cast<unsigned char>((nibble(value[i]) << 4) | nibble(value[i + 1])));
    return bytes;
}
std::string encode_hex(const unsigned char* data, std::size_t size) {
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        result += hex[data[i] >> 4]; result += hex[data[i] & 15];
    }
    return result;
}
void validate(const ReleaseSignatureEnvelopeV1& value) {
    if (value.contract != signature_contract || !safe_version(value.version) ||
        !safe_archive(value.archive) || !lowercase_hex(value.archive_sha256, 64) ||
        !lowercase_hex(value.key_id, 64) || !lowercase_hex(value.signature_ed25519, 128)) invalid();
}
void validate(const ReleasePublicKeyV1& value) {
    if (value.contract != key_contract || !lowercase_hex(value.key_id, 64) ||
        !lowercase_hex(value.public_key_ed25519, 64) ||
        value.key_id != release_key_id_v1(value.public_key_ed25519)) invalid();
}
class CanonicalFields {
public:
    explicit CanonicalFields(std::string_view value) : value_(value) {}
    std::string take(std::string_view name, bool first) {
        const std::string prefix = std::string(first ? "{\"" : ",\"") + std::string(name) + "\":\"";
        if (!value_.substr(offset_).starts_with(prefix)) invalid();
        offset_ += prefix.size();
        const auto end = value_.find('"', offset_);
        if (end == std::string_view::npos) invalid();
        const auto result = value_.substr(offset_, end - offset_);
        if (result.find('\\') != std::string_view::npos) invalid();
        offset_ = end + 1;
        return std::string(result);
    }
    void finish() const { if (value_.substr(offset_) != "}\n") invalid(); }
private:
    std::string_view value_;
    std::size_t offset_ = 0;
};
bool equal_hex(std::string_view left, std::string_view right) {
    return left.size() == right.size() &&
        CRYPTO_memcmp(left.data(), right.data(), left.size()) == 0;
}
}
std::string release_key_id_v1(std::string_view public_key_hex) {
    if (!lowercase_hex(public_key_hex, 64)) invalid();
    const auto bytes = decode_hex(public_key_hex);
    return sha256(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}
std::string render_release_signature_v1(const ReleaseSignatureEnvelopeV1& value) {
    validate(value);
    const auto bytes = "{\"archive\":\"" + value.archive + "\",\"archive_sha256\":\"" + value.archive_sha256 +
        "\",\"contract\":\"" + value.contract + "\",\"key_id\":\"" + value.key_id +
        "\",\"signature_ed25519\":\"" + value.signature_ed25519 + "\",\"version\":\"" + value.version + "\"}\n";
    if (bytes.size() > max_release_signature_json_v1) invalid();
    return bytes;
}
ReleaseSignatureEnvelopeV1 parse_release_signature_v1(std::string_view bytes) {
    if (bytes.size() > max_release_signature_json_v1) invalid();
    CanonicalFields fields(bytes);
    ReleaseSignatureEnvelopeV1 result;
    result.archive = fields.take("archive", true);
    result.archive_sha256 = fields.take("archive_sha256", false);
    result.contract = fields.take("contract", false);
    result.key_id = fields.take("key_id", false);
    result.signature_ed25519 = fields.take("signature_ed25519", false);
    result.version = fields.take("version", false);
    fields.finish();
    if (render_release_signature_v1(result) != bytes) invalid();
    return result;
}
std::string render_release_public_key_v1(const ReleasePublicKeyV1& value) {
    validate(value);
    const auto bytes = "{\"contract\":\"" + value.contract + "\",\"key_id\":\"" + value.key_id +
        "\",\"public_key_ed25519\":\"" + value.public_key_ed25519 + "\"}\n";
    if (bytes.size() > max_release_public_key_json_v1) invalid();
    return bytes;
}
ReleasePublicKeyV1 parse_release_public_key_v1(std::string_view bytes) {
    if (bytes.size() > max_release_public_key_json_v1) invalid();
    CanonicalFields fields(bytes);
    ReleasePublicKeyV1 result;
    result.contract = fields.take("contract", true);
    result.key_id = fields.take("key_id", false);
    result.public_key_ed25519 = fields.take("public_key_ed25519", false);
    fields.finish();
    if (render_release_public_key_v1(result) != bytes) invalid();
    return result;
}
std::string release_signature_preimage_v1(const ReleaseSignatureEnvelopeV1& value) {
    validate(value);
    return "BRUCE_RELEASE_SIGNATURE_V1\ncontract=" + value.contract + "\nversion=" + value.version +
        "\narchive=" + value.archive + "\narchive_sha256=" + value.archive_sha256 +
        "\nkey_id=" + value.key_id + "\n";
}
ReleaseSignatureEnvelopeV1 sign_release_signature_v1(const ReleaseSignatureEnvelopeV1& value,
    const std::array<unsigned char, 32>& private_seed, const ReleasePublicKeyV1& pinned_key) {
    validate(value); validate(pinned_key);
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, private_seed.data(), private_seed.size()), EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    std::array<unsigned char, 32> public_bytes{};
    std::size_t public_size = public_bytes.size();
    if (!key || !context || EVP_PKEY_get_raw_public_key(key.get(), public_bytes.data(), &public_size) != 1 ||
        public_size != public_bytes.size()) throw Error("ReleaseCryptoFailure");
    const auto derived_hex = encode_hex(public_bytes.data(), public_bytes.size());
    if (!equal_hex(derived_hex, pinned_key.public_key_ed25519) || !equal_hex(value.key_id, pinned_key.key_id)) untrusted();
    const auto message = release_signature_preimage_v1(value);
    std::array<unsigned char, 64> signed_bytes{};
    std::size_t signed_size = signed_bytes.size();
    if (EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1 ||
        EVP_DigestSign(context.get(), signed_bytes.data(), &signed_size,
            reinterpret_cast<const unsigned char*>(message.data()), message.size()) != 1 ||
        signed_size != signed_bytes.size()) throw Error("ReleaseCryptoFailure");
    auto result = value;
    result.signature_ed25519 = encode_hex(signed_bytes.data(), signed_bytes.size());
    return result;
}
void verify_release_signature_v1(const ReleaseSignatureEnvelopeV1& value,
    const ReleasePublicKeyV1& bundled_key, const ReleasePublicKeyV1& pinned_key) {
    validate(value); validate(bundled_key); validate(pinned_key);
    if (!equal_hex(bundled_key.public_key_ed25519, pinned_key.public_key_ed25519) ||
        !equal_hex(bundled_key.key_id, pinned_key.key_id) ||
        !equal_hex(value.key_id, pinned_key.key_id)) untrusted();
    const auto public_bytes = decode_hex(pinned_key.public_key_ed25519);
    const auto signature = decode_hex(value.signature_ed25519);
    const auto message = release_signature_preimage_v1(value);
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, public_bytes.data(), public_bytes.size()), EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!key || !context || EVP_DigestVerifyInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1)
        throw Error("ReleaseCryptoFailure");
    if (EVP_DigestVerify(context.get(), signature.data(), signature.size(),
        reinterpret_cast<const unsigned char*>(message.data()), message.size()) != 1)
        throw Error("ReleaseSignatureInvalid");
}
}
