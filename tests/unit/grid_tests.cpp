#include "bruce/application/grid.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>

namespace {
void require(bool value,const char* message) {if(!value) throw bruce::Error(message);}
template<class F> void rejected(F call) {
    bool failed=false;try {call();} catch(const bruce::Error&) {failed=true;}
    require(failed,"Invalid grid evidence or task accepted");
}
}
int main() {
    using namespace bruce;
    try {
        GridTask t;t.path="2,6/4,3";
        const auto exact=evaluate_grid(t);
        require(exact.value==64 && exact.path.steps.size()==2 && !exact.stopped,"Wrong leaf evaluation");
        const auto bundle=pack_grid(exact);
        require(render_grid(replay_grid(bundle))==render_grid(exact),"Grid replay differs");
        for(std::size_t n=0;n<bundle.size();++n) rejected([&]{(void)replay_grid(std::string_view(bundle).substr(0,n));});
        const auto digest_end=bundle.find('\n',bundle.find('\n')+1);
        const auto body=bundle.substr(digest_end+1);
        for(std::size_t n=0;n<body.size();++n) {
            const auto shortened=body.substr(0,n);
            rejected([&]{(void)replay_grid("BRUCE_GRID_BUNDLE_V1\n"+sha256(shortened)+"\n"+shortened);});
        }
        t.mode="modular";rejected([&]{(void)evaluate_grid(t);});
        t.modulus=0;rejected([&]{(void)evaluate_grid(t);});
        t.modulus=UINT64_MAX;require(evaluate_grid(t).value==64,"Maximum modulus corrupted");
        t.mode="exact";rejected([&]{(void)evaluate_grid(t);});
        t.modulus.reset();t.max_bits=6;rejected([&]{(void)evaluate_grid(t);});
        t.mode="metric";require(evaluate_grid(t).value==64,"Metric inherited exact bit cutoff");
        t.max_bits=65;rejected([&]{(void)evaluate_grid(t);});
        t.max_bits=64;t.path="0,0";t.zero_zero="stop";
        const auto stopped=evaluate_grid(t);
        require(stopped.stopped && stopped.trace.size()==4,"Sentinel did not stop");
        require(render_grid(replay_grid(pack_grid(stopped)))==render_grid(stopped),"Stopped replay differs");
        t.path="0,0/2,6";require(!evaluate_grid(t).stopped,"Ancestor sentinel stopped leaf-local query");
        t.path=std::string(max_file_bytes+1,'0');rejected([&]{(void)evaluate_grid(t);});
        rejected([&]{(void)replay_grid(std::string(max_file_bytes+1,'0'));});
        std::cout<<"Grid application, bounded input, all truncations and canonical replay passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
