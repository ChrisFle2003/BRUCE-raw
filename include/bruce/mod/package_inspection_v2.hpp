#pragma once

#include "bruce/mod/logic_mod_plugin_v1.hpp"
#include "bruce/mod/runtime_package.hpp"

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace bruce {

enum class ModPackageKindV2 { data, logic };

class DetectedModPackageV2 {
public:
    explicit DetectedModPackageV2(RuntimeDataModPackageV1 package);
    explicit DetectedModPackageV2(LogicModPackageV1 package);

    [[nodiscard]] ModPackageKindV2 kind() const noexcept;
    [[nodiscard]] const RuntimeDataModPackageV1* data_package() const noexcept;
    [[nodiscard]] const LogicModPackageV1* logic_package() const noexcept;

private:
    std::variant<RuntimeDataModPackageV1, LogicModPackageV1> package_;
};

struct ModPackageInspectionV2 {
    ModPackageKindV2 kind = ModPackageKindV2::data;
    std::string mod_id;
    std::string version;
    std::string package_digest;
    std::vector<std::string> capabilities;
};

// Detection is closed over the two exact V1 envelope magics and parses the
// selected package exactly once. Both package and file readers enforce 1 MiB.
[[nodiscard]] DetectedModPackageV2 detect_mod_package_v2(std::string_view bytes);
[[nodiscard]] std::string read_mod_package_file_v2(std::string_view path);
// Reads an explicit CLI trust store through the same nonblocking, symlink-safe
// regular-file boundary as Mod packages, with a 65536-byte limit.
[[nodiscard]] std::string read_mod_trust_store_file_v1(std::string_view path);
[[nodiscard]] ModPackageInspectionV2 inspect_mod_package_v2(const DetectedModPackageV2& package);
void verify_mod_package_v2(const DetectedModPackageV2& package,
                           const RuntimeModTrustStoreV1& trust);
[[nodiscard]] std::string render_mod_package_inspection_v2(const ModPackageInspectionV2& inspection);
[[nodiscard]] std::string render_mod_package_verification_v2(const ModPackageInspectionV2& inspection);

} // namespace bruce
