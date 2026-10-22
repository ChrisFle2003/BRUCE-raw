#pragma once

#include <string>
#include <string_view>

namespace bruce::artifact_v1 {

// Canonical manifest identity used by the V5 host-side artifact contract.
// The input framing is part of the versioned contract and must not be
// reimplemented by callers (including inspectors).
[[nodiscard]] std::string v5_manifest_id_v1(std::string_view run_id,
                                            std::string_view node_id,
                                            std::string_view attempt_id);

} // namespace bruce::artifact_v1
