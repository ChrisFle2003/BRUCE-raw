#include "bruce/artifact/artifact_payload_v1.hpp"
#include "bruce/artifact/artifact_manifest_v1.hpp"
#include "bruce/artifact/artifact_store_v1.hpp"
#include "bruce/core/types.hpp"

#include <filesystem>
#include <fcntl.h>
#include <atomic>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <chrono>
#include <thread>
#include <sys/stat.h>
#include <fstream>
#include <unistd.h>

using namespace bruce;
using namespace bruce::artifact_v1;

namespace {
void require(bool value, const char* message) {
    if (!value) throw Error(message);
}

template<class F>
void rejects(F&& action, ArtifactPayloadErrorCodeV13 code, const char* message) {
    try {
        action();
    } catch (const ArtifactPayloadErrorV13& error) {
        require(error.code() == code, message);
        return;
    }
    throw Error(message);
}

std::filesystem::path object_path(const std::filesystem::path& root, const ArtifactIdV1& id) {
    return root / "objects" / id.sha256.substr(0, 2) / (id.sha256 + ".blob");
}

void write_file(const std::filesystem::path& path, std::string_view bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output) throw Error("TestWriteFailed");
}

int create_private_file(const std::filesystem::path& path) {
    const int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) throw Error("TestOpenFailed");
    return fd;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

template<class F>
void rejects_materialization(F&& action, ArtifactPayloadMaterializationErrorV13 code,
                             const char* message) {
    try {
        action();
    } catch (const ArtifactPayloadMaterializationExceptionV13& error) {
        require(error.code() == code, message);
        return;
    }
    throw Error(message);
}

std::map<std::string, std::uintmax_t> snapshot_tree(const std::filesystem::path& root) {
    std::map<std::string, std::uintmax_t> result;
    if (!std::filesystem::exists(root)) return result;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        const auto relative = std::filesystem::relative(entry.path(), root).string();
        const auto status = entry.symlink_status();
        if (std::filesystem::is_regular_file(status)) {
            result.emplace(relative, std::filesystem::file_size(entry.path()));
        } else if (std::filesystem::is_directory(status)) {
            result.emplace(relative, 0);
        } else if (std::filesystem::is_symlink(status)) {
            result.emplace(relative, std::uintmax_t(-1));
        }
    }
    return result;
}

ArtifactManifestV1 ready_manifest(const ArtifactIdV1& object, std::string node = "node-1") {
    ArtifactManifestV1 manifest;
    manifest.run_id = "run-1";
    manifest.node_id = std::move(node);
    manifest.attempt_id = "attempt-1";
    manifest.status = "READY";
    manifest.result_code = "OK";
    manifest.objects.push_back({object, "application/octet-stream", "worker-output"});
    return manifest;
}
}

