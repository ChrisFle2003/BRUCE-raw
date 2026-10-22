#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Host-only test interposition at actual log/filesystem boundaries. Worker
// adapters and production interfaces contain no fault controls.
static int triggered;
static int expired;
static int ready_seen;
static int host(void) {
    const char *pid = getenv("BRUCE_TEST_STAGE3D_HOST_PID");
    return pid && strtol(pid, NULL, 10) == getpid();
}
static const char *mode(void) {
    const char *value = getenv("BRUCE_TEST_STAGE3D_MODE");
    return value ? value : "";
}
static int fd_path(int fd, char *path, size_t size) {
    char descriptor[64];
    snprintf(descriptor, sizeof descriptor, "/proc/self/fd/%d", fd);
    ssize_t n = readlink(descriptor, path, size - 1);
    if (n < 0) return 0;
    path[n] = 0;
    return 1;
}
static void trigger(int expire) {
    if (triggered) return;
    triggered = 1;
    expired = expire;
    const char *marker = getenv("BRUCE_TEST_STAGE3D_MARKER");
    const char *release = getenv("BRUCE_TEST_STAGE3D_RELEASE");
    int (*real_open)(const char *, int, ...) = dlsym(RTLD_NEXT, "open");
    ssize_t (*real_write)(int, const void *, size_t) = dlsym(RTLD_NEXT, "write");
    int fd = marker ? real_open(marker, O_WRONLY | O_CREAT | O_EXCL, 0600) : -1;
    if (fd >= 0) { real_write(fd, "1", 1); close(fd); }
    if (release) {
        for (int n = 0; n < 10000 && access(release, F_OK) != 0; ++n) usleep(1000);
    }
}
int clock_gettime(clockid_t clock, struct timespec *value) {
    int (*real_clock_gettime)(clockid_t, struct timespec *) = dlsym(RTLD_NEXT, "clock_gettime");
    int result = real_clock_gettime(clock, value);
    if (result == 0 && clock == CLOCK_MONOTONIC && expired && host()) value->tv_sec += 3600;
    return result;
}
ssize_t write(int fd, const void *bytes, size_t size) {
    ssize_t (*real_write)(int, const void *, size_t) = dlsym(RTLD_NEXT, "write");
    ssize_t result = real_write(fd, bytes, size);
    if (!host() || triggered || result <= 0) return result;
    char path[4096];
    if (!fd_path(fd, path, sizeof path)) return result;
    const char *action = mode();
    if (strstr(path, "/control/run-log.jsonl")) {
        int ready = memmem(bytes, size, "\"event\":\"RESULT_RECEIVED\"", 25) &&
                    memmem(bytes, size, "\"status\":\"READY\"", 16);
        if (ready) ready_seen = 1;
        if ((!strcmp(action, "before_dispatch") && memmem(bytes, size, "RUN_CREATED", 11)) ||
            (!strcmp(action, "between_retries") && memmem(bytes, size, "RESULT_RECEIVED", 15) &&
             memmem(bytes, size, "\"status\":\"FAILED\"", 17)) ||
            (!strcmp(action, "ready") && ready)) trigger(0);
        if (!strcmp(action, "deadline_ready") && ready) trigger(1);
    }
    if (strstr(path, "/payload-inputs/") &&
        (!strcmp(action, "materialize") || !strcmp(action, "deadline_materialize")))
        trigger(!strcmp(action, "deadline_materialize"));
    if (strstr(path, "/payload-inputs/") && strstr(path, "a-2-2-0.bin") &&
        !strcmp(action, "materialize_retry")) trigger(0);
    if (ready_seen && strstr(path, "/store/objects/.tmp.") && !strcmp(action, "publication")) trigger(0);
    return result;
}
ssize_t read(int fd, void *bytes, size_t size) {
    ssize_t (*real_read)(int, void *, size_t) = dlsym(RTLD_NEXT, "read");
    ssize_t result = real_read(fd, bytes, size);
    char path[4096];
    if (host() && ready_seen && !triggered && result > 0 && !strcmp(mode(), "digest") &&
        fd_path(fd, path, sizeof path) && strstr(path, "/output/result.bin")) {
        ((char *)bytes)[0] ^= 1;
        trigger(0);
    }
    return result;
}
int fsync(int fd) {
    int (*real_fsync)(int) = dlsym(RTLD_NEXT, "fsync");
    int result = real_fsync(fd);
    if (!host() || !ready_seen || triggered) return result;
    char path[4096];
    if (!fd_path(fd, path, sizeof path)) return result;
    if (strstr(path, "/store/objects/.tmp.") && !strcmp(mode(), "store")) {
        trigger(0);
        errno = EIO;
        return -1;
    }
    size_t n = strlen(path);
    if (n >= 16 && !strcmp(path + n - 16, "/store/manifests")) {
        if (!strcmp(mode(), "acceptance")) trigger(0);
        if (!strcmp(mode(), "deadline_publish")) trigger(1);
    }
    return result;
}
