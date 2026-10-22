#include "bruce/worker/worker_boundary_v1.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/core/types.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>

using namespace bruce::worker_v1;

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

template<class F> void rejects(F&& operation, const char* message) {
    try { operation(); }
    catch (const bruce::Error&) { return; }
    std::cerr << message << '\n';
    std::exit(1);
}

template<class T> concept HasPayloadBytes = requires(T value) { value.payload_bytes; };
}

int main() {
    static_assert(!HasPayloadBytes<WorkerResult>);
    const std::string payload("alpha\0beta\nlast", 15);
    const auto encoded = encode_frame(FrameType::REQUEST, payload);
    const auto decoded = decode_frame(encoded, 64);
    require(decoded.type == FrameType::REQUEST && decoded.payload == payload,
            "embedded NUL/newline round trip failed");
    const std::string prefix = "BRUCE_WORKER_FRAME_V1\ntype=REQUEST\npayload-bytes=15\npayload-sha256=";
    require(encoded.substr(0, prefix.size()) == prefix, "header prefix is not canonical");
    const auto separator = encoded.find("\n\n");
    require(separator != std::string::npos && encoded.size() == separator + 2 + payload.size(),
            "frame does not contain exactly the declared raw payload bytes");
    const auto result = decode_frame(encode_frame(FrameType::RESULT, "ok"), 2);
    require(result.type == FrameType::RESULT && result.payload == "ok", "result round trip failed");

    auto wrong_digest = encoded;
    const auto digest_at = wrong_digest.find("payload-sha256=") + 15;
    wrong_digest[digest_at] = wrong_digest[digest_at] == '0' ? '1' : '0';
    rejects([&] { (void)decode_frame(wrong_digest, 64); }, "wrong digest accepted");
    rejects([&] { (void)decode_frame(encoded.substr(0, encoded.size() - 1), 64); }, "truncated payload accepted");
    rejects([&] { (void)decode_frame("BRUCE_WORKER_FRAME_V1\ntype=NOPE\npayload-bytes=0\npayload-sha256=" + bruce::sha256("") + "\n\n", 64); }, "unknown type accepted");
    auto duplicate = encoded;
    duplicate.insert(duplicate.find("payload-bytes="), "type=REQUEST\n");
    rejects([&] { (void)decode_frame(duplicate, 64); }, "duplicate header accepted");
    rejects([&] { (void)decode_frame(encoded, payload.size() - 1); }, "payload-limit violation accepted");
    auto trailing = encode_frame(FrameType::ACK, "x");
    trailing.push_back('x');
    rejects([&] { (void)decode_frame(trailing, 8); }, "trailing bytes accepted");
    rejects([&] { (void)decode_frame("BRUCE_WORKER_FRAME_V1\ntype=ACK\npayload-bytes=01\npayload-sha256=" + bruce::sha256("x") + "\n\nx", 8); }, "noncanonical decimal accepted");
    std::cout << "worker boundary v1 contracts passed\n";
}
