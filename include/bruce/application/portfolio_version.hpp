#pragma once
#include "bruce/application/exact_research.hpp"
#include <optional>

namespace bruce {
// Shared wire identity for all supported five-family portfolio generations.
struct PortfolioVersion {
    std::string header,engine,planner;
};
PortfolioVersion portfolio_version(const ExactResearchTask&);
std::optional<PortfolioVersion> portfolio_version_from_header(std::string_view);
}
