#include "bruce/toy_relation/toy_relation_v1.hpp"
#include "bruce/core/types.hpp"

#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace bruce;
using Status = ToyRelationVerificationStatusV1;
const ToyCyclicGroupV1 group{101, 1, 17};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class Action>
void rejected(Action action, const char* code) {
    try { action(); }
    catch (const Error& error) {
        require(std::string(error.what()) == code, "wrong controlled error code");
        return;
    }
    throw std::runtime_error("invalid input accepted");
}

ToyGroupProvenanceV1 left() { return {"left", make_toy_coefficient_state_v1(group, 2, 3)}; }
ToyGroupProvenanceV1 right() { return {"right", make_toy_coefficient_state_v1(group, 36, 1)}; }

std::string projection(const ToyCollisionRecordV1& c, const ToyRelationRecordV1& r) {
    std::ostringstream out;
    out << c.canonical_point << '|' << c.existing_provenance_id << '|'
        << c.new_provenance_id << '|' << r.modulus << '|'
        << r.coefficient_delta << '|' << r.value_delta << '|'
        << r.candidate_k << '|' << static_cast<int>(r.verification_status);
    return out.str();
}

void positive_pending_then_verified() {
    const auto l = left(), r = right();
    require(l.coefficient_state.canonical_point == 53 && r.coefficient_state.canonical_point == 53,
            "positive reference point");
    const auto collision = detect_toy_relation_collision_v1(l, r);
    auto relation = derive_toy_relation_v1(group, l, r, collision);
    require(projection(collision, relation) == "53|left|right|101|2|34|17|0",
            "reference relation must be pending");
    const auto verified = verify_toy_relation_v1(group, l, r, relation);
    require(projection(collision, verified) == "53|left|right|101|2|34|17|1",
            "reference relation must verify independently");
    require(relation.verification_status == Status::pending, "verifier mutated input");
    require(verify_toy_relation_v1(group, l, r, verified).verification_status == Status::verified,
            "verified records must be independently recheckable");
}

void repeated_and_swapped_inputs() {
    const auto l = left(), r = right();
    for (int i = 0; i < 5; ++i) {
        const auto c = detect_toy_relation_collision_v1(r, l);
        const auto relation = derive_toy_relation_v1(group, r, l, c);
        require(projection(c, relation) == "53|left|right|101|2|34|17|0", "swapped/repeated output changed");
        require(projection(c, verify_toy_relation_v1(group, r, l, relation)) ==
                "53|left|right|101|2|34|17|1", "swapped verification changed");
    }
    // Reverse the association of IDs and states: signs must normalize modulo n.
    const ToyGroupProvenanceV1 first{"a", r.coefficient_state}, second{"z", l.coefficient_state};
    const auto c = detect_toy_relation_collision_v1(second, first);
    require(projection(c, derive_toy_relation_v1(group, second, first, c)) ==
            "53|a|z|101|99|67|17|0", "negative deltas did not normalize");
}

void noninvertible_has_no_partial_relation() {
    const ToyCyclicGroupV1 g{15, 1, 4};
    const ToyGroupProvenanceV1 l{"left", make_toy_coefficient_state_v1(g, 0, 5)},
                               r{"right", make_toy_coefficient_state_v1(g, 5, 0)};
    const auto c = detect_toy_relation_collision_v1(l, r);
    require(c.canonical_point == 5, "noninvertible collision missing");
    ToyRelationRecordV1 sentinel{101, 2, 34, 17, Status::pending};
    rejected([&] { sentinel = derive_toy_relation_v1(g, l, r, c); }, "NonInvertibleToyRelationCoefficient");
    require(sentinel.modulus == 101 && sentinel.candidate_k == 17 && sentinel.coefficient_delta == 2 &&
            sentinel.value_delta == 34 && sentinel.verification_status == Status::pending,
            "failure exposed a partial relation");
    const ToyGroupProvenanceV1 duplicate_state{"other", l.coefficient_state};
    const auto zero = detect_toy_relation_collision_v1(l, duplicate_state);
    rejected([&] { derive_toy_relation_v1(g, l, duplicate_state, zero); }, "NonInvertibleToyRelationCoefficient");
}

void invalid_group_parameters() {
    for (const auto n : {0u, 1u, 2u, 65536u, std::numeric_limits<std::uint32_t>::max()}) {
        rejected([&] { make_toy_coefficient_state_v1({n, 0, 0}, 0, 0); }, "InvalidToyRelationModulus");
    }
    for (const auto g : {0u, 101u, 102u, std::numeric_limits<std::uint32_t>::max()}) {
        rejected([&] { make_toy_coefficient_state_v1({101, g, 17}, 0, 0); }, "InvalidToyRelationGenerator");
    }
    rejected([] { make_toy_coefficient_state_v1({15, 5, 0}, 0, 0); }, "InvalidToyRelationGenerator");
    rejected([] { make_toy_coefficient_state_v1({15, 3, 0}, 0, 0); }, "InvalidToyRelationGenerator");
    for (const auto q : {101u, 102u, std::numeric_limits<std::uint32_t>::max()}) {
        rejected([&] { make_toy_coefficient_state_v1({101, 1, q}, 0, 0); }, "InvalidToyRelationPublicPoint");
    }
}

