#pragma once
#include "bruce/core/types.hpp"
namespace bruce {
struct Coordinate {
    std::uint64_t x{}, y{};
    bool operator==(const Coordinate&) const = default;
};
std::uint64_t pair(std::uint64_t x, std::uint64_t y);
Coordinate unpair(std::uint64_t z);
std::uint8_t local_pair(std::uint64_t x, std::uint64_t y);
Coordinate local_unpair(std::uint64_t z);
struct GridPath { std::vector<Coordinate> steps; };
GridPath parse_path(std::string_view path);
}
