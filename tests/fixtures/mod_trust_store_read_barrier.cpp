// Test-only LD_PRELOAD barrier: pause the first read of the selected trust
// store so the integration test can change its metadata deterministically.
#include <cstdlib>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
bool paused = false;

bool target(const int fd) {
    const auto* inode = ::getenv("BRUCE_TEST_TRUST_INODE");
    const auto* device = ::getenv("BRUCE_TEST_TRUST_DEVICE");
    struct stat info {};
    return inode && device && ::fstat(fd, &info) == 0 &&
        info.st_ino == static_cast<ino_t>(::strtoull(inode, nullptr, 10)) &&
        info.st_dev == static_cast<dev_t>(::strtoull(device, nullptr, 10));
}
}

extern "C" ssize_t read(const int fd, void* buffer, const size_t size) {
    static const auto real_read = reinterpret_cast<ssize_t (*)(int, void*, size_t)>(
        ::dlsym(RTLD_NEXT, "read"));
    const auto count = real_read(fd, buffer, size);
    if (!paused && count > 0 && target(fd)) {
        paused = true;
        const char ready = 'R';
        if (::write(::atoi(::getenv("BRUCE_TEST_READY_FD")), &ready, 1) != 1) ::_exit(90);
        char resume = 0;
        if (real_read(::atoi(::getenv("BRUCE_TEST_RESUME_FD")), &resume, 1) != 1) ::_exit(90);
    }
    return count;
}
