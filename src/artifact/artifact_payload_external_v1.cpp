#include "bruce/artifact/artifact_payload_external_v1.hpp"

#include "bruce/artifact/artifact_store_v1.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <unistd.h>

namespace bruce::artifact_v1 {
namespace {

const char* error_text(ExternalInputsErrorCodeV14 code) noexcept {
    switch (code) {
    case ExternalInputsErrorCodeV14::file_missing: return "ExternalInputsFileMissing";
    case ExternalInputsErrorCodeV14::file_too_large: return "ExternalInputsFileTooLarge";
    case ExternalInputsErrorCodeV14::file_not_regular: return "ExternalInputsFileNotRegular";
    case ExternalInputsErrorCodeV14::file_symlink: return "ExternalInputsFileSymlink";
    case ExternalInputsErrorCodeV14::file_permissions: return "ExternalInputsFilePermissions";
    case ExternalInputsErrorCodeV14::file_changed: return "ExternalInputsFileChanged";
    case ExternalInputsErrorCodeV14::invalid_json: return "ExternalInputsInvalidJson";
    case ExternalInputsErrorCodeV14::invalid_contract: return "ExternalInputsInvalidContract";
    case ExternalInputsErrorCodeV14::invalid_field: return "ExternalInputsInvalidField";
    case ExternalInputsErrorCodeV14::duplicate_field: return "ExternalInputsDuplicateField";
    case ExternalInputsErrorCodeV14::invalid_identifier: return "ExternalInputsInvalidIdentifier";
    case ExternalInputsErrorCodeV14::invalid_digest: return "ExternalInputsInvalidDigest";
    case ExternalInputsErrorCodeV14::invalid_number: return "ExternalInputsInvalidNumber";
    case ExternalInputsErrorCodeV14::binding_invalid: return "ExternalInputsBindingInvalid";
    case ExternalInputsErrorCodeV14::duplicate_binding: return "ExternalInputsDuplicateBinding";
    case ExternalInputsErrorCodeV14::duplicate_object: return "ExternalInputsDuplicateObject";
    case ExternalInputsErrorCodeV14::input_limit_exceeded: return "ExternalInputsInputLimitExceeded";
    }
    return "ExternalInputsInvalid";
}

class Digest {
public:
    Digest() : context_(EVP_MD_CTX_new(), EVP_MD_CTX_free) {
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

bool ascii_identifier(std::string_view value) {
    if (value.empty() || value.size() > artifact_manifest_token_limit_v1)
        return false;
    const auto first = static_cast<unsigned char>(value.front());
    const auto alnum = [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9');
    };
    if (!alnum(first)) return false;
    for (const auto c : value) {
        const auto byte = static_cast<unsigned char>(c);
        if (!alnum(byte) && byte != '_' && byte != '-' && byte != '.') return false;
    }
    return true;
}

bool digest_text(std::string_view value) {
    if (value.size() != 64) return false;
    for (const auto c : value) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

class Parser {
public:
    explicit Parser(std::string_view bytes) : bytes_(bytes) {}

    ExternalInputsSidecarV14 parse() {
        ExternalInputsSidecarV14 result;
        literal("{\"contract\":");
        result.contract = string();
        if (result.contract != artifact_payload_external_contract_v14)
            fail(ExternalInputsErrorCodeV14::invalid_contract);
        literal(",\"bindings\":[");
        if (!consume(']')) {
            while (true) {
                result.bindings.push_back(binding());
                for (std::size_t index = 0; index + 1 < result.bindings.size(); ++index) {
                    if (result.bindings[index].node_id == result.bindings.back().node_id)
                        fail(ExternalInputsErrorCodeV14::duplicate_binding);
                }
                if (consume(']')) break;
                expect(',');
            }
        }
        expect('}');
        if (position_ != bytes_.size()) fail(ExternalInputsErrorCodeV14::invalid_json);
        if (result.bindings.size() > artifact_payload_external_input_limit_v14)
            fail(ExternalInputsErrorCodeV14::input_limit_exceeded);
        return result;
    }

private:
    std::string_view bytes_;
    std::size_t position_ = 0;

    [[noreturn]] void fail(ExternalInputsErrorCodeV14 code) const {
        throw ExternalInputsErrorV14(code);
    }
    void expect(char value) {
        if (position_ >= bytes_.size() || bytes_[position_] != value)
            fail(ExternalInputsErrorCodeV14::invalid_json);
        ++position_;
    }
    bool consume(char value) {
        if (position_ < bytes_.size() && bytes_[position_] == value) {
            ++position_;
            return true;
        }
        return false;
    }
    void literal(std::string_view value) {
        if (bytes_.substr(position_, value.size()) != value)
            fail(ExternalInputsErrorCodeV14::invalid_json);
        position_ += value.size();
    }
    std::string string() {
        expect('"');
        const auto start = position_;
        while (position_ < bytes_.size()) {
            const auto c = static_cast<unsigned char>(bytes_[position_++]);
            if (c == '"') return std::string(bytes_.substr(start, position_ - start - 1));
            if (c < 0x20 || c == '\\') fail(ExternalInputsErrorCodeV14::invalid_json);
            if (c >= 0x80) fail(ExternalInputsErrorCodeV14::invalid_json);
        }
        fail(ExternalInputsErrorCodeV14::invalid_json);
    }
    std::uint64_t number() {
        const auto start = position_;
        if (position_ >= bytes_.size() || bytes_[position_] < '0' || bytes_[position_] > '9')
            fail(ExternalInputsErrorCodeV14::invalid_number);
        if (bytes_[position_] == '0') {
            ++position_;
            if (position_ < bytes_.size() && bytes_[position_] >= '0' && bytes_[position_] <= '9')
                fail(ExternalInputsErrorCodeV14::invalid_number);
        } else {
            while (position_ < bytes_.size() && bytes_[position_] >= '0' &&
                   bytes_[position_] <= '9') ++position_;
        }
        std::uint64_t result{};
        const auto parsed = std::from_chars(bytes_.data() + start, bytes_.data() + position_, result);
        if (parsed.ec != std::errc{} || parsed.ptr != bytes_.data() + position_)
            fail(ExternalInputsErrorCodeV14::invalid_number);
        return result;
    }
    void identifier(std::string_view value) {
        if (!ascii_identifier(value) || value.find("..") != std::string_view::npos ||
            value.find('/') != std::string_view::npos || value.find('\\') != std::string_view::npos)
            fail(ExternalInputsErrorCodeV14::invalid_identifier);
    }
    ExternalInputBindingV14 binding() {
        ExternalInputBindingV14 result;
        literal("{\"node_id\":");
        result.node_id = string();
        identifier(result.node_id);
        literal(",\"inputs\":[");
        if (!consume(']')) {
            while (true) {
                if (result.inputs.size() >= artifact_payload_external_input_limit_v14)
                    fail(ExternalInputsErrorCodeV14::input_limit_exceeded);
                result.inputs.push_back(input());
                for (std::size_t index = 0; index + 1 < result.inputs.size(); ++index) {
                    const auto& previous = result.inputs[index];
                    const auto& current = result.inputs.back();
                    if (previous.manifest_id == current.manifest_id &&
                        previous.object_index == current.object_index)
                        fail(ExternalInputsErrorCodeV14::duplicate_object);
                }
                if (consume(']')) break;
                expect(',');
            }
        }
        expect('}');
        return result;
    }
    ExternalInputRefV14 input() {
        ExternalInputRefV14 result;
        literal("{\"manifest_id\":");
        result.manifest_id = string();
        identifier(result.manifest_id);
        literal(",\"object_index\":");
        result.object_index = number();
        literal(",\"sha256\":");
        result.sha256 = string();
        if (!digest_text(result.sha256)) fail(ExternalInputsErrorCodeV14::invalid_digest);
        literal(",\"size_bytes\":");
        result.size_bytes = number();
        if (result.size_bytes > max_file_bytes)
            fail(ExternalInputsErrorCodeV14::input_limit_exceeded);
        literal(",\"source_run_id\":");
        result.source_run_id = string();
        identifier(result.source_run_id);
        literal(",\"source_node_id\":");
        result.source_node_id = string();
        identifier(result.source_node_id);
        literal(",\"source_attempt_id\":");
        result.source_attempt_id = string();
        identifier(result.source_attempt_id);
        expect('}');
        return result;
    }
};

class Fd {
public:
    explicit Fd(int fd = -1) : fd_(fd) {}
    ~Fd() { if (fd_ >= 0) ::close(fd_); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    int get() const noexcept { return fd_; }
private:
    int fd_;
};

} // namespace

ExternalInputsErrorV14::ExternalInputsErrorV14(ExternalInputsErrorCodeV14 code)
    : Error(error_text(code)), code_(code) {}

ExternalInputsSidecarV14 parse_external_inputs_v14(std::string_view bytes) {
    if (bytes.empty() || bytes.size() > max_file_bytes)
        throw ExternalInputsErrorV14(bytes.empty() ? ExternalInputsErrorCodeV14::invalid_json
                                                    : ExternalInputsErrorCodeV14::file_too_large);
    try {
        return Parser(bytes).parse();
    } catch (const ExternalInputsErrorV14&) {
        throw;
    } catch (...) {
        throw ExternalInputsErrorV14(ExternalInputsErrorCodeV14::invalid_json);
    }
}

std::string digest_external_inputs_v14(std::string_view bytes) {
    Digest digest;
    digest.update(bytes.data(), bytes.size());
    return digest.finish();
}

ExternalInputsFileV14 read_external_inputs_v14(const std::filesystem::path& path) {
    Fd file(::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
    if (file.get() < 0) {
        if (errno == ELOOP) throw ExternalInputsErrorV14(ExternalInputsErrorCodeV14::file_symlink);
        throw ExternalInputsErrorV14(ExternalInputsErrorCodeV14::file_missing);
    }
    struct stat before{};
    if (::fstat(file.get(), &before) != 0)
        throw ExternalInputsErrorV14(ExternalInputsErrorCodeV14::file_missing);
    if (!S_ISREG(before.st_mode))
        throw ExternalInputsErrorV14(ExternalInputsErrorCodeV14::file_not_regular);
    if (before.st_uid != ::geteuid() || (before.st_mode & 0077) != 0 || before.st_nlink != 1)
        throw ExternalInputsErrorV14(ExternalInputsErrorCodeV14::file_permissions);
    if (before.st_size < 0 || static_cast<std::uint64_t>(before.st_size) > max_file_bytes)
        throw ExternalInputsErrorV14(ExternalInputsErrorCodeV14::file_too_large);
    if (::lseek(file.get(), 0, SEEK_SET) < 0)
        throw ExternalInputsErrorV14(ExternalInputsErrorCodeV14::file_missing);

    std::string bytes;
    bytes.reserve(static_cast<std::size_t>(before.st_size));
    Digest digest;
    std::array<char, 65536> buffer{};
    while (true) {
        const auto count = ::read(file.get(), buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) throw ExternalInputsErrorV14(ExternalInputsErrorCodeV14::file_missing);
        if (count == 0) break;
        if (bytes.size() > max_file_bytes - static_cast<std::size_t>(count))
            throw ExternalInputsErrorV14(ExternalInputsErrorCodeV14::file_too_large);
        bytes.append(buffer.data(), static_cast<std::size_t>(count));
        digest.update(buffer.data(), static_cast<std::size_t>(count));
    }
    struct stat after{};
    if (::fstat(file.get(), &after) != 0 || after.st_dev != before.st_dev ||
        after.st_ino != before.st_ino || after.st_size != before.st_size ||
        after.st_uid != ::geteuid() || (after.st_mode & 0077) != 0 || after.st_nlink != 1)
        throw ExternalInputsErrorV14(ExternalInputsErrorCodeV14::file_changed);
    auto sidecar = parse_external_inputs_v14(bytes);
    return ExternalInputsFileV14{std::move(sidecar), digest.finish(),
                                 static_cast<std::uint64_t>(bytes.size())};
}

} // namespace bruce::artifact_v1
