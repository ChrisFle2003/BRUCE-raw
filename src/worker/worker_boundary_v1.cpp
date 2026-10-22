#include "bruce/worker/worker_boundary_v1.hpp"

#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <charconv>
#include <limits>

namespace bruce::worker_v1 {
namespace {
constexpr std::string_view magic = "BRUCE_WORKER_FRAME_V1";
constexpr std::size_t max_header_bytes = 256;

std::string_view type_name(FrameType type) {
    switch (type) {
    case FrameType::REQUEST: return "REQUEST";
    case FrameType::RESULT: return "RESULT";
    case FrameType::STATUS_QUERY: return "STATUS_QUERY";
    case FrameType::STATUS: return "STATUS";
    case FrameType::CANCEL: return "CANCEL";
    case FrameType::ACK: return "ACK";
    case FrameType::EXIT: return "EXIT";
    }
    throw Error("InvalidWorkerFrameType");
}

FrameType parse_type(std::string_view value) {
    for (const auto type : {FrameType::REQUEST, FrameType::RESULT, FrameType::STATUS_QUERY,
                            FrameType::STATUS, FrameType::CANCEL, FrameType::ACK, FrameType::EXIT}) {
        if (type_name(type) == value) return type;
    }
    throw Error("InvalidWorkerFrameType");
}

std::string_view line(std::string_view bytes, std::size_t& cursor) {
    const auto end = bytes.find('\n', cursor);
    if (end == std::string_view::npos || end - cursor > max_header_bytes)
        throw Error("InvalidWorkerFrameHeader");
    const auto value = bytes.substr(cursor, end - cursor);
    cursor = end + 1;
    return value;
}

std::size_t parse_size(std::string_view text) {
    if (text.empty() || (text.size() > 1 && text.front() == '0'))
        throw Error("InvalidWorkerFrameSize");
    for (const char c : text) if (c < '0' || c > '9') throw Error("InvalidWorkerFrameSize");
    std::size_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size())
        throw Error("InvalidWorkerFrameSize");
    return value;
}

bool lowercase_sha256(std::string_view value) {
    if (value.size() != 64) return false;
    for (const char c : value) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}
}

std::string encode_frame(FrameType type, std::string_view payload) {
    (void)type_name(type);
    if (payload.size() > std::numeric_limits<std::size_t>::max() - max_header_bytes)
        throw Error("WorkerFrameTooLarge");
    std::string result;
    const auto digest = bruce::sha256(payload);
    result.reserve(max_header_bytes + payload.size());
    result.append(magic);
    result.append("\ntype=");
    result.append(type_name(type));
    result.append("\npayload-bytes=");
    result.append(std::to_string(payload.size()));
    result.append("\npayload-sha256=");
    result.append(digest);
    result.append("\n\n");
    result.append(payload.data(), payload.size());
    return result;
}

Frame decode_frame(std::string_view bytes, std::size_t max_payload_bytes) {
    std::size_t cursor = 0;
    if (line(bytes, cursor) != magic) throw Error("InvalidWorkerFrameMagic");
    const auto type_header = line(bytes, cursor);
    constexpr std::string_view type_prefix = "type=";
    if (!type_header.starts_with(type_prefix)) throw Error("InvalidWorkerFrameHeader");
    const auto type = parse_type(type_header.substr(type_prefix.size()));

    const auto size_header = line(bytes, cursor);
    constexpr std::string_view size_prefix = "payload-bytes=";
    if (!size_header.starts_with(size_prefix)) throw Error("InvalidWorkerFrameHeader");
    const auto payload_size = parse_size(size_header.substr(size_prefix.size()));
    if (payload_size > max_payload_bytes) throw Error("WorkerFramePayloadLimitExceeded");

    const auto digest_header = line(bytes, cursor);
    constexpr std::string_view digest_prefix = "payload-sha256=";
    if (!digest_header.starts_with(digest_prefix)) throw Error("InvalidWorkerFrameHeader");
    const auto digest = digest_header.substr(digest_prefix.size());
    if (!lowercase_sha256(digest)) throw Error("InvalidWorkerFrameDigest");
    if (cursor >= bytes.size() || bytes[cursor] != '\n') throw Error("InvalidWorkerFrameHeader");
    ++cursor;

    if (payload_size > bytes.size() - cursor || bytes.size() - cursor != payload_size)
        throw Error("InvalidWorkerFrameLength");
    const auto payload = bytes.substr(cursor, payload_size);
    if (bruce::sha256(payload) != digest) throw Error("InvalidWorkerFrameDigest");
    return {type, std::string(payload)};
}

} // namespace bruce::worker_v1
