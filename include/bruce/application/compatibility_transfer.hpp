#pragma once
#include "bruce/application/compatibility.hpp"
namespace bruce {
// A content-bound selection task, not a permission or cached verification.
class CompatibilityTransfer {
public:
    static CompatibilityTransfer plan(const std::string& source,const std::string& rules,
        std::int64_t at,ZBudget budget={},std::size_t max_source=33554432,const ReplayCheckpoint& checkpoint={});
    explicit CompatibilityTransfer(const std::string& bytes);
    CompatibilityRun apply(const std::string& source,const std::string& rules,const ReplayCheckpoint& checkpoint={}) const;
    std::string bytes() const;
    std::string report() const;
    // Formats the supplied result; does not independently verify it.
    std::string result_report(const CompatibilityRun&) const;
    std::size_t max_source() const {return max_source_;}
private:
    CompatibilityTransfer()=default;
    std::string source_digest_,rules_digest_;
    std::int64_t at_=0;
    ZBudget budget_;
    std::size_t max_source_=33554432;
    void validate() const;
};
}
