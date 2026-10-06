#include "bruce/application/execution_session.hpp"
#include <iostream>
namespace {void require(bool b,const char* m){if(!b)throw bruce::Error(m);}}
int main(){try{using namespace bruce;
 // An incomplete source with a large, legal zero-padded path is still a real replayable run.
 GridBatchTask batch;batch.paths={std::string(70000,'0')+"2,6","4,3"};batch.query_steps=1;
 GridResumeTask task{pack_grid_batch(evaluate_grid_batch(batch)),1};
 const auto run=resume_grid_batch(task.parent,1);
 require(run.parent.size()>65536,"Fixture must cross copy blocks");
 const auto plain=pack_grid_resume(run);
 unsigned seen=0;
 const auto packed=pack_grid_resume(run,{},[&]{++seen;});
 const auto old_points=1+run.batch.trace.size()+run.batch.queries.size();
 const auto copy_points=(run.parent.size()-1)/65536;
 require(seen==old_points+copy_points,"Large parent copy has no inner checkpoints");
 require(packed==plain,"Block copy changed native evidence bytes");
 require(render_grid_resume(replay_grid_resume(packed))==render_grid_resume(run),"Copied parent failed replay");
 // The first point precedes header/parent, the following points interrupt inside the parent.
 for(unsigned stop=2;stop<=1+copy_points;++stop){
  unsigned calls=0,hashes=0;bool rejected=false;
  try{(void)pack_grid_resume(run,[&]{++hashes;},[&]{if(++calls==stop)throw NumericLimit();});}
  catch(const NumericLimit&){rejected=true;}
  require(rejected && calls==stop && hashes==0,"Copy failure swallowed or hashing started");
 }
 const auto ample=execute_checked_application(task,{},{},{},{{1000000}});
 require(ample.bundle==plain,"Budgeted session changed evidence");
 for(unsigned copy=0;copy<copy_points;++copy){
  // EXECUTING, then SERIALIZING boundary, initial parent record, then copy points.
  const auto limit=ample.session_usage.executing+2+copy;
  bool rejected=false;
  try{(void)execute_checked_application(task,{},{},{},{{limit}});}
  catch(const ExecutionSessionError& e){rejected=e.failed_at()==ExecutionPhase::serializing && e.cause().starts_with("SessionCheckpointBudgetExceeded:") && e.session_usage().checkpoints==limit;}
  require(rejected,"Shared budget did not interrupt parent copying");
 }
 VerificationControl c;c.serialization_grid_records=true;unsigned calls=0;
 c.cancelled=[&]{return ++calls==3;};bool rejected=false;
 try{(void)execute_checked_application(task,{},{},c);}catch(const ExecutionSessionError& e){rejected=e.failed_at()==ExecutionPhase::serializing && e.cause()=="VerificationCancelled";}
 require(rejected && calls==3,"Signal callback did not interrupt copy");
 // Serialization boundaries, including embedded NULs; these artificial parents
 // exercise byte transport only, not validity of their mathematical provenance.
 for(std::size_t bytes:{0U,65535U,65536U,65537U,131072U,131073U}){
  auto synthetic=run;synthetic.parent.assign(bytes,'x');
  for(std::size_t i=0;i<bytes;i+=257)synthetic.parent[i]='\0';
  unsigned points=0;
  auto observed=pack_grid_resume(synthetic,{},[&]{++points;});
  const auto marker="parent_bytes "+std::to_string(bytes)+"\n";
  const auto start=observed.find(marker);
  require(start!=std::string::npos && observed.substr(start+marker.size(),bytes)==synthetic.parent,
          "Parent transport lost embedded bytes");
  require(observed.substr(start+marker.size()+bytes,7)=="\nTRACE\n","Parent copy crossed its boundary");
  require(observed==pack_grid_resume(synthetic),"Boundary or binary parent bytes changed");
  require(points==old_points+(bytes ? (bytes-1)/65536 : 0),"Wrong exact copy boundary count");
 }
 // Subsequent calls have no retained partial state.
 require(pack_grid_resume(run)==plain,"Copy cancellation retained state");
 std::cout<<"Large resume parent: inner copy checkpoints, unchanged replay, exceptions and session budget verified\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
