#pragma once
#include <functional>
#include <string>

namespace bruce {
struct LocalDataModBuildPathsV1 {
    // Empty preserves the legacy <project_root>/.env contract. New callers
    // provide an explicit owner-only regular signing file.
    std::string project_root;
    std::string manifest_source;
    std::string tree_source;
    std::string golden_source;
    std::string package_output;
    std::string truststore_output;
    std::string signing_env;
};
struct LocalDataModBuildResultV1 {
    std::string mod_id;
    std::string version;
    std::string package_digest;
};
LocalDataModBuildResultV1 build_local_data_mod_v1(const LocalDataModBuildPathsV1&);
using LocalDataModBeforePublishV1 = std::function<void(const LocalDataModBuildResultV1&)>;
LocalDataModBuildResultV1 build_local_data_mod_v1(
    const LocalDataModBuildPathsV1&, const LocalDataModBeforePublishV1&);

struct LocalLogicModBuildPathsV1 {
    std::string source_directory;
    std::string package_output;
    std::string truststore_output;
    std::string signing_env;
};
struct LocalLogicModBuildResultV1 {
    std::string mod_id;
    std::string version;
    std::string signing_key_id;
    std::string package_digest;
};
LocalLogicModBuildResultV1 build_local_logic_mod_v1(const LocalLogicModBuildPathsV1&);
using LocalLogicModBeforePublishV1 = std::function<void(const LocalLogicModBuildResultV1&)>;
LocalLogicModBuildResultV1 build_local_logic_mod_v1(
    const LocalLogicModBuildPathsV1&, const LocalLogicModBeforePublishV1&);
}
