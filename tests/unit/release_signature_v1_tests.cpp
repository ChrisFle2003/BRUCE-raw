#include "bruce/release/release_signature_v1.hpp"
#include "bruce/core/types.hpp"
#include <array>
#include <iostream>
#include <string>
#include <string_view>

namespace {
void require(bool value, const char* reason) { if (!value) throw bruce::Error(reason); }
template<class F> void rejected(F action) {
    try { action(); } catch (const bruce::Error&) { return; }
    throw bruce::Error("Invalid release contract accepted");
}
// RFC 8032 example seed, conspicuously NOT a production BRUCE release key.
constexpr std::array<unsigned char, 32> non_production_seed{
    0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,
    0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
const std::string digest = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
const std::string key_id = "21fe31dfa154a261626bf854046fd2271b7bed4b6abe45aa58877ef47f9721b9";
const std::string public_hex = "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";
const std::string other_public_hex = "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c";
const std::string other_key_id = "39f713d0a644253f04529421b9f51b9b08979d08295959c4f3990ee617f5139f";
const std::string signature = "67c85de43cb5aad8c936a4dcd298e27114094e08d1d37106883330541ed0b84ae13586f1a1302043f43365c412a9da4f3a0352a75d4316f69c459f925dee1c0c";
const std::string expected_preimage = "BRUCE_RELEASE_SIGNATURE_V1\ncontract=bruce-release-signature-v1\nversion=0.102.0\narchive=bruce-0.102.0-linux.tar.gz\narchive_sha256=" + digest + "\nkey_id=" + key_id + "\n";
const std::string expected_envelope = "{\"archive\":\"bruce-0.102.0-linux.tar.gz\",\"archive_sha256\":\"" + digest + "\",\"contract\":\"bruce-release-signature-v1\",\"key_id\":\"" + key_id + "\",\"signature_ed25519\":\"" + signature + "\",\"version\":\"0.102.0\"}\n";
const std::string expected_key = "{\"contract\":\"bruce-release-key-v1\",\"key_id\":\"" + key_id + "\",\"public_key_ed25519\":\"" + public_hex + "\"}\n";
bruce::ReleaseSignatureEnvelopeV1 envelope() {
    return {"bruce-release-signature-v1", "0.102.0", "bruce-0.102.0-linux.tar.gz", digest, key_id, signature};
}
bruce::ReleasePublicKeyV1 pinned() {
    return {"bruce-release-key-v1", key_id, public_hex};
}
void contracts() {
    const auto value = envelope();
    const auto anchor = pinned();
    require(bruce::render_release_signature_v1(value) == expected_envelope, "Envelope canonical bytes/order/LF changed");
    require(bruce::render_release_public_key_v1(anchor) == expected_key, "Public key canonical bytes/order/LF changed");
    require(bruce::parse_release_signature_v1(expected_envelope) == value, "Envelope parse failed");
    require(bruce::parse_release_public_key_v1(expected_key) == anchor, "Key parse failed");
    require(bruce::render_release_signature_v1(bruce::parse_release_signature_v1(expected_envelope)) == expected_envelope, "Envelope identity failed");
    require(bruce::render_release_public_key_v1(bruce::parse_release_public_key_v1(expected_key)) == expected_key, "Key identity failed");
    require(bruce::release_signature_preimage_v1(value) == expected_preimage, "Preimage contract changed");
    require(bruce::release_key_id_v1(public_hex) == key_id, "Raw public-key fingerprint changed");
    require(bruce::sign_release_signature_v1(value, non_production_seed, anchor).signature_ed25519 == signature, "Independent Ed25519 vector failed");
    bruce::verify_release_signature_v1(value, anchor, anchor);

    // Each mutation below names a wrong acceptance branch the test must catch.
    for (const auto& bad : {std::string("../archive.tar.gz"), std::string("a/b.tar.gz"), std::string(".hidden"), std::string("a\\b.tar.gz"), std::string("a..b.tar.gz"), std::string(256, 'a')}) {
        auto e = value; e.archive = bad; rejected([&] { (void)bruce::render_release_signature_v1(e); });
    }
    for (const auto& bad : {std::string(), std::string(65, 'v'), std::string("0.102.0\nattack"), std::string("/0.102.0")}) {
        auto e = value; e.version = bad; rejected([&] { (void)bruce::render_release_signature_v1(e); });
    }
    for (const auto& bad : {std::string(63, 'a'), std::string(65, 'a'), std::string(64, 'A')}) {
        auto e = value; e.archive_sha256 = bad; rejected([&] { (void)bruce::render_release_signature_v1(e); });
        e = value; e.key_id = bad; rejected([&] { (void)bruce::render_release_signature_v1(e); });
        auto k = anchor; k.key_id = bad; rejected([&] { (void)bruce::render_release_public_key_v1(k); });
    }
    for (const auto& bad : {std::string(127, 'a'), std::string(129, 'a'), std::string(128, 'A')}) {
        auto e = value; e.signature_ed25519 = bad; rejected([&] { (void)bruce::render_release_signature_v1(e); });
    }
    auto replace = [](std::string text, std::string_view from, std::string_view to) {
        const auto at = text.find(from); require(at != std::string::npos, "Bad test fixture"); text.replace(at, from.size(), to); return text;
    };
    rejected([&] { (void)bruce::parse_release_signature_v1(replace(expected_envelope, "\"archive\":", "\"unknown\":")); });
    rejected([&] { (void)bruce::parse_release_signature_v1(replace(expected_envelope, "\"archive_sha256\":", "\"archive\":")); });
    rejected([&] { (void)bruce::parse_release_signature_v1(replace(expected_envelope, ",\"version\":", ",\"extra\":\"x\",\"version\":")); });
    rejected([&] { (void)bruce::parse_release_signature_v1(replace(expected_envelope, ",\"version\":\"0.102.0\"", "")); });
    rejected([&] { (void)bruce::parse_release_signature_v1(expected_envelope.substr(0, expected_envelope.size() - 1)); });
    rejected([&] { (void)bruce::parse_release_signature_v1(" " + expected_envelope); });
    rejected([&] { (void)bruce::parse_release_signature_v1(std::string(4097, 'x')); });
    rejected([&] { (void)bruce::parse_release_public_key_v1(replace(expected_key, "\"public_key_ed25519\":", "\"key_id\":")); });
    rejected([&] { (void)bruce::parse_release_public_key_v1(replace(expected_key, ",\"public_key_ed25519\":", ",\"unknown\":\"x\",\"public_key_ed25519\":")); });
    rejected([&] { (void)bruce::parse_release_public_key_v1(replace(expected_key, ",\"public_key_ed25519\":\"" + public_hex + "\"", "")); });
    rejected([&] { (void)bruce::parse_release_public_key_v1(" " + expected_key); });
    rejected([&] { (void)bruce::parse_release_public_key_v1(std::string(4097, 'x')); });
    for (const auto& bad : {std::string(63, 'a'), std::string(65, 'a'), std::string(64, 'A')}) {
        auto k = anchor; k.public_key_ed25519 = bad; rejected([&] { (void)bruce::render_release_public_key_v1(k); });
    }
    auto bad_anchor = anchor; bad_anchor.key_id = std::string(64, '0');
    rejected([&] { bruce::verify_release_signature_v1(value, bad_anchor, anchor); });
    bad_anchor = anchor; bad_anchor.public_key_ed25519 = std::string(64, '0');
    rejected([&] { bruce::verify_release_signature_v1(value, bad_anchor, anchor); });
    rejected([&] { bruce::verify_release_signature_v1(value, anchor, bad_anchor); });
    rejected([&] { (void)bruce::sign_release_signature_v1(value, non_production_seed, bad_anchor); });
    const bruce::ReleasePublicKeyV1 other_valid_anchor{"bruce-release-key-v1", other_key_id, other_public_hex};
    require(bruce::parse_release_public_key_v1(bruce::render_release_public_key_v1(other_valid_anchor)) == other_valid_anchor,
        "Other independently pinned key invalid");
    rejected([&] { bruce::verify_release_signature_v1(value, other_valid_anchor, anchor); });
    rejected([&] { bruce::verify_release_signature_v1(value, anchor, other_valid_anchor); });
    rejected([&] { (void)bruce::sign_release_signature_v1(value, non_production_seed, other_valid_anchor); });
    for (int mutation = 0; mutation < 5; ++mutation) {
        auto e = value;
        if (mutation == 0) e.version = "0.103.0";
        if (mutation == 1) e.archive = "bruce-0.102.0-other.tar.gz";
        if (mutation == 2) e.archive_sha256 = std::string(64, '0');
        if (mutation == 3) e.key_id = std::string(64, '0');
        if (mutation == 4) e.signature_ed25519[0] = '0';
        rejected([&] { bruce::verify_release_signature_v1(e, anchor, anchor); });
    }
}
}
int main(int argc, char**) {
    try {
        if (argc == 2) { std::cout << bruce::render_release_signature_v1(envelope()) << bruce::render_release_public_key_v1(pinned()); return 0; }
        contracts(); std::cout << "release signature v1 contracts passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
