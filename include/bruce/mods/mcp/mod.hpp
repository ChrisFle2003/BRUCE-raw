#pragma once

#include "bruce/mod/mod_api.hpp"

namespace bruce {

[[nodiscard]] const ModManifestV1& mcp_mod_manifest_v1();
int mcp_cli(int argc, char** argv);

} // namespace bruce
