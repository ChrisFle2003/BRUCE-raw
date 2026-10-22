#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace bruce {
inline constexpr std::size_t max_release_signature_json_v1 = 4096;
inline constexpr std::size_t max_release_public_key_json_v1 = 4096;

struct ReleaseSignatureEnvelopeV1 {
    std::string contract;
    std::string version;
    std::string archive;
    std::string archive_sha256;
    std::string key_id;
    std::string signature_ed25519;
    bool operator==(const ReleaseSignatureEnvelopeV1&) const = default;
};
struct ReleasePublicKeyV1 {
    std::string contract;
    std::string key_id;
    std::string public_key_ed25519;
    bool operator==(const ReleasePublicKeyV1&) const = default;
};

std::string render_release_signature_v1(const ReleaseSignatureEnvelopeV1&);
ReleaseSignatureEnvelopeV1 parse_release_signature_v1(std::string_view);
std::string render_release_public_key_v1(const ReleasePublicKeyV1&);
ReleasePublicKeyV1 parse_release_public_key_v1(std::string_view);
std::string release_key_id_v1(std::string_view public_key_hex);
std::string release_signature_preimage_v1(const ReleaseSignatureEnvelopeV1&);
ReleaseSignatureEnvelopeV1 sign_release_signature_v1(
    const ReleaseSignatureEnvelopeV1&,
    const std::array<unsigned char, 32>& private_seed,
    const ReleasePublicKeyV1& independently_pinned_key);
void verify_release_signature_v1(
    const ReleaseSignatureEnvelopeV1&,
    const ReleasePublicKeyV1& bundled_key,
    const ReleasePublicKeyV1& independently_pinned_key);
}
