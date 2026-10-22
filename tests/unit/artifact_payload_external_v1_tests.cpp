#include "bruce/artifact/artifact_payload_external_v1.hpp"

#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace bruce;
using namespace bruce::artifact_v1;

namespace {

void require(bool value, const char* message) {
    if (!value) throw Error(message);
}

template<class F>
void rejects(F&& action, ExternalInputsErrorCodeV14 code, const char* message) {
    try {
        action();
    } catch (const ExternalInputsErrorV14& error) {
        require(error.code() == code, message);
        return;
    }
    throw Error(message);
}

std::string digest(char fill = 'a') {
    return std::string(64, fill);
}

std::string valid_sidecar(std::string_view node = "consumer",
                          std::string_view manifest = "m-source-1",
                          std::uint64_t index = 0) {
    return "{\"contract\":\"bruce-dag-scheduler-v5-external-inputs-v14\",\"bindings\":[{\"node_id\":" +
        bruce::json_string(node) + ",\"inputs\":[{\"manifest_id\":" + bruce::json_string(manifest) +
        ",\"object_index\":" + std::to_string(index) + ",\"sha256\":\"" + digest() +
        "\",\"size_bytes\":12,\"source_run_id\":\"run-source\",\"source_node_id\":\"producer\",\"source_attempt_id\":\"a-1\"}]}]}";
}

std::filesystem::path make_directory() {
    std::string pattern = (std::filesystem::temp_directory_path() / "bruce-external-inputs-XXXXXX").string();
    std::vector<char> bytes(pattern.begin(), pattern.end());
    bytes.push_back('\0');
    require(::mkdtemp(bytes.data()) != nullptr, "temporary directory creation failed");
    return std::filesystem::path(bytes.data());
}

void write_owner_file(const std::filesystem::path& path, std::string_view bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "sidecar creation failed");
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    require(::chmod(path.c_str(), 0600) == 0, "sidecar chmod failed");
}

void parser_accepts_canonical_sidecar() {
    const auto bytes = valid_sidecar();
    const auto parsed = parse_external_inputs_v14(bytes);
    require(parsed.contract == artifact_payload_external_contract_v14, "contract mismatch");
    require(parsed.bindings.size() == 1 && parsed.bindings.front().node_id == "consumer",
            "binding mismatch");
    require(parsed.bindings.front().inputs.size() == 1, "input count mismatch");
    const auto& input = parsed.bindings.front().inputs.front();
    require(input.manifest_id == "m-source-1" && input.object_index == 0 && input.size_bytes == 12,
            "reference mismatch");
    require(input.source_run_id == "run-source" && input.source_node_id == "producer" &&
                input.source_attempt_id == "a-1",
            "provenance mismatch");
}

void parser_rejects_noncanonical_bytes() {
    rejects([] { parse_external_inputs_v14(" {\"contract\":\"bruce-dag-scheduler-v5-external-inputs-v14\",\"bindings\":[]}"); },
            ExternalInputsErrorCodeV14::invalid_json, "whitespace accepted");
    auto unknown = valid_sidecar();
    unknown.insert(unknown.find("\"bindings\""), "\"extra\":0,");
    rejects([&] { parse_external_inputs_v14(unknown); },
            ExternalInputsErrorCodeV14::invalid_json, "unknown field accepted");
    auto reordered = valid_sidecar();
    const std::string_view contract = "{\"contract\":\"bruce-dag-scheduler-v5-external-inputs-v14\",\"bindings\":[{";
    require(reordered.starts_with(contract), "test fixture malformed");
    reordered = reordered.substr(0, contract.size()) + "\"inputs\":[],\"node_id\":\"consumer\"}]}";
    rejects([&] { parse_external_inputs_v14(reordered); },
            ExternalInputsErrorCodeV14::invalid_json, "field order accepted");
    auto leading_zero = valid_sidecar();
    const std::string_view marker = "\"object_index\":0";
    leading_zero.replace(leading_zero.find(marker), marker.size(), "\"object_index\":00");
    rejects([&] { parse_external_inputs_v14(leading_zero); },
            ExternalInputsErrorCodeV14::invalid_number, "leading zero accepted");
}

