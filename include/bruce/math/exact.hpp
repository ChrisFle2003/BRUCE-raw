#pragma once
#include "bruce/core/types.hpp"
namespace bruce {
std::uint64_t power(std::uint64_t base, std::uint64_t exponent, unsigned max_bits);
std::uint64_t modular_power(std::uint64_t base, std::uint64_t exponent, std::uint64_t modulus);
std::int64_t affine(std::int64_t a, std::int64_t b, std::int64_t x);
bool countercheck(std::int64_t a, std::int64_t b, Sample sample);
}
