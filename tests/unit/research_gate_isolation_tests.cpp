#include "bruce/application/research_gates.hpp"
int main() {
    using namespace bruce;
    const auto allowed=research_gates({"VERIFIED_ON_SAMPLES","Completed",true,true,true,true});
    const auto denied=research_gates({"INCOMPLETE","Cancelled",true,true,true,true});
    return allowed.selection && allowed.experience && !allowed.learn_promotion && !allowed.publish &&
        !allowed.general_proof && !denied.selection && !denied.experience &&
        research_gate_reason_name(denied.selection_reason)=="StatusNotCompleted" ? 0 : 1;
}
