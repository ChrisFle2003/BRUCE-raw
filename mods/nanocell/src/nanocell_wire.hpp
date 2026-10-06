#pragma once

#include "bruce/mods/nanocell/nanocell.hpp"

namespace bruce::nanocell_detail {

[[nodiscard]] std::string pack_nanocell_request_v1(const NanoCellRequestV1& request);
[[nodiscard]] NanoCellRequestV1 unpack_nanocell_request_v1(std::string_view bytes);

} // namespace bruce::nanocell_detail
