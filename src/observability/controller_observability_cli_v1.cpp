#include "bruce/observability/controller_observability_cli_v1.hpp"

#include "bruce/core/types.hpp"
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace bruce {
namespace {
[[noreturn]] void invalid() { throw Error("ControllerObservabilityInvalidEvidence"); }
struct Fd {
    int value;
    explicit Fd(int fd) : value(fd) { if (fd < 0) invalid(); }
    ~Fd() { if (value >= 0) ::close(value); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
};
std::string bytes_from_private_record(const std::filesystem::path& path) {
    if (path.empty() || path.filename().empty()) invalid();
    std::vector<std::string> names;
    for (const auto& component : path) {
        const auto name = component.string();
        if (name == "/" || name == ".") continue;
        if (name.empty() || name == ".." || name.find('\0') != std::string::npos) invalid();
        names.push_back(name);
    }
    if (names.empty()) invalid();
    Fd parent(::open(path.is_absolute() ? "/" : ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    for (std::size_t index = 0; index + 1 < names.size(); ++index) {
        Fd next(::openat(parent.value, names[index].c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
        struct stat dir{};
        if (::fstat(next.value, &dir) != 0 || !S_ISDIR(dir.st_mode)) invalid();
        std::swap(parent.value, next.value);
    }
    Fd file(::openat(parent.value, names.back().c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY));
    struct stat stat{};
    if (::fstat(file.value, &stat) != 0 || !S_ISREG(stat.st_mode) || stat.st_uid != ::geteuid() ||
        (stat.st_mode & 0077) != 0 || stat.st_nlink != 1 || stat.st_size < 0 ||
        static_cast<std::uint64_t>(stat.st_size) > logic_mod_artifact_dag_record_limit_v6) invalid();
    std::string bytes;
    bytes.reserve(static_cast<std::size_t>(stat.st_size));
    std::array<char, 8192> buffer{};
    while (true) {
        const auto count = ::read(file.value, buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) invalid();
        if (count == 0) return bytes;
        const auto size = static_cast<std::size_t>(count);
        if (size > logic_mod_artifact_dag_record_limit_v6 - bytes.size()) invalid();
        bytes.append(buffer.data(), size);
    }
}
}
LogicModArtifactDagRunV6 read_controller_observability_v6_record_v1(const std::filesystem::path& record) {
    const auto bytes = bytes_from_private_record(record);
    try {
        const auto parsed = read_logic_mod_artifact_dag_record_v6(bytes);
        if (logic_mod_artifact_dag_record_v6(parsed) != bytes) invalid();
        return parsed;
    } catch (...) { invalid(); }
}
std::string render_controller_observability_error_v1() {
    return "{\"contract\":\"controller-observability-v1\",\"ok\":false,\"error\":\"INVALID_EVIDENCE\"}\n";
}
} // namespace bruce
