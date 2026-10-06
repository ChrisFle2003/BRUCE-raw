#include "bruce/application/execution_session.hpp"
namespace bruce {
void SessionUsage::consume(ExecutionPhase phase){
 if(!max_checkpoints)return;
 std::uint64_t* count=nullptr;
 switch(phase){
 case ExecutionPhase::executing:count=&executing;break;
 case ExecutionPhase::serializing:count=&serializing;break;
 case ExecutionPhase::replaying:count=&replaying;break;
 default:throw Error("SessionCheckpointOutsideWorkPhase");
 }
 // Check before increment: UINT64_MAX is a valid limit, never a wrapping counter.
 if(checkpoints>=*max_checkpoints)throw Error("SessionCheckpointBudgetExceeded: phase="+
  std::string(phase_name(phase))+" checkpoints="+std::to_string(checkpoints)+" limit="+std::to_string(*max_checkpoints));
 ++checkpoints;++*count;
}
std::string render_session_usage(const SessionUsage& u){
 if(!u.max_checkpoints)throw Error("SessionBudgetNotEnabled");
 return "{\"model\":\"session-checkpoints-v1\",\"max_checkpoints\":"+std::to_string(*u.max_checkpoints)+
  ",\"checkpoints\":"+std::to_string(u.checkpoints)+",\"by_phase\":{\"EXECUTING\":"+std::to_string(u.executing)+
  ",\"SERIALIZING\":"+std::to_string(u.serializing)+",\"REPLAYING\":"+std::to_string(u.replaying)+"}}";
}
}
