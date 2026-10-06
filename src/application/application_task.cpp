#include "bruce/application/application_task.hpp"
#include <array>
#include <type_traits>
namespace bruce {
namespace {
using Checkpoint=std::function<bool(std::uint64_t)>;
Run execute_native(Task t,const Checkpoint& c){return research(std::move(t),c);}
ExactResearchRun execute_native(ExactResearchTask t,const Checkpoint& c){return exact_research(std::move(t),c);}
GridRun execute_native(GridTask t,const Checkpoint&){return evaluate_grid(std::move(t));}
GridBatchRun execute_native(GridBatchTask t,const Checkpoint& c){return evaluate_grid_batch(std::move(t),c);}
GridResumeRun execute_native(GridResumeTask t,const Checkpoint& c){return resume_grid_batch(std::move(t.parent),t.additional_queries,c);}
GridResearchRun execute_native(GridResearchTask t,const Checkpoint& c){
    return research_from_grid(std::move(t.research),std::move(t.training),std::move(t.holdout),c,std::move(t.axis),t.total_operations);
}
FractionComparisonRun execute_native(FractionComparisonTask t,const Checkpoint& c){return compare_fractions(std::move(t),c);}
CompatibilityRun execute_native(CompatibilityTask t,const Checkpoint& c){return run_compatibility(std::move(t),c);}
template<class T> struct Codec;
template<> struct Codec<Run>{static constexpr auto render=render_run;static constexpr auto pack=pack_run;};
template<> struct Codec<ExactResearchRun>{static constexpr auto render=render_exact_research;static constexpr auto pack=pack_exact_research;};
template<> struct Codec<GridRun>{static constexpr auto render=render_grid;static constexpr auto pack=pack_grid;};
template<> struct Codec<GridBatchRun>{static constexpr auto render=render_grid_batch;static constexpr auto pack=pack_grid_batch;};
template<> struct Codec<GridResumeRun>{static constexpr auto render=render_grid_resume;static constexpr auto pack=pack_grid_resume;};
template<> struct Codec<GridResearchRun>{static constexpr auto render=render_grid_research;static constexpr auto pack=pack_grid_research;};
template<> struct Codec<FractionComparisonRun>{static constexpr auto render=render_fraction_comparison;static constexpr auto pack=pack_fraction_comparison;};
template<> struct Codec<CompatibilityRun>{static constexpr auto render=render_compatibility;static constexpr auto pack=pack_compatibility;};
constexpr std::array<std::string_view,8> names={"research-int64","research-exact","grid","grid-batch","grid-resume","grid-research","fraction-comparison","compatibility"};
static_assert(names.size()==std::variant_size_v<ApplicationTask> && names.size()==std::variant_size_v<ApplicationRun>);
}
ApplicationRun execute_application(ApplicationTask task,const ExecutionControl& control){
    Checkpoint checkpoint;
    // Preserve an absent callback; some executors distinguish it from a false callback.
    if(control.cancelled || control.work_checkpoint) checkpoint=[&](std::uint64_t){
        if(control.work_checkpoint)detail::observe_replay_checkpoint(control.work_checkpoint);
        return control.cancelled && control.cancelled();
    };
    const auto with_work=[&](ReplayCheckpoint local)->ReplayCheckpoint{
        if(!control.work_checkpoint)return local;
        return [&,local]{
            detail::observe_replay_checkpoint(control.work_checkpoint);
            if(local)local();
        };
    };
    ReplayCheckpoint experience_checkpoint;
    if(control.experience_cancelled)experience_checkpoint=[&]{if(control.experience_cancelled())throw Error("ExecutionExperienceCancelled");};
    ReplayCheckpoint grid_checkpoint;
    if(control.grid_source_cancelled)grid_checkpoint=[&]{if(control.grid_source_cancelled())throw Error("ExecutionGridSourceCancelled");};
    ReplayCheckpoint compatibility_checkpoint;
    if(control.compatibility_source_cancelled)compatibility_checkpoint=[&]{if(control.compatibility_source_cancelled())throw Error("ExecutionCompatibilitySourceCancelled");};
    ReplayCheckpoint resume_checkpoint;
    if(control.resume_import_cancelled)resume_checkpoint=[&]{if(control.resume_import_cancelled())throw Error("ExecutionResumeImportCancelled");};
    try{return std::visit([&](auto&& t)->ApplicationRun{
        using T=std::decay_t<decltype(t)>;
        if constexpr(!std::is_same_v<T,GridResumeTask>){
            if(resume_checkpoint)throw Error("ExecutionResumeImportRequiresResumeTask");
        }
        if constexpr(!std::is_same_v<T,CompatibilityTask>){
            if(compatibility_checkpoint)throw Error("ExecutionCompatibilitySourceRequiresCompatibilityTask");
        }
        if constexpr(!std::is_same_v<T,GridResearchTask>){
            if(grid_checkpoint)throw Error("ExecutionGridSourceCheckpointsRequireGridResearch");
        }
        if constexpr(std::is_same_v<T,ExactResearchTask> || std::is_same_v<T,GridResearchTask>){
            if(experience_checkpoint){
                const auto& research_task=[&]() -> const ExactResearchTask& {
                    if constexpr(std::is_same_v<T,ExactResearchTask>)return t;
                    else return t.research;
                }();
                if(research_task.planner!="experience-order")throw Error("ExecutionExperienceCheckpointsRequireLearnedResearch");
            }
            const auto& planner=[&]() -> const std::string& {
                if constexpr(std::is_same_v<T,ExactResearchTask>)return t.planner;
                else return t.research.planner;
            }();
            if(planner=="experience-order")experience_checkpoint=with_work(experience_checkpoint);
            if constexpr(std::is_same_v<T,GridResearchTask>){
                grid_checkpoint=with_work(grid_checkpoint);
                if(experience_checkpoint || grid_checkpoint)return research_from_grid(std::move(t.research),std::move(t.training),std::move(t.holdout),checkpoint,std::move(t.axis),t.total_operations,experience_checkpoint,grid_checkpoint);
            }else if(experience_checkpoint)return exact_research(std::move(t),checkpoint,experience_checkpoint);
        }else if(experience_checkpoint)throw Error("ExecutionExperienceCheckpointsRequireLearnedResearch");
        if constexpr(std::is_same_v<T,CompatibilityTask>){
            if(!t.origin.empty())compatibility_checkpoint=with_work(compatibility_checkpoint);
            if(compatibility_checkpoint)return run_compatibility(std::move(t),checkpoint,compatibility_checkpoint);
        }
        if constexpr(std::is_same_v<T,GridResumeTask>){
            resume_checkpoint=with_work(resume_checkpoint);
            if(resume_checkpoint)return resume_grid_batch(std::move(t.parent),t.additional_queries,checkpoint,resume_checkpoint);
        }
        return execute_native(std::move(t),checkpoint);
    },std::move(task));
    }catch(const detail::ReplayCheckpointRaised& raised){std::rethrow_exception(raised.original);}
}
std::string render_application(const ApplicationRun& run){
    return std::visit([](const auto& r){return Codec<std::decay_t<decltype(r)>>::render(r);},run);
}
std::string pack_application(const ApplicationRun& run,const ReplayCheckpoint& checkpoint,const ReplayCheckpoint& records){
    return std::visit([&](const auto& r){
        using T=std::decay_t<decltype(r)>;
        if constexpr(std::is_same_v<T,GridBatchRun> || std::is_same_v<T,GridResumeRun>)return Codec<T>::pack(r,checkpoint,records);
        else if constexpr(std::is_same_v<T,GridResearchRun>)return records ? pack_grid_research_records(r,checkpoint,records) : Codec<T>::pack(r,checkpoint);
        else if constexpr(std::is_same_v<T,ExactResearchRun>)return records ? pack_exact_research_records(r,checkpoint,records) : Codec<T>::pack(r,checkpoint);
        else if constexpr(std::is_same_v<T,CompatibilityRun>)return records ? pack_compatibility_records(r,checkpoint,records) : Codec<T>::pack(r,checkpoint);
        else if constexpr(std::is_same_v<T,FractionComparisonRun>)return records ? pack_fraction_comparison_records(r,checkpoint,records) : Codec<T>::pack(r,checkpoint);
        else {
            if(records)throw Error("SerializationRecordsRequireBatchOrResume");
            return Codec<T>::pack(r,checkpoint);
        }
    },run);
}
std::string_view application_kind(const ApplicationRun& run){return names.at(run.index());}
std::string render_application_contract(){
    std::string out="{\"contract\":\"application-task-v1\",\"dynamic_plugins\":false,\"unified_budget_units\":false,\"cancellation\":\"executor-local-checkpoints\",\"tasks\":[";
    for(std::size_t i=0;i<names.size();++i){if(i)out+=',';out+=json_string(names[i]);}
    return out+"]}";
}
}
