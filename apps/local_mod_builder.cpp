#include "bruce/mod/local_builder.hpp"
#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"
#include <filesystem>
#include <iostream>
#include <map>
#include <set>

namespace {
std::string data_report(const bruce::LocalDataModBuildResultV1& result) {
    std::string report = "{\"contract\":\"local-data-mod-build-v1\",\"mod_id\":";
    report += bruce::json_string(result.mod_id);
    report += ",\"version\":";
    report += bruce::json_string(result.version);
    report += ",\"package_digest\":";
    report += bruce::json_string(result.package_digest);
    report += "}\n";
    return report;
}
std::string logic_report(const bruce::LocalLogicModBuildResultV1& result) {
    std::string report = "{\"contract\":\"local-logic-mod-build-v1\",\"kind\":\"logic\",\"mod_id\":";
    report += bruce::json_string(result.mod_id);
    report += ",\"version\":";
    report += bruce::json_string(result.version);
    report += ",\"signing_key_id\":";
    report += bruce::json_string(result.signing_key_id);
    report += ",\"package_digest\":";
    report += bruce::json_string(result.package_digest);
    report += "}\n";
    return report;
}
}

int main(int argc, char** argv) {
    const bool modern = argc >= 2 && (std::string_view(argv[1]) == "data" ||
                                      std::string_view(argv[1]) == "logic");
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--help") {
            std::cout << "bruce-mod-builder data SOURCE_DIR --output PACKAGE.brucemod "
                "--trust-store-output TRUST.pub --signing-env FILE\n"
                "bruce-mod-builder logic SOURCE_DIR --output PACKAGE.brucemod "
                "--trust-store-output TRUST.pub --signing-env FILE\n"
                "bruce-mod-builder --manifest META.json --tree TREE.json --golden CASES.json "
                "--output PACKAGE.brucemod --trust-store-output TRUST.pub\n"
                "Legacy grammar reads only .env in the current project directory; data requires --signing-env.\n"
                "The signing file must be owner-only mode 0600.\n"
                "Key assignment: BRUCE_MOD_ED25519_SEED_HEX=<64 lowercase hexadecimal characters>.\n"
                "Manifest fields: mod_id, signing_key_id, version. All source JSON must be canonical.\n"
                "Output directories must be owned by you without group/world write permission.\n"
                "Existing outputs are never replaced. No package is installed or activated.\n";
            return 0;
        }
        const std::set<std::string> names = modern
            ? std::set<std::string>{"--output", "--trust-store-output", "--signing-env"}
            : std::set<std::string>{"--manifest", "--tree", "--golden", "--output", "--trust-store-output"};
        if (modern && (argc < 3 || std::string_view(argv[2]).empty()))
            throw bruce::Error("InvalidLocalModBuilderOptions");
        std::map<std::string, std::string> args;
        for (int i = modern ? 3 : 1; i < argc; i += 2) {
            if (i + 1 >= argc || !names.contains(argv[i]) || std::string_view(argv[i + 1]).empty() ||
                !args.emplace(argv[i], argv[i + 1]).second) throw bruce::Error("InvalidLocalModBuilderOptions");
        }
        if (args.size() != names.size()) throw bruce::Error("InvalidLocalModBuilderOptions");
        const auto project = std::filesystem::current_path();
        const auto package_output = args.at("--output");
        const auto truststore_output = args.at("--trust-store-output");
        if (modern && std::string_view(argv[1]) == "logic") {
            const bruce::LocalLogicModBuildPathsV1 paths = {
                argv[2], package_output, truststore_output, args.at("--signing-env")};
            std::string report;
            (void)bruce::build_local_logic_mod_v1(paths,
                [&report](const bruce::LocalLogicModBuildResultV1& result) {
                    report = logic_report(result);
                });
            std::cout << report;
            return 0;
        }
        bruce::LocalDataModBuildPathsV1 paths;
        paths.project_root = project.string();
        paths.package_output = package_output;
        paths.truststore_output = truststore_output;
        if (modern) {
            const std::filesystem::path source(argv[2]);
            paths.manifest_source = (source / "manifest.json").string();
            paths.tree_source = (source / "tree.json").string();
            paths.golden_source = (source / "golden-cases.json").string();
            paths.signing_env = args.at("--signing-env");
        } else {
            paths.manifest_source = args.at("--manifest");
            paths.tree_source = args.at("--tree");
            paths.golden_source = args.at("--golden");
        }
        std::string report;
        const auto result = modern
            ? bruce::build_local_data_mod_v1(paths,
                [&report](const bruce::LocalDataModBuildResultV1& prepared) {
                    report = data_report(prepared);
                })
            : bruce::build_local_data_mod_v1(paths);
        if (!modern) report = data_report(result);
        std::cout << report;
        return 0;
    } catch (...) {
        // Includes invalid options/paths: caller-supplied strings are never echoed.
        if (modern) {
            std::cerr << "{\"error\":\"LocalModBuildFailed\"}\n";
            return 2;
        }
        std::cerr << "LocalModBuildFailed\n";
        return 1;
    }
}
