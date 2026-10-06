#include "bruce/application/execution_session.hpp"
namespace bruce {
std::string_view phase_name(ExecutionPhase p){
 switch(p){case ExecutionPhase::created:return "CREATED";case ExecutionPhase::executing:return "EXECUTING";
 case ExecutionPhase::serializing:return "SERIALIZING";case ExecutionPhase::replaying:return "REPLAYING";
 case ExecutionPhase::ready:return "READY";case ExecutionPhase::failed:return "FAILED";}
 throw Error("UnknownExecutionPhase");
}
void ExecutionPhases::advance(ExecutionPhase next){
 const bool legal=(current_==ExecutionPhase::created && next==ExecutionPhase::executing) ||
 (current_==ExecutionPhase::executing && next==ExecutionPhase::serializing) ||
 (current_==ExecutionPhase::serializing && next==ExecutionPhase::replaying) ||
 (current_==ExecutionPhase::replaying && next==ExecutionPhase::ready) ||
 ((current_==ExecutionPhase::executing || current_==ExecutionPhase::serializing || current_==ExecutionPhase::replaying) && next==ExecutionPhase::failed);
 if(!legal) throw Error("InvalidExecutionTransition");
 history_.push_back(next);current_=next;
}
ExecutionSessionError::ExecutionSessionError(ExecutionPhases phases,std::string cause,SessionUsage usage,SerializationUsage serialization)
 :Error("ExecutionSessionFailed at "+std::string(phase_name(phases.current()))+": "+cause),
 failed_at_(phases.current()),phases_(std::move(phases)),cause_(std::move(cause)),session_usage_(usage),serialization_usage_(serialization){
 phases_.advance(ExecutionPhase::failed);
}
ExecutionSession execute_checked_application(ApplicationTask task,const ExecutionControl& control,VerificationBudget budget,VerificationControl verification,SessionBudget session_budget,SerializationBudget serialization_budget){
 ExecutionPhases phases;
 SessionUsage usage{session_budget.max_checkpoints};
 SerializationUsage serialization_usage{serialization_budget.max_serialized_bytes};
 const auto work=[&]{usage.consume(phases.current());};
 const auto cancelled=[&]{if(verification.cancelled && verification.cancelled())throw Error("VerificationCancelled");};
 const auto checkpoint=[&]{work();cancelled();};
 try{
  phases.advance(ExecutionPhase::executing);
  if(static_cast<unsigned>(verification.grid_replay_checkpoints)+static_cast<unsigned>(verification.research_replay_checkpoints)+
     static_cast<unsigned>(verification.comparison_replay_checkpoints)+static_cast<unsigned>(verification.compatibility_replay_checkpoints)>1)throw Error("ConflictingReplayCheckpoints");
  if(verification.compatibility_replay_checkpoints){
   if(!verification.cancelled)throw Error("CompatibilityReplayCheckpointsRequireCallback");
   if(!std::holds_alternative<CompatibilityTask>(task))throw Error("CompatibilityReplayCheckpointsRequireCompatibilityTask");
  }
  if(verification.comparison_replay_checkpoints){
   if(!verification.cancelled)throw Error("ComparisonReplayCheckpointsRequireCallback");
   if(!std::holds_alternative<FractionComparisonTask>(task))throw Error("ComparisonReplayCheckpointsRequireComparisonTask");
  }
  if(verification.research_replay_checkpoints){
   if(!verification.cancelled)throw Error("ResearchReplayCheckpointsRequireCallback");
   if(!std::holds_alternative<Task>(task) && !std::holds_alternative<ExactResearchTask>(task) && !std::holds_alternative<GridResearchTask>(task))
    throw Error("ResearchReplayCheckpointsRequireResearchTask");
  }
  if(verification.grid_replay_checkpoints){
   if(!verification.cancelled)throw Error("GridReplayCheckpointsRequireCallback");
   if(!std::holds_alternative<GridBatchTask>(task) && !std::holds_alternative<GridResumeTask>(task))
    throw Error("GridReplayCheckpointsRequireBatchOrResume");
  }
  if(verification.serialization_grid_records && verification.serialization_digest_checkpoints)throw Error("ConflictingSerializationCheckpoints");
  if(verification.serialization_grid_records && !std::holds_alternative<GridBatchTask>(task) && !std::holds_alternative<GridResumeTask>(task) && !std::holds_alternative<GridResearchTask>(task))throw Error("SerializationRecordsRequireBatchResumeOrResearch");
  if((verification.serialization_digest_checkpoints || verification.serialization_grid_records) && !verification.cancelled)throw Error("SerializationCheckpointsRequireCallback");
  auto execution_control=control;
  if(session_budget.max_checkpoints){
   work();
   execution_control.work_checkpoint=[&,observer=control.work_checkpoint]{work();if(observer)observer();};
  }
  auto run=execute_application(std::move(task),execution_control);
  phases.advance(ExecutionPhase::serializing);
  checkpoint();
  ReplayCheckpoint serialization_checkpoint;
  const bool serial_cancel=verification.serialization_digest_checkpoints || verification.serialization_grid_records;
  if(serial_cancel || session_budget.max_checkpoints)serialization_checkpoint=[&]{work();if(serial_cancel)cancelled();};
  ReplayCheckpoint records;
  if(verification.serialization_grid_records || (session_budget.max_checkpoints &&
    (std::holds_alternative<GridBatchRun>(run) || std::holds_alternative<GridResumeRun>(run) || std::holds_alternative<GridResearchRun>(run) || std::holds_alternative<ExactResearchRun>(run) || std::holds_alternative<CompatibilityRun>(run) || std::holds_alternative<FractionComparisonRun>(run))))
   records=[&]{work();if(verification.serialization_grid_records)cancelled();};
  auto bundle=pack_application(run,serialization_checkpoint,records);
  serialization_usage.serialized_bytes=bundle.size();
  if(serialization_budget.max_serialized_bytes && bundle.size()>*serialization_budget.max_serialized_bytes)
   throw Error("SerializationByteBudgetExceeded: bytes="+std::to_string(bundle.size())+" limit="+std::to_string(*serialization_budget.max_serialized_bytes));
  phases.advance(ExecutionPhase::replaying);
  checkpoint();
  if(budget.max_replay_bytes && bundle.size()>*budget.max_replay_bytes)
   throw Error("ReplayByteBudgetExceeded: bytes="+std::to_string(bundle.size())+" limit="+std::to_string(*budget.max_replay_bytes));
  ReplayCheckpoint replay_checkpoint;
  const bool replay_cancel=verification.grid_replay_checkpoints || verification.research_replay_checkpoints || verification.comparison_replay_checkpoints || verification.compatibility_replay_checkpoints;
  if(replay_cancel || (session_budget.max_checkpoints && !std::holds_alternative<GridRun>(run)))
   replay_checkpoint=[&]{work();if(replay_cancel)cancelled();};
  const auto checked=replay_application(bundle,replay_checkpoint);
  if(checked.index()!=run.index() || render_application(checked)!=render_application(run)) throw Error("ApplicationReplayMismatch");
  checkpoint();
  phases.advance(ExecutionPhase::ready);
  return {std::move(run),std::move(bundle),std::move(phases),budget,static_cast<bool>(verification.cancelled),verification.grid_replay_checkpoints,verification.research_replay_checkpoints,verification.comparison_replay_checkpoints,verification.compatibility_replay_checkpoints,verification.serialization_digest_checkpoints,verification.serialization_grid_records,static_cast<bool>(control.experience_cancelled),static_cast<bool>(control.grid_source_cancelled),static_cast<bool>(control.compatibility_source_cancelled),static_cast<bool>(control.resume_import_cancelled),usage,serialization_usage};
 }catch(const std::exception& e){
  throw ExecutionSessionError(std::move(phases),e.what(),usage,serialization_usage);
 }
}
std::string render_execution_session(const ExecutionSession& s){
 if(s.phases.current()!=ExecutionPhase::ready)throw Error("ExecutionSessionNotReady");
 std::string out="{\"execution\":{\"contract\":"+json_string(s.serialization_usage.max_serialized_bytes ? "application-execution-v15" : s.session_usage.max_checkpoints ? "application-execution-v14" : s.execution_resume_import_cancellation ? "application-execution-v13" : s.execution_compatibility_source_cancellation ? "application-execution-v12" : s.execution_grid_source_cancellation ? "application-execution-v11" : s.execution_experience_cancellation ? "application-execution-v10" : s.serialization_grid_records ? "application-execution-v9" : s.serialization_digest_checkpoints ? "application-execution-v8" : s.compatibility_replay_checkpoints ? "application-execution-v7" : s.comparison_replay_checkpoints ? "application-execution-v6" : s.research_replay_checkpoints ? "application-execution-v5" : s.grid_replay_checkpoints ? "application-execution-v4" : s.verification_cancellation ? "application-execution-v3" : s.verification_budget.max_replay_bytes ? "application-execution-v2" : "application-execution-v1")+
  ",\"replay_verified\":true,\"result_complete\":";
 out+=assess_application(s.run).complete ? "true" : "false";
 out+=",\"phases\":[";
 bool first=true;for(auto p:s.phases.history()){if(!first)out+=',';first=false;out+=json_string(phase_name(p));}
 out+=']';
 if(s.execution_experience_cancellation || s.execution_grid_source_cancellation || s.execution_compatibility_source_cancellation || s.execution_resume_import_cancellation)out+=",\"execution_cancellation\":"+json_string(s.execution_resume_import_cancellation ? "resume-import-checkpoints" : s.execution_compatibility_source_cancellation ? "compatibility-source-checkpoints" : s.execution_grid_source_cancellation ? (s.execution_experience_cancellation ? "grid-and-experience-checkpoints" : "grid-source-checkpoints") : "experience-checkpoints");
 if(s.verification_cancellation)out+=",\"verification_cancellation\":"+json_string(s.compatibility_replay_checkpoints ? "compatibility-checkpoints" : s.comparison_replay_checkpoints ? "comparison-checkpoints" : s.research_replay_checkpoints ? "research-checkpoints" : s.grid_replay_checkpoints ? "grid-query-boundaries" : "phase-boundaries");
 if(s.serialization_grid_records || s.serialization_digest_checkpoints)out+=",\"serialization_cancellation\":"+json_string(s.serialization_grid_records ? "grid-record-checkpoints" : "digest-checkpoints");
 if(s.verification_budget.max_replay_bytes)out+=",\"verification_budget\":{\"max_replay_bytes\":"+
  std::to_string(*s.verification_budget.max_replay_bytes)+",\"replay_bytes\":"+std::to_string(s.bundle.size())+"}";
 if(s.session_usage.max_checkpoints)out+=",\"session_budget\":"+render_session_usage(s.session_usage);
 if(s.serialization_usage.max_serialized_bytes)out+=",\"serialization_budget\":{\"max_serialized_bytes\":"+std::to_string(*s.serialization_usage.max_serialized_bytes)+",\"serialized_bytes\":"+std::to_string(s.serialization_usage.serialized_bytes)+"}";
 return out+"},\"result\":"+render_application(s.run)+"}";
}
std::string render_execution_failure(const ExecutionSessionError& e){
 std::string out="{\"error\":"+json_string(e.what())+
  ",\"execution\":{\"contract\":"+json_string(e.serialization_usage().max_serialized_bytes ? "application-execution-failure-v3" : e.session_usage().max_checkpoints ? "application-execution-failure-v2" : "application-execution-failure-v1")+",\"state\":\"FAILED\",\"failed_at\":"+
  json_string(phase_name(e.failed_at()))+",\"phases\":[";
 bool first=true;for(auto p:e.phases().history()){if(!first)out+=',';first=false;out+=json_string(phase_name(p));}
 out+="],\"cause\":"+json_string(e.cause());
 if(e.session_usage().max_checkpoints)out+=",\"session_budget\":"+render_session_usage(e.session_usage());
 if(e.serialization_usage().max_serialized_bytes)out+=",\"serialization_budget\":{\"max_serialized_bytes\":"+std::to_string(*e.serialization_usage().max_serialized_bytes)+",\"serialized_bytes\":"+std::to_string(e.serialization_usage().serialized_bytes)+"}";
 return out+"}}";
}
}
