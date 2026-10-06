#pragma once
#include "bruce/experience/store.hpp"
namespace bruce {
// Replays and audits the supplied source; no destination-store or rule authorization.
ExperienceRecord verify_experience_source(std::string_view,const ReplayCheckpoint& checkpoint={});
}
