#pragma once
#include "bruce/application/exact_research.hpp"

namespace bruce {
struct ExactEvidenceChainRun {
    ExactResearchRun research;
    std::string training_csv, holdout_csv;
};
ExactEvidenceChainRun create_exact_evidence_chain(
    ExactResearchTask task,std::string training_csv,std::string holdout_csv,
    const std::function<bool(std::uint64_t)>& cancelled={});
std::string render_exact_evidence_chain(const ExactEvidenceChainRun&);
std::string pack_exact_evidence_chain(const ExactEvidenceChainRun&,const ReplayCheckpoint& checkpoint={});
ExactEvidenceChainRun replay_exact_evidence_chain(
    std::string_view bundle,std::uint64_t operation_limit=10000000,
    const ReplayCheckpoint& checkpoint={});
}
