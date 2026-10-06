#include "bruce/evidence/bundle.hpp"
#include "bruce/application/execution_session.hpp"
#include <iostream>
int main(){try{using namespace bruce;
 std::string binary;for(unsigned i=0;i<1024;++i)for(unsigned j=0;j<256;++j)binary+=static_cast<char>(j);
 const std::vector<std::pair<std::string,std::string>> vectors={{std::string{},"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},{std::string{"abc"},"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},{std::string(65536,'a'),"bf718b6f653bebc184e1479f1935b8da974d701b893afcf49e701f3e2f9f9c5a"},{std::string(65537,'a'),"008ffc88d3c96a9f307524eb361e47c5222a887fc45fa0c1fb8d429c5c23b430"},{binary,"2312394bd99545d9de131c24efb781e765ac1aec243f2ed9347597a793a415e9"}};
 for(const auto& [value,want]:vectors){
  unsigned calls=0;
  if(sha256(value)!=want || sha256(value,[&]{++calls;})!=want)throw Error("SHA256 known vector mismatch");
  if(calls!=(value.size()+65535)/65536+1)throw Error("Digest block boundary missing");
  for(unsigned stop=1;stop<=calls;++stop){unsigned seen=0;bool caught=false;
   try{(void)sha256(value,[&]{if(++seen==stop)throw NumericLimit();});}catch(const NumericLimit&){caught=true;}
   if(!caught || seen!=stop)throw Error("Digest callback swallowed or continued");
   if(sha256(value,[]{})!=want)throw Error("Digest context leaked state after cancellation");
  }
 }
 VerificationControl control;control.serialization_digest_checkpoints=true;bool rejected=false;
 try{(void)execute_checked_application(GridTask{}, {},{},control);}
 catch(const ExecutionSessionError& e){rejected=e.failed_at()==ExecutionPhase::executing && e.cause()=="SerializationCheckpointsRequireCallback";}
 if(!rejected)throw Error("Serialization accepted absent callback");
 ExactResearchRun experience;experience.task.planner="experience-order";experience.task.experience_snapshot.assign(131073,'x');
 experience.status="INCOMPLETE";experience.stop_reason="Cancelled";
 unsigned records=0,digests=0;
 const auto plain=pack_application(experience);
 const auto checked=pack_application(experience,[&]{++digests;},[&]{++records;});
 if(checked!=plain || records<3 || digests<2)throw Error("Exact research record boundaries missing or bytes changed");
 for(unsigned stop=1;stop<=records;++stop){unsigned seen=0;bool caught=false;
  try{(void)pack_application(experience,[&]{++digests;},[&]{if(++seen==stop)throw NumericLimit();});}
  catch(const NumericLimit&){caught=true;}
  if(!caught || seen!=stop)throw Error("Exact research record cancellation failed");
 }
 CompatibilityRun compatibility;compatibility.task.rules="BRUCE_RULES_V1\nsnapshot budget\n";compatibility.task.candidates.assign(131073,'c');
 unsigned compatibility_records=0;
 const auto compatibility_plain=pack_application(compatibility);
 const auto compatibility_checked=pack_application(compatibility,{},[&]{++compatibility_records;});
 if(compatibility_checked!=compatibility_plain || compatibility_records<3)throw Error("Compatibility record boundaries missing");
 FractionComparisonTask fraction_task;fraction_task.left_numerator="1";fraction_task.right_numerator="2";
 const auto fraction=compare_fractions(fraction_task);unsigned fraction_records=0;
 const auto fraction_plain=pack_application(fraction);
 const auto fraction_checked=pack_application(fraction,{},[&]{++fraction_records;});
 if(fraction_checked!=fraction_plain || fraction_records<3)throw Error("Fraction comparison record boundaries missing");
 std::cout<<"Digest vectors, block boundaries and exceptions verified\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
