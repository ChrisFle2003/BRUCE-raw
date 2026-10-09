#include "bruce/mod/local_builder.hpp"
#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"
#include <filesystem>
#include <iostream>
#include <map>
#include <set>

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--help") {
            std::cout << "bruce-mod-builder --manifest META.json --tree TREE.json --golden CASES.json "
                "--output PACKAGE.brucemod --trust-store-output TRUST.pub\n"
                "Reads only .env in the current project directory (owner-only mode 0600).\n"
                "Key assignment: BRUCE_MOD_ED25519_SEED_HEX=<64 lowercase hexadecimal characters>.\n"
                "Manifest fields: mod_id, signing_key_id, version. All source JSON must be canonical.\n"
                "Output directories must be owned by you without group/world write permission.\n"
                "Existing outputs are never replaced. No package is installed or activated.\n";
            return 0;
        }
        const std::set<std::string> names = {"--manifest", "--tree", "--golden", "--output", "--trust-store-output"};
        std::map<std::string, std::string> args;
        for (int i = 1; i < argc; i += 2) {
            if (i + 1 >= argc || !names.contains(argv[i]) || std::string_view(argv[i + 1]).empty() ||
                !args.emplace(argv[i], argv[i + 1]).second) throw bruce::Error("InvalidLocalModBuilderOptions");
        }
        if (args.size() != names.size()) throw bruce::Error("InvalidLocalModBuilderOptions");
        const bruce::LocalDataModBuildPathsV1 paths = {std::filesystem::current_path().string(),
            args.at("--manifest"), args.at("--tree"), args.at("--golden"), args.at("--output"), args.at("--trust-store-output")};
        const auto result = bruce::build_local_data_mod_v1(paths);
        std::cout << "{\"contract\":\"local-data-mod-build-v1\",\"mod_id\":" << bruce::json_string(result.mod_id)
            << ",\"version\":" << bruce::json_string(result.version) << ",\"package_digest\":" << bruce::json_string(result.package_digest) << "}\n";
        return 0;
    } catch (...) {
        // Includes invalid options/paths: caller-supplied strings are never echoed.
        std::cerr << "LocalModBuildFailed\n";
        return 1;
    }
}
