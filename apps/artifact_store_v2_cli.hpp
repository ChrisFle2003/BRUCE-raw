#pragma once

#include "bruce/artifact/artifact_store_v2.hpp"
#include "bruce/core/json.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace bruce::artifact_store_v2_cli_detail {

struct Request {
    artifact_v1::ArtifactIdV1 expected;
    std::filesystem::path store;
    std::string provider;
    std::filesystem::path providers;
};

inline bool path_token(std::string_view value) {
    return !value.empty() && !value.starts_with('-') && value.find('\0') == std::string_view::npos;
}

inline bool provider_token(std::string_view value) {
    const auto alphanumeric = [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    };
    if (value.empty() || value.size() > 128 || !alphanumeric(value.front())) return false;
    for (const unsigned char c : value) {
        if (!alphanumeric(c) && c != '.' && c != '_' && c != '-') return false;
    }
    return true;
}

inline Request parse(const std::vector<std::string_view>& args) {
    if (args.size() != 9 || args[0] != "fetch" || args[3] != "--store" ||
        args[5] != "--provider" || args[7] != "--providers" ||
        !path_token(args[4]) || !provider_token(args[6]) || !path_token(args[8]))
        throw Error("PROTOCOL_ERROR");
    if (args[1].size() != 64) throw Error("PROTOCOL_ERROR");
    for (const unsigned char c : args[1]) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            throw Error("PROTOCOL_ERROR");
    }
    const auto size = args[2];
    if (size.empty() || (size.size() > 1 && size.front() == '0')) throw Error("PROTOCOL_ERROR");
    std::uint64_t bytes = 0;
    for (const unsigned char c : size) {
        if (c < '0' || c > '9') throw Error("PROTOCOL_ERROR");
        const auto digit = static_cast<unsigned>(c - '0');
        if (bytes > (max_file_bytes - digit) / 10) throw Error("PROTOCOL_ERROR");
        bytes = bytes * 10 + digit;
    }
    return {{std::string(args[1]), bytes}, std::filesystem::path(args[4]),
            std::string(args[6]), std::filesystem::path(args[8])};
}

inline Request parse(int argc, char** argv) {
    if (argc != 11) throw Error("PROTOCOL_ERROR");
    std::vector<std::string_view> args;
    args.reserve(9);
    for (int index = 2; index < argc; ++index) args.emplace_back(argv[index]);
    return parse(args);
}

inline std::string render(std::string_view provider, const artifact_v1::ArtifactIdV1& id) {
    return "{\"contract\":\"artifact-store-fetch-v2\",\"action\":\"fetch\",\"status\":\"FETCHED\","
        "\"provider\":" + json_string(provider) + ",\"sha256\":" + json_string(id.sha256) +
        ",\"size_bytes\":" + std::to_string(id.size_bytes) + "}";
}

} // namespace bruce::artifact_store_v2_cli_detail

namespace bruce {

inline int artifact_store_v2_cli(int argc, char** argv) {
    try {
        const auto request = artifact_store_v2_cli_detail::parse(argc, argv);
        const auto config = artifact_v2::read_provider_config_v2(request.providers);
        const auto id = artifact_v2::fetch_to_store_v2(
            config, request.provider, request.expected, request.store);
        std::cout << artifact_store_v2_cli_detail::render(request.provider, id) << '\n';
        return 0;
    } catch (...) {
        std::cerr << "{\"contract\":\"artifact-store-fetch-v2\",\"error\":\"PROTOCOL_ERROR\"}\n";
        return 2;
    }
}

} // namespace bruce