int main() {
    const auto base = std::filesystem::temp_directory_path() / "bruce-artifact-payload-v1-tests";
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);
    try {
        const auto valid_digest = std::string(64, 'a');
        const ArtifactInputRefV13 valid{"manifest-1", 0, valid_digest, 17};
        require(is_valid_artifact_input_ref_v13(valid), "valid payload ref rejected");
        validate_artifact_input_ref_v13(valid);
        require(valid.manifest_id == "manifest-1" && valid.object_index == 0 &&
                    valid.sha256 == valid_digest && valid.size_bytes == 17,
                "payload ref fields changed");
        require(!is_valid_artifact_input_ref_v13(
                    ArtifactInputRefV13{"../escape", 0, valid_digest, 17}),
                "path-like manifest id accepted");
        require(!is_valid_artifact_input_ref_v13(
                    ArtifactInputRefV13{"/absolute", 0, valid_digest, 17}),
                "absolute manifest id accepted");
        require(!is_valid_artifact_input_ref_v13(
                    ArtifactInputRefV13{"manifest-1", 0, std::string(64, 'A'), 17}),
                "uppercase digest accepted");
        require(!is_valid_artifact_input_ref_v13(
                    ArtifactInputRefV13{"manifest-1", 0, std::string(63, 'a'), 17}),
                "short digest accepted");
        require(!is_valid_artifact_input_ref_v13(
                    ArtifactInputRefV13{"manifest-1", 0, valid_digest, max_file_bytes + 1}),
                "oversized ref accepted");
        rejects([&] { validate_artifact_input_ref_v13(
                      ArtifactInputRefV13{"manifest-1", 0, std::string(63, 'a'), 17}); },
                ArtifactPayloadErrorCodeV13::invalid_reference, "invalid ref did not throw typed error");

        const auto root = base / "store";
        auto store = ArtifactStoreV1::open(root);
        const auto first = store.put_bytes("first payload");
        const auto second = store.put_bytes("second payload");
        auto manifest = ready_manifest(first);
        manifest.objects.push_back({second, "application/octet-stream", "second-origin"});
        store.publish_manifest("manifest-ready", manifest);
        const ArtifactPayloadProvenanceV13 provenance{"run-1", "node-1", "attempt-1"};
        const ArtifactInputRefV13 first_ref{"manifest-ready", 0, first.sha256, first.size_bytes};
        const auto before = snapshot_tree(root);
        const auto selected = select_artifact_input_v13(store, first_ref, provenance);
        require(selected.reference == first_ref, "selected ref mismatch");
        require(selected.object == first && selected.origin == "worker-output",
                "selected object mismatch");
        require(selected.run_id == "run-1" && selected.node_id == "node-1" &&
                    selected.attempt_id == "attempt-1" && selected.result_code == "OK",
                "selected provenance mismatch");
        require(snapshot_tree(root) == before, "selection modified Artifact Store");

        const ArtifactInputRefV13 second_ref{"manifest-ready", 1, second.sha256, second.size_bytes};
        const auto second_selected = select_artifact_input_v13(store, second_ref, provenance);
        require(second_selected.object == second && second_selected.origin == "second-origin",
                "second object selection mismatch");

        const auto input_path = base / "private-input.bin";
        const int input_fd = create_private_file(input_path);
        const auto materialized = materialize_artifact_inputs_v13(
            store, std::vector<ArtifactPayloadSelectionV13>{selected, second_selected}, input_fd,
            first.size_bytes + second.size_bytes,
            std::chrono::steady_clock::now() + std::chrono::seconds(2));
        ::close(input_fd);
        require(materialized.object_count == 2 && materialized.bytes_written ==
                    first.size_bytes + second.size_bytes && materialized.objects.size() == 2,
                "materialization usage mismatch");
        require(read_file(input_path) == "first payloadsecond payload",
                "materialization bytes mismatch");

        const auto limited_path = base / "limited-input.bin";
        const int limited_fd = create_private_file(limited_path);
        rejects_materialization(
            [&] { (void)materialize_artifact_inputs_v13(
                      store, std::vector<ArtifactPayloadSelectionV13>{selected, second_selected},
                      limited_fd, first.size_bytes,
                      std::chrono::steady_clock::now() + std::chrono::seconds(2)); },
            ArtifactPayloadMaterializationErrorV13::input_limit_exceeded,
            "aggregate input limit accepted");
        ::close(limited_fd);
        require(read_file(limited_path).empty(), "limited materialization left partial bytes");

        std::vector<ArtifactPayloadSelectionV13> too_many_inputs;
        for (std::size_t index = 0; index < artifact_payload_input_object_limit_v13 + 1; ++index)
            too_many_inputs.push_back(selected);
        const auto count_path = base / "count-input.bin";
        const int count_fd = create_private_file(count_path);
        rejects_materialization(
            [&] { (void)materialize_artifact_inputs_v13(
                      store, too_many_inputs, count_fd, max_file_bytes,
                      std::chrono::steady_clock::now() + std::chrono::seconds(2)); },
            ArtifactPayloadMaterializationErrorV13::object_limit_exceeded,
            "object count limit accepted");
        ::close(count_fd);
        require(read_file(count_path).empty(), "object count rejection wrote bytes");

        const auto overwrite_path = base / "overwrite-input.bin";
        write_file(overwrite_path, "existing");
        std::filesystem::permissions(overwrite_path,
                                     std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::replace);
        const int overwrite_fd = ::open(overwrite_path.c_str(), O_RDWR | O_CLOEXEC | O_NOFOLLOW);
        if (overwrite_fd < 0) throw Error("TestOpenFailed");
        rejects_materialization(
            [&] { (void)materialize_artifact_inputs_v13(
                      store, std::vector<ArtifactPayloadSelectionV13>{selected}, overwrite_fd,
                      max_file_bytes, std::chrono::steady_clock::now() + std::chrono::seconds(2)); },
            ArtifactPayloadMaterializationErrorV13::destination_not_empty,
            "destination overwrite accepted");
        ::close(overwrite_fd);
        require(read_file(overwrite_path) == "existing", "destination was overwritten");

        const auto missing_path = base / "missing-input.bin";
        const int missing_fd = create_private_file(missing_path);
        const ArtifactIdV1 missing{"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", 7};
        ArtifactPayloadSelectionV13 missing_selection;
        missing_selection.reference = ArtifactInputRefV13{"manifest-ready", 0, missing.sha256, missing.size_bytes};
        missing_selection.object = missing;
        rejects_materialization(
            [&] { (void)materialize_artifact_inputs_v13(
                      store, std::vector<ArtifactPayloadSelectionV13>{missing_selection}, missing_fd,
                      max_file_bytes, std::chrono::steady_clock::now() + std::chrono::seconds(2)); },
            ArtifactPayloadMaterializationErrorV13::object_copy_failed,
            "missing object accepted");
        ::close(missing_fd);
        require(read_file(missing_path).empty(), "missing object left partial bytes");

        const auto binding_path = base / "binding-input.bin";
        const int binding_fd = create_private_file(binding_path);
        auto bad_binding = selected;
        bad_binding.reference.sha256 = std::string(64, 'b');
        rejects_materialization(
            [&] { (void)materialize_artifact_inputs_v13(
                      store, std::vector<ArtifactPayloadSelectionV13>{bad_binding}, binding_fd,
                      max_file_bytes, std::chrono::steady_clock::now() + std::chrono::seconds(2)); },
            ArtifactPayloadMaterializationErrorV13::object_copy_failed,
            "wrong digest binding accepted");
        ::close(binding_fd);
        require(read_file(binding_path).empty(), "wrong binding wrote payload bytes");

        const auto late_path = base / "late-input.bin";
        const int late_fd = create_private_file(late_path);
        write_file(object_path(root, second), "late corruption");
        rejects_materialization(
            [&] { (void)materialize_artifact_inputs_v13(
                      store, std::vector<ArtifactPayloadSelectionV13>{selected, second_selected},
                      late_fd, max_file_bytes,
                      std::chrono::steady_clock::now() + std::chrono::seconds(2)); },
            ArtifactPayloadMaterializationErrorV13::object_copy_failed,
            "late object corruption accepted");
        ::close(late_fd);
        require(read_file(late_path).empty(), "late corruption left partial bytes");

        const auto expired_path = base / "expired-input.bin";
        const int expired_fd = create_private_file(expired_path);
        rejects_materialization(
            [&] { (void)materialize_artifact_inputs_v13(
                      store, std::vector<ArtifactPayloadSelectionV13>{selected}, expired_fd,
                      max_file_bytes, std::chrono::steady_clock::now() - std::chrono::seconds(1)); },
            ArtifactPayloadMaterializationErrorV13::deadline_exceeded,
            "expired deadline accepted");
        ::close(expired_fd);
        require(read_file(expired_path).empty(), "expired deadline left partial bytes");

        const auto first_object_path = object_path(root, first);
        const auto outside_path = base / "outside-object.bin";
        write_file(outside_path, "first payload");
        std::filesystem::permissions(outside_path,
                                     std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::replace);
        std::filesystem::remove(first_object_path);
        std::filesystem::create_symlink(outside_path, first_object_path);
        const auto symlink_path = base / "symlink-input.bin";
        const int symlink_fd = create_private_file(symlink_path);
        rejects_materialization(
            [&] { (void)materialize_artifact_inputs_v13(
                      store, std::vector<ArtifactPayloadSelectionV13>{selected}, symlink_fd,
                      max_file_bytes, std::chrono::steady_clock::now() + std::chrono::seconds(2)); },
            ArtifactPayloadMaterializationErrorV13::object_copy_failed,
            "symlink object accepted");
        ::close(symlink_fd);
        require(read_file(symlink_path).empty(), "symlink rejection left partial bytes");
        std::filesystem::remove(first_object_path);
        write_file(first_object_path, "first payload");
        std::filesystem::permissions(first_object_path,
                                     std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::replace);

        const auto hardlink_path = base / "hardlink-object.bin";
        std::filesystem::create_hard_link(first_object_path, hardlink_path);
        const auto hardlink_input_path = base / "hardlink-input.bin";
        const int hardlink_fd = create_private_file(hardlink_input_path);
        rejects_materialization(
            [&] { (void)materialize_artifact_inputs_v13(
                      store, std::vector<ArtifactPayloadSelectionV13>{selected}, hardlink_fd,
                      max_file_bytes, std::chrono::steady_clock::now() + std::chrono::seconds(2)); },
            ArtifactPayloadMaterializationErrorV13::object_copy_failed,
            "hardlinked object accepted");
        ::close(hardlink_fd);
        std::filesystem::remove(hardlink_path);
        require(read_file(hardlink_input_path).empty(), "hardlink rejection left partial bytes");

        std::filesystem::permissions(first_object_path, std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_write |
                                         std::filesystem::perms::group_read,
                                     std::filesystem::perm_options::replace);
        const auto mode_input_path = base / "mode-input.bin";
        const int mode_fd = create_private_file(mode_input_path);
        rejects_materialization(
            [&] { (void)materialize_artifact_inputs_v13(
                      store, std::vector<ArtifactPayloadSelectionV13>{selected}, mode_fd,
                      max_file_bytes, std::chrono::steady_clock::now() + std::chrono::seconds(2)); },
            ArtifactPayloadMaterializationErrorV13::object_copy_failed,
            "insecure object accepted");
        ::close(mode_fd);
        require(read_file(mode_input_path).empty(), "insecure rejection left partial bytes");
        std::filesystem::permissions(first_object_path,
                                     std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::replace);

        std::filesystem::remove(first_object_path);
        std::filesystem::create_directory(first_object_path);
        const auto regular_input_path = base / "regular-input.bin";
        const int regular_fd = create_private_file(regular_input_path);
        rejects_materialization(
            [&] { (void)materialize_artifact_inputs_v13(
                      store, std::vector<ArtifactPayloadSelectionV13>{selected}, regular_fd,
                      max_file_bytes, std::chrono::steady_clock::now() + std::chrono::seconds(2)); },
            ArtifactPayloadMaterializationErrorV13::object_copy_failed,
            "non-regular object accepted");
        ::close(regular_fd);
        require(read_file(regular_input_path).empty(), "non-regular rejection left partial bytes");
        std::filesystem::remove_all(first_object_path);
        write_file(first_object_path, "first payload");
        std::filesystem::permissions(first_object_path,
                                     std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::replace);

        const std::string tamper_bytes(16 * 1024 * 1024, 'x');
        const auto tamper_object = store.put_bytes(tamper_bytes);
        ArtifactPayloadSelectionV13 tamper_selection;
        tamper_selection.reference = ArtifactInputRefV13{
            "manifest-ready", 0, tamper_object.sha256, tamper_object.size_bytes};
        tamper_selection.object = tamper_object;
        const auto tamper_path = base / "tamper-input.bin";
        const int tamper_fd = create_private_file(tamper_path);
        std::atomic<bool> tamper_done{false};
        std::atomic<bool> tamper_started{false};
        std::thread tamper_thread([&] {
            const char marker = 'y';
            while (!tamper_done.load(std::memory_order_relaxed)) {
                struct stat status{};
                if (::fstat(tamper_fd, &status) == 0 &&
                    status.st_size >= static_cast<off_t>(tamper_bytes.size())) {
                    tamper_started.store(true, std::memory_order_relaxed);
                    (void)::pwrite(tamper_fd, &marker, 1, 0);
                }
            }
        });
        bool saw_integrity_failure = false;
        try {
            (void)materialize_artifact_inputs_v13(
                store, std::vector<ArtifactPayloadSelectionV13>{tamper_selection}, tamper_fd,
                max_file_bytes, std::chrono::steady_clock::now() + std::chrono::seconds(5));
        } catch (const ArtifactPayloadMaterializationExceptionV13& error) {
            saw_integrity_failure =
                error.code() == ArtifactPayloadMaterializationErrorV13::object_copy_failed;
        }
        tamper_done.store(true, std::memory_order_relaxed);
        tamper_thread.join();
        ::close(tamper_fd);
        require(tamper_started.load(std::memory_order_relaxed) && saw_integrity_failure,
                "destination digest mutation was accepted");

        rejects([&] { (void)select_artifact_input_v13(
                      store, ArtifactInputRefV13{"manifest-ready", 2, first.sha256, first.size_bytes}, provenance); },
                ArtifactPayloadErrorCodeV13::object_index_out_of_range,
                "out-of-range object index accepted");
        rejects([&] { (void)select_artifact_input_v13(
                      store, ArtifactInputRefV13{"manifest-ready", 0, second.sha256, first.size_bytes}, provenance); },
                ArtifactPayloadErrorCodeV13::reference_mismatch,
                "digest/size mismatch accepted");
        rejects([&] { (void)select_artifact_input_v13(
                      store, first_ref, ArtifactPayloadProvenanceV13{"run-1", "other-node", "attempt-1"}); },
                ArtifactPayloadErrorCodeV13::provenance_mismatch,
                "unbound provenance accepted");

        auto pending = manifest;
        pending.status = "PENDING";
        store.publish_manifest("manifest-pending", pending);
        rejects([&] { (void)select_artifact_input_v13(store,
                      ArtifactInputRefV13{"manifest-pending", 0, first.sha256, first.size_bytes}, provenance); },
                ArtifactPayloadErrorCodeV13::manifest_not_ready,
                "non-READY manifest accepted");

        ArtifactManifestV1 empty_manifest = manifest;
        empty_manifest.objects.clear();
        store.publish_manifest("manifest-empty", empty_manifest);
        rejects([&] { (void)select_artifact_input_v13(store,
                      ArtifactInputRefV13{"manifest-empty", 0, first.sha256, first.size_bytes}, provenance); },
                ArtifactPayloadErrorCodeV13::object_index_out_of_range,
                "zero-object manifest accepted for object selection");

        ArtifactManifestV1 too_many = manifest;
        for (std::size_t index = too_many.objects.size(); index < artifact_payload_input_object_limit_v13 + 1; ++index)
            too_many.objects.push_back({first, "application/octet-stream", "repeat"});
        store.publish_manifest("manifest-too-many", too_many);
        rejects([&] { (void)select_artifact_input_v13(store,
                      ArtifactInputRefV13{"manifest-too-many", 0, first.sha256, first.size_bytes}, provenance); },
                ArtifactPayloadErrorCodeV13::object_limit_exceeded,
                "manifest object limit accepted");

        ArtifactManifestV1 corrupt_manifest = ready_manifest(first);
        store.publish_manifest("manifest-corrupt", corrupt_manifest);
        write_file(object_path(root, first), "corrupt");
        rejects([&] { (void)select_artifact_input_v13(store,
                      ArtifactInputRefV13{"manifest-corrupt", 0, first.sha256, first.size_bytes}, provenance); },
                ArtifactPayloadErrorCodeV13::object_verification_failed,
                "corrupt object accepted");

        std::filesystem::remove_all(base);
        std::cout << "Artifact Payload V1 core contract tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove_all(base);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
