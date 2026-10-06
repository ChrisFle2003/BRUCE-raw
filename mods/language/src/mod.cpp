#include "bruce/mods/language/mod.hpp"

namespace bruce {

const ModManifestV1& language_mod_manifest_v1() {
    static const ModManifestV1 manifest{
        .id = "language",
        .version = "language-mod-v1",
        .api_version = std::string(mod_api_version_v1),
        .capabilities = {ModCapabilityV1::language_interpretation,
                         ModCapabilityV1::core_task_submission},
    };
    return manifest;
}

} // namespace bruce
