#pragma once
#include <charconv>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {
struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

template<class T> T integer(std::string_view text) {
    T value{};
    const auto [end,ec]=std::from_chars(text.data(),text.data()+text.size(),value);
    if (text.empty() || ec!=std::errc{} || end!=text.data()+text.size())
        throw Error("InvalidInteger: " + std::string(text.substr(0,80)));
    return value;
}

struct Sample {
    std::int64_t x{}, y{};
    bool operator==(const Sample&) const = default;
};

struct Budget {
    std::uint64_t candidates=100000;
    std::uint64_t operations=1000000;
    std::uint64_t trace_events=100002;
    std::uint64_t results=128;
};

struct Task {
    std::int64_t minimum=-8, maximum=8;
    std::vector<Sample> training, holdout;
    Budget budget;
    std::optional<std::uint64_t> cancel_after_candidates;
};

inline constexpr std::string_view engine_version="bruce-core-v1";
inline constexpr std::string_view rule_version="affine-integer-v1";
inline constexpr std::string_view policy_version="sample-evidence-v1";
inline constexpr std::size_t max_file_bytes=32*1024*1024;
}
