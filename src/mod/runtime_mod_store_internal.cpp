#include "runtime_mod_store_internal.hpp"

#include "bruce/core/types.hpp"

#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace bruce::runtime_mod_store_detail {
namespace { std::string active_failpoint; }

void set_test_failpoint(std::string name) { active_failpoint = std::move(name); }
void clear_test_failpoint() { active_failpoint.clear(); }
void test_failpoint(const std::string_view name) {
    if (active_failpoint == name) throw Error("RuntimeModTestFailpoint");
}

void check_private_node(const int fd, const bool directory) {
    struct stat info {};
    if (::fstat(fd, &info) != 0 || info.st_uid != ::geteuid() || (info.st_mode & 077) != 0 ||
        (directory ? !S_ISDIR(info.st_mode) : !S_ISREG(info.st_mode) || info.st_nlink != 1))
        throw Error("UnsafeRuntimeModFilePermissions");
}

void check_private_directory(const std::string& path, const bool create) {
    if (create && ::mkdir(path.c_str(), 0700) != 0 && errno != EEXIST)
        throw Error("RuntimeModRootCreateFailed");
    const int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) throw Error("RuntimeModFileOpenFailed");
    try { check_private_node(fd, true); }
    catch (...) { ::close(fd); throw; }
    ::close(fd);
}

ReadOnlyRoot::ReadOnlyRoot(const std::string& path) {
    if (path.find('\0') != std::string::npos) throw Error("RuntimeModFileOpenFailed");
    const auto absolute = std::filesystem::absolute(path);
    fd_ = ::open("/", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd_ < 0) throw Error("RuntimeModFileOpenFailed");
    try {
        // Do not normalize away components: alias/.. must reject alias before
        // visiting .., and alias/. must reject alias before visiting . .
        for (const auto& component : absolute.relative_path()) {
            if (component.empty()) continue;
            const int next = ::openat(fd_, component.c_str(),
                O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (next < 0) throw Error("RuntimeModFileOpenFailed");
            ::close(fd_);
            fd_ = next;
        }
        check_private_node(fd_, true);
    } catch (...) {
        ::close(fd_); fd_ = -1; throw;
    }
}

ReadOnlyRoot::~ReadOnlyRoot() { if (fd_ >= 0) ::close(fd_); }

std::string ReadOnlyRoot::anchored_path() const {
    // Only this internally generated proc descriptor link is traversed. The
    // original pathname is never resolved again after the component walk.
    return "/proc/self/fd/" + std::to_string(fd_) + "/.";
}

RootLock::RootLock(const std::string& root, const bool read_only, const bool create)
    : fd_(::open((root + "/registry.lock").c_str(),
                 (read_only ? O_RDONLY | O_NOATIME | O_NONBLOCK : (create ? O_CREAT : 0) | O_RDWR) |
                     O_NOFOLLOW | O_CLOEXEC, 0600)) {
    if (fd_ < 0) throw Error("RuntimeModFileOpenFailed");
    try {
        check_private_node(fd_, false);
        while (::flock(fd_, read_only ? LOCK_SH | LOCK_NB : LOCK_EX) != 0) {
            if (errno != EINTR) throw Error("RuntimeModStoreLockFailed");
        }
    } catch (...) {
        ::close(fd_); fd_ = -1; throw;
    }
}

RootLock::~RootLock() { if (fd_ >= 0) ::close(fd_); }

} // namespace bruce::runtime_mod_store_detail
