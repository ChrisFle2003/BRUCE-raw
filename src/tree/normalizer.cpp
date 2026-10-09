#include "bruce/tree/normalizer.hpp"
#include "bruce/core/types.hpp"

namespace bruce {
namespace {
bool valid_utf8(std::string_view text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        if (lead < 0x80) { ++i; continue; }
        unsigned count = 0;
        std::uint32_t code = 0;
        if (lead >= 0xc2 && lead <= 0xdf) { count = 2; code = lead & 0x1f; }
        else if (lead >= 0xe0 && lead <= 0xef) { count = 3; code = lead & 0x0f; }
        else if (lead >= 0xf0 && lead <= 0xf4) { count = 4; code = lead & 0x07; }
        else return false;
        if (count > text.size() - i) return false;
        for (unsigned j = 1; j < count; ++j) {
            const auto c = static_cast<unsigned char>(text[i+j]);
            if ((c & 0xc0) != 0x80) return false;
            code = (code << 6) | (c & 0x3f);
        }
        if ((count == 2 && code < 0x80) || (count == 3 && code < 0x800) ||
            (count == 4 && code < 0x10000) || (code >= 0xd800 && code <= 0xdfff) || code > 0x10ffff)
            return false;
        i += count;
    }
    return true;
}
bool whitespace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
}
NormalizedRouteInputV1 normalize_route_text_v1(std::string_view text) {
    if (!valid_utf8(text)) throw Error("InvalidRouteUtf8");
    NormalizedRouteInputV1 result;
    std::string token;
    const auto finish_token = [&] {
        if (token.empty()) return;
        if (!result.normalized_text.empty()) result.normalized_text += ' ';
        result.normalized_text += token;
        result.tokens.push_back(token);
        token.clear();
    };
    for (const unsigned char c : text) {
        if (whitespace(c)) finish_token();
        else token += static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    }
    finish_token();
    return result;
}
}
