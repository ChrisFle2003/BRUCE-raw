#include "bruce/mod/package_inspection_v2.hpp"

#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace bruce {
namespace {

constexpr std::string_view data_header = "BRUCE_RUNTIME_DATA_MOD_V1\n";
constexpr std::string_view logic_header = "BRUCE_LOGIC_MOD_PLUGIN_V1\n";

class FileDescriptor {
public:
    explicit FileDescriptor(const int value) : value_(value) {}
    ~FileDescriptor() { if (value_ >= 0) ::close(value_); }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    FileDescriptor(FileDescriptor&& other) noexcept : value_(std::exchange(other.value_, -1)) {}
    FileDescriptor& operator=(FileDescriptor&& other) noexcept {
        if (this != &other) {
            if (value_ >= 0) ::close(value_);
            value_ = std::exchange(other.value_, -1);
        }
        return *this;
    }
    [[nodiscard]] int get() const noexcept { return value_; }
private:
    int value_;
};

[[noreturn]] void file_rejected() { throw Error("ModPackageFileRejected"); }
[[noreturn]] void trust_store_file_rejected() { throw Error("ModTrustStoreFileRejected"); }

template <typename Reject>
[[nodiscard]] FileDescriptor open_anchored_regular_file(const std::string_view path,
                                                        Reject reject) {
    if (path.empty() || path.find('\0') != std::string_view::npos) reject();
    const std::filesystem::path input{std::string(path)};
    if (input.filename().empty() || input.filename() == "." || input.filename() == "..") reject();
    std::vector<std::string> components;
    for (const auto& component : input) {
        const auto name = component.string();
        if (name == "/" || name == ".") continue;
        if (name.empty() || name == ".." || name.find('\0') != std::string::npos) reject();
        components.push_back(name);
    }
    if (components.empty()) reject();
    FileDescriptor current(::open(input.is_absolute() ? "/" : ".",
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    if (current.get() < 0) reject();
    for (std::size_t index = 0; index + 1 < components.size(); ++index) {
        FileDescriptor next(::openat(current.get(), components[index].c_str(),
            O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
        if (next.get() < 0) reject();
        current = std::move(next);
    }
    FileDescriptor result(::openat(current.get(), components.back().c_str(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY));
    if (result.get() < 0) reject();
    return result;
}

[[nodiscard]] bool same_file_snapshot(const struct stat& before,
                                      const struct stat& after) noexcept {
    return before.st_dev == after.st_dev && before.st_ino == after.st_ino &&
        before.st_mode == after.st_mode && before.st_nlink == after.st_nlink &&
        before.st_size == after.st_size &&
        before.st_mtim.tv_sec == after.st_mtim.tv_sec &&
        before.st_mtim.tv_nsec == after.st_mtim.tv_nsec &&
        before.st_ctim.tv_sec == after.st_ctim.tv_sec &&
        before.st_ctim.tv_nsec == after.st_ctim.tv_nsec;
}

[[nodiscard]] std::string_view kind_name(const ModPackageKindV2 kind) {
    switch (kind) {
    case ModPackageKindV2::data: return "data";
    case ModPackageKindV2::logic: return "logic";
    }
    throw Error("InvalidModPackageKind");
}

[[nodiscard]] std::string render(const ModPackageInspectionV2& inspection,
                                 const bool verified) {
    std::string result = "{\"contract\":" + json_string(verified
        ? "mod-package-verification-v2" : "mod-package-inspection-v2") +
        ",\"kind\":" + json_string(kind_name(inspection.kind)) +
        ",\"mod_id\":" + json_string(inspection.mod_id) +
        ",\"version\":" + json_string(inspection.version) +
        ",\"package_digest\":" + json_string(inspection.package_digest) +
        ",\"capabilities\":[";
    for (std::size_t index = 0; index < inspection.capabilities.size(); ++index) {
        if (index != 0) result += ',';
        result += json_string(inspection.capabilities[index]);
    }
    result += ']';
    if (verified) result += ",\"verified\":true";
    return result + '}';
}

} // namespace

DetectedModPackageV2::DetectedModPackageV2(RuntimeDataModPackageV1 package)
    : package_(std::move(package)) {}

DetectedModPackageV2::DetectedModPackageV2(LogicModPackageV1 package)
    : package_(std::move(package)) {}

ModPackageKindV2 DetectedModPackageV2::kind() const noexcept {
    return std::holds_alternative<RuntimeDataModPackageV1>(package_)
        ? ModPackageKindV2::data : ModPackageKindV2::logic;
}

const RuntimeDataModPackageV1* DetectedModPackageV2::data_package() const noexcept {
    return std::get_if<RuntimeDataModPackageV1>(&package_);
}

const LogicModPackageV1* DetectedModPackageV2::logic_package() const noexcept {
    return std::get_if<LogicModPackageV1>(&package_);
}

DetectedModPackageV2 detect_mod_package_v2(const std::string_view bytes) {
    if (bytes.size() > max_runtime_mod_package_bytes_v1) throw Error("RuntimeModPackageTooLarge");
    if (bytes.starts_with(data_header)) {
        return DetectedModPackageV2(unpack_runtime_data_mod_package_v1(bytes));
    }
    if (bytes.starts_with(logic_header)) {
        return DetectedModPackageV2(unpack_logic_mod_package_v1(bytes));
    }
    throw Error("InvalidModPackageEnvelope");
}

std::string read_mod_package_file_v2(const std::string_view path) {
    if (path.empty() || path.find('\0') != std::string_view::npos) file_rejected();
    const FileDescriptor file(::open(std::string(path).c_str(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    if (file.get() < 0) file_rejected();
    struct stat before {};
    if (::fstat(file.get(), &before) != 0 || !S_ISREG(before.st_mode) || before.st_size < 0 ||
        static_cast<std::uint64_t>(before.st_size) > max_runtime_mod_package_bytes_v1) file_rejected();
    std::string bytes;
    bytes.reserve(static_cast<std::size_t>(before.st_size));
    std::array<char, 8192> buffer {};
    while (true) {
        const auto count = ::read(file.get(), buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) file_rejected();
        if (count == 0) break;
        if (static_cast<std::size_t>(count) > max_runtime_mod_package_bytes_v1 - bytes.size())
            throw Error("RuntimeModPackageTooLarge");
        bytes.append(buffer.data(), static_cast<std::size_t>(count));
    }
    struct stat after {};
    if (::fstat(file.get(), &after) != 0 || before.st_dev != after.st_dev ||
        before.st_ino != after.st_ino || before.st_size != after.st_size ||
        before.st_mtim.tv_sec != after.st_mtim.tv_sec ||
        before.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
        static_cast<std::uint64_t>(after.st_size) != bytes.size()) file_rejected();
    return bytes;
}

std::string read_mod_trust_store_file_v1(const std::string_view path) {
    constexpr std::size_t byte_limit = 65536;
    auto reject = [] { trust_store_file_rejected(); };
    const auto file = open_anchored_regular_file(path, reject);
    struct stat before {};
    if (::fstat(file.get(), &before) != 0 || !S_ISREG(before.st_mode) || before.st_size < 0 ||
        static_cast<std::uint64_t>(before.st_size) > byte_limit) reject();
    std::string bytes;
    bytes.reserve(static_cast<std::size_t>(before.st_size));
    std::array<char, 8192> buffer {};
    while (true) {
        const auto count = ::read(file.get(), buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) reject();
        if (count == 0) break;
        if (static_cast<std::size_t>(count) > byte_limit - bytes.size()) reject();
        bytes.append(buffer.data(), static_cast<std::size_t>(count));
    }
    struct stat after {};
    if (::fstat(file.get(), &after) != 0 || !same_file_snapshot(before, after) ||
        static_cast<std::uint64_t>(after.st_size) != bytes.size()) reject();
    return bytes;
}

ModPackageInspectionV2 inspect_mod_package_v2(const DetectedModPackageV2& package) {
    if (const auto* data = package.data_package()) {
        return {ModPackageKindV2::data, data->manifest().mod_id, data->manifest().version,
            data->package_digest(), data->manifest().capabilities};
    }
    const auto* logic = package.logic_package();
    if (!logic) throw Error("InvalidModPackageKind");
    return {ModPackageKindV2::logic, logic->manifest().mod_id, logic->manifest().version,
        logic->package_digest(), logic->capabilities()};
}

void verify_mod_package_v2(const DetectedModPackageV2& package,
                           const RuntimeModTrustStoreV1& trust) {
    if (const auto* data = package.data_package()) {
        verify_runtime_data_mod_package_v1(*data, trust);
        return;
    }
    const auto* logic = package.logic_package();
    if (!logic) throw Error("InvalidModPackageKind");
    verify_logic_mod_package_v1(*logic, trust);
}

std::string render_mod_package_inspection_v2(const ModPackageInspectionV2& inspection) {
    return render(inspection, false);
}

std::string render_mod_package_verification_v2(const ModPackageInspectionV2& inspection) {
    return render(inspection, true);
}

} // namespace bruce
