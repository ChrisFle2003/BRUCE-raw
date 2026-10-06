#include "bruce/experience/store.hpp"
#include "bruce/application/experience_source.hpp"
#include "bruce/core/json.hpp"
#include "bruce/evidence/bundle.hpp"
#include <algorithm>
#include <sstream>
#include <set>

namespace bruce {
namespace {
constexpr std::string_view prefix="BRUCE_EXPERIENCE_V1\n";
struct Reader {
    std::string_view data;
    std::string field(std::string_view key) {
        const auto end=data.find('\n');
        const auto start=std::string(key)+" ";
        if(end==std::string_view::npos || !data.substr(0,end).starts_with(start)) throw Error("InvalidExperienceField");
        const auto value=std::string(data.substr(start.size(),end-start.size())); data.remove_prefix(end+1); return value;
    }
    std::string block(std::string_view key) {
        const auto n=integer<std::size_t>(field(key));
        if(n>experience_byte_limit || n>data.size()) throw Error("ExperienceByteLimit");
        auto result=std::string(data.substr(0,n)); data.remove_prefix(n); return result;
    }
};
std::string pack(const ExperienceStore& store) {
    std::string body=std::string(store.replacements ? "policy routing-experience-v13\nevents " : store.grid_origin ? "policy routing-experience-v12\nevents " : store.witnessed ? "policy routing-experience-v11\nevents " : store.domain_cached ? "policy routing-experience-v10\nevents " : store.modular ? "policy routing-experience-v9\nevents " : store.rational_q ? "policy routing-experience-v8\nevents " : store.cached ? "policy routing-experience-v7\nevents " : store.powered ? "policy routing-experience-v6\nevents " : store.selected ? "policy routing-experience-v5\nevents " : store.indexed ? "policy routing-experience-v4\nevents " : store.expression ? "policy routing-experience-v3\nevents " : store.quadratic ? "policy routing-experience-v2\nevents " : "policy routing-experience-v1\nevents ")+std::to_string(store.event_bodies.size())+"\n";
    for(const auto& event:store.event_bodies) body+="event_bytes "+std::to_string(event.size())+"\n"+event;
    const auto output=std::string(store.replacements ? "BRUCE_EXPERIENCE_V13\n" : store.grid_origin ? "BRUCE_EXPERIENCE_V12\n" : store.witnessed ? "BRUCE_EXPERIENCE_V11\n" : store.domain_cached ? "BRUCE_EXPERIENCE_V10\n" : store.modular ? "BRUCE_EXPERIENCE_V9\n" : store.rational_q ? "BRUCE_EXPERIENCE_V8\n" : store.cached ? "BRUCE_EXPERIENCE_V7\n" : store.powered ? "BRUCE_EXPERIENCE_V6\n" : store.selected ? "BRUCE_EXPERIENCE_V5\n" : store.indexed ? "BRUCE_EXPERIENCE_V4\n" : store.expression ? "BRUCE_EXPERIENCE_V3\n" : store.quadratic ? "BRUCE_EXPERIENCE_V2\n" : prefix)+sha256(body)+"\n"+body;
    if(output.size()>experience_byte_limit) throw Error("ExperienceByteLimit");
    return output;
}
std::string event_header(const ExperienceStore& store,std::string_view kind) {
    if(store.event_bodies.size()>=16) throw Error("ExperienceEventLimit");
    return "sequence "+std::to_string(store.event_bodies.size()+1)+"\nparent "+
        (store.event_bodies.empty() ? "none" : sha256(store.event_bodies.back()))+"\nkind "+std::string(kind)+"\n";
}
void apply_event(ExperienceStore& store,const std::string& event,const ReplayCheckpoint& checkpoint={}) {
    Reader reader{event};
    if(reader.field("sequence")!=std::to_string(store.event_bodies.size()+1) ||
       reader.field("parent")!=(store.event_bodies.empty() ? "none" : sha256(store.event_bodies.back())))
        throw Error("InvalidExperienceChain");
    const auto kind=reader.field("kind"),id=sha256(event);
    if(kind=="add" || kind=="replace") {
        std::string target,reason;
        std::set<std::string> ancestors;
        if(kind=="replace") {
            target=reader.field("target");reason=reader.field("reason");
            if(reason.empty() || reason.size()>256 || reason.find_first_of("\r\n")!=std::string::npos || reason.find('\0')!=std::string::npos)
                throw Error("InvalidExperienceCorrectionReason");
            auto found=std::find_if(store.records.begin(),store.records.end(),[&](const auto& r){return r.id==target;});
            if(found==store.records.end() || !found->active) throw Error("UnknownOrRevokedExperience");
            auto ancestor=target;
            while(!ancestor.empty()) {
                if(!ancestors.insert(ancestor).second) throw Error("InvalidExperienceReplacementChain");
                const auto old=std::find_if(store.records.begin(),store.records.end(),[&](const auto& r){return r.id==ancestor;});
                if(old==store.records.end()) throw Error("InvalidExperienceReplacementChain");
                ancestor=old->replaces;
            }
        }
        if(store.records.size()>=8) throw Error("ExperienceRecordLimit");
        auto record=verify_experience_source(reader.block("source_bytes"),checkpoint);
        for(const auto& old:store.records) if(old.training_fingerprint==record.training_fingerprint && !ancestors.contains(old.id))
            throw Error("DuplicateExperienceTrainingInput");
        if(kind=="replace") {
            auto found=std::find_if(store.records.begin(),store.records.end(),[&](const auto& r){return r.id==target;});
            if(found->source_digest==record.source_digest) throw Error("UnchangedExperienceReplacement");
            found->active=false;record.replaces=target;store.replacements=true;
            store.corrections.push_back({id,target,reason,id});
        }
        if(record.grid_origin) store.grid_origin=true;
        if(record.witnessed) store.witnessed=true;
        if(record.domain_cached) store.domain_cached=true;
        if(record.modular) store.modular=true;
        if(record.rational_q) store.rational_q=true;
        if(record.cached) store.cached=true;
        if(record.powered) store.powered=true;
        if(record.selected) store.selected=true;
        if(record.indexed) store.indexed=true;
        if(record.votes.size()==5) store.expression=true;
        if(record.votes.size()==4) store.quadratic=true;
        record.id=id; store.audit_operations+=record.audit_operations; store.records.push_back(std::move(record));
    } else if(kind=="revoke") {
        const auto target=reader.field("target"),reason=reader.field("reason");
        if(reason.empty() || reason.size()>256 || reason.find_first_of("\r\n")!=std::string::npos || reason.find('\0')!=std::string::npos)
            throw Error("InvalidExperienceCorrectionReason");
        auto found=std::find_if(store.records.begin(),store.records.end(),[&](const auto& r){return r.id==target;});
        if(found==store.records.end() || !found->active) throw Error("UnknownOrRevokedExperience");
        found->active=false; store.corrections.push_back({id,target,reason});
    } else throw Error("UnknownExperienceEvent");
    if(!reader.data.empty()) throw Error("TrailingExperienceEventData");
    store.event_bodies.push_back(event);
}
}
static ExperienceStore read_experience_impl(std::string_view snapshot,const ReplayCheckpoint& checkpoint) {
    if(snapshot.size()>experience_byte_limit) throw Error("ExperienceByteLimit");
    const bool replaced=snapshot.starts_with("BRUCE_EXPERIENCE_V13\n");
    const bool grid=snapshot.starts_with("BRUCE_EXPERIENCE_V12\n");
    const bool witnessed=snapshot.starts_with("BRUCE_EXPERIENCE_V11\n");
    const bool domain_cached=snapshot.starts_with("BRUCE_EXPERIENCE_V10\n");
    const bool modular=snapshot.starts_with("BRUCE_EXPERIENCE_V9\n");
    const bool rational_q=snapshot.starts_with("BRUCE_EXPERIENCE_V8\n");
    const bool cached=snapshot.starts_with("BRUCE_EXPERIENCE_V7\n");
    const bool powered=snapshot.starts_with("BRUCE_EXPERIENCE_V6\n");
    const bool selected=snapshot.starts_with("BRUCE_EXPERIENCE_V5\n");
    const bool indexed=snapshot.starts_with("BRUCE_EXPERIENCE_V4\n");
    const bool five=snapshot.starts_with("BRUCE_EXPERIENCE_V3\n");
    const bool four=snapshot.starts_with("BRUCE_EXPERIENCE_V2\n");
    if(!replaced && !grid && !witnessed && !domain_cached && !modular && !rational_q && !cached && !powered && !selected && !indexed && !five && !four && !snapshot.starts_with(prefix)) throw Error("UnknownExperienceVersion");
    auto remaining=snapshot.substr((replaced || grid || witnessed || domain_cached) ? std::string_view("BRUCE_EXPERIENCE_V10\n").size() : prefix.size());
    if(remaining.size()<65 || remaining[64]!='\n') throw Error("InvalidExperienceDigestHeader");
    const auto digest=remaining.substr(0,64),body=remaining.substr(65);
    if(sha256(body)!=digest) throw Error("InvalidExperienceDigest");
    if(checkpoint) checkpoint();
    Reader reader{body};
    if(reader.field("policy")!=(replaced ? "routing-experience-v13" : grid ? "routing-experience-v12" : witnessed ? "routing-experience-v11" : domain_cached ? "routing-experience-v10" : modular ? "routing-experience-v9" : rational_q ? "routing-experience-v8" : cached ? "routing-experience-v7" : powered ? "routing-experience-v6" : selected ? "routing-experience-v5" : indexed ? "routing-experience-v4" : five ? "routing-experience-v3" : four ? "routing-experience-v2" : "routing-experience-v1")) throw Error("UnknownExperiencePolicy");
    const auto count=integer<std::size_t>(reader.field("events"));
    if(count>16) throw Error("ExperienceEventLimit");
    ExperienceStore store; store.digest=std::string(digest);
    for(std::size_t i=0;i<count;++i) {
        if(checkpoint) checkpoint();
        apply_event(store,reader.block("event_bytes"),checkpoint);
    }
    if(checkpoint) checkpoint();
    if(!reader.data.empty() || pack(store)!=snapshot) throw Error("NoncanonicalExperienceSnapshot");
    return store;
}
ExperienceStore read_experience(std::string_view snapshot,const ReplayCheckpoint& checkpoint) {
    ReplayCheckpoint protected_checkpoint;
    if(checkpoint) protected_checkpoint=[&]{(void)detail::observe_replay_checkpoint(checkpoint);};
    try {return read_experience_impl(snapshot,protected_checkpoint);}
    catch(const detail::ReplayCheckpointRaised& stop){std::rethrow_exception(stop.original);}
}
std::string append_experience(std::string_view previous,std::string_view source,const ReplayCheckpoint& checkpoint) {
    if(checkpoint)checkpoint();
    auto store=previous.empty() ? ExperienceStore{} : read_experience(previous,checkpoint);
    if(source.size()>experience_byte_limit) throw Error("ExperienceByteLimit");
    const auto event=event_header(store,"add")+"source_bytes "+std::to_string(source.size())+"\n"+std::string(source);
    apply_event(store,event,checkpoint);
    if(checkpoint)checkpoint();
    auto result=pack(store);
    if(checkpoint)checkpoint();
    return result;
}
std::string revoke_experience(std::string_view previous,std::string_view target,std::string_view reason,const ReplayCheckpoint& checkpoint) {
    auto store=read_experience(previous,checkpoint);
    if(reason.empty() || reason.size()>256 || reason.find_first_of("\r\n")!=std::string_view::npos || reason.find('\0')!=std::string_view::npos)
        throw Error("InvalidExperienceCorrectionReason");
    const auto event=event_header(store,"revoke")+"target "+std::string(target)+"\nreason "+std::string(reason)+"\n";
    apply_event(store,event,checkpoint); return pack(store);
}
std::string replace_experience(std::string_view previous,std::string_view target,std::string_view reason,std::string_view source,const ReplayCheckpoint& checkpoint) {
    auto store=read_experience(previous,checkpoint);
    if(reason.empty() || reason.size()>256 || reason.find_first_of("\r\n")!=std::string_view::npos || reason.find('\0')!=std::string_view::npos)
        throw Error("InvalidExperienceCorrectionReason");
    if(source.size()>experience_byte_limit) throw Error("ExperienceByteLimit");
    const auto event=event_header(store,"replace")+"target "+std::string(target)+"\nreason "+std::string(reason)+
        "\nsource_bytes "+std::to_string(source.size())+"\n"+std::string(source);
    apply_event(store,event,checkpoint);return pack(store);
}
std::string render_experience(const ExperienceStore& store) {
    std::ostringstream out;
    const auto active=std::count_if(store.records.begin(),store.records.end(),[](const auto& r){return r.active;});
    out<<"{\"engine\":"<<json_string(store.replacements ? "bruce-experience-v13" : store.grid_origin ? "bruce-experience-v12" : store.witnessed ? "bruce-experience-v11" : store.domain_cached ? "bruce-experience-v10" : store.modular ? "bruce-experience-v9" : store.rational_q ? "bruce-experience-v8" : store.cached ? "bruce-experience-v7" : store.powered ? "bruce-experience-v6" : store.selected ? "bruce-experience-v5" : store.indexed ? "bruce-experience-v4" : store.expression ? "bruce-experience-v3" : store.quadratic ? "bruce-experience-v2" : "bruce-experience-v1")
       <<",\"policy\":"<<json_string(store.replacements ? "routing-experience-v13" : store.grid_origin ? "routing-experience-v12" : store.witnessed ? "routing-experience-v11" : store.domain_cached ? "routing-experience-v10" : store.modular ? "routing-experience-v9" : store.rational_q ? "routing-experience-v8" : store.cached ? "routing-experience-v7" : store.powered ? "routing-experience-v6" : store.selected ? "routing-experience-v5" : store.indexed ? "routing-experience-v4" : store.expression ? "routing-experience-v3" : store.quadratic ? "routing-experience-v2" : "routing-experience-v1")<<",\"digest\":"<<json_string(store.digest)
       <<",\"audit_operations\":"<<store.audit_operations<<",\"events\":"<<store.event_bodies.size()<<",\"active_records\":"<<active
       <<",\"learn_gate\":false,\"publish_gate\":false,\"scope\":\"Routing statistics from replayed holdout runs; no promotion of mathematical truth\",\"records\":[";
    for(std::size_t i=0;i<store.records.size();++i) {
        if(i) out<<',';
        const auto& r=store.records[i];
        out<<"{\"id\":"<<json_string(r.id)<<",\"active\":"<<(r.active ? "true" : "false")
           <<",\"source_digest\":"<<json_string(r.source_digest)<<",\"training_fingerprint\":"<<json_string(r.training_fingerprint)
           <<",\"context\":"<<json_string(r.context)<<",\"votes\":[";
        for(std::size_t j=0;j<r.votes.size();++j) {if(j) out<<','; out<<r.votes[j];}
        out<<"]";
        if(store.replacements) out<<",\"replaces\":"<<json_string(r.replaces);
        if(store.grid_origin) out<<",\"grid_origin\":"<<(r.grid_origin ? "true" : "false");
        out<<"}";
    }
    out<<"],\"corrections\":[";
    for(std::size_t i=0;i<store.corrections.size();++i) {
        if(i) out<<',';
        const auto& c=store.corrections[i];
        out<<"{\"id\":"<<json_string(c.id)<<",\"target\":"<<json_string(c.target)<<",\"reason\":"<<json_string(c.reason);
        if(store.replacements) out<<",\"replacement\":"<<json_string(c.replacement);
        out<<'}';
    }
    out<<"]}"; return out.str();
}
}
