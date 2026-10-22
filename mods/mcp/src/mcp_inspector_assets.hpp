#pragma once

#include <string>

namespace bruce {

[[nodiscard]] const std::string& mcp_inspector_html();
// Preserve the existing asset entry point for callers.
[[nodiscard]] const std::string& mcp_inspector_html_v1();

} // namespace bruce
