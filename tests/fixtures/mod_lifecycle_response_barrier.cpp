// Test-only LD_PRELOAD barrier: release the mutation lock after a real journal
// commit, then let the test peer acquire it before the CLI constructs its reply.
#include <cstdlib>
#include <cerrno>
#include <dlfcn.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
bool committed_lock_closed = false;
bool response_lock_seen = false;
bool target_lock(const int fd) {
    const auto* inode = ::getenv("BRUCE_TEST_LOCK_INODE");
    const auto* device = ::getenv("BRUCE_TEST_LOCK_DEVICE");
    struct stat info{};
    return inode && device && ::fstat(fd, &info) == 0 &&
        info.st_ino == ::strtoull(inode, nullptr, 10) &&
        info.st_dev == ::strtoull(device, nullptr, 10);
}
void notify(const char signal) {
    if (::write(::atoi(::getenv("BRUCE_TEST_READY_FD")), &signal, 1) != 1) ::_exit(90);
}
}

extern "C" int close(const int fd) {
    static const auto real_close = reinterpret_cast<int (*)(int)>(::dlsym(RTLD_NEXT, "close"));
    const auto* journal = ::getenv("BRUCE_TEST_JOURNAL");
    const auto* old_size = ::getenv("BRUCE_TEST_JOURNAL_SIZE");
    const auto* ready = ::getenv("BRUCE_TEST_READY_FD");
    const auto* resume = ::getenv("BRUCE_TEST_RESUME_FD");
    struct stat journal_info{};
    if (!committed_lock_closed && journal && old_size && ready && resume && target_lock(fd) &&
        ::stat(journal, &journal_info) == 0 &&
        journal_info.st_size > ::strtoll(old_size, nullptr, 10)) {
        committed_lock_closed = true;
        const auto result = real_close(fd);
        char acknowledgement = 0;
        notify('R');
        if (::read(::atoi(resume), &acknowledgement, 1) != 1) ::_exit(90);
        return result;
    }
    return real_close(fd);
}

extern "C" int flock(const int fd, const int operation) {
    static const auto real_flock = reinterpret_cast<int (*)(int, int)>(::dlsym(RTLD_NEXT, "flock"));
    if (committed_lock_closed && !response_lock_seen && target_lock(fd)) {
        response_lock_seen = true;
        if (operation & LOCK_NB) {
            // Observe the actual nonblocking result while the peer still holds
            // its lock, so scheduler timing cannot make the regression pass.
            const auto result = real_flock(fd, operation);
            const auto error = errno;
            notify('N');
            errno = error;
            return result;
        }
        notify('B');
    }
    return real_flock(fd, operation);
}
