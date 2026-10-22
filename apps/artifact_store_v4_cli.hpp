#pragma once

#include "bruce/artifact/artifact_store_v4.hpp"
#include "bruce/core/json.hpp"

#include <iostream>
#include <vector>

namespace bruce::artifact_store_v4_cli_detail {

struct Request {
    bool restore;
    std::string manifest_id;
    std::filesystem::path store;
    std::string remote;
};

inline Request parse(const std::vector<std::string_view>& args) {
    if (args.size() != 6 || (args[0] != "mirror" && args[0] != "restore") ||
        !artifact_v1::is_valid_manifest_id_v1(args[1]) || args[2] != "--store" ||
        args[3].empty() || args[3].starts_with('-') || args[3].find('\0') != std::string_view::npos ||
        args[4] != "--remote") throw Error("PROTOCOL_ERROR");
    // Validate the complete closed grammar before accessing a local path.
    (void)artifact_v3::https_request_v3(args[5], "/manifests/check.mirror",
        std::chrono::steady_clock::now() + std::chrono::seconds(30));
    return {args[0] == "restore", std::string(args[1]), std::filesystem::path(args[3]), std::string(args[5])};
}

inline std::string render(bool restore, const artifact_v4::MirrorResultV4& result) {
    return "{\"contract\":\"artifact-store-mirror-v4\",\"action\":" + json_string(restore ? "restore" : "mirror") +
        ",\"status\":" + json_string(restore ? "RESTORED" : "MIRRORED") +
        ",\"manifest_id\":" + json_string(result.manifest_id) +
        ",\"manifest_sha256\":" + json_string(result.manifest_sha256) +
        ",\"manifest_size_bytes\":" + std::to_string(result.manifest_size_bytes) +
        ",\"object_count\":" + std::to_string(result.object_count) +
        ",\"total_object_bytes\":" + std::to_string(result.total_object_bytes) + "}";
}

} // namespace bruce::artifact_store_v4_cli_detail

namespace bruce {
inline int artifact_store_v4_cli(int argc, char** argv) {
    try {
        if (argc != 8) throw Error("PROTOCOL_ERROR");
        std::vector<std::string_view> args;
        for (int index = 2; index < argc; ++index) args.emplace_back(argv[index]);
        const auto request = artifact_store_v4_cli_detail::parse(args);
        const auto result = request.restore ?
            artifact_v4::restore_from_remote_v4(request.manifest_id, request.store, request.remote) :
            artifact_v4::mirror_to_remote_v4(request.manifest_id, request.store, request.remote);
        std::cout << artifact_store_v4_cli_detail::render(request.restore, result) << '\n';
        return 0;
    } catch (...) {
        std::cerr << "{\"contract\":\"artifact-store-mirror-v4\",\"error\":\"PROTOCOL_ERROR\"}\n";
        return 2;
    }
}
} // namespace bruce
