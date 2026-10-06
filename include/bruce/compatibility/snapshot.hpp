#pragma once
#include "bruce/compatibility/selection.hpp"
#include <functional>
#include "bruce/core/replay_control.hpp"

namespace bruce {
struct CompatibilityRule {
    std::string id, kind, field, operation;
    std::int64_t argument{}, remainder{}, weight{};
    std::string evidence;
};
class CompatibilitySnapshot {
public:
    explicit CompatibilitySnapshot(std::string_view text,const ReplayCheckpoint& checkpoint={});
    const std::string& id() const { return id_; }
    const std::vector<CompatibilityRule>& rules() const { return rules_; }
    std::string canonical(const ReplayCheckpoint& checkpoint={}) const;
private:
    std::string id_;
    std::vector<CompatibilityRule> rules_;
};
struct ZCandidate {
    std::uint64_t id{}, layer{}, x{}, y{}, z{};
    std::int64_t value{};
    std::string domain;
};
struct ZBudget {
    std::uint64_t candidates=4096, checks=1048576, trace=65536;
    std::optional<std::uint64_t> cancel_after_candidates;
};
struct FilterGroup {
    std::vector<std::uint64_t> candidate_ids;
    std::uint64_t positive_match_mask=0, hard_reject_mask=0, negative_evidence_mask=0;
    std::uint64_t context_role_mask=0, complete_mask=0, eligible_mask=0;
};
struct ZAssessment {
    ZCandidate candidate;
    std::int64_t score=0;
    std::uint64_t rules_checked=0;
    bool complete=false, hard_rejected=false, negative=false, context_matches=true;
};
struct RuleTrace {
    std::uint64_t candidate_id;
    std::string rule_id;
    bool matched;
    std::string effect;
};
struct ZResult {
    std::string status, stop_reason="Completed";
    ZBudget budget;
    std::uint64_t started=0, checks=0;
    std::vector<ZAssessment> candidates;
    std::vector<FilterGroup> groups;
    std::vector<RuleTrace> trace;
    Selection selection;
};
bool identifier(std::string_view text);
std::vector<ZCandidate> parse_candidates(std::string_view text);
std::string canonical_candidates(std::vector<ZCandidate> candidates);
ZResult evaluate_compatibility(const CompatibilitySnapshot& snapshot,std::vector<ZCandidate> candidates,
    ZBudget budget={},const std::function<bool(std::uint64_t)>& cancelled={},const ReplayCheckpoint& rules_checkpoint={});
}
