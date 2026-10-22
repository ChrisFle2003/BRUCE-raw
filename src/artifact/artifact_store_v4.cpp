#include "bruce/artifact/artifact_store_v4.hpp"
#include "bruce/evidence/bundle.hpp"

#include <algorithm>
#include <charconv>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace bruce::artifact_v4 {
namespace {
using namespace artifact_v1;
using Clock = std::chrono::steady_clock;
[[noreturn]] void invalid() { throw Error("PROTOCOL_ERROR"); }
void timely(Clock::time_point deadline) { if (Clock::now() >= deadline) invalid(); }
struct Fd {
    int fd;
    std::string path;
    ~Fd() { if (fd >= 0) ::close(fd); if (!path.empty()) ::unlink(path.c_str()); }
};
std::string object_path(const ArtifactIdV1& id) {
    return "/objects/" + id.sha256.substr(0, 2) + "/" + id.sha256 + ".blob";
}
MirrorResultV4 metadata(std::string_view id, const ArtifactManifestV1& manifest,
                        std::string_view bytes) {
    if (manifest.status != "READY" || bytes.size() > mirror_manifest_limit_v4) invalid();
    MirrorResultV4 result{std::string(id), sha256(bytes), bytes.size(), manifest.objects.size(), 0};
    for (const auto& object : manifest.objects) {
        if (object.id.size_bytes > mirror_total_object_limit_v4 - result.total_object_bytes) invalid();
        result.total_object_bytes += object.id.size_bytes;
    }
    return result;
}
std::string envelope(const MirrorResultV4& result, std::string_view bytes) {
    auto encoded = "BRUCE_ARTIFACT_MIRROR_V4\nmanifest_id=" + result.manifest_id +
        "\nmanifest_sha256=" + result.manifest_sha256 + "\nmanifest_size_bytes=" +
        std::to_string(result.manifest_size_bytes) + "\n" + std::string(bytes);
    if (encoded.size() > mirror_manifest_limit_v4) invalid();
    return encoded;
}
std::string_view line(std::string_view& bytes, std::string_view key) {
    if (!bytes.starts_with(key)) invalid();
    bytes.remove_prefix(key.size());
    const auto end = bytes.find('\n');
    if (end == std::string_view::npos) invalid();
    const auto value = bytes.substr(0, end); bytes.remove_prefix(end + 1); return value;
}
ArtifactManifestV1 decode_envelope(std::string_view bytes, std::string_view id) {
    if (line(bytes, "") != "BRUCE_ARTIFACT_MIRROR_V4" || line(bytes, "manifest_id=") != id) invalid();
    const auto digest = line(bytes, "manifest_sha256=");
    const auto text = line(bytes, "manifest_size_bytes=");
    std::uint64_t size{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), size);
    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        std::to_string(size) != text || size != bytes.size() || sha256(bytes) != digest) invalid();
    auto manifest = decode_manifest_v1(bytes);
    if (manifest.status != "READY") invalid();
    return manifest;
}
std::string exchange(const HttpsTransportV4& transport, std::string_view remote,
                     std::string_view path, std::string method, std::string_view upload,
                     std::uint64_t limit, Clock::time_point deadline,
                     std::optional<std::uint64_t> expected = {}) {
    timely(deadline);
    const HttpsRequestV4 request{artifact_v3::https_request_v3(remote, path, deadline), std::move(method), upload};
    bool have_headers = false;
    std::optional<std::uint64_t> declared;
    std::string bytes;
    transport(request, [&](int status, std::optional<std::uint64_t> length) {
        timely(deadline);
        if (have_headers || (request.method == "GET" ? status != 200 :
            (status != 200 && status != 201 && status != 204)) ||
            (length && (*length > limit || (expected && *length != *expected)))) invalid();
        have_headers = true; declared = length;
    }, [&](std::string_view chunk) {
        timely(deadline);
        if (!have_headers || chunk.size() > limit - bytes.size()) invalid();
        bytes.append(chunk);
    });
    timely(deadline);
    if (!have_headers || (declared && bytes.size() != *declared) ||
        (expected && bytes.size() != *expected)) invalid();
    return bytes;
}
std::string read_object(const ArtifactStoreV1& local, const ArtifactIdV1& id, Clock::time_point deadline) {
    char pattern[] = "/tmp/bruce-mirror-snapshot-XXXXXX";
    Fd file{::mkostemp(pattern, O_CLOEXEC), pattern};
    if (file.fd < 0 || ::fchmod(file.fd, 0600) != 0) invalid();
    local.copy_object_to_fd(id, file.fd, id.size_bytes, deadline);
    if (::lseek(file.fd, 0, SEEK_SET) < 0) invalid();
    std::string bytes(id.size_bytes, '\0');
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        timely(deadline);
        const auto count = ::read(file.fd, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) invalid();
        offset += static_cast<std::size_t>(count);
    }
    return bytes;
}
std::optional<ArtifactStoreV1> existing_store(const std::filesystem::path& store) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(store, error);
    if (error && error != std::errc::no_such_file_or_directory) invalid();
    if (status.type() == std::filesystem::file_type::not_found) return {};
    return ArtifactStoreV1::open_existing(store);
}
void require_missing_manifest(const ArtifactStoreV1& local, std::string_view id) {
    if (local.manifest_entry_exists(id)) invalid();
}
void validate_existing_objects(const ArtifactStoreV1& local, const ArtifactManifestV1& manifest) {
    for (const auto& object : manifest.objects) {
        try { if (local.inspect(object.id.sha256).id != object.id) invalid(); }
        catch (const Error& e) {
            if (std::string_view(e.what()) != "ArtifactObjectMissing") throw;
        }
    }
}
} // namespace

