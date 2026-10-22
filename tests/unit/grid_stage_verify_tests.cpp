#include "bruce/application/grid_stage_verify.hpp"
#include "bruce/application/application_task.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>

namespace {
void require(bool value,const char* message) {if(!value) throw bruce::Error(message);}
template<class F> void rejected(F call,std::string_view reason) {
    try {call();} catch(const bruce::Error& error) {
        require(std::string_view(error.what()).starts_with(reason),"Wrong rejection reason");return;
    }
    throw bruce::Error("Invalid verification accepted");
}
std::string mutate(const std::string& bundle,std::string_view before,std::string_view after) {
    const auto header_end=bundle.find('\n');
    auto body=bundle.substr(header_end+66);
    const auto at=body.find(before);
    require(at!=std::string::npos,"Mutation target missing");body.replace(at,before.size(),after);
    return bundle.substr(0,header_end+1)+bruce::sha256(body)+"\n"+body;
}
}
int main() {
    using namespace bruce;
    try {
        // Catches skipped stages, wrong fixed formula, wrong domain and Metric value comparison.
        GridStageSweepTask source;source.path="0,0";source.to_stage=57;
        const auto sweep=pack_grid_stage_sweep(evaluate_grid_stage_sweep(source));
        const auto run=evaluate_grid_stage_verify({sweep});
        require(run.status=="VERIFIED_ON_DOMAIN" && run.records.size()==58 && !run.first_mismatch,"Exact verification wrong");
        require(run.formula=="100*(2^stage-1)" && run.claim_scope=="finite-uint64-scaled-exponential-stage-domain-0..57","Finite scope wrong");
        require(run.sweep_digest==sha256(sweep) && run.task.sweep_bundle==sweep,"Source not bound exactly");
        require(run.records[0].stage==0 && run.records[0].expected==0 && run.records[0].observed==0,"Stage zero wrong");
        require(run.records[1].expected==100 && run.records[1].observed==100,"Stage one wrong");
        require(run.records[57].stage==57 && run.records[57].expected==14411518807585587100ULL &&
            run.records[57].observed==14411518807585587100ULL,"Stage 57 wrong");
        for(std::size_t i=0;i<58;++i) require(run.records[i].stage==i && run.records[i].matches,"Gap or mismatch in complete domain");
        const auto bundle=pack_grid_stage_verify(run);
        require(bundle.starts_with("BRUCE_GRID_STAGE_VERIFY_BUNDLE_V1\n"),"Verify header wrong");
        require(bundle.find(sweep)!=std::string::npos,"Embedded source bytes missing");
        require(pack_grid_stage_verify(replay_grid_stage_verify(bundle))==bundle,"Exact replay changed bytes");
        // Catches missing generic replay/codec wiring and unbounded proof classification.
        const auto generic=replay_application(bundle);
        require(application_kind(generic)=="grid-stage-verify","Generic verify kind wrong");
        require(pack_application(generic)==bundle && render_application(generic)==render_grid_stage_verify(run),"Generic verify codec wrong");
        require(pack_application(execute_application(GridStageVerifyTask{sweep}))==bundle,"Generic verify execution wrong");
        const auto assessment=assess_application(generic);
        require(assessment.complete && assessment.status=="VERIFIED_ON_DOMAIN" && assessment.stop_reason=="Completed" &&
            !assessment.proof_established,"Verify assessment wrong");
        require(assess_application_bundle(bundle).find("\"claim_scope\":\"finite-uint64-scaled-exponential-stage-domain-0..57\"")!=std::string::npos,
            "Assessment lost finite domain");
        const auto inspection=inspect_application(bundle);
        require(inspection.find("\"kind\":\"grid-stage-verify\"")!=std::string::npos &&
            inspection.find("\"claim_scope\":\"finite-uint64-scaled-exponential-stage-domain-0..57\"")!=std::string::npos,
            "Inspection lost finite domain");
        const auto json=render_grid_stage_verify(run);
        require(json.find("\"checked_entries\":58")!=std::string::npos && json.find("\"verified_domain\":\"0..57\"")!=std::string::npos,"Domain report missing");
        source.mode="metric";source.max_bits=1;
        const auto metric=evaluate_grid_stage_verify({pack_grid_stage_sweep(evaluate_grid_stage_sweep(source))});
        require(metric.status=="VERIFIED_ON_DOMAIN" && metric.records.size()==58 && metric.mode=="metric","Metric verification wrong");
        require(metric.records[0].expected==0 && metric.records[1].expected==7 && metric.records[2].expected==9 &&
            metric.records[57].expected==64 && metric.records[57].observed==64,"Metric not checked by bit length");
        const auto metric_bundle=pack_grid_stage_verify(metric);
        require(pack_grid_stage_verify(replay_grid_stage_verify(metric_bundle))==metric_bundle,"Metric replay changed bytes");

        // Catches accepting invalid sources or returning publishable partial verification results.
        source.mode="modular";source.modulus=128;
        rejected([&]{(void)evaluate_grid_stage_verify({pack_grid_stage_sweep(evaluate_grid_stage_sweep(source))});},"InvalidSweepVerificationSource");
        source.mode="exact";source.modulus.reset();source.max_bits=63;
        rejected([&]{(void)evaluate_grid_stage_verify({pack_grid_stage_sweep(evaluate_grid_stage_sweep(source))});},"InvalidSweepVerificationSource");
        source.max_bits=64;source.from_stage=1;
        rejected([&]{(void)evaluate_grid_stage_verify({pack_grid_stage_sweep(evaluate_grid_stage_sweep(source))});},"InvalidSweepVerificationSource");
        source.from_stage=0;source.to_stage=56;
        rejected([&]{(void)evaluate_grid_stage_verify({pack_grid_stage_sweep(evaluate_grid_stage_sweep(source))});},"InvalidSweepVerificationSource");
        rejected([&]{(void)evaluate_grid_stage_verify({"foreign"});},"InvalidSweepVerificationSource");
        rejected([&]{(void)evaluate_grid_stage_verify({mutate(sweep,"\"value\":\"100\"","\"value\":\"101\"")});},"InvalidSweepVerificationSource");
        const auto invalid_source=mutate(sweep,"\"value\":\"100\"","\"value\":\"101\"");
        rejected([&]{(void)replay_grid_stage_verify(mutate(bundle,sweep,invalid_source));},"ReplayMismatch");
        // A different replay-valid source must also change the bound digest and canonical report.
        GridStageSweepTask alternate;alternate.path="1,0";alternate.to_stage=57;
        const auto other_sweep=pack_grid_stage_sweep(evaluate_grid_stage_sweep(alternate));
        require(other_sweep.size()==sweep.size(),"Alternate source fixture changed size");
        rejected([&]{(void)replay_grid_stage_verify(mutate(bundle,sweep,other_sweep));},"ReplayMismatch");

        // Catches digest-only replay and unbound formula/scope/results/source diagnostics.
        for(const auto& [before,after]:std::vector<std::pair<std::string,std::string>>{
                {"formula 100*(2^stage-1)","formula 100*(2^stage-2)"},
                {"claim_scope finite-uint64-scaled-exponential-stage-domain-0..57","claim_scope unbounded"},
                {"sweep_digest "+run.sweep_digest,"sweep_digest "+std::string(64,'0')},
                {"\"expected\":\"100\"","\"expected\":\"101\""},
                {"\"observed\":\"100\"","\"observed\":\"101\""},
                {"\"matches\":true","\"matches\":false"},
                {"VERIFIED_ON_DOMAIN","REFUTED"},
                {"\"checked_entries\":58","\"checked_entries\":57"},
                {"\"first_mismatch\":null","\"first_mismatch\":1"},
                {"\"value\":\"100\"","\"value\":\"101\""}}) {
            rejected([&]{(void)replay_grid_stage_verify(mutate(bundle,before,after));},"ReplayMismatch");
        }
        auto corrupt=bundle;corrupt.back()='x';
        rejected([&]{(void)replay_grid_stage_verify(corrupt);},"InvalidEvidence");
        // Catches treating control cancellation as an invalid mathematical source.
        int checkpoints=0;
        (void)replay_grid_stage_verify(bundle,[&]{++checkpoints;});
        require(checkpoints>58,"Source and comparison replay did not check control");
        for(const int stop:{1,3,checkpoints-1}) {
            int count=0;
            rejected([&]{(void)replay_grid_stage_verify(bundle,[&]{if(++count==stop) throw Error("VerifyCancelled");});},"VerifyCancelled");
        }
        // A fabricated refutation cannot be made replay-valid by repacking its hash.
        auto fabricated=run;fabricated.status="REFUTED";
        fabricated.records[1].observed=101;fabricated.records[1].matches=false;
        fabricated.first_mismatch=fabricated.records[1];
        require(render_grid_stage_verify(fabricated).find("\"first_mismatch\":{\"stage\":1,\"expected\":\"100\",\"observed\":\"101\"")!=std::string::npos,
            "Defensive first mismatch report missing");
        rejected([&]{(void)replay_grid_stage_verify(pack_grid_stage_verify(fabricated));},"ReplayMismatch");
        std::cout<<"grid stage verify contracts passed\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
