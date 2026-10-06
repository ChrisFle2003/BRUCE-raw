#include "bruce/application/grid_batch.hpp"
#include "bruce/application/research.hpp"
#include "bruce/evidence/bundle.hpp"
#include <algorithm>
#include <sstream>

namespace bruce {
namespace detail { std::string render_grid_batch_records(const GridBatchRun&,const ReplayCheckpoint&); }
namespace {
constexpr std::string_view header="BRUCE_GRID_BATCH_BUNDLE_V1\n";
constexpr std::string_view partial_header="BRUCE_GRID_BATCH_BUNDLE_V2\n";
bool bounded(const GridBatchTask& task) {return task.query_steps.has_value() || task.cancel_after_queries.has_value();}
std::string optional_count(std::optional<std::size_t> n) {return n ? std::to_string(*n) : "none";}
constexpr std::string_view cache_policy="fifo-64-canonical-evidence-bytes-v1";
std::string field(std::istream& input,std::string_view name) {
    std::string line,key,value,extra;
    if(!std::getline(input,line)) throw Error("TruncatedBundle");
    std::istringstream row(line);
    if(!(row>>key>>value) || key!=name || (row>>extra)) throw Error("InvalidBundleField: "+std::string(name));
    return value;
}
std::string stats_json(const GridCacheStats& s) {
    return "{\"hits\":"+std::to_string(s.hits)+",\"misses\":"+std::to_string(s.misses)+
        ",\"inserts\":"+std::to_string(s.inserts)+",\"evictions\":"+std::to_string(s.evictions)+
        ",\"bypasses\":"+std::to_string(s.bypasses)+",\"entries\":"+std::to_string(s.entries)+
        ",\"bytes\":"+std::to_string(s.bytes)+",\"peak_bytes\":"+std::to_string(s.peak_bytes)+"}";
}
std::string payload(const GridBatchRun& run,const ReplayCheckpoint& records={}) {
    const auto& t=run.task;const auto& s=t.spec;
    std::string out="spec "+std::string(GridSpec::version)+"\nengine "+std::string(GridSpec::engine)+
        "\naddress_rule "+std::string(GridSpec::address_rule)+"\nvalue_rule "+std::string(GridSpec::value_rule)+
        "\npolicy "+std::string(GridSpec::policy)+"\ndomain "+std::string(GridSpec::domain)+
        "\ncache_policy "+std::string(cache_policy)+
        (bounded(t) ? "\nexecution_policy query-boundaries-v1\nquery_steps "+optional_count(t.query_steps)+
            "\ncancel_after "+optional_count(t.cancel_after_queries) : "")+"\nmode "+s.mode+"\nmax_bits "+std::to_string(s.max_bits)+
        "\nmodulus "+(s.modulus ? std::to_string(*s.modulus) : "none")+"\nzero_zero "+s.zero_zero+
        "\ncache_bytes "+std::to_string(t.cache_bytes)+"\nquery_count "+std::to_string(t.paths.size())+"\n";
    for(const auto& path:t.paths) {if(records) records();out+="path "+path+"\n";}
    out+="TRACE\n";
    for(const auto& event:run.trace) {if(records) records();out+=event+'\n';}
    return out+"RESULT\n"+detail::render_grid_batch_records(run,records)+"\n";
}
}
GridCache::GridCache(std::size_t bytes):limit_(bytes) {
    if(bytes>max_grid_cache_bytes) throw Error("GridCacheByteLimit");
}
CachedGridRun GridCache::evaluate(GridTask task) {
    const auto key=grid_cache_key(task);
    const auto digest=sha256(key);
    for(const auto& entry:entries_) if(entry.key==key) {
        auto run=entry.run;
        run.task=std::move(task); // Preserve this request's spelling, not the cache's.
        ++stats_.hits;
        return {std::move(run),digest,"HIT"};
    }
    auto run=evaluate_grid(std::move(task));
    if(run.stopped) {++stats_.bypasses;return {std::move(run),digest,"STOP_BYPASS"};}
    ++stats_.misses;++stats_.evaluations;
    auto canonical=run;canonical.task.path=canonical_grid_path(run.task.path);
    const auto bytes=key.size()+pack_grid(canonical).size();
    if(bytes>limit_) {++stats_.bypasses;return {std::move(run),digest,"MISS_BYPASS"};}
    while(entries_.size()>=64 || stats_.bytes+bytes>limit_) {
        stats_.bytes-=entries_.front().bytes;entries_.pop_front();++stats_.evictions;
    }
    entries_.push_back({key,std::move(canonical),bytes});
    ++stats_.inserts;stats_.entries=entries_.size();stats_.bytes+=bytes;
    stats_.peak_bytes=std::max(stats_.peak_bytes,stats_.bytes);
    return {std::move(run),digest,"MISS_INSERT"};
}
namespace {
GridBatchRun finish_batch(GridBatchRun run,GridCache& cache,const std::function<bool(std::size_t)>& cancelled,const ReplayCheckpoint& checkpoint={}) {
    for(std::size_t index=run.queries.size();index<run.task.paths.size();++index) {
        if(checkpoint) checkpoint();
        const auto& path=run.task.paths[index];
        const auto done=run.queries.size();
        if((run.task.cancel_after_queries && done>=*run.task.cancel_after_queries) || (cancelled && cancelled(done))) {
            run.task.cancel_after_queries=done;run.status="INCOMPLETE";run.stop_reason="Cancelled";break;
        }
        if(run.task.query_steps && done>=*run.task.query_steps) {
            run.status="INCOMPLETE";run.stop_reason="QueryBudgetExceeded";break;
        }
        auto query=cache.evaluate(grid_task(run.task.spec,path));
        run.trace.push_back("QUERY index="+std::to_string(run.queries.size())+" key="+query.key_digest+
            " cache="+query.cache_event+" bytes="+std::to_string(cache.stats().bytes)+
            " evictions="+std::to_string(cache.stats().evictions));
        run.queries.push_back(std::move(query));
    }
    if(bounded(run.task)) run.trace.push_back("END status="+run.status+" reason="+run.stop_reason+" processed="+std::to_string(run.queries.size()));
    run.cache=cache.stats();return run;
}
}
namespace {
GridBatchRun evaluate_batch(GridBatchTask task,const std::function<bool(std::size_t)>& cancelled,const ReplayCheckpoint& checkpoint) {
    if(task.paths.empty() || task.paths.size()>max_grid_batch_queries) throw Error("GridQueryLimit");
    if(task.query_steps && *task.query_steps>max_grid_batch_queries) throw Error("InvalidGridQueryBudget");
    if(task.cancel_after_queries && *task.cancel_after_queries>task.paths.size()) throw Error("InvalidGridCancellation");
    std::size_t bytes=0;
    for(const auto& path:task.paths) {
        if(checkpoint) checkpoint();
        if(path.size()>max_grid_batch_input_bytes-bytes) throw Error("GridBatchInputByteLimit");
        bytes+=path.size();
    }
    if(bounded(task) || cancelled) for(const auto& path:task.paths) {if(checkpoint) checkpoint();(void)canonical_grid_path(path);}
    (void)grid_spec(grid_task(task.spec,""));
    GridCache cache(task.cache_bytes);
    GridBatchRun run;run.task=std::move(task);
    return finish_batch(std::move(run),cache,cancelled,checkpoint);
}
}
GridBatchRun evaluate_grid_batch(GridBatchTask task,const std::function<bool(std::size_t)>& cancelled) {
    return evaluate_batch(std::move(task),cancelled,{});
}

std::string detail::render_grid_batch_records(const GridBatchRun& run,const ReplayCheckpoint& records) {
    std::string out="{\"spec\":"+render_grid_spec(run.task.spec)+",\"cache_policy\":"+json_string(cache_policy)+
        ",\"cache_byte_limit\":"+std::to_string(run.task.cache_bytes)+",\"status\":"+json_string(run.status)+",\"evaluations\":"+
        std::to_string(run.cache.evaluations)+",\"cache\":"+stats_json(run.cache)+",\"queries\":[";
    bool first=true;
    for(const auto& q:run.queries) {
        if(records) records();
        if(!first) out+=',';
        first=false;
        out+="{\"key\":"+json_string(q.key_digest)+",\"cache_event\":"+json_string(q.cache_event)+
            ",\"result\":"+render_grid(q.run)+"}";
    }
    out+="]";
    if(bounded(run.task)) out+=",\"stop_reason\":"+json_string(run.stop_reason)+
        ",\"processed_queries\":"+std::to_string(run.queries.size())+",\"requested_queries\":"+std::to_string(run.task.paths.size());
    return out+"}";
}
std::string render_grid_batch(const GridBatchRun& run) {return detail::render_grid_batch_records(run,{});}
std::string pack_grid_batch(const GridBatchRun& run,const ReplayCheckpoint& checkpoint,const ReplayCheckpoint& records) {
    const auto body=payload(run,records);
    const auto bundle=std::string(bounded(run.task) ? partial_header : header)+sha256(body,checkpoint)+"\n"+body;
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    return bundle;
}
GridBatchRun replay_grid_batch(std::string_view bundle,const ReplayCheckpoint& checkpoint) {
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    const bool partial=bundle.starts_with(partial_header);
    if(!partial && !bundle.starts_with(header)) throw Error("UnknownBundleVersion");
    bundle.remove_prefix(partial ? partial_header.size() : header.size());
    if(bundle.size()<65 || bundle[64]!='\n') throw Error("InvalidDigestHeader");
    const auto body=bundle.substr(65);
    if(sha256(body)!=bundle.substr(0,64)) throw Error("InvalidEvidence: digest mismatch");
    if(checkpoint) checkpoint();
    std::istringstream input{std::string(body)};
    if(field(input,"spec")!=GridSpec::version || field(input,"engine")!=GridSpec::engine ||
       field(input,"address_rule")!=GridSpec::address_rule || field(input,"value_rule")!=GridSpec::value_rule ||
       field(input,"policy")!=GridSpec::policy || field(input,"domain")!=GridSpec::domain ||
       field(input,"cache_policy")!=cache_policy) throw Error("UnknownGridSpecOrPolicy");
    GridBatchTask task;
    if(partial) {
        if(field(input,"execution_policy")!="query-boundaries-v1") throw Error("UnknownGridExecutionPolicy");
        const auto steps=field(input,"query_steps"),cancel=field(input,"cancel_after");
        if(steps!="none") task.query_steps=integer<std::size_t>(steps);
        if(cancel!="none") task.cancel_after_queries=integer<std::size_t>(cancel);
        if(!bounded(task)) throw Error("MissingGridExecutionBoundary");
    }
    task.spec.mode=field(input,"mode");task.spec.max_bits=integer<unsigned>(field(input,"max_bits"));
    const auto modulus=field(input,"modulus");
    if(modulus!="none") task.spec.modulus=integer<std::uint64_t>(modulus);
    task.spec.zero_zero=field(input,"zero_zero");task.cache_bytes=integer<std::size_t>(field(input,"cache_bytes"));
    const auto count=integer<std::size_t>(field(input,"query_count"));
    if(count<1 || count>max_grid_batch_queries) throw Error("GridQueryLimit");
    for(std::size_t i=0;i<count;++i) {if(checkpoint) checkpoint();task.paths.push_back(field(input,"path"));}
    auto run=evaluate_batch(std::move(task),{},checkpoint);
    if(checkpoint) checkpoint();
    if(payload(run)!=body) throw Error("ReplayMismatch: grid batch, cache, result or trace differs");
    return run;
}
}

namespace bruce::detail {
GridBatchRun continue_validated_grid_batch(GridBatchRun run,std::size_t additional,
    const std::function<bool(std::size_t)>& cancelled,const ReplayCheckpoint& checkpoint,const ReplayCheckpoint& cache_checkpoint) {
    GridCache cache(run.task.cache_bytes);
    for(std::size_t i=0;i<run.queries.size();++i) {if(cache_checkpoint) cache_checkpoint();(void)cache.evaluate(grid_task(run.task.spec,run.task.paths[i]));}
    if(!run.trace.empty() && run.trace.back().starts_with("END ")) run.trace.pop_back();
    run.task.query_steps=std::min(run.task.paths.size(),run.queries.size()+additional);
    run.task.cancel_after_queries.reset();run.status="COMPLETED";run.stop_reason="Completed";
    return finish_batch(std::move(run),cache,cancelled,checkpoint);
}
}