void parser_rejects_invalid_bindings() {
    auto bad_contract = valid_sidecar();
    const std::string_view contract = "bruce-dag-scheduler-v5-external-inputs-v14";
    bad_contract.replace(bad_contract.find(contract), contract.size(), "wrong-contract");
    rejects([&] { parse_external_inputs_v14(bad_contract); },
            ExternalInputsErrorCodeV14::invalid_contract, "wrong contract accepted");
    auto bad_id = valid_sidecar("../consumer");
    rejects([&] { parse_external_inputs_v14(bad_id); },
            ExternalInputsErrorCodeV14::invalid_identifier, "path identifier accepted");
    auto bad_digest = valid_sidecar();
    bad_digest.replace(bad_digest.find(digest()), 64, "ABC" + std::string(61, 'a'));
    rejects([&] { parse_external_inputs_v14(bad_digest); },
            ExternalInputsErrorCodeV14::invalid_digest, "uppercase digest accepted");
    auto duplicate_binding = valid_sidecar();
    const auto binding_begin = duplicate_binding.find("{\"node_id\"");
    const auto binding_end = duplicate_binding.rfind('}', duplicate_binding.size() - 2);
    const auto binding = duplicate_binding.substr(binding_begin, binding_end - binding_begin + 1);
    duplicate_binding.insert(duplicate_binding.size() - 2, "," + binding);
    rejects([&] { parse_external_inputs_v14(duplicate_binding); },
            ExternalInputsErrorCodeV14::duplicate_binding, "duplicate binding accepted");
    auto duplicate_object = valid_sidecar();
    const auto object_begin = duplicate_object.find("{\"manifest_id\"");
    const auto object_end = duplicate_object.find('}', object_begin);
    const auto object = duplicate_object.substr(object_begin, object_end - object_begin + 1);
    duplicate_object.insert(object_end + 1, "," + object);
    rejects([&] { parse_external_inputs_v14(duplicate_object); },
            ExternalInputsErrorCodeV14::duplicate_object, "duplicate object accepted");
}

void file_reader_enforces_security_and_digest() {
    const auto directory = make_directory();
    const auto path = directory / "inputs.json";
    const auto bytes = valid_sidecar();
    write_owner_file(path, bytes);
    const auto file = read_external_inputs_v14(path);
    require(file.sidecar == parse_external_inputs_v14(bytes), "file parse mismatch");
    require(file.digest == digest_external_inputs_v14(bytes), "file digest mismatch");
    require(file.size_bytes == bytes.size(), "file size mismatch");

    const auto relative = std::filesystem::relative(path, std::filesystem::current_path());
    // Absolute and relative paths use the same descriptor-relative checks.
    if (!relative.empty()) {
        const auto old = std::filesystem::current_path();
        std::filesystem::current_path(directory);
        const auto relative_file = read_external_inputs_v14("inputs.json");
        require(relative_file.digest == file.digest, "relative path digest mismatch");
        std::filesystem::current_path(old);
    }

    const auto link = directory / "symlink.json";
    require(::symlink(path.c_str(), link.c_str()) == 0, "symlink setup failed");
    rejects([&] { read_external_inputs_v14(link); },
            ExternalInputsErrorCodeV14::file_symlink, "symlink accepted");

    const auto hardlink = directory / "hardlink.json";
    require(::link(path.c_str(), hardlink.c_str()) == 0, "hardlink setup failed");
    rejects([&] { read_external_inputs_v14(hardlink); },
            ExternalInputsErrorCodeV14::file_permissions, "multiple hardlinks accepted");

    const auto insecure = directory / "insecure.json";
    write_owner_file(insecure, bytes);
    require(::chmod(insecure.c_str(), 0640) == 0, "insecure chmod failed");
    rejects([&] { read_external_inputs_v14(insecure); },
            ExternalInputsErrorCodeV14::file_permissions, "group-readable sidecar accepted");

    std::filesystem::remove_all(directory);
}

} // namespace

int main() {
    try {
        parser_accepts_canonical_sidecar();
        parser_rejects_noncanonical_bytes();
        parser_rejects_invalid_bindings();
        file_reader_enforces_security_and_digest();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
