#include "bruce/application/execution_session.hpp"
#include <iostream>
namespace { void require(bool value,const char* message){if(!value)throw bruce::Error(message);} }
int main(){try{using namespace bruce;
 GridBatchRun run;
 run.task.paths={"2,6"};
 run.trace.push_back(std::string(131073,'T'));
 const auto plain=pack_grid_batch(run);
 unsigned points=0;
 const auto checked=pack_grid_batch(run,{},[&]{++points;});
 require(checked==plain,"Large grid record changed bundle bytes");
 require(points==3,"Large record did not expose exact block boundaries");
 for(unsigned stop=1;stop<=points;++stop){unsigned seen=0;bool raised=false;
  try{(void)pack_grid_batch(run,{},[&]{if(++seen==stop)throw NumericLimit();});}
  catch(const NumericLimit&){raised=true;}
  require(raised && seen==stop,"Large record callback was swallowed or skipped");
 }
 unsigned session_points=0;
 VerificationControl control;control.serialization_grid_records=true;
 control.cancelled=[&]{return ++session_points==3;};
 bool failed=false;try{(void)execute_checked_application(run,{}, {}, control);}
 catch(const ExecutionSessionError& error){failed=error.failed_at()==ExecutionPhase::serializing && error.cause()=="VerificationCancelled";}
 require(failed && session_points==3,"Session did not stop inside large record");
 GridBatchRun small;small.task.paths={"2,6"};small.trace={"short"};unsigned old=0;
 const auto small_plain=pack_grid_batch(small,{},[&]{++old;});
 require(small_plain==pack_grid_batch(small) && old==3,"Small record callback compatibility changed");
 std::cout<<"Large grid records: block callbacks, unchanged bytes and serializing failure verified\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
