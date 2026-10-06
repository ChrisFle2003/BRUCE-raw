#include "bruce/compatibility/snapshot.hpp"
#include "bruce/coordinate/address.hpp"
#include <algorithm>
#include <set>
#include <sstream>

namespace bruce {
bool identifier(std::string_view text) {
    if(text.empty() || text.size()>64 || text=="-") return false;
    return std::all_of(text.begin(),text.end(),[](unsigned char c) {
        return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-' || c=='.';
    });
}
namespace {
std::vector<std::string> words(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> result;
    for(std::string word;in>>word;) result.push_back(word);
    return result;
}
void validate_candidates(std::vector<ZCandidate>& candidates) {
    if(candidates.empty() || candidates.size()>4096) throw Error("InvalidCandidateCount: expected 1..4096");
    std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b) { return a.id<b.id; });
    std::optional<std::uint64_t> previous;
    for(auto& candidate:candidates) {
        if(previous && *previous==candidate.id) throw Error("DuplicateCandidateId");
        previous=candidate.id;
        if(!identifier(candidate.domain)) throw Error("InvalidDomainId");
        if(candidate.domain!=candidates.front().domain || candidate.layer!=candidates.front().layer)
            throw Error("MixedCoordinateContext");
        candidate.z=pair(candidate.x,candidate.y);
    }
}
bool matches(const CompatibilityRule& rule,const ZCandidate& c) {
    const __int128_t value=rule.field=="x" ? __int128_t(c.x) : rule.field=="y" ? __int128_t(c.y) :
        rule.field=="z" ? __int128_t(c.z) : rule.field=="layer" ? __int128_t(c.layer) : __int128_t(c.value);
    const __int128_t argument=rule.argument;
    if(rule.operation=="eq") return value==argument;
    if(rule.operation=="ne") return value!=argument;
    if(rule.operation=="lt") return value<argument;
    if(rule.operation=="le") return value<=argument;
    if(rule.operation=="gt") return value>argument;
    if(rule.operation=="ge") return value>=argument;
    // Euclidean remainder also works at INT64_MIN, with positive modulus.
    return (value%argument+argument)%argument==rule.remainder;
}
}

CompatibilitySnapshot::CompatibilitySnapshot(std::string_view text,const ReplayCheckpoint& checkpoint) {
    if(checkpoint)checkpoint();
    if(text.size()>1024*1024) throw Error("SnapshotByteLimit");
    std::istringstream input{std::string(text)};
    std::string line;
    if(!std::getline(input,line) || words(line)!=std::vector<std::string>{"BRUCE_RULES_V1"})
        throw Error("UnknownRuleVersion");
    if(!std::getline(input,line)) throw Error("MissingSnapshotId");
    const auto header=words(line);
    if(header.size()!=2 || header[0]!="snapshot" || !identifier(header[1])) throw Error("InvalidSnapshotId");
    id_=header[1];
    std::set<std::string> seen;
    while(std::getline(input,line)) {
        if(checkpoint)checkpoint();
        const auto row=words(line);
        if(row.size()!=10 || row[0]!="rule") throw Error("InvalidRuleRow: expected 10 fields");
        CompatibilityRule rule{row[1],row[2],row[3],row[4],integer<std::int64_t>(row[5]),
                               integer<std::int64_t>(row[6]),integer<std::int64_t>(row[7]),row[9]};
        if(!identifier(rule.id) || !seen.insert(rule.id).second) throw Error("InvalidOrDuplicateRuleId");
        if(!std::set<std::string>{"positive","reject","negative","context"}.contains(rule.kind)) throw Error("UnknownRuleKind");
        if(!std::set<std::string>{"x","y","z","value","layer"}.contains(rule.field)) throw Error("UnknownRuleField");
        if(!std::set<std::string>{"eq","ne","lt","le","gt","ge","mod_eq"}.contains(rule.operation)) throw Error("UnknownRuleOperator");
        if(row[8]!="manual") throw Error("UnsupportedRuleSource");
        if(rule.kind=="negative" ? !identifier(rule.evidence) : rule.evidence!="-") throw Error("InvalidEvidenceReference");
        if(rule.weight < -1000000 || rule.weight>1000000 || (rule.kind!="positive" && rule.weight!=0)) throw Error("InvalidRuleWeight");
        if(rule.operation=="mod_eq") {
            if(rule.argument<=0 || rule.remainder<0 || rule.remainder>=rule.argument) throw Error("InvalidModularPredicate");
        } else if(rule.remainder!=0) throw Error("UnusedRuleArgumentMustBeZero");
        rules_.push_back(std::move(rule));
        if(rules_.size()>128) throw Error("RuleBudgetExceeded");
    }
}

std::string CompatibilitySnapshot::canonical(const ReplayCheckpoint& checkpoint) const {
    if(checkpoint)checkpoint();
    std::ostringstream out;
    out << "BRUCE_RULES_V1\nsnapshot " << id_ << '\n';
    for(const auto& rule:rules_) {
        if(checkpoint)checkpoint();
        out << "rule " << rule.id << ' ' << rule.kind << ' ' << rule.field << ' ' << rule.operation << ' '
            << rule.argument << ' ' << rule.remainder << ' ' << rule.weight << " manual " << rule.evidence << '\n';
    }
    return out.str();
}

