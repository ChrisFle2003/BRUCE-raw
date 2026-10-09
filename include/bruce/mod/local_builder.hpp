#pragma once
#include <string>

namespace bruce {
struct LocalDataModBuildPathsV1 {
    // The signing file is always <project_root>/.env, never a separate key path.
    std::string project_root;
    std::string manifest_source;
    std::string tree_source;
    std::string golden_source;
    std::string package_output;
    std::string truststore_output;
};
struct LocalDataModBuildResultV1 {
    std::string mod_id;
    std::string version;
    std::string package_digest;
};
LocalDataModBuildResultV1 build_local_data_mod_v1(const LocalDataModBuildPathsV1&);
}
