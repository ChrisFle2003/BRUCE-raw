#include "bruce/artifact/artifact_store_v1.hpp"
#include "bruce/artifact/artifact_manifest_v1.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

using namespace bruce;
using namespace bruce::artifact_v1;

namespace {
void require(bool value, const char* message) {
    if (!value) throw Error(message);
}

template<class F>
void rejects(F&& action, std::string_view category, const char* message) {
    try {
        action();
    } catch (const Error& error) {
        require(std::string_view(error.what()) == category, message);
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

void expect_owner_only(const std::filesystem::path& path) {
    struct stat status{};
    require(::lstat(path.c_str(), &status) == 0, "stat failed");
    require((status.st_mode & 0077) == 0, "store entry is not owner-only");
}

std::map<std::string, std::uintmax_t> snapshot_tree(const std::filesystem::path& root) {
    std::map<std::string, std::uintmax_t> result;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             root, std::filesystem::directory_options::none)) {
        const auto relative = std::filesystem::relative(entry.path(), root).string();
        const auto status = entry.symlink_status();
        if (std::filesystem::is_regular_file(status)) {
            result.emplace(relative, std::filesystem::file_size(entry.path()));
        } else if (std::filesystem::is_directory(status)) {
            result.emplace(relative, 0);
        } else if (std::filesystem::is_symlink(status)) {
            result.emplace(relative, std::numeric_limits<std::uintmax_t>::max());
        }
    }
    return result;
}
}