std::vector<ZCandidate> parse_candidates(std::string_view text) {
    if(text.size()>1024*1024) throw Error("CandidateByteLimit");
    std::istringstream in{std::string(text)};
    std::string line;
    if(!std::getline(in,line)) throw Error("MissingCandidateHeader");
    if(!line.empty() && line.back()=='\r') line.pop_back();
    if(line!="id,domain,layer,x,y,value") throw Error("InvalidCandidateHeader");
    std::vector<ZCandidate> result;
    while(std::getline(in,line)) {
        if(!line.empty() && line.back()=='\r') line.pop_back();
        std::vector<std::string_view> columns;
        std::string_view rest=line;
        while(true) {
            const auto comma=rest.find(',');
            columns.push_back(rest.substr(0,comma));
            if(comma==std::string_view::npos) break;
            rest.remove_prefix(comma+1);
        }
        if(columns.size()!=6) throw Error("InvalidCandidateRow");
        result.push_back({integer<std::uint64_t>(columns[0]),integer<std::uint64_t>(columns[2]),
            integer<std::uint64_t>(columns[3]),integer<std::uint64_t>(columns[4]),0,
            integer<std::int64_t>(columns[5]),std::string(columns[1])});
        if(result.size()>4096) throw Error("CandidateInputLimit");
    }
    validate_candidates(result);
    return result;
}
std::string canonical_candidates(std::vector<ZCandidate> candidates) {
    validate_candidates(candidates);
    std::ostringstream out;
    out << "id,domain,layer,x,y,value\n";
    for(const auto& c:candidates) out << c.id << ',' << c.domain << ',' << c.layer << ',' << c.x << ',' << c.y << ',' << c.value << '\n';
    return out.str();
}

ZResult evaluate_compatibility(const CompatibilitySnapshot& snapshot,std::vector<ZCandidate> candidates,
    ZBudget budget,const std::function<bool(std::uint64_t)>& cancelled,const ReplayCheckpoint& rules_checkpoint) {
    validate_candidates(candidates);
    if(!budget.candidates || budget.candidates>4096 || !budget.checks || budget.checks>1048576 || !budget.trace || budget.trace>65536)
        throw Error("InvalidCompatibilityBudget");
    ZResult result;
    result.budget=budget;
    result.groups.resize((candidates.size()+63)/64);
    for(std::size_t i=0;i<candidates.size();++i) {
        result.candidates.push_back({candidates[i]});
        result.groups[i/64].candidate_ids.push_back(candidates[i].id);
    }
    for(std::size_t i=0;i<candidates.size();++i) {
        if((budget.cancel_after_candidates && result.started>=*budget.cancel_after_candidates) || (cancelled && cancelled(result.started))) {
            result.budget.cancel_after_candidates=result.started;
            result.stop_reason="Cancelled"; break;
        }
        if(result.started>=budget.candidates) { result.stop_reason="CandidateBudgetExceeded"; break; }
        ++result.started;
        auto& assessment=result.candidates[i];
        auto& group=result.groups[i/64];
        const auto bit=std::uint64_t{1}<<(i%64);
        for(const auto& rule:snapshot.rules()) {
            if(result.checks>=budget.checks) { result.stop_reason="OperationBudgetExceeded"; break; }
            if(result.trace.size()>=budget.trace) { result.stop_reason="TraceBudgetExceeded"; break; }
            if(rules_checkpoint)rules_checkpoint();
            ++result.checks; ++assessment.rules_checked;
            const bool match=matches(rule,assessment.candidate);
            std::string effect="no_change";
            if(rule.kind=="positive" && match) {
                assessment.score+=rule.weight; group.positive_match_mask|=bit; effect="score";
            } else if(rule.kind=="reject" && match) {
                assessment.hard_rejected=true; group.hard_reject_mask|=bit; effect="hard_reject";
            } else if(rule.kind=="negative" && match) {
                assessment.negative=true; group.negative_evidence_mask|=bit; effect="negative_evidence";
            } else if(rule.kind=="context" && !match) {
                assessment.context_matches=false; effect="context_reject";
            }
            result.trace.push_back({assessment.candidate.id,rule.id,match,effect});
        }
        assessment.complete=assessment.rules_checked==snapshot.rules().size();
        if(assessment.complete) {
            group.complete_mask|=bit;
            if(assessment.context_matches) group.context_role_mask|=bit;
        }
        if(result.stop_reason!="Completed") break;
    }
    for(auto& group:result.groups) {
        // Only fully evaluated, context-compatible lanes may survive. The
        // complete mask also clears unused bits in the last 64-lane group.
        group.eligible_mask=group.complete_mask & group.context_role_mask
                            & ~group.hard_reject_mask & ~group.negative_evidence_mask;
    }
    if(result.stop_reason!="Completed") result.status="INCOMPLETE";
    else {
        std::vector<RankedCandidate> ranked;
        for(std::size_t i=0;i<result.candidates.size();++i) {
            const auto& c=result.candidates[i];
            const bool eligible=(result.groups[i/64].eligible_mask & (std::uint64_t{1}<<(i%64)))!=0;
            ranked.push_back({c.candidate.id,c.score,0,!eligible});
        }
        result.selection=select_candidates(ranked);
        result.status=result.selection.ids.empty() ? "NO_COMPATIBLE_CANDIDATE" : result.selection.ambiguous ? "AMBIGUOUS" : "SELECTED";
    }
    return result;
}
}
