#include "bruce/mods/mcp/mod.hpp"
#include "mcp_inspector_server.hpp"

#include <charconv>
#include <string_view>
#include <system_error>

namespace bruce {

const ModManifestV1& mcp_mod_manifest_v1() {
    static const ModManifestV1 manifest{
        .id = "mcp",
        .version = "mcp-mod-v1",
        .api_version = std::string(mod_api_version_v1),
        .capabilities = {ModCapabilityV1::presentation},
    };
    return manifest;
}

int mcp_cli(const int argc, char** argv) {
    std::uint16_t port = 8080;
    if (argc != 2) {
        if (argc != 4 || std::string_view(argv[2]) != "--port") {
            throw Error("InvalidMcpArguments");
        }
        const std::string_view text(argv[3]);
        unsigned int value = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 10);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
            value == 0 || value > 65535) {
            throw Error("InvalidMcpPort");
        }
        port = static_cast<std::uint16_t>(value);
    }
    return run_mcp_inspector_server(port);
}

} // namespace bruce
