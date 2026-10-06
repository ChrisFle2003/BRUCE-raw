#pragma once
#include "bruce/application/experience_task.hpp"
#include <cstddef>
namespace bruce {
// An explicit add-only transfer, never an authorization or cached audit result.
class ExperienceTransfer {
public:
    static ExperienceTransfer plan(const std::string& source,const std::string& previous,
                                  std::size_t max_source=1048576,std::size_t max_previous=1048576,const ReplayCheckpoint& checkpoint={});
    explicit ExperienceTransfer(const std::string& bytes);
    ExperienceResult apply(const std::string& source,const std::string& previous,const ReplayCheckpoint& checkpoint={}) const;
    std::string bytes() const;
    std::string report() const;
    std::string result_report(const ExperienceResult&) const;
    std::size_t max_source() const {return max_source_;}
    std::size_t max_previous() const {return max_previous_;}
private:
    ExperienceTransfer()=default;
    std::string source_digest_,previous_digest_;
    std::size_t max_source_=1048576,max_previous_=1048576;
    void check_sizes(const std::string&,const std::string&) const;
};
}
