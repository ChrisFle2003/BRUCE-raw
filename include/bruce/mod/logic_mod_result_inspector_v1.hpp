#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace bruce {
// Metadata projected from the canonical record. READY proves the local output
// artifact only; it does not establish fresh registry authorization or replay.
struct LogicModResultInspectionV1 {
    std::string status = "INVALID_RECORD";
    bool accepted = false;
    bool input_present = false;
    bool output_present = false;
    // Lowercase SHA-256 of the record's opaque legacy binding field. Never
    // reflect those unconstrained bytes into public inspector output.
    std::optional<std::string> binding_identity;
    std::optional<std::string> output_manifest_id, output_sha256, output_codec;
    std::optional<std::uint64_t> output_size_bytes;
};

// Host-only, read-only: no registry, package, worker, Store creation or repair.
// Rejects symlinks in every path component and accepts only a private regular
// single-link file owned by the current user, bounded to the record byte limit.
std::string read_logic_mod_result_record_file_v1(const std::filesystem::path&);
LogicModResultInspectionV1 inspect_logic_mod_result_v1(
    std::string_view record, const std::filesystem::path& local_store);
std::string render_logic_mod_result_inspection_v1(const LogicModResultInspectionV1&);
}
