#include "bruce/artifact/artifact_payload_v1.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <array>
#include <memory>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utility>

namespace bruce::artifact_v1 {
namespace {

class PayloadDigest {
public:
    PayloadDigest() : context_(EVP_MD_CTX_new(), EVP_MD_CTX_free) {
        if (!context_ || EVP_DigestInit_ex(context_.get(), EVP_sha256(), nullptr) != 1)
            throw Error("ArtifactDigestFailure");
    }

    void update(const void* data, std::size_t size) {
        if (EVP_DigestUpdate(context_.get(), data, size) != 1)
            throw Error("ArtifactDigestFailure");
    }

    std::string finish() {
        std::array<unsigned char, EVP_MAX_MD_SIZE> bytes{};
        unsigned length = 0;
        if (EVP_DigestFinal_ex(context_.get(), bytes.data(), &length) != 1)
            throw Error("ArtifactDigestFailure");
        static constexpr char hex[] = "0123456789abcdef";
        std::string result;
        result.reserve(length * 2);
        for (unsigned index = 0; index < length; ++index) {
            result.push_back(hex[bytes[index] >> 4]);
            result.push_back(hex[bytes[index] & 15]);
        }
        return result;
    }

private:
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context_;
};

bool digest_text(std::string_view value) noexcept {
    if (value.size() != 64) return false;
    for (const unsigned char c : value) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

const char* error_text(ArtifactPayloadErrorCodeV13 code) noexcept {
    switch (code) {
    case ArtifactPayloadErrorCodeV13::invalid_reference:
        return "ArtifactPayloadReferenceInvalid";
    case ArtifactPayloadErrorCodeV13::manifest_invalid:
        return "ArtifactPayloadManifestInvalid";
    case ArtifactPayloadErrorCodeV13::manifest_not_ready:
        return "ArtifactPayloadManifestNotReady";
    case ArtifactPayloadErrorCodeV13::object_index_out_of_range:
        return "ArtifactPayloadObjectIndexOutOfRange";
    case ArtifactPayloadErrorCodeV13::reference_mismatch:
        return "ArtifactPayloadReferenceMismatch";
    case ArtifactPayloadErrorCodeV13::provenance_mismatch:
        return "ArtifactPayloadProvenanceMismatch";
    case ArtifactPayloadErrorCodeV13::object_limit_exceeded:
        return "ArtifactPayloadObjectLimitExceeded";
    case ArtifactPayloadErrorCodeV13::object_verification_failed:
        return "ArtifactPayloadObjectVerificationFailed";
    }
    return "ArtifactPayloadInvalid";
}

} // namespace

ArtifactPayloadErrorV13::ArtifactPayloadErrorV13(ArtifactPayloadErrorCodeV13 code)
    : Error(error_text(code)), code_(code) {}

namespace {

const char* materialization_error_text(ArtifactPayloadMaterializationErrorV13 code) noexcept {
    switch (code) {
    case ArtifactPayloadMaterializationErrorV13::destination_invalid:
        return "ArtifactPayloadDestinationInvalid";
    case ArtifactPayloadMaterializationErrorV13::destination_not_empty:
        return "ArtifactPayloadDestinationNotEmpty";
    case ArtifactPayloadMaterializationErrorV13::input_limit_exceeded:
        return "ArtifactPayloadInputLimitExceeded";
    case ArtifactPayloadMaterializationErrorV13::object_limit_exceeded:
        return "ArtifactPayloadObjectLimitExceeded";
    case ArtifactPayloadMaterializationErrorV13::deadline_exceeded:
        return "ArtifactPayloadDeadlineExceeded";
    case ArtifactPayloadMaterializationErrorV13::object_copy_failed:
        return "ArtifactPayloadObjectCopyFailed";
    }
    return "ArtifactPayloadMaterializationFailed";
}

bool destination_is_private(int destination_fd, std::uint64_t& size) {
    if (destination_fd < 0) return false;
    struct stat status{};
    if (::fstat(destination_fd, &status) != 0 || !S_ISREG(status.st_mode) ||
        status.st_size < 0 || status.st_uid != ::geteuid() ||
        (status.st_mode & 0077) != 0 || status.st_nlink != 1)
        return false;
    size = static_cast<std::uint64_t>(status.st_size);
    return ::lseek(destination_fd, 0, SEEK_SET) >= 0;
}

void clear_destination(int destination_fd) noexcept {
    if (destination_fd < 0) return;
    (void)::ftruncate(destination_fd, 0);
    (void)::lseek(destination_fd, 0, SEEK_SET);
}

void verify_materialized_destination(
    int destination_fd,
    const std::vector<ArtifactPayloadSelectionV13>& inputs,
    std::uint64_t expected_size,
    std::chrono::steady_clock::time_point deadline) {
    struct stat status{};
    if (::fstat(destination_fd, &status) != 0 || !S_ISREG(status.st_mode) ||
        status.st_size < 0 || status.st_uid != ::geteuid() ||
        (status.st_mode & 0077) != 0 || status.st_nlink != 1 ||
        static_cast<std::uint64_t>(status.st_size) != expected_size)
        throw Error("ArtifactPayloadDestinationIntegrityMismatch");
    if (::lseek(destination_fd, 0, SEEK_SET) < 0)
        throw Error("ArtifactPayloadDestinationIntegrityMismatch");

    std::array<char, 65536> buffer{};
    for (const auto& input : inputs) {
        PayloadDigest digest;
        std::uint64_t remaining = input.object.size_bytes;
        while (remaining != 0) {
            if (std::chrono::steady_clock::now() >= deadline)
                throw Error("ArtifactCopyDeadlineExceeded");
            const auto requested = std::min<std::uint64_t>(remaining, buffer.size());
            const auto count = ::read(destination_fd, buffer.data(), static_cast<std::size_t>(requested));
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) throw Error("ArtifactPayloadDestinationIntegrityMismatch");
            digest.update(buffer.data(), static_cast<std::size_t>(count));
            remaining -= static_cast<std::uint64_t>(count);
        }
        if (digest.finish() != input.reference.sha256)
            throw Error("ArtifactPayloadDestinationIntegrityMismatch");
    }
    struct stat final_status{};
    if (::fstat(destination_fd, &final_status) != 0 || final_status.st_size < 0 ||
        final_status.st_uid != ::geteuid() || (final_status.st_mode & 0077) != 0 ||
        final_status.st_nlink != 1 ||
        static_cast<std::uint64_t>(final_status.st_size) != expected_size)
        throw Error("ArtifactPayloadDestinationIntegrityMismatch");
}

} // namespace

ArtifactPayloadMaterializationExceptionV13::ArtifactPayloadMaterializationExceptionV13(
    ArtifactPayloadMaterializationErrorV13 code)
    : Error(materialization_error_text(code)), code_(code) {}

bool is_valid_artifact_input_ref_v13(const ArtifactInputRefV13& reference) noexcept {
    return is_valid_manifest_id_v1(reference.manifest_id) && digest_text(reference.sha256) &&
           reference.size_bytes <= artifact_store_max_object_bytes_v1;
}

void validate_artifact_input_ref_v13(const ArtifactInputRefV13& reference) {
    if (!is_valid_artifact_input_ref_v13(reference))
        throw ArtifactPayloadErrorV13(ArtifactPayloadErrorCodeV13::invalid_reference);
}

ArtifactPayloadSelectionV13 select_artifact_input_v13(
    const ArtifactStoreV1& store,
    const ArtifactInputRefV13& reference,
    const ArtifactPayloadProvenanceV13& provenance) {
    validate_artifact_input_ref_v13(reference);

    ArtifactManifestV1 manifest;
    try {
        manifest = store.read_manifest(reference.manifest_id);
    } catch (const Error&) {
        throw ArtifactPayloadErrorV13(ArtifactPayloadErrorCodeV13::manifest_invalid);
    }

    if (manifest.status != "READY")
        throw ArtifactPayloadErrorV13(ArtifactPayloadErrorCodeV13::manifest_not_ready);
    if (manifest.objects.size() > artifact_payload_input_object_limit_v13)
        throw ArtifactPayloadErrorV13(ArtifactPayloadErrorCodeV13::object_limit_exceeded);
    if (provenance.run_id != manifest.run_id || provenance.node_id != manifest.node_id ||
        provenance.attempt_id != manifest.attempt_id)
        throw ArtifactPayloadErrorV13(ArtifactPayloadErrorCodeV13::provenance_mismatch);
    if (reference.object_index >= manifest.objects.size())
        throw ArtifactPayloadErrorV13(ArtifactPayloadErrorCodeV13::object_index_out_of_range);

    const auto& selected = manifest.objects[static_cast<std::size_t>(reference.object_index)];
    if (selected.id.sha256 != reference.sha256 || selected.id.size_bytes != reference.size_bytes)
        throw ArtifactPayloadErrorV13(ArtifactPayloadErrorCodeV13::reference_mismatch);

    try {
        // The Artifact Store verifies regular-file type, owner-only mode,
        // single-hardlink identity, exact size, and complete SHA-256 content.
        store.verify(selected.id);
    } catch (const Error&) {
        throw ArtifactPayloadErrorV13(ArtifactPayloadErrorCodeV13::object_verification_failed);
    }

    ArtifactPayloadSelectionV13 result;
    result.reference = reference;
    result.object = selected.id;
    result.run_id = manifest.run_id;
    result.node_id = manifest.node_id;
    result.attempt_id = manifest.attempt_id;
    result.result_code = manifest.result_code;
    result.origin = selected.origin;
    return result;
}

ArtifactPayloadMaterializationV13 materialize_artifact_inputs_v13(
    const ArtifactStoreV1& store,
    const std::vector<ArtifactPayloadSelectionV13>& inputs,
    int destination_fd,
    std::uint64_t max_input_bytes,
    std::chrono::steady_clock::time_point deadline) {
    if (inputs.size() > artifact_payload_input_object_limit_v13)
        throw ArtifactPayloadMaterializationExceptionV13(
            ArtifactPayloadMaterializationErrorV13::object_limit_exceeded);
    std::uint64_t destination_size = 0;
    if (!destination_is_private(destination_fd, destination_size))
        throw ArtifactPayloadMaterializationExceptionV13(
            ArtifactPayloadMaterializationErrorV13::destination_invalid);
    if (destination_size != 0)
        throw ArtifactPayloadMaterializationExceptionV13(
            ArtifactPayloadMaterializationErrorV13::destination_not_empty);

    ArtifactPayloadMaterializationV13 result;
    result.objects.reserve(inputs.size());
    try {
        for (const auto& input : inputs) {
            if (std::chrono::steady_clock::now() >= deadline)
                throw ArtifactPayloadMaterializationExceptionV13(
                    ArtifactPayloadMaterializationErrorV13::deadline_exceeded);
            if (input.reference.sha256 != input.object.sha256 ||
                input.reference.size_bytes != input.object.size_bytes ||
                !is_valid_artifact_input_ref_v13(input.reference))
                throw ArtifactPayloadMaterializationExceptionV13(
                    ArtifactPayloadMaterializationErrorV13::object_copy_failed);
            if (input.object.size_bytes > max_input_bytes ||
                result.bytes_written > max_input_bytes - input.object.size_bytes)
                throw ArtifactPayloadMaterializationExceptionV13(
                    ArtifactPayloadMaterializationErrorV13::input_limit_exceeded);

            const auto usage = store.copy_object_to_fd(
                input.object, destination_fd, max_input_bytes, deadline);
            if (usage.bytes_written != input.object.size_bytes)
                throw ArtifactPayloadMaterializationExceptionV13(
                    ArtifactPayloadMaterializationErrorV13::object_copy_failed);
            result.bytes_written += usage.bytes_written;
            result.objects.push_back(ArtifactPayloadMaterializedObjectV13{
                input.reference, usage.object, usage.bytes_written, usage.chunks});
        }
        if (::fsync(destination_fd) != 0)
            throw ArtifactPayloadMaterializationExceptionV13(
                ArtifactPayloadMaterializationErrorV13::object_copy_failed);
        struct stat status{};
        if (::fstat(destination_fd, &status) != 0 || status.st_size < 0 ||
            static_cast<std::uint64_t>(status.st_size) != result.bytes_written)
            throw ArtifactPayloadMaterializationExceptionV13(
                ArtifactPayloadMaterializationErrorV13::object_copy_failed);
        try {
            verify_materialized_destination(destination_fd, inputs, result.bytes_written, deadline);
        } catch (const Error& error) {
            const auto message = std::string_view(error.what());
            if (message == "ArtifactCopyDeadlineExceeded")
                throw ArtifactPayloadMaterializationExceptionV13(
                    ArtifactPayloadMaterializationErrorV13::deadline_exceeded);
            throw ArtifactPayloadMaterializationExceptionV13(
                ArtifactPayloadMaterializationErrorV13::object_copy_failed);
        }
        result.object_count = static_cast<std::uint64_t>(result.objects.size());
        return result;
    } catch (const ArtifactPayloadMaterializationExceptionV13&) {
        clear_destination(destination_fd);
        throw;
    } catch (const Error& error) {
        clear_destination(destination_fd);
        const auto message = std::string_view(error.what());
        if (message == "ArtifactCopyDeadlineExceeded")
            throw ArtifactPayloadMaterializationExceptionV13(
                ArtifactPayloadMaterializationErrorV13::deadline_exceeded);
        if (message == "ArtifactCopyLimitExceeded")
            throw ArtifactPayloadMaterializationExceptionV13(
                ArtifactPayloadMaterializationErrorV13::input_limit_exceeded);
        throw ArtifactPayloadMaterializationExceptionV13(
            ArtifactPayloadMaterializationErrorV13::object_copy_failed);
    }
}

} // namespace bruce::artifact_v1
