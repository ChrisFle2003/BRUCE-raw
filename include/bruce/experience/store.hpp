#pragma once
#include "bruce/core/types.hpp"
#include "bruce/core/replay_control.hpp"
namespace bruce {
inline constexpr std::size_t experience_byte_limit=1024*1024;
struct ExperienceRecord {
    std::string id,source_digest,training_fingerprint,context,replaces;
    std::vector<std::uint64_t> votes{0,0,0};
    bool grid_origin=false,active=true,indexed=false,selected=false,powered=false,cached=false,rational_q=false,modular=false,domain_cached=false,witnessed=false;
    std::uint64_t audit_operations=0;
};
struct ExperienceCorrection { std::string id,target,reason; std::string replacement={}; };
struct ExperienceStore {
    bool replacements=false,grid_origin=false,quadratic=false,expression=false,indexed=false,selected=false,powered=false,cached=false,rational_q=false,modular=false,domain_cached=false,witnessed=false;
    std::string digest;
    std::uint64_t audit_operations=0;
    std::vector<std::string> event_bodies;
    std::vector<ExperienceRecord> records;
    std::vector<ExperienceCorrection> corrections;
};
// Audits one source without reading or mutating a store; throws existing import errors.
void audit_experience_source(std::string_view source,const ReplayCheckpoint& checkpoint={});
ExperienceStore read_experience(std::string_view snapshot,const ReplayCheckpoint& checkpoint={});
std::string append_experience(std::string_view previous,std::string_view source,const ReplayCheckpoint& checkpoint={});
std::string revoke_experience(std::string_view previous,std::string_view target,std::string_view reason,const ReplayCheckpoint& checkpoint={});
std::string replace_experience(std::string_view previous,std::string_view target,std::string_view reason,std::string_view source,const ReplayCheckpoint& checkpoint={});
std::string render_experience(const ExperienceStore&);
}