void https_transport_v4(const HttpsRequestV4& request,
    const artifact_v3::HttpsHeadersV3& headers, const artifact_v3::HttpsBodyV3& body) {
    if (request.method == "GET" && request.upload_bytes.empty()) artifact_v3::https_get_v3(request.endpoint, headers, body);
    else if (request.method == "PUT") artifact_v3::https_put_v3(request.endpoint, request.upload_bytes, headers, body);
    else invalid();
}
MirrorResultV4 mirror_to_remote_v4(std::string_view manifest_id,
    const std::filesystem::path& store, std::string_view remote, const HttpsTransportV4& transport) {
    try {
        const auto deadline = Clock::now() + std::chrono::seconds(30);
        if (!is_valid_manifest_id_v1(manifest_id)) invalid();
        (void)artifact_v3::https_request_v3(remote, "/manifests/check.mirror", deadline);
        auto local = ArtifactStoreV1::open_existing(store);
        const auto manifest = local.read_manifest(manifest_id);
        const auto bytes = encode_manifest_v1(manifest);
        const auto result = metadata(manifest_id, manifest, bytes);
        const auto wrapped = envelope(result, bytes);
        std::vector<std::string> objects;
        for (const auto& object : manifest.objects) objects.push_back(read_object(local, object.id, deadline));
        for (std::size_t i = 0; i < objects.size(); ++i)
            (void)exchange(transport, remote, object_path(manifest.objects[i].id), "PUT", objects[i], 65536, deadline);
        (void)exchange(transport, remote, "/manifests/" + std::string(manifest_id) + ".mirror", "PUT", wrapped, 65536, deadline);
        return result;
    } catch (...) { invalid(); }
}
MirrorResultV4 restore_from_remote_v4(std::string_view manifest_id,
    const std::filesystem::path& store, std::string_view remote, const HttpsTransportV4& transport) {
    try {
        const auto deadline = Clock::now() + std::chrono::seconds(30);
        if (!is_valid_manifest_id_v1(manifest_id)) invalid();
        (void)artifact_v3::https_request_v3(remote, "/manifests/check.mirror", deadline);
        auto existing = existing_store(store);
        if (existing) require_missing_manifest(*existing, manifest_id);
        const auto wrapped = exchange(transport, remote, "/manifests/" + std::string(manifest_id) + ".mirror",
                                      "GET", {}, mirror_manifest_limit_v4, deadline);
        const auto manifest = decode_envelope(wrapped, manifest_id);
        const auto result = metadata(manifest_id, manifest, encode_manifest_v1(manifest));
        if (existing) validate_existing_objects(*existing, manifest);
        std::vector<std::string> objects;
        for (const auto& object : manifest.objects) {
            auto bytes = exchange(transport, remote, object_path(object.id), "GET", {}, object.id.size_bytes,
                                  deadline, object.id.size_bytes);
            if (sha256(bytes) != object.id.sha256) invalid();
            objects.push_back(std::move(bytes));
        }
        timely(deadline);
        auto local = existing ? std::move(*existing) : ArtifactStoreV1::open(store);
        require_missing_manifest(local, manifest_id);
        validate_existing_objects(local, manifest);
        for (std::size_t i = 0; i < objects.size(); ++i) {
            timely(deadline);
            try { if (local.put_bytes(objects[i]) != manifest.objects[i].id) invalid(); }
            catch (const Error& e) {
                if (std::string_view(e.what()) != "ArtifactPublishSyncFailed") throw;
                local.verify(manifest.objects[i].id);
            }
        }
        timely(deadline);
        try { local.publish_manifest(manifest_id, manifest); }
        catch (const Error& e) {
            if (std::string_view(e.what()) != "ArtifactManifestSyncFailed" || local.read_manifest(manifest_id) != manifest) throw;
        }
        return result;
    } catch (...) { invalid(); }
}
} // namespace bruce::artifact_v4
