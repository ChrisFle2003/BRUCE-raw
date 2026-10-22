#pragma once

#include "bruce/artifact/artifact_store_v1.hpp"
#include "bruce/core/json.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {
namespace artifact_store_v1_cli_detail {

enum class Action { import_file, inspect, verify, resolve };

struct Request {
    Action action{};
    std::filesystem::path source;
    std::filesystem::path store;
    std::string digest;
    std::string manifest_id;
};

inline constexpr std::string_view usage_import =
    "Usage: bruce artifact-store-v1 import FILE --store STORE";
inline constexpr std::string_view usage_inspect =
    "Usage: bruce artifact-store-v1 inspect DIGEST --store STORE";
inline constexpr std::string_view usage_verify =
    "Usage: bruce artifact-store-v1 verify DIGEST --store STORE";
inline constexpr std::string_view usage_resolve =
    "Usage: bruce artifact-store-v1 resolve MANIFEST_ID --store STORE";

inline bool path_token(std::string_view value) {
    return !value.empty() && !value.starts_with('-');
}

inline bool digest_token(std::string_view value) {
    if (value.size() != 64) return false;
    for (const unsigned char c : value) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

inline Request parse(std::vector<std::string_view> args) {
    if (args.size() < 2) throw Error("ArtifactStoreUsage");
    const auto action = args[0];
    if (action != "import" && action != "inspect" && action != "verify" &&
        action != "resolve")
        throw Error("ArtifactStoreUsage");
    if (args.size() != 4 || args[2] != "--store" || !path_token(args[1]) ||
        !path_token(args[3])) throw Error("ArtifactStoreUsage");
    Request result;
    result.store = std::filesystem::path(args[3]);
    if (action == "import") {
        result.action = Action::import_file;
        result.source = std::filesystem::path(args[1]);
    } else if (action == "resolve") {
        result.action = Action::resolve;
        if (!artifact_v1::is_valid_manifest_id_v1(args[1]))
            throw Error("ArtifactManifestIdInvalid");
        result.manifest_id = std::string(args[1]);
    } else {
        result.action = action == "inspect" ? Action::inspect : Action::verify;
        if (!digest_token(args[1])) throw Error("ArtifactStoreDigestInvalid");
        result.digest = std::string(args[1]);
    }
    return result;
}

inline Request parse(int argc, char** argv) {
    if (argc < 1) throw Error("ArtifactStoreUsage");
    std::vector<std::string_view> args;
    args.reserve(argc > 2 ? static_cast<std::size_t>(argc - 2) : 0);
    for (int index = 2; index < argc; ++index) args.emplace_back(argv[index]);
    return parse(std::move(args));
}

inline std::string action_name(Action action) {
    switch (action) {
    case Action::import_file: return "import";
    case Action::inspect: return "inspect";
    case Action::verify: return "verify";
    case Action::resolve: return "resolve";
    }
    throw Error("ArtifactStoreActionInvalid");
}

inline std::string render_resolve(const artifact_v1::ArtifactResolvedManifestV1& manifest) {
    std::string result =
        "{\"contract\":\"artifact-store-resolve-v1\",\"action\":\"resolve\","
        "\"status\":\"RESOLVED\",\"manifest_id\":" + json_string(manifest.manifest_id) +
        ",\"run_id\":" + json_string(manifest.run_id) +
        ",\"node_id\":" + json_string(manifest.node_id) +
        ",\"attempt_id\":" + json_string(manifest.attempt_id) +
        ",\"manifest_status\":" + json_string(manifest.manifest_status) +
        ",\"result_code\":" + json_string(manifest.result_code) + ",\"objects\":[";
    for (std::size_t index = 0; index < manifest.objects.size(); ++index) {
        if (index != 0) result += ',';
        const auto& object = manifest.objects[index];
        result += "{\"sha256\":" + json_string(object.id.sha256) +
            ",\"size_bytes\":" + std::to_string(object.id.size_bytes) +
            ",\"origin\":" + json_string(object.origin) + "}";
    }
    result += "]}";
    return result;
}

inline std::string render(Action action, const artifact_v1::ArtifactIdV1& id) {
    const auto status = action == Action::import_file ? "IMPORTED" :
        (action == Action::inspect ? "INSPECTED" : "VERIFIED");
    return "{\"contract\":\"artifact-store-v1\",\"action\":" +
        json_string(action_name(action)) + ",\"status\":" + json_string(status) +
        ",\"sha256\":" + json_string(id.sha256) +
        ",\"size_bytes\":" + std::to_string(id.size_bytes) + "}";
}

inline std::string usage(Action action) {
    switch (action) {
    case Action::import_file: return std::string(usage_import);
    case Action::inspect: return std::string(usage_inspect);
    case Action::verify: return std::string(usage_verify);
    case Action::resolve: return std::string(usage_resolve);
    }
    return std::string(usage_import);
}

inline int artifact_store_v1_cli(int argc, char** argv) {
    const bool resolve_route = argc > 2 && std::string_view(argv[2]) == "resolve";
    try {
        const auto request = parse(argc, argv);
        auto store = request.action == Action::resolve
            ? artifact_v1::ArtifactStoreV1::open_existing(request.store)
            : artifact_v1::ArtifactStoreV1::open(request.store);
        if (request.action == Action::resolve) {
            const auto resolved = store.resolve_manifest(request.manifest_id);
            std::cout << render_resolve(resolved) << '\n';
            return 0;
        }
        artifact_v1::ArtifactIdV1 id;
        if (request.action == Action::import_file) {
            id = store.put_file(request.source);
        } else {
            id.sha256 = request.digest;
            const auto metadata = store.inspect(request.digest);
            id = metadata.id;
        }
        // The store API requires the exact byte count for digest-addressed
        // verification. Inspect returns the authoritative metadata; a CLI
        // digest request therefore cannot expose a guessed size.
        if (request.action == Action::verify) {
            store.verify(request.digest);
            std::cout << render(request.action, id) << '\n';
        } else {
            std::cout << render(request.action, id) << '\n';
        }
        return 0;
    } catch (const std::exception&) {
        // Do not pass OS diagnostics, source paths, or store paths through the
        // public contract. All failures use one compact protocol error.
        if (resolve_route) {
            std::cerr << "{\"contract\":\"artifact-store-resolve-v1\",\"error\":\"PROTOCOL_ERROR\"}\n";
        } else {
            std::cerr << "{\"contract\":\"artifact-store-v1\",\"error\":\"PROTOCOL_ERROR\"}\n";
        }
        return 2;
    }
}

} // namespace artifact_store_v1_cli_detail

inline int artifact_store_v1_cli(int argc, char** argv) {
    return artifact_store_v1_cli_detail::artifact_store_v1_cli(argc, argv);
}

} // namespace bruce
