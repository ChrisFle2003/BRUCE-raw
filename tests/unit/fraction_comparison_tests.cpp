#include "bruce/application/fraction_compare.hpp"
#include <iostream>

namespace {
void require(bool condition,const char* message) {if(!condition) throw bruce::Error(message);}
void replay_equal(const bruce::FractionComparisonRun& run) {
    require(bruce::render_fraction_comparison(bruce::replay_fraction_comparison(bruce::pack_fraction_comparison(run)))==
        bruce::render_fraction_comparison(run),"Comparison replay differs");
}
}
int main() {
    using namespace bruce;
    try {
        const NumberDomain d("rational",256);
        const auto spend=[](std::uint64_t){};
        // Arbitrary rational coefficients, negative scaling and repeated factors
        // must preserve domains; no caller-side normalization is required.
        auto domain=compare_rational_domains(d,{0,-2,0,-2,0},{0,ExactValue(1,3)},spend);
        require(domain.relation=="EQUAL","Common zeros or rootless factor differ");
        domain=compare_rational_domains(d,{ExactValue(-1,2),0,2},{1},spend);
        require(domain.relation=="DIFFERENT" && domain.witness &&
            (*domain.witness==ExactValue(1,2) || *domain.witness==ExactValue(-1,2)),"Rational quadratic witness failed");
        domain=compare_rational_domains(d,{0,0},{1},spend);
        require(domain.relation=="DIFFERENT" && domain.reason=="ONE_EMPTY","Zero polynomial domain failed");
        domain=compare_rational_domains(d,{0,0},{0},spend);
        require(domain.relation=="EQUAL" && domain.reason=="BOTH_EMPTY","Empty domains differ");
        domain=compare_rational_domains(d,{1,0,1},{-2,0,1},spend);
        require(domain.relation=="EQUAL","Irrational roots treated as rational exclusions");
        domain=compare_rational_domains(d,{-2,0,0,1},{1},spend);
        require(domain.relation=="UNKNOWN","Unresolved cubic promoted to proof");
        for(unsigned invalid=0;invalid<4;++invalid) {
            bool rejected=false;
            try {
                if(invalid==0) (void)compare_rational_domains(d,{}, {1},spend);
                if(invalid==1) (void)compare_rational_domains(d,std::vector<ExactValue>(66,0), {1},spend);
                if(invalid==2) (void)compare_rational_domains(NumberDomain("bigint",256),{1},{1},spend);
                if(invalid==3) (void)compare_rational_domains(d,{1},{1},{});
            } catch(const Error&) {rejected=true;}
            require(rejected,"Invalid domain comparison accepted");
        }
        for(const std::string policy:{"quadratic","modular"})
        for(const std::string denominator:{"0,1,0,1","-1,0,1","-2,0,1","-2,0,0,1","0"}) {
            FractionComparisonTask task;task.domain_policy=policy;
            task.left_numerator=task.left_denominator=denominator;
            task.right_numerator=task.right_denominator="1";
            const auto full=compare_fractions(task);replay_equal(full);
            require(full.status!="INCOMPLETE" && full.operations>1,"Reference comparison failed");
            for(std::uint64_t limit=1;limit<full.operations;++limit) {
                task.max_operations=limit;
                const auto stopped=compare_fractions(task);
                require(stopped.status=="INCOMPLETE" && stopped.stop_reason=="OperationBudgetExceeded" &&
                    stopped.operations==limit,"Operation cutoff not exact");
                replay_equal(stopped);
            }
            task.max_operations=full.operations;
            require(compare_fractions(task).status==full.status,"Exact operation budget failed");
            task.max_operations=1000000;
            for(std::uint64_t cancel=0;cancel<full.operations;++cancel) {
                task.cancel_after_operations=cancel;
                const auto stopped=compare_fractions(task);
                require(stopped.status=="INCOMPLETE" && stopped.stop_reason=="Cancelled" &&
                    stopped.operations==cancel,"Cancellation cutoff not exact");
                replay_equal(stopped);
            }
            task.cancel_after_operations.reset();
            const auto stopped=compare_fractions(task,[](std::uint64_t n){return n==7;});
            require(stopped.status=="INCOMPLETE" && stopped.stop_reason=="Cancelled" &&
                stopped.task.cancel_after_operations==7,"Callback cancellation not recorded");
            replay_equal(stopped);
        }
        std::cout<<"Rational-domain certificates, every budget/cancellation boundary and replay passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
