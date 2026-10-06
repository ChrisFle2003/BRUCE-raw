#include "bruce/application/fraction_compare.hpp"
#include <iostream>

namespace {
void require(bool condition,const char* message) {if(!condition) throw bruce::Error(message);}
void replay_equal(const bruce::FractionComparisonRun& run) {
    require(bruce::render_fraction_comparison(bruce::replay_fraction_comparison(bruce::pack_fraction_comparison(run)))==
        bruce::render_fraction_comparison(run),"Witness replay differs");
}
}
int main() {
    using namespace bruce;
    try {
        // Each cutoff catches publishing a partly checked witness, charging after
        // execution, or failing to preserve the exact cancellation in replay.
        for(const std::string denominator:{"-8,0,0,1","-1,0,0,8"}) {
            FractionComparisonTask task;task.domain_policy="modular-witness";
            task.left_numerator=task.left_denominator=denominator;task.right_numerator="1";
            const auto full=compare_fractions(task);replay_equal(full);
            require(full.status=="PROVEN_DIFFERENT","Reference witness missing");
            for(std::uint64_t limit=1;limit<full.operations;++limit) {
                task.max_operations=limit;
                const auto stopped=compare_fractions(task);
                require(stopped.status=="INCOMPLETE" && stopped.stop_reason=="OperationBudgetExceeded" &&
                    stopped.operations==limit,"Witness operation cutoff not exact");
                if(stopped.domains.witness) {
                    require(stopped.domains.relation=="DIFFERENT" && stopped.domains.left_roots=="PRESENT",
                        "Partial witness published");
                } else require(stopped.domains.left_witness_candidates==0,"Partial domain result published");
                replay_equal(stopped);
            }
            task.max_operations=full.operations;
            require(compare_fractions(task).status==full.status,"Exact witness budget failed");
            task.max_operations=1000000;
            for(std::uint64_t cancel=0;cancel<full.operations;++cancel) {
                task.cancel_after_operations=cancel;
                const auto stopped=compare_fractions(task);
                require(stopped.status=="INCOMPLETE" && stopped.stop_reason=="Cancelled" &&
                    stopped.operations==cancel,"Witness cancellation cutoff not exact");
                replay_equal(stopped);
            }
            task.cancel_after_operations.reset();
            const auto stopped=compare_fractions(task,[&](std::uint64_t n){return n==full.operations-3;});
            require(stopped.status=="INCOMPLETE" && stopped.stop_reason=="Cancelled" &&
                stopped.task.cancel_after_operations==full.operations-3,"Witness callback cancellation not recorded");
            replay_equal(stopped);
        }
        // Finite exhaustion cannot establish absence: actual root is 17.
        FractionComparisonTask exhausted;exhausted.domain_policy="modular-witness";
        exhausted.left_numerator=exhausted.left_denominator="-4913,0,0,1";exhausted.right_numerator="1";
        const auto full=compare_fractions(exhausted);
        require(full.status=="UNDECIDED" && full.domains.left_witness_candidates>100 &&
            !full.domains.witness,"Finite exhaustion promoted to proof");replay_equal(full);
        for(const auto limit:{full.operations/2,full.operations-1,full.operations}) {
            exhausted.max_operations=limit;
            const auto run=compare_fractions(exhausted);
            require(run.operations<=limit && run.status==(limit==full.operations ? "UNDECIDED" : "INCOMPLETE"),
                "Exhaustion budget invalid");replay_equal(run);
        }
        // Probe arithmetic must obey the same bit limit as the proof arithmetic.
        FractionComparisonTask bit;bit.domain_policy="modular-witness";bit.max_bits=8;
        bit.left_numerator=bit.left_denominator="-1,0,0,8";bit.right_numerator="1";
        const auto limited=compare_fractions(bit);
        require(limited.status=="INCOMPLETE" && limited.stop_reason=="IntegerBitLimit","Witness bit budget ignored");
        replay_equal(limited);
        bool rejected=false;
        try {(void)compare_rational_domains(NumberDomain("rational",256),{1},{1},[](std::uint64_t){},false,true);}
        catch(const Error&) {rejected=true;}
        require(rejected,"Witness-only unversioned policy accepted");
        std::cout<<"Exact witnesses, finite exhaustion, every cutoff, numeric limits and replay passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