int main() {
    const auto base = std::filesystem::temp_directory_path() / "bruce-artifact-store-v1-tests";
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);
    try {
        const auto root = base / "store";
        auto store = ArtifactStoreV1::open(root);
        const auto missing_store = base / "missing-resolve-store";
        rejects([&] { (void)ArtifactStoreV1::open_existing(missing_store); },
                "ArtifactStoreOpenFailed", "read-only open accepted a missing store");
        require(!std::filesystem::exists(missing_store),
                "read-only open created a missing store");
        expect_owner_only(root);
        expect_owner_only(root / "objects");
        expect_owner_only(root / "manifests");

        const auto empty = store.put_bytes("");
        auto reopened = ArtifactStoreV1::open_existing(root);
        require(reopened.contains(empty), "read-only open could not inspect an existing store");
        require(empty.size_bytes == 0 && empty.sha256 == sha256(""), "empty object identity mismatch");
        store.verify(empty);
        require(store.contains(empty), "empty object missing");
        require(store.inspect(empty).id == empty, "empty inspect mismatch");

        const std::string small_bytes = "bruce-artifact";
        const auto small = store.put_bytes(small_bytes);
        require(small.size_bytes == small_bytes.size() && small.sha256 == sha256(small_bytes),
                "small object identity mismatch");
        const auto duplicate = store.put_bytes(small_bytes);
        require(duplicate == small, "duplicate publication changed identity");
        require(std::filesystem::hard_link_count(object_path(root, small)) == 1,
                "published object has unexpected hardlink count");

        const auto mode_id = store.put_bytes("mode-protected");
        require(::chmod(object_path(root, mode_id).c_str(), 0644) == 0, "chmod regression setup failed");
        rejects([&] { store.verify(mode_id); }, "ArtifactObjectCorrupt", "world-readable object verified");
        require(!store.contains(mode_id), "world-readable object reported as present");
        rejects([&] { (void)store.inspect(mode_id); }, "ArtifactObjectCorrupt", "world-readable object inspected");
        rejects([&] { store.put_bytes("mode-protected"); }, "ArtifactObjectConflict",
                "world-readable duplicate object accepted");
        require(::chmod(object_path(root, mode_id).c_str(), 0600) == 0, "chmod regression cleanup failed");
        store.verify(mode_id);

        const std::string maximal(max_file_bytes, 'm');
        const auto maximum = store.put_bytes(maximal);
        require(maximum.size_bytes == max_file_bytes, "maximal object rejected or resized");
        store.verify(maximum);
        const auto maximum_path = object_path(root, maximum);
        const auto maximum_alias = base / "maximum-alias";
        std::filesystem::create_hard_link(maximum_path, maximum_alias);
        std::filesystem::remove(maximum_path);
        std::filesystem::create_hard_link(maximum_alias, maximum_path);
        rejects([&] { store.verify(maximum); }, "ArtifactObjectCorrupt", "hardlinked object verified");
        std::filesystem::remove(maximum_path);
        std::filesystem::remove(maximum_alias);
        const auto object_target = base / "object-target";
        write_file(object_target, "object-target");
        std::filesystem::create_symlink(object_target, maximum_path);
        rejects([&] { store.verify(maximum); }, "ArtifactObjectCorrupt", "symlink object verified");
        std::filesystem::remove(maximum_path);

        const auto source = base / "source.bin";
        write_file(source, "file-source");
        require(::chmod(source.c_str(), 0600) == 0, "source chmod setup failed");
        const auto source_dir = base / "source-dir";
        std::filesystem::create_directory(source_dir);
        std::filesystem::create_symlink(base, source_dir / "alias");
        rejects([&] { store.put_file(source_dir / "alias" / "source.bin"); },
                "ArtifactSourceSymlink", "symlink source component accepted");
        const auto file_id = store.put_file(source);
        require(file_id.sha256 == sha256("file-source") && file_id.size_bytes == 11,
                "file publication mismatch");
        require(store.put_file(source, file_id.sha256, file_id.size_bytes) == file_id,
                "idempotent file publication mismatch");
        const auto insecure_source = base / "insecure-source.bin";
        write_file(insecure_source, "insecure-source");
        require(::chmod(insecure_source.c_str(), 0644) == 0, "0644 source chmod setup failed");
        rejects([&] { store.put_file(insecure_source); }, "ArtifactSourcePermissions",
                "0644 source accepted");
        require(::chmod(insecure_source.c_str(), 0664) == 0, "0664 source chmod setup failed");
        rejects([&] { store.put_file(insecure_source); }, "ArtifactSourcePermissions",
                "0664 source accepted");
        rejects([&] { store.put_file(source, std::string(64, '0')); },
                "ArtifactExpectedDigestMismatch", "wrong expected digest accepted");
        rejects([&] { store.put_file(source, {}, 12); },
                "ArtifactExpectedSizeMismatch", "wrong expected size accepted");

        const auto missing = ArtifactIdV1{std::string(64, 'f'), 1};
        require(!store.contains(missing), "missing object reported as present");
        rejects([&] { store.verify(missing); }, "ArtifactObjectMissing", "missing object verified");
        rejects([&] { (void)store.inspect(missing); }, "ArtifactObjectMissing", "missing object inspected");
        rejects([&] { store.put_bytes(std::string(max_file_bytes + 1, 'x')); },
                "ArtifactObjectTooLarge", "oversized bytes accepted");

        const auto symlink = base / "source-symlink";
        std::filesystem::create_symlink(source, symlink);
        rejects([&] { store.put_file(symlink); }, "ArtifactSourceSymlink", "symlink source accepted");
        const auto hardlink = base / "source-hardlink";
        std::filesystem::create_hard_link(source, hardlink);
        rejects([&] { store.put_file(hardlink); }, "ArtifactSourceHardlink", "hardlink source accepted");
        const auto directory = base / "source-directory";
        std::filesystem::create_directory(directory);
        rejects([&] { store.put_file(directory); }, "ArtifactSourceNotRegular", "directory source accepted");
        const auto escaped = base / ".." / "escaped-source";
        rejects([&] { store.put_file(escaped); }, "ArtifactSourcePathEscape", "escaping source accepted");
        const auto too_large = base / "too-large.bin";
        std::ofstream large(too_large, std::ios::binary);
        large.seekp(static_cast<std::streamoff>(max_file_bytes));
        large.put('x');
        large.close();
        require(::chmod(too_large.c_str(), 0600) == 0, "oversized source chmod setup failed");
        rejects([&] { store.put_file(too_large); }, "ArtifactObjectTooLarge", "oversized file accepted");

        const auto corrupt = object_path(root, small);
        write_file(corrupt, "corrupt");
        rejects([&] { store.verify(small); }, "ArtifactObjectCorrupt", "corrupt object verified");
        ArtifactManifestV1 corrupt_manifest;
        corrupt_manifest.run_id = "run-corrupt";
        corrupt_manifest.node_id = "node-corrupt";
        corrupt_manifest.attempt_id = "attempt-corrupt";
        corrupt_manifest.status = "COMPLETED";
        corrupt_manifest.result_code = "CORRUPT";
        corrupt_manifest.objects.push_back({small, "application/octet-stream", "corrupt"});
        store.publish_manifest("manifest-corrupt", corrupt_manifest);
        rejects([&] { (void)store.resolve_manifest("manifest-corrupt"); },
                "ArtifactObjectCorrupt", "corrupt object resolved");
        rejects([&] { store.put_bytes(small_bytes); }, "ArtifactObjectConflict", "corrupt duplicate replaced");

        ArtifactManifestV1 manifest;
        manifest.run_id = "run-1";
        manifest.node_id = "node-1";
        manifest.attempt_id = "attempt-1";
        manifest.status = "COMPLETED";
        manifest.result_code = "OK";
        manifest.objects.push_back({maximum, "application/octet-stream", "worker"});
        auto non_ascii_manifest = manifest;
        non_ascii_manifest.run_id = "run-\xC3\xA9";
        rejects([&] { (void)encode_manifest_v1(non_ascii_manifest); },
                "ArtifactManifestInvalid", "non-ASCII manifest token accepted");
        const auto canonical = encode_manifest_v1(manifest);
        require(canonical ==
            "BRUCE_ARTIFACT_MANIFEST_V1\n"
            "run_id=run-1\n"
            "node_id=node-1\n"
            "attempt_id=attempt-1\n"
            "status=COMPLETED\n"
            "result_code=OK\n"
            "object_count=1\n"
            "object_0_sha256=" + maximum.sha256 + "\n"
            "object_0_size_bytes=33554432\n"
            "object_0_media_type=application/octet-stream\n"
            "object_0_origin=worker\n", "manifest field order is not canonical");
        require(decode_manifest_v1(canonical).objects.front().id == maximum,
                "manifest round trip lost object identity");
        const auto manifest_id = store.publish_manifest("manifest-1", manifest);
        require(manifest_id == "manifest-1", "manifest publication id mismatch");
        require(store.read_manifest("manifest-1").run_id == "run-1", "manifest read mismatch");
        require(is_valid_manifest_id_v1("manifest-1"), "valid manifest id rejected");
        require(!is_valid_manifest_id_v1("../escape"), "escaping manifest id accepted");
        require(!is_valid_manifest_id_v1("/absolute"), "absolute manifest id accepted");
        ArtifactManifestV1 resolve_manifest_data = manifest;
        resolve_manifest_data.objects.clear();
        resolve_manifest_data.objects.push_back({file_id, "application/octet-stream", "worker"});
        store.publish_manifest("manifest-resolve", resolve_manifest_data);
        const auto resolved = store.resolve_manifest("manifest-resolve");
        require(resolved.manifest_id == "manifest-resolve" && resolved.run_id == "run-1" &&
                    resolved.node_id == "node-1" && resolved.attempt_id == "attempt-1" &&
                    resolved.manifest_status == "COMPLETED" && resolved.result_code == "OK",
                "resolved manifest metadata mismatch");
        require(resolved.objects.size() == 1 && resolved.objects.front().id == file_id &&
                    resolved.objects.front().origin == "worker",
                "resolved manifest object mismatch");

        ArtifactManifestV1 ordered;
        ordered.run_id = "run-ordered";
        ordered.node_id = "node-ordered";
        ordered.attempt_id = "attempt-ordered";
        ordered.status = "COMPLETED";
        ordered.result_code = "OK";
        ordered.objects.push_back({file_id, "application/octet-stream", "first"});
        ordered.objects.push_back({empty, "application/octet-stream", ""});
        store.publish_manifest("manifest-ordered", ordered);
        const auto before_ordered_resolve = snapshot_tree(root);
        const auto resolved_ordered = store.resolve_manifest("manifest-ordered");
        require(resolved_ordered.objects.size() == 2 &&
                    resolved_ordered.objects[0].id == file_id &&
                    resolved_ordered.objects[0].origin == "first" &&
                    resolved_ordered.objects[1].id == empty &&
                    resolved_ordered.objects[1].origin.empty(),
                "resolved object order or empty origin mismatch");
        require(snapshot_tree(root) == before_ordered_resolve,
                "resolve modified the artifact store");

        ArtifactManifestV1 empty_manifest;
        empty_manifest.run_id = "run-empty";
        empty_manifest.node_id = "node-empty";
        empty_manifest.attempt_id = "attempt-empty";
        empty_manifest.status = "COMPLETED";
        empty_manifest.result_code = "EMPTY";
        store.publish_manifest("manifest-empty", empty_manifest);
        const auto resolved_empty = store.resolve_manifest("manifest-empty");
        require(resolved_empty.objects.empty() && resolved_empty.result_code == "EMPTY",
                "empty manifest resolution mismatch");

        ArtifactManifestV1 late_failure = ordered;
        late_failure.objects.push_back({ArtifactIdV1{std::string(64, 'f'), 9}, {}, "late"});
        store.publish_manifest("manifest-late-failure", late_failure);
        const auto before_late_failure = snapshot_tree(root);
        rejects([&] { (void)store.resolve_manifest("manifest-late-failure"); },
                "ArtifactObjectMissing", "late missing object resolved");
        require(snapshot_tree(root) == before_late_failure,
                "failed resolve modified the artifact store");
        rejects([&] { (void)store.resolve_manifest("missing-manifest"); },
                "ArtifactManifestMissing", "missing manifest resolved");
        rejects([&] { (void)store.resolve_manifest("../escape"); },
                "ArtifactManifestIdInvalid", "invalid manifest id resolved");
        require(store.publish_manifest("manifest-1", manifest) == "manifest-1",
                "duplicate manifest publication was not idempotent");
        const auto oversized_manifest_path = root / "manifests" / "manifest-oversize.manifest";
        write_file(oversized_manifest_path, std::string(max_file_bytes + 1, 'x'));
        require(::chmod(oversized_manifest_path.c_str(), 0600) == 0,
                "oversized manifest chmod setup failed");
        rejects([&] { (void)store.read_manifest("manifest-oversize"); },
                "ArtifactManifestTooLarge", "oversized existing manifest read");
        rejects([&] { store.publish_manifest("manifest-oversize", manifest); },
                "ArtifactManifestTooLarge", "oversized existing manifest compared");
        auto changed = manifest;
        changed.result_code = "CHANGED";
        rejects([&] { store.publish_manifest("manifest-1", changed); },
                "ArtifactManifestConflict", "conflicting manifest replaced");

        rejects([&] { decode_manifest_v1(canonical + "junk"); },
                "ArtifactManifestTrailingBytes", "manifest trailing bytes accepted");
        rejects([&] { decode_manifest_v1("BRUCE_ARTIFACT_MANIFEST_V1\nunknown=x\n"); },
                "ArtifactManifestUnknownField", "manifest unknown field accepted");
        rejects([&] { decode_manifest_v1(canonical + "payload"); },
                "ArtifactManifestTrailingBytes", "manifest payload accepted");
        rejects([&] { store.publish_manifest("../escape", manifest); },
                "ArtifactManifestIdInvalid", "escaping manifest id accepted");
        rejects([&] { store.publish_manifest("/absolute", manifest); },
                "ArtifactManifestIdInvalid", "absolute manifest id accepted");
        rejects([&] { decode_manifest_v1(canonical.substr(0, canonical.size() - 1)); },
                "ArtifactManifestTrailingBytes", "unterminated manifest accepted");

        std::filesystem::remove_all(base);
        std::cout << "Artifact Store V1 contract tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove_all(base);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
