#include "bruce/worker/worker_abort_control_v1.hpp"

#include "bruce/core/types.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

namespace bruce::worker_v1 {
namespace {

class Fd {
public:
    explicit Fd(int value = -1) : value_(value) {}
    ~Fd() { if (value_ >= 0) ::close(value_); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept : value_(other.value_) { other.value_ = -1; }
    Fd& operator=(Fd&& other) noexcept {
        if (this != &other) {
            if (value_ >= 0) ::close(value_);
            value_ = other.value_;
            other.value_ = -1;
        }
        return *this;
    }
    int get() const { return value_; }
    explicit operator bool() const { return value_ >= 0; }

private:
    int value_;
};

[[noreturn]] void protocol_error() { throw bruce::Error("PROTOCOL_ERROR"); }

std::vector<std::string> path_components(const std::filesystem::path& path) {
    if (path.empty()) protocol_error();
    std::vector<std::string> result;
    for (const auto& component : path) {
        const std::string name = component.string();
        if (name.empty() || name == "." || name == "/") continue;
        if (name == ".." || name.find('/') != std::string::npos) protocol_error();
        result.push_back(name);
    }
    if (result.empty()) protocol_error();
    return result;
}

void check_directory(int fd) {
    struct stat status{};
    if (::fstat(fd, &status) != 0 || !S_ISDIR(status.st_mode) ||
        status.st_uid != ::geteuid() || (status.st_mode & 07777) != 0700)
        protocol_error();
}

Fd open_directory_path(const std::filesystem::path& path) {
    const auto names = path_components(path);
    Fd current(::open(path.is_absolute() ? "/" : ".",
                       O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (!current) protocol_error();
    for (const auto& name : names) {
        Fd next(::openat(current.get(), name.c_str(),
                         O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
        if (!next) protocol_error();
        current = std::move(next);
    }
    check_directory(current.get());
    return current;
}

Fd open_control(const std::filesystem::path& run_root) {
    Fd root = open_directory_path(run_root);
    Fd control(::openat(root.get(), "control",
                         O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (!control) protocol_error();
    check_directory(control.get());
    return control;
}

bool same_identity(const struct stat& left, const struct stat& right) {
    return left.st_dev == right.st_dev && left.st_ino == right.st_ino &&
           left.st_mode == right.st_mode && left.st_uid == right.st_uid &&
           left.st_gid == right.st_gid && left.st_nlink == right.st_nlink &&
           left.st_size == right.st_size && left.st_mtim.tv_sec == right.st_mtim.tv_sec &&
           left.st_mtim.tv_nsec == right.st_mtim.tv_nsec &&
           left.st_ctim.tv_sec == right.st_ctim.tv_sec &&
           left.st_ctim.tv_nsec == right.st_ctim.tv_nsec;
}

bool read_exact_marker(int control_fd) {
    Fd marker(::openat(control_fd, "cancel.request",
                        O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK));
    if (!marker) {
        if (errno == ENOENT) return false;
        protocol_error();
    }

    struct stat before{};
    if (::fstat(marker.get(), &before) != 0 || !S_ISREG(before.st_mode) ||
        before.st_uid != ::geteuid() || (before.st_mode & 07777) != 0600 ||
        before.st_nlink != 1 || before.st_size != static_cast<off_t>(kMarker.size()))
        protocol_error();

    std::array<char, kMarker.size()> bytes{};
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const ssize_t count = ::read(marker.get(), bytes.data() + offset,
                                     bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) protocol_error();
        offset += static_cast<std::size_t>(count);
    }
    char extra = 0;
    while (true) {
        const ssize_t count = ::read(marker.get(), &extra, 1);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) protocol_error();
        if (count != 0) protocol_error();
        break;
    }
    struct stat after{};
    if (::fstat(marker.get(), &after) != 0 || !same_identity(before, after))
        protocol_error();
    if (std::string_view(bytes.data(), bytes.size()) != kMarker) protocol_error();
    return true;
}

AbortProbeResult probe_control_fd(int control_fd) {
    const bool exists = read_exact_marker(control_fd);
    return exists ? AbortProbeResult::REQUESTED : AbortProbeResult::NOT_REQUESTED;
}

void write_all(int fd, std::string_view bytes) {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const ssize_t count = ::write(fd, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) protocol_error();
        offset += static_cast<std::size_t>(count);
    }
}

void validate_created_marker(int fd) {
    struct stat status{};
    if (::fstat(fd, &status) != 0 || !S_ISREG(status.st_mode) ||
        status.st_uid != ::geteuid() || (status.st_mode & 07777) != 0600 ||
        status.st_nlink != 1 || status.st_size != static_cast<off_t>(kMarker.size()))
        protocol_error();
}
}

AbortRequestResult request_abort(const std::filesystem::path& run_root) {
    Fd control = open_control(run_root);
    static std::atomic<std::uint64_t> sequence{0};
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        const std::string temporary = ".cancel.request-" + std::to_string(::getpid()) +
            "-" + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
        Fd marker(::openat(control.get(), temporary.c_str(),
                           O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
        if (!marker) {
            if (errno == EEXIST) continue;
            protocol_error();
        }
        bool adopted = false;
        try {
            write_all(marker.get(), kMarker);
            validate_created_marker(marker.get());
            if (::fsync(marker.get()) != 0) protocol_error();
            if (::syscall(SYS_renameat2, control.get(), temporary.c_str(), control.get(),
                          "cancel.request", RENAME_NOREPLACE) != 0) {
                const int error = errno;
                struct stat owned{};
                struct stat named{};
                if (::fstat(marker.get(), &owned) == 0 &&
                    ::fstatat(control.get(), temporary.c_str(), &named, AT_SYMLINK_NOFOLLOW) == 0 &&
                    owned.st_dev == named.st_dev && owned.st_ino == named.st_ino)
                    (void)::unlinkat(control.get(), temporary.c_str(), 0);
                if (error == EEXIST && probe_control_fd(control.get()) == AbortProbeResult::REQUESTED) {
                    if (::fsync(control.get()) != 0) protocol_error();
                    return AbortRequestResult::ALREADY_REQUESTED;
                }
                protocol_error();
            }
            adopted = true;
            if (::fsync(control.get()) != 0) protocol_error();
            return AbortRequestResult::REQUESTED;
        } catch (...) {
            if (!adopted) {
                struct stat owned{};
                struct stat named{};
                if (::fstat(marker.get(), &owned) == 0 &&
                    ::fstatat(control.get(), temporary.c_str(), &named, AT_SYMLINK_NOFOLLOW) == 0 &&
                    owned.st_dev == named.st_dev && owned.st_ino == named.st_ino)
                    (void)::unlinkat(control.get(), temporary.c_str(), 0);
            }
            throw;
        }
    }
    protocol_error();
}

AbortProbeResult probe_abort(const std::filesystem::path& run_root) {
    try {
        Fd control = open_control(run_root);
        return probe_control_fd(control.get());
    } catch (const bruce::Error&) {
        return AbortProbeResult::INVALID;
    }
}

} // namespace bruce::worker_v1
