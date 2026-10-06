#include "bruce/application/research.hpp"
#include "bruce/compatibility/selection.hpp"
#include "bruce/coordinate/address.hpp"
#include "bruce/math/exact.hpp"
#include "bruce/evidence/bundle.hpp"
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool value, const char* label) {
    ++checks;
    if (!value) throw std::runtime_error(label);
}
void rejects(const std::function<void()>& action, const char* label) {
    bool failed = false;
    try { action(); } catch (const bruce::Error&) { failed = true; }
    check(failed, label);
}
}

int main() {
    using namespace bruce;
    try {
        for (std::uint64_t x = 0; x < 64; ++x) {
            for (std::uint64_t y = 0; y < 64; ++y) {
                check(unpair(pair(x,y)) == Coordinate{x,y}, "Cantor roundtrip");
                if (x < 8 && y < 8)
                    check(local_unpair(local_pair(x,y)) == Coordinate{x,y}, "8x8 roundtrip");
            }
        }
        const auto max = std::numeric_limits<std::uint64_t>::max();
        const auto last = unpair(max);
        check(pair(last.x,last.y) == max, "uint64 address boundary");
        rejects([&] { (void)pair(max,max); }, "Cantor overflow");
        rejects([] { (void)local_pair(8,0); }, "local bounds");
        check(pair(2,6) != pair(4,3), "address is not value");
        const auto path = parse_path("0,0/1,2/2,3/3,4/4,5/5,6/6,7/7,0/0,1/1,2/2,3/2,6");
        check(path.steps.size() == 12, "72-bit path supported");
        rejects([] { (void)parse_path("2,6/"); }, "empty path step");
        rejects([] { (void)parse_path("8,1"); }, "invalid path coordinate");
        check(power(2,6,64) == 64 && power(4,3,64) == 64, "equal powers");
        check(power(0,5,64) == 0, "zero power");
        check(power(0,0,64) == 1, "0^0 value one");
        rejects([] { (void)power(2,64,64); }, "power overflow");
        rejects([] { (void)power(2,6,6); }, "bit budget");
        check(modular_power(max,2,max-1) == 1, "wide modular product");
        rejects([] { (void)modular_power(2,6,0); }, "zero modulus");
        check(affine(2,3,10) == 23, "affine exact");
        check(countercheck(2,3,{10,23}), "independent inverse check");
        check(!countercheck(2,3,{10,24}), "nonzero remainder rejected");
        check(countercheck(0,7,{123,7}), "constant candidate");
        rejects([] { (void)affine(INT64_MAX,1,2); }, "affine overflow");
        auto selection = select_candidates({{7,100,0,true},{9,2,1,false},{3,2,1,false}});
        check(selection.ids == std::vector<std::uint64_t>{3,9}, "hard reject and ambiguity");
        check(selection.ambiguous, "stable id does not erase ambiguity");
        check(select_candidates({{1,5,2,false},{2,5,1,false}}).ids == std::vector<std::uint64_t>{1}, "evidence tie-break");
        std::vector<RankedCandidate> many;
        for (std::uint64_t i=0;i<130;++i) many.push_back({i,1,0,false});
        check(select_candidates(many).ids.size()==130, "more than 64 candidates");
        Task task;
        task.training={{0,3},{1,5},{4,11}};
        task.holdout={{2,7},{10,23}};
        auto run=research(task);
        check(run.status=="VERIFIED_ON_SAMPLES", "held-out scope");
        check(run.hypotheses.size()==1 && run.hypotheses[0].a==2 && run.hypotheses[0].b==3, "dynamic formula");
        check(run.candidates==289, "bounded complete search");
        check(render_run(run)==render_run(research(task)), "deterministic trace");
        task.holdout={{10,24}};
        check(research(task).status=="REFUTED", "holdout counterexample");
        task.holdout.clear();
        check(research(task).status=="CANDIDATE", "training fit not verified");
        task.training={{0,3}};
        check(research(task).status=="AMBIGUOUS", "underdetermined model");
        task.training={{0,3},{1,5},{2,100}};
        check(research(task).status=="NO_CANDIDATE", "no candidate in family");
        task.training={{0,3},{1,5}};
        task.holdout={{1,5}};
        rejects([&] { (void)research(task); }, "holdout leakage");
        task.holdout.clear();
        task.budget.candidates=1;
        check(research(task).stop_reason=="CandidateBudgetExceeded", "candidate budget");
        task.budget.candidates=1000;
        task.budget.operations=1;
        check(research(task).status=="INCOMPLETE", "operation budget");
        task.budget.operations=100000;
        task.budget.trace_events=2;
        check(research(task).stop_reason=="TraceBudgetExceeded", "trace budget");
        task.budget.trace_events=1000;
        auto cancelled=research(task,[](std::uint64_t n) { return n>=3; });
        check(cancelled.stop_reason=="Cancelled" && cancelled.candidates==3, "controlled cancellation");
        check(render_run(research(cancelled.task))==render_run(cancelled), "cancel replay");
        check(sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA256 known vector");
        auto packed=pack_run(run);
        check(render_run(replay(packed))==render_run(run), "bundle replay");
        packed.back() = packed.back()=='x' ? 'y' : 'x';
        rejects([&] { (void)replay(packed); }, "tampered bundle");
        std::cout << checks << " checks passed\n";
    } catch(const std::exception& e) {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
