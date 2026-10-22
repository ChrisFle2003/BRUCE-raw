#pragma once

#include <cstdint>

namespace bruce {

// Serves the read-only inspector on the requested IPv4 loopback port.
int run_mcp_inspector_server(std::uint16_t port);

} // namespace bruce