void invalid_coefficients_and_canonical_points() {
    for (const auto value : {101u, std::numeric_limits<std::uint32_t>::max()}) {
        rejected([&] { make_toy_coefficient_state_v1(group, value, 0); }, "InvalidToyRelationCoefficient");
        rejected([&] { make_toy_coefficient_state_v1(group, 0, value); }, "InvalidToyRelationCoefficient");
    }
    const auto l = left(), r = right();
    const auto c = detect_toy_relation_collision_v1(l, r);
    for (int field = 0; field < 3; ++field) {
        auto bad = l;
        if (field == 0) bad.coefficient_state.a = 101;
        if (field == 1) bad.coefficient_state.b = 101;
        if (field == 2) bad.coefficient_state.canonical_point = 54;
        rejected([&] { derive_toy_relation_v1(group, bad, r, c); }, "InvalidToyRelationCoefficient");
    }
}

void invalid_ids_and_different_points() {
    const auto l = left(), r = right();
    for (auto bad : std::vector<ToyGroupProvenanceV1>{{"", l.coefficient_state}, {"right", l.coefficient_state}}) {
        rejected([&] { detect_toy_relation_collision_v1(bad, r); }, "InvalidToyRelationProvenanceId");
        rejected([&] { derive_toy_relation_v1(group, bad, r, {53, "left", "right"}); }, "InvalidToyRelationProvenanceId");
        rejected([&] { verify_toy_relation_v1(group, bad, r, {101, 2, 34, 17}); }, "InvalidToyRelationProvenanceId");
    }
    const ToyGroupProvenanceV1 different{"right", make_toy_coefficient_state_v1(group, 0, 0)};
    rejected([&] { detect_toy_relation_collision_v1(l, different); }, "NoToyRelationCollision");
    rejected([&] { derive_toy_relation_v1(group, l, different, {53, "left", "right"}); }, "NoToyRelationCollision");
}

void tampered_collision_record() {
    const auto l = left(), r = right();
    for (const auto point : {54u, 101u, std::numeric_limits<std::uint32_t>::max()}) {
        rejected([&] { derive_toy_relation_v1(group, l, r, {point, "left", "right"}); }, "NoToyRelationCollision");
    }
    for (const auto& ids : std::vector<std::pair<std::string, std::string>>{
            {"right", "left"}, {"left", "unknown"}, {"", "right"}, {"left", "left"}}) {
        rejected([&] { derive_toy_relation_v1(group, l, r, {53, ids.first, ids.second}); },
                 "InvalidToyRelationProvenanceId");
    }
}

void verifier_rejects_tampered_relation() {
    const auto l = left(), r = right();
    const ToyRelationRecordV1 good{101, 2, 34, 17, Status::pending};
    std::vector<ToyRelationRecordV1> bad{
        {101, 2, 34, 18}, {101, 2, 34, 118}, {100, 2, 34, 17},
        {101, 3, 34, 17}, {101, 103, 34, 17}, {101, 2, 35, 17},
        {101, 2, 135, 17}, {101, 2, 34, 17, static_cast<Status>(99)}};
    for (const auto& record : bad) {
        rejected([&] { verify_toy_relation_v1(group, l, r, record); }, "RejectedToyRelationVerification");
    }
    auto bad_point = l;
    bad_point.coefficient_state.canonical_point = 54;
    rejected([&] { verify_toy_relation_v1(group, bad_point, r, good); }, "RejectedToyRelationVerification");
    auto bad_a = l;
    bad_a.coefficient_state.a = 101;
    rejected([&] { verify_toy_relation_v1(group, bad_a, r, good); }, "InvalidToyRelationCoefficient");
    const ToyGroupProvenanceV1 different{"right", make_toy_coefficient_state_v1(group, 0, 0)};
    rejected([&] { verify_toy_relation_v1(group, l, different, good); }, "RejectedToyRelationVerification");
    const ToyCyclicGroupV1 noninv{15, 1, 4};
    const ToyGroupProvenanceV1 nl{"left", {0, 5, 5}}, nr{"right", {5, 0, 5}};
    rejected([&] { verify_toy_relation_v1(noninv, nl, nr, {15, 5, 5, 4}); }, "RejectedToyRelationVerification");
}

