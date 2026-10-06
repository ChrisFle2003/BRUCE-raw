#include "bruce/application/experience_task.hpp"
#include "bruce/experience/store.hpp"
#include <type_traits>
namespace bruce {
ExperienceResult execute_experience(const ExperienceTask& task,const ReplayCheckpoint& checkpoint) {
    if(checkpoint)checkpoint();
    auto snapshot=std::visit([&](const auto& action) {
        using T=std::decay_t<decltype(action)>;
        if constexpr(std::is_same_v<T,ExperienceAddTask>)
            return append_experience(action.previous,action.source,checkpoint);
        else if constexpr(std::is_same_v<T,ExperienceRevokeTask>)
            return revoke_experience(action.previous,action.target,action.reason,checkpoint);
        else if constexpr(std::is_same_v<T,ExperienceReplaceTask>)
            return replace_experience(action.previous,action.target,action.reason,action.source,checkpoint);
        else return action.snapshot;
    },task);
    auto report=render_experience(read_experience(snapshot,checkpoint));
    if(checkpoint)checkpoint();
    return {std::move(snapshot),std::move(report)};
}
}
