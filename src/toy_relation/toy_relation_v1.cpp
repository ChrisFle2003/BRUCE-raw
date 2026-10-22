#include "bruce/toy_relation/toy_relation_v1.hpp"
#include "bruce/core/types.hpp"

#include <numeric>
#include <utility>

namespace bruce {
namespace {
void validate_group(const ToyCyclicGroupV1& group) {
    if (group.modulus < 3 || group.modulus > 65535)
        throw Error("InvalidToyRelationModulus");
    if (group.generator == 0 || group.generator >= group.modulus ||
        std::gcd(group.generator, group.modulus) != 1)
        throw Error("InvalidToyRelationGenerator");
    if (group.public_q >= group.modulus)
        throw Error("InvalidToyRelationPublicPoint");
}

void validate_coefficients(const ToyCyclicGroupV1& group, std::uint32_t a, std::uint32_t b) {
    if (a >= group.modulus || b >= group.modulus)
        throw Error("InvalidToyRelationCoefficient");
}

std::uint32_t point(const ToyCyclicGroupV1& group, std::uint32_t a, std::uint32_t b) {
    return static_cast<std::uint32_t>((std::uint64_t{a} * group.generator +
                                      std::uint64_t{b} * group.public_q) % group.modulus);
}

void validate_state(const ToyCyclicGroupV1& group, const ToyCoefficientStateV1& state,
                    const char* mismatch_error) {
    validate_coefficients(group, state.a, state.b);
    if (state.canonical_point != point(group, state.a, state.b))
        throw Error(mismatch_error);
}

using Pair = std::pair<const ToyGroupProvenanceV1&, const ToyGroupProvenanceV1&>;
Pair canonical_pair(const ToyGroupProvenanceV1& first, const ToyGroupProvenanceV1& second) {
    if (first.provenance_id.empty() || second.provenance_id.empty() ||
        first.provenance_id == second.provenance_id)
        throw Error("InvalidToyRelationProvenanceId");
    if (first.provenance_id < second.provenance_id) return {first, second};
    return {second, first};
}

std::uint32_t delta(std::uint32_t first, std::uint32_t second, std::uint32_t modulus) {
    return (first + modulus - second) % modulus;
}

std::uint32_t inverse(std::uint32_t coefficient, std::uint32_t modulus) {
    // Extended Euclid tracks the coefficient of the original input. The fixed
    // 16-bit modulus bound keeps signed updates comfortably within int64_t.
    std::int64_t old_r = modulus, r = coefficient;
    std::int64_t old_t = 0, t = 1;
    while (r != 0) {
        const auto quotient = old_r / r;
        const auto next_r = old_r - quotient * r;
        const auto next_t = old_t - quotient * t;
        old_r = r;
        r = next_r;
        old_t = t;
        t = next_t;
    }
    if (old_r != 1) throw Error("NonInvertibleToyRelationCoefficient");
    return static_cast<std::uint32_t>((old_t % modulus + modulus) % modulus);
}
} // namespace

ToyCoefficientStateV1 make_toy_coefficient_state_v1(
    const ToyCyclicGroupV1& group, std::uint32_t a, std::uint32_t b) {
    validate_group(group);
    validate_coefficients(group, a, b);
    return {a, b, point(group, a, b)};
}

ToyCollisionRecordV1 detect_toy_relation_collision_v1(
    const ToyGroupProvenanceV1& first, const ToyGroupProvenanceV1& second) {
    const auto [left, right] = canonical_pair(first, second);
    if (left.coefficient_state.canonical_point != right.coefficient_state.canonical_point)
        throw Error("NoToyRelationCollision");
    return {left.coefficient_state.canonical_point, left.provenance_id, right.provenance_id};
}

ToyRelationRecordV1 derive_toy_relation_v1(
    const ToyCyclicGroupV1& group, const ToyGroupProvenanceV1& first,
    const ToyGroupProvenanceV1& second, const ToyCollisionRecordV1& collision) {
    validate_group(group);
    const auto [left, right] = canonical_pair(first, second);
    validate_state(group, left.coefficient_state, "InvalidToyRelationCoefficient");
    validate_state(group, right.coefficient_state, "InvalidToyRelationCoefficient");
    const auto expected = detect_toy_relation_collision_v1(left, right);
    if (collision.existing_provenance_id != expected.existing_provenance_id ||
        collision.new_provenance_id != expected.new_provenance_id)
        throw Error("InvalidToyRelationProvenanceId");
    if (collision.canonical_point != expected.canonical_point)
        throw Error("NoToyRelationCollision");
    const auto coefficient = delta(left.coefficient_state.b, right.coefficient_state.b, group.modulus);
    const auto value = delta(right.coefficient_state.a, left.coefficient_state.a, group.modulus);
    const auto candidate = static_cast<std::uint32_t>(
        (std::uint64_t{value} * inverse(coefficient, group.modulus)) % group.modulus);
    return {group.modulus, coefficient, value, candidate, ToyRelationVerificationStatusV1::pending};
}

ToyRelationRecordV1 verify_toy_relation_v1(
    const ToyCyclicGroupV1& group, const ToyGroupProvenanceV1& first,
    const ToyGroupProvenanceV1& second, const ToyRelationRecordV1& relation) {
    validate_group(group);
    const auto [left, right] = canonical_pair(first, second);
    constexpr auto rejected = "RejectedToyRelationVerification";
    validate_state(group, left.coefficient_state, rejected);
    validate_state(group, right.coefficient_state, rejected);
    if (relation.modulus != group.modulus || relation.candidate_k >= group.modulus ||
        relation.coefficient_delta >= group.modulus || relation.value_delta >= group.modulus ||
        (relation.verification_status != ToyRelationVerificationStatusV1::pending &&
         relation.verification_status != ToyRelationVerificationStatusV1::verified))
        throw Error(rejected);
    const auto coefficient = delta(left.coefficient_state.b, right.coefficient_state.b, group.modulus);
    const auto value = delta(right.coefficient_state.a, left.coefficient_state.a, group.modulus);
    if (left.coefficient_state.canonical_point != right.coefficient_state.canonical_point ||
        relation.coefficient_delta != coefficient || relation.value_delta != value ||
        std::gcd(coefficient, group.modulus) != 1 ||
        (std::uint64_t{coefficient} * relation.candidate_k) % group.modulus != value)
        throw Error(rejected);
    const auto candidate_q = static_cast<std::uint32_t>(
        (std::uint64_t{relation.candidate_k} * group.generator) % group.modulus);
    if (candidate_q != group.public_q) throw Error(rejected);
    const ToyCyclicGroupV1 candidate_group{group.modulus, group.generator, candidate_q};
    if (point(candidate_group, left.coefficient_state.a, left.coefficient_state.b) !=
            left.coefficient_state.canonical_point ||
        point(candidate_group, right.coefficient_state.a, right.coefficient_state.b) !=
            right.coefficient_state.canonical_point)
        throw Error(rejected);
    auto verified = relation;
    verified.verification_status = ToyRelationVerificationStatusV1::verified;
    return verified;
}
} // namespace bruce