void validation_precedes_derivation_and_verification() {
    const auto l = left(), r = right();
    const ToyCollisionRecordV1 c{53, "left", "right"};
    const ToyRelationRecordV1 relation{101, 2, 34, 17};
    const std::vector<std::pair<ToyCyclicGroupV1, const char*>> cases{
        {{0, 0, 0}, "InvalidToyRelationModulus"},
        {{15, 5, 0}, "InvalidToyRelationGenerator"},
        {{101, 1, 101}, "InvalidToyRelationPublicPoint"}};
    for (const auto& [g, code] : cases) {
        rejected([&] { derive_toy_relation_v1(g, l, r, c); }, code);
        rejected([&] { verify_toy_relation_v1(g, l, r, relation); }, code);
    }
}

void limits_zero_q_and_nonunit_generator() {
    require(make_toy_coefficient_state_v1({3, 2, 0}, 2, 2).canonical_point == 1, "lower limit or zero Q rejected");
    require(make_toy_coefficient_state_v1({65535, 65534, 65534}, 65534, 65534).canonical_point == 2,
            "upper-limit products overflowed");
    const ToyCyclicGroupV1 g{101, 3, 51};
    const ToyGroupProvenanceV1 l{"left", make_toy_coefficient_state_v1(g, 2, 3)},
                               r{"right", make_toy_coefficient_state_v1(g, 36, 1)};
    const auto c = detect_toy_relation_collision_v1(l, r);
    require(c.canonical_point == 58, "nonunit generator point");
    require(verify_toy_relation_v1(g, l, r, derive_toy_relation_v1(g, l, r, c)).candidate_k == 17,
            "nonunit generator derivation");
    const ToyCyclicGroupV1 z{15, 2, 0};
    const ToyGroupProvenanceV1 zl{"left", {0, 1, 0}}, zr{"right", {0, 0, 0}};
    require(verify_toy_relation_v1(z, zl, zr, derive_toy_relation_v1(z, zl, zr,
            detect_toy_relation_collision_v1(zl, zr))).candidate_k == 0, "zero candidate invalid");
}

void modular_inverse_reference_grid() {
    // Hand-constructed collision (0,d) vs (d*k mod n,0) gives the known k.
    for (std::uint32_t n = 3; n <= 40; ++n) {
        for (std::uint32_t d = 1; d < n; ++d) {
            const ToyCyclicGroupV1 g{n, 1, n - 1};
            const ToyGroupProvenanceV1 l{"left", make_toy_coefficient_state_v1(g, 0, d)},
                r{"right", make_toy_coefficient_state_v1(g, n - d, 0)};
            const auto c = detect_toy_relation_collision_v1(l, r);
            if (std::gcd(d, n) != 1) {
                rejected([&] { derive_toy_relation_v1(g, l, r, c); }, "NonInvertibleToyRelationCoefficient");
            } else {
                const auto relation = derive_toy_relation_v1(g, l, r, c);
                require(relation.candidate_k == n - 1, "modular inverse did not recover known candidate");
                require(verify_toy_relation_v1(g, l, r, relation).verification_status == Status::verified,
                        "grid verification");
            }
        }
    }
    const ToyCyclicGroupV1 g{65535, 65534, 1};
    const ToyGroupProvenanceV1 l{"left", make_toy_coefficient_state_v1(g, 0, 65534)},
                               r{"right", make_toy_coefficient_state_v1(g, 1, 0)};
    const auto relation = derive_toy_relation_v1(g, l, r, detect_toy_relation_collision_v1(l, r));
    require(relation.candidate_k == 65534, "upper-limit inverse candidate");
    require(verify_toy_relation_v1(g, l, r, relation).verification_status == Status::verified,
            "upper-limit inverse verification");
}
} // namespace

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> cases{
        {"positive_pending_then_verified", positive_pending_then_verified},
        {"repeated_and_swapped_inputs", repeated_and_swapped_inputs},
        {"noninvertible_has_no_partial_relation", noninvertible_has_no_partial_relation},
        {"invalid_group_parameters", invalid_group_parameters},
        {"invalid_coefficients_and_canonical_points", invalid_coefficients_and_canonical_points},
        {"invalid_ids_and_different_points", invalid_ids_and_different_points},
        {"tampered_collision_record", tampered_collision_record},
        {"verifier_rejects_tampered_relation", verifier_rejects_tampered_relation},
        {"validation_precedes_derivation_and_verification", validation_precedes_derivation_and_verification},
        {"limits_zero_q_and_nonunit_generator", limits_zero_q_and_nonunit_generator},
        {"modular_inverse_reference_grid", modular_inverse_reference_grid}};
    std::size_t passed = 0;
    for (const auto& [name, action] : cases) {
        try { action(); ++passed; }
        catch (const std::exception& error) { std::cerr << name << ": " << error.what() << '\n'; return 1; }
    }
    std::cout << passed << " toy relation V1 contracts passed\n";
}
