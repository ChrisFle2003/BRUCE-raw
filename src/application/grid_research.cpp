#include "bruce/application/grid_research.hpp"
#include "bruce/application/grid_resume.hpp"
#include "bruce/application/research.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/experience/store.hpp"
#include <set>
#include <algorithm>

namespace bruce {
namespace {
constexpr std::string_view header_x="BRUCE_GRID_RESEARCH_BUNDLE_V1\n";
constexpr std::string_view header_y="BRUCE_GRID_RESEARCH_BUNDLE_V2\n";
constexpr std::string_view header_budget="BRUCE_GRID_RESEARCH_BUNDLE_V3\n";
constexpr std::string_view experience_header="BRUCE_GRID_RESEARCH_BUNDLE_V4\n";
constexpr std::string_view experience_policy="grid-and-experience-bytes-plus-research-v1";
constexpr std::string_view work_policy="grid-source-bytes-plus-research-v1";
std::uint64_t research_limit(const std::string& training,const std::string& holdout,std::uint64_t total,std::uint64_t requested,std::uint64_t experience_bytes=0) {
    if(training.empty() || training.size()>grid_research_source_limit || holdout.size()>grid_research_source_limit)
        throw Error("GridResearchSourceByteLimit");
    if(!total || total>10000000 || !requested || requested>10000000) throw Error("InvalidGridTotalBudget");
    if(experience_bytes>experience_byte_limit) throw Error("ExperienceByteLimit");
    const auto source=training.size()+holdout.size()+experience_bytes;
    if(total<=source) throw Error("InsufficientGridImportBudget");
    return std::min<std::uint64_t>(requested,total-source);
}
std::string_view projection(std::string_view axis) {
    if(axis!="x" && axis!="y") throw Error("UnknownGridResearchAxis");
    return axis=="x" ? "fixed-slice-leaf-x-v1" : "fixed-slice-leaf-y-v1";
}
struct Projected {
    std::vector<ExactSample> training,holdout;
    std::string prefix;
    unsigned fixed=0;
};
Projected project(const std::string& training,const std::string& holdout,std::string_view axis,const ReplayCheckpoint& checkpoint={}) {
    (void)projection(axis);
    if(training.empty() || training.size()>grid_research_source_limit || holdout.size()>grid_research_source_limit)
        throw Error("GridResearchSourceByteLimit");
    const auto train=replay_grid_source(training,checkpoint);
    if(train.task.spec.mode!="exact") throw Error("GridResearchRequiresExactValues");
    const auto spec=render_grid_spec(train.task.spec);
    Projected out;std::set<std::uint64_t> seen;
    bool first=true;
    auto append=[&](const GridBatchRun& batch,std::vector<ExactSample>& samples) {
        if(batch.status!="COMPLETED") throw Error("IncompleteGridSource");
        if(render_grid_spec(batch.task.spec)!=spec) throw Error("GridResearchSpecMismatch");
        for(const auto& query:batch.queries) {
            if(checkpoint) checkpoint();
            const auto& run=query.run;
            if(run.stopped) throw Error("GridResearchStoppedSample");
            const auto leaf=run.path.steps.back();
            std::string prefix;
            for(std::size_t i=0;i+1<run.path.steps.size();++i) {
                if(!prefix.empty()) prefix+='/';
                prefix+=std::to_string(run.path.steps[i].x)+","+std::to_string(run.path.steps[i].y);
            }
            const auto input=axis=="x" ? leaf.x : leaf.y;
            const auto fixed=axis=="x" ? leaf.y : leaf.x;
            if(first) {out.prefix=prefix;out.fixed=fixed;first=false;}
            else if(out.prefix!=prefix || out.fixed!=fixed) throw Error("GridResearchRequiresFixedSlice");
            if(!seen.insert(input).second) throw Error("DuplicateOrOverlappingGridInput");
            samples.push_back({ExactValue(mpz_class(std::to_string(input))),ExactValue(mpz_class(std::to_string(run.value)))});
        }
    };
    append(train,out.training);
    if(!holdout.empty()) append(replay_grid_source(holdout,checkpoint),out.holdout);
    return out;
}
bool same_samples(const std::vector<ExactSample>& a,const std::vector<ExactSample>& b,const ReplayCheckpoint& checkpoint) {
    if(a.size()!=b.size()) return false;
    for(std::size_t i=0;i<a.size();++i) {if(checkpoint) checkpoint();if(a[i].x!=b[i].x || a[i].y!=b[i].y) return false;}
    return true;
}
constexpr std::size_t serialization_block_size=64*1024;
void append_chunked(std::string& out,std::string_view text,const ReplayCheckpoint& records) {
    for(std::size_t offset=0;offset<text.size();) {
        if(records) records();
        const auto count=std::min(serialization_block_size,text.size()-offset);
        out.append(text.substr(offset,count));
        offset+=count;
    }
}
std::string payload(const GridResearchRun& run,const ReplayCheckpoint& records={}) {
    if(run.total_operations) {
        auto inner=run;inner.total_operations.reset();inner.requested_operations=0;inner.budget_experience=false;
        const auto nested=pack_grid_research(inner);
        std::string out=std::string(run.budget_experience ? "engine bruce-grid-research-v4\nwork_policy " : "engine bruce-grid-research-v3\nwork_policy ")+std::string(run.budget_experience ? experience_policy : work_policy)+
            "\ntotal_operations "+std::to_string(*run.total_operations)+
            "\nrequested_operations "+std::to_string(run.requested_operations)+
            (run.budget_experience ? "\nexperience_import_operations "+std::to_string(run.research.task.experience_snapshot.size()) : "")+
            "\ngrid_research_bytes "+std::to_string(nested.size())+"\n";
        append_chunked(out,nested,records);
        out+="RESULT\n"+render_grid_research(run)+"\n";
        return out;
    }
    const auto policy=projection(run.axis);
    std::string out=std::string(run.axis=="x" ? "engine bruce-grid-research-v1\n" : "engine bruce-grid-research-v2\n")+
        "projection "+std::string(policy)+"\n";
    auto block=[&](std::string_view name,const std::string& text) {
        out+=std::string(name)+" "+std::to_string(text.size())+"\n";
        append_chunked(out,text,records);
    };
    block("training_bytes",run.training_source);block("holdout_bytes",run.holdout_source);
    block("research_bytes",pack_exact_research_records(run.research,{},records));
    return out+"RESULT\n"+render_grid_research(run)+"\n";
}
class Reader {
    std::string_view data_;
public:
    explicit Reader(std::string_view data):data_(data) {}
    std::string field(std::string_view name) {
        const auto end=data_.find('\n');
        const auto prefix=std::string(name)+" ";
        if(end==std::string_view::npos || !data_.substr(0,end).starts_with(prefix)) throw Error("InvalidGridResearchField");
        const auto value=std::string(data_.substr(prefix.size(),end-prefix.size()));
        data_.remove_prefix(end+1);return value;
    }
    std::string block(std::string_view name,std::size_t limit) {
        const auto count=integer<std::size_t>(field(name));
        if(count>limit || count>data_.size()) throw Error("InvalidGridResearchBlockSize");
        auto out=std::string(data_.substr(0,count));data_.remove_prefix(count);return out;
    }
};
}
GridResearchRun research_from_grid(ExactResearchTask task,std::string training,std::string holdout,
                                 const std::function<bool(std::uint64_t)>& cancelled,std::string axis,std::optional<std::uint64_t> total_operations,const ReplayCheckpoint& experience_checkpoint,const ReplayCheckpoint& grid_checkpoint) {
    if(!task.training.empty() || !task.holdout.empty()) throw Error("UseGridOrExplicitSamples");
    (void)projection(axis);
    const auto requested=task.budget.operations;
    const bool budget_experience=total_operations && task.planner=="experience-order";
    if(budget_experience && task.experience_snapshot.empty()) throw Error("MissingExperienceSnapshot");
    if(total_operations) task.budget.operations=research_limit(training,holdout,*total_operations,requested,
        budget_experience ? task.experience_snapshot.size() : 0);
    auto projected=project(training,holdout,axis,grid_checkpoint);
    task.training=std::move(projected.training);task.holdout=std::move(projected.holdout);
    auto result=exact_research(std::move(task),cancelled,experience_checkpoint);
    return {std::move(training),std::move(holdout),std::move(projected.prefix),axis=="x" ? projected.fixed : 0,
        std::move(result),axis,axis=="y" ? projected.fixed : 0,total_operations,total_operations ? requested : 0,budget_experience};
}
std::string render_grid_research(const GridResearchRun& run) {
    std::string out= "{\"status\":"+json_string(run.research.status)+",\"grid_origin\":{\"projection\":"+json_string(projection(run.axis))+
        ",\"prefix\":"+json_string(run.prefix)+(run.axis=="x" ? ",\"exponent\":"+std::to_string(run.exponent) : ",\"base\":"+std::to_string(run.base))+
        ",\"input_min\":0,\"input_max\":7,\"training_digest\":"+json_string(sha256(run.training_source))+
        ",\"holdout_digest\":"+(run.holdout_source.empty() ? "null" : json_string(sha256(run.holdout_source)))+
        "},\"research\":"+render_exact_research(run.research)+"}";
    if(run.total_operations) {
        const auto experience=run.budget_experience ? run.research.task.experience_snapshot.size() : 0;
        if(run.budget_experience && (run.research.task.planner!="experience-order" || !experience)) throw Error("GridExperienceBudgetMismatch");
        const auto limit=research_limit(run.training_source,run.holdout_source,*run.total_operations,run.requested_operations,experience);
        if(run.research.task.budget.operations!=limit || run.research.operations>limit) throw Error("GridWorkBudgetMismatch");
        const auto source=run.training_source.size()+run.holdout_source.size();
        out.pop_back();
        out+=",\"work_budget\":{\"policy\":"+json_string(run.budget_experience ? experience_policy : work_policy)+",\"limit\":"+std::to_string(*run.total_operations)+
            ",\"source_operations\":"+std::to_string(source)+(run.budget_experience ? ",\"experience_import_operations\":"+std::to_string(experience) : "")+",\"requested_research_limit\":"+std::to_string(run.requested_operations)+
            ",\"research_limit\":"+std::to_string(limit)+",\"used\":"+std::to_string(source+experience+run.research.operations)+"}}";
    }
    return out;
}
std::string pack_grid_research_records(const GridResearchRun& run,const ReplayCheckpoint& checkpoint,const ReplayCheckpoint& records) {
    const auto body=payload(run,records);
    const auto bundle=std::string(run.total_operations ? (run.budget_experience ? experience_header : header_budget) : run.axis=="x" ? header_x : header_y)+sha256(body,checkpoint)+"\n"+body;
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    return bundle;
}
std::string pack_grid_research(const GridResearchRun& run,const ReplayCheckpoint& checkpoint) {
    return pack_grid_research_records(run,checkpoint,{});
}
static GridResearchRun replay_grid_research_impl(std::string_view bundle,std::uint64_t operation_limit,
    const std::function<void(std::string_view)>& admit_research,const ReplayCheckpoint& checkpoint,
    std::optional<std::uint64_t> admitted_total={},std::uint64_t admitted_requested=0,std::optional<std::uint64_t> admitted_experience={}) {
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    const bool experience_budget=bundle.starts_with(experience_header);
    if(experience_budget || bundle.starts_with(header_budget)) {
        bundle.remove_prefix(experience_budget ? experience_header.size() : header_budget.size());
        if(bundle.size()<65 || bundle[64]!='\n') throw Error("InvalidDigestHeader");
        const auto body=bundle.substr(65);
        if(sha256(body)!=bundle.substr(0,64)) throw Error("InvalidEvidence: digest mismatch");
        if(checkpoint) checkpoint();
        Reader reader(body);
        if(reader.field("engine")!=(experience_budget ? "bruce-grid-research-v4" : "bruce-grid-research-v3") || reader.field("work_policy")!=(experience_budget ? experience_policy : work_policy)) throw Error("UnknownGridWorkPolicy");
        const auto total=integer<std::uint64_t>(reader.field("total_operations"));
        const auto requested=integer<std::uint64_t>(reader.field("requested_operations"));
        std::optional<std::uint64_t> experience;
        if(experience_budget) {
            experience=integer<std::uint64_t>(reader.field("experience_import_operations"));
            if(!*experience || *experience>experience_byte_limit) throw Error("GridExperienceBudgetMismatch");
        }
        const auto nested=reader.block("grid_research_bytes",max_file_bytes);
        if(!nested.starts_with(header_x) && !nested.starts_with(header_y)) throw Error("GridBudgetRequiresUnbudgetedOrigin");
        auto run=replay_grid_research_impl(nested,operation_limit,admit_research,checkpoint,total,requested,experience);
        run.total_operations=total;run.requested_operations=requested;run.budget_experience=experience_budget;
        if(checkpoint) checkpoint();
        if(payload(run)!=body) throw Error("ReplayMismatch: grid work budget differs");
        return run;
    }
    const std::string axis=bundle.starts_with(header_x) ? "x" : bundle.starts_with(header_y) ? "y" : "";
    if(axis.empty()) throw Error("UnknownBundleVersion");
    bundle.remove_prefix(axis=="x" ? header_x.size() : header_y.size());
    if(bundle.size()<65 || bundle[64]!='\n') throw Error("InvalidDigestHeader");
    const auto body=bundle.substr(65);
    if(sha256(body)!=bundle.substr(0,64)) throw Error("InvalidEvidence: digest mismatch");
    if(checkpoint) checkpoint();
    Reader reader(body);
    if(reader.field("engine")!=(axis=="x" ? "bruce-grid-research-v1" : "bruce-grid-research-v2") || reader.field("projection")!=projection(axis))
        throw Error("UnknownGridResearchVersion");
    auto training=reader.block("training_bytes",grid_research_source_limit);
    auto holdout=reader.block("holdout_bytes",grid_research_source_limit);
    const auto limit=admitted_total ? research_limit(training,holdout,*admitted_total,admitted_requested,admitted_experience.value_or(0)) : 10000000;
    const auto research_source=reader.block("research_bytes",max_file_bytes);
    if(admit_research) admit_research(research_source);
    const auto projected=project(training,holdout,axis,checkpoint);
    auto research=replay_exact_research(research_source,std::min(limit,operation_limit),[&](const ExactResearchTask& task) {
        if(admitted_experience && (task.planner!="experience-order" || task.experience_snapshot.size()!=*admitted_experience))
            throw Error("GridExperienceBudgetMismatch");
    },checkpoint);
    if(!same_samples(projected.training,research.task.training,checkpoint) || !same_samples(projected.holdout,research.task.holdout,checkpoint))
        throw Error("GridResearchSampleBindingMismatch");
    GridResearchRun run{std::move(training),std::move(holdout),projected.prefix,axis=="x" ? projected.fixed : 0,
        std::move(research),axis,axis=="y" ? projected.fixed : 0,{},0};
    if(checkpoint) checkpoint();
    if(payload(run)!=body) throw Error("ReplayMismatch: grid research origin or result differs");
    return run;
}
GridResearchRun replay_grid_research(std::string_view bundle,std::uint64_t operation_limit,
    const std::function<void(std::string_view)>& admit_research,const ReplayCheckpoint& checkpoint) {
    return replay_grid_research_impl(bundle,operation_limit,admit_research,checkpoint);
}
}
