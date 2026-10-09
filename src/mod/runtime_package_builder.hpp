#pragma once
#include <string>
#include <string_view>
namespace bruce::runtime_package_builder_detail {
// Shares the runtime's bounded canonical parser. No second permissive schema.
std::string unsigned_bytes(std::string_view metadata, std::string_view tree, std::string_view golden);
}
