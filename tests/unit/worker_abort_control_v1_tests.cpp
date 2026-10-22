#include "bruce/core/types.hpp"
#include "bruce/worker/worker_abort_control_v1.hpp"

#include <filesystem>
#include <fstream>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace bruce;
using namespace bruce::worker_v1;
namespace fs = std::filesystem;

namespace {
bool mutate_during_probe = false;
dev_t mutation_device = 0;
ino_t mutation_inode = 0;
bool probe_during_marker_write = false;
std::filesystem::path marker_write_run_root;
AbortProbeResult marker_write_probe = AbortProbeResult::INVALID;
bool hold_control_fsync = false;
dev_t control_fsync_device = 0;
ino_t control_fsync_inode = 0;
std::atomic<unsigned> control_fsync_count = 0;
std::mutex control_fsync_mutex;
std::condition_variable control_fsync_cv;
bool first_control_fsync_entered = false;
bool release_first_control_fsync = false;
}

extern "C" ssize_t __real_read(int fd, void* buffer, size_t size);
extern "C" ssize_t __real_write(int fd, const void* buffer, size_t size);
extern "C" int __real_fsync(int fd);
extern "C" ssize_t __wrap_read(int fd, void* buffer, size_t size) {
    const ssize_t result = __real_read(fd, buffer, size);
    if (!mutate_during_probe || result <= 0) return result;
    struct stat before{};
    if (::fstat(fd, &before) != 0 || before.st_dev != mutation_device ||
        before.st_ino != mutation_inode) return result;
    mutate_during_probe = false;
    const timespec mtime[2] = {before.st_atim, before.st_mtim};
    if (::pwrite(fd, kMarker.data(), kMarker.size(), 0) !=
            static_cast<ssize_t>(kMarker.size()) ||
        ::futimens(fd, mtime) != 0)
        throw Error("test marker mutation failed");
    return result;
}

extern "C" ssize_t __wrap_write(int fd, const void* buffer, size_t size) {
    if (probe_during_marker_write) {
        probe_during_marker_write = false;
        marker_write_probe = probe_abort(marker_write_run_root);
    }
    return __real_write(fd, buffer, size);
}

extern "C" int __wrap_fsync(int fd) {
    struct stat status{};
    if (hold_control_fsync && ::fstat(fd, &status) == 0 &&
        status.st_dev == control_fsync_device && status.st_ino == control_fsync_inode) {
        const unsigned count = control_fsync_count.fetch_add(1) + 1;
        std::unique_lock lock(control_fsync_mutex);
        control_fsync_cv.notify_all();
        if (count == 1) {
            first_control_fsync_entered = true;
            control_fsync_cv.notify_all();
            control_fsync_cv.wait(lock, [] { return release_first_control_fsync; });
        }
    }
    return __real_fsync(fd);
}

namespace {
void require(bool value, const char* message) {
    if (!value) throw Error(message);
}

template<class F>
void rejects_protocol(F&& action, const char* message) {
    try {
        action();
    } catch (const Error& error) {
        require(std::string_view(error.what()) == "PROTOCOL_ERROR", message);
        return;
    }
    throw Error(message);
}

void make_private_tree(const fs::path& run_root) {
    fs::create_directories(run_root / "control");
    require(::chmod(run_root.c_str(), 0700) == 0, "run root chmod failed");
    require(::chmod((run_root / "control").c_str(), 0700) == 0,
            "control chmod failed");
}

struct stat_info {
    struct stat value{};
};

stat_info lstat_path(const fs::path& path) {
    stat_info result;
    require(::lstat(path.c_str(), &result.value) == 0, "lstat failed");
    return result;
}

void require_marker_private(const fs::path& marker) {
    const auto status = lstat_path(marker).value;
    require(S_ISREG(status.st_mode), "marker is not regular");
    require(status.st_uid == ::geteuid(), "marker owner mismatch");
    require((status.st_mode & 0777) == 0600, "marker is not owner-only 0600");
    require(status.st_nlink == 1, "marker has unexpected hardlinks");
    require(status.st_size == static_cast<off_t>(kMarker.size()),
            "marker size mismatch");
}

std::string read_marker(const fs::path& marker) {
    std::ifstream input(marker, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
}

int main() {
    const fs::path base = fs::temp_directory_path() / "bruce-worker-abort-control-v1-tests";
    fs::remove_all(base);
    fs::create_directories(base);
    try {
        const fs::path run_root = base / "run";
        make_private_tree(run_root);
        require(probe_abort(run_root) == AbortProbeResult::NOT_REQUESTED,
                "valid tree without marker is not NOT_REQUESTED");
        require(request_abort(run_root) == AbortRequestResult::REQUESTED,
                "first request did not create marker");
        const fs::path marker = run_root / "control" / "cancel.request";
        require_marker_private(marker);
        require(read_marker(marker) == kMarker, "marker bytes are not canonical");
        require(probe_abort(run_root) == AbortProbeResult::REQUESTED,
                "canonical marker is not REQUESTED");
        require(request_abort(run_root) == AbortRequestResult::ALREADY_REQUESTED,
                "second request was not idempotent");
        require(read_marker(marker) == kMarker, "second request changed marker bytes");

        const fs::path atomic_publish = base / "atomic-publish";
        make_private_tree(atomic_publish);
        marker_write_run_root = atomic_publish;
        marker_write_probe = AbortProbeResult::INVALID;
        probe_during_marker_write = true;
        require(request_abort(atomic_publish) == AbortRequestResult::REQUESTED,
                "atomic-publish request failed");
        require(!probe_during_marker_write,
                "atomic-publish write probe did not run");
        require(marker_write_probe == AbortProbeResult::NOT_REQUESTED,
                "partially written marker became visible to probe");
        require_marker_private(atomic_publish / "control" / "cancel.request");
        require(probe_abort(atomic_publish) == AbortProbeResult::REQUESTED,
                "atomically published marker is not REQUESTED");

        const fs::path concurrent = base / "concurrent-publish";
        make_private_tree(concurrent);
        const auto control_status = lstat_path(concurrent / "control").value;
        control_fsync_device = control_status.st_dev;
        control_fsync_inode = control_status.st_ino;
        control_fsync_count = 0;
        {
            std::lock_guard lock(control_fsync_mutex);
            first_control_fsync_entered = false;
            release_first_control_fsync = false;
            hold_control_fsync = true;
        }
        AbortRequestResult first_result = AbortRequestResult::ALREADY_REQUESTED;
        AbortRequestResult second_result = AbortRequestResult::REQUESTED;
        std::thread first([&] { first_result = request_abort(concurrent); });
        bool first_reached_fsync = false;
        {
            std::unique_lock lock(control_fsync_mutex);
            first_reached_fsync = control_fsync_cv.wait_for(lock, std::chrono::seconds(2), [] {
                return first_control_fsync_entered;
            });
        }
        std::thread second([&] { second_result = request_abort(concurrent); });
        bool second_synced_control = false;
        {
            std::unique_lock lock(control_fsync_mutex);
            second_synced_control = control_fsync_cv.wait_for(lock, std::chrono::seconds(2), [] {
                return control_fsync_count.load() >= 2;
            });
            release_first_control_fsync = true;
            control_fsync_cv.notify_all();
        }
        first.join();
        second.join();
        hold_control_fsync = false;
        require(first_reached_fsync, "first publisher did not reach control fsync");
        require(second_synced_control, "concurrent requester returned before control fsync");
        require(first_result == AbortRequestResult::REQUESTED &&
                second_result == AbortRequestResult::ALREADY_REQUESTED,
                "concurrent request results are not idempotent");

        const fs::path missing = base / "missing";
        require(probe_abort(missing) == AbortProbeResult::INVALID,
                "missing run root was reported as NOT_REQUESTED");
        rejects_protocol([&] { (void)request_abort(missing); },
                         "missing run root was accepted");

        const fs::path symlink_target = base / "symlink-target";
        make_private_tree(symlink_target);
        const fs::path root_symlink = base / "root-symlink";
        fs::create_symlink(symlink_target, root_symlink);
        require(probe_abort(root_symlink) == AbortProbeResult::INVALID,
                "root symlink was accepted");
        rejects_protocol([&] { (void)request_abort(root_symlink); },
                         "root symlink request was accepted");

        const fs::path control_target = base / "control-target";
        fs::create_directories(control_target);
        require(::chmod(control_target.c_str(), 0700) == 0, "control target chmod failed");
        const fs::path control_symlink_root = base / "control-symlink-run";
        fs::create_directory(control_symlink_root);
        require(::chmod(control_symlink_root.c_str(), 0700) == 0,
                "control symlink root chmod failed");
        fs::create_symlink(control_target, control_symlink_root / "control");
        require(probe_abort(control_symlink_root) == AbortProbeResult::INVALID,
                "control symlink was accepted");
        rejects_protocol([&] { (void)request_abort(control_symlink_root); },
                         "control symlink request was accepted");

        const fs::path marker_symlink_root = base / "marker-symlink-run";
        make_private_tree(marker_symlink_root);
        const fs::path marker_target = base / "marker-target";
        std::ofstream(marker_target, std::ios::binary) << kMarker;
        require(::chmod(marker_target.c_str(), 0600) == 0, "marker target chmod failed");
        fs::create_symlink(marker_target, marker_symlink_root / "control" / "cancel.request");
        require(probe_abort(marker_symlink_root) == AbortProbeResult::INVALID,
                "marker symlink was accepted");
        rejects_protocol([&] { (void)request_abort(marker_symlink_root); },
                         "marker symlink request was accepted");

        const fs::path wrong_root = base / "wrong-root";
        make_private_tree(wrong_root);
        require(::chmod(wrong_root.c_str(), 0750) == 0, "wrong root chmod failed");
        require(probe_abort(wrong_root) == AbortProbeResult::INVALID,
                "insecure run root was accepted");
        rejects_protocol([&] { (void)request_abort(wrong_root); },
                         "insecure run root request was accepted");

        const fs::path special_root = base / "special-root";
        make_private_tree(special_root);
        require(::chmod(special_root.c_str(), 01700) == 0, "special root chmod failed");
        require(probe_abort(special_root) == AbortProbeResult::INVALID,
                "special run-root bits were accepted");
        rejects_protocol([&] { (void)request_abort(special_root); },
                         "special run-root request was accepted");

        const fs::path wrong_control = base / "wrong-control";
        make_private_tree(wrong_control);
        require(::chmod((wrong_control / "control").c_str(), 0750) == 0,
                "wrong control chmod failed");
        require(probe_abort(wrong_control) == AbortProbeResult::INVALID,
                "insecure control directory was accepted");
        rejects_protocol([&] { (void)request_abort(wrong_control); },
                         "insecure control request was accepted");

        const fs::path special_control = base / "special-control";
        make_private_tree(special_control);
        require(::chmod((special_control / "control").c_str(), 01700) == 0,
                "special control chmod failed");
        require(probe_abort(special_control) == AbortProbeResult::INVALID,
                "special control bits were accepted");
        rejects_protocol([&] { (void)request_abort(special_control); },
                         "special control request was accepted");

        const fs::path wrong_marker = base / "wrong-marker";
        make_private_tree(wrong_marker);
        require(request_abort(wrong_marker) == AbortRequestResult::REQUESTED,
                "wrong-marker setup failed");
        require(::chmod((wrong_marker / "control" / "cancel.request").c_str(), 0640) == 0,
                "wrong marker chmod failed");
        require(probe_abort(wrong_marker) == AbortProbeResult::INVALID,
                "wrong marker mode was accepted");
        rejects_protocol([&] { (void)request_abort(wrong_marker); },
                         "wrong marker mode request was accepted");

        const fs::path special_marker = base / "special-marker";
        make_private_tree(special_marker);
        require(request_abort(special_marker) == AbortRequestResult::REQUESTED,
                "special-marker setup failed");
        require(::chmod((special_marker / "control" / "cancel.request").c_str(), 01600) == 0,
                "special marker chmod failed");
        require(probe_abort(special_marker) == AbortProbeResult::INVALID,
                "special marker bits were accepted");
        rejects_protocol([&] { (void)request_abort(special_marker); },
                         "special marker request was accepted");

        const fs::path hardlink_marker = base / "hardlink-marker";
        make_private_tree(hardlink_marker);
        require(request_abort(hardlink_marker) == AbortRequestResult::REQUESTED,
                "hardlink-marker setup failed");
        fs::create_hard_link(hardlink_marker / "control" / "cancel.request",
                             hardlink_marker / "marker-alias");
        require(probe_abort(hardlink_marker) == AbortProbeResult::INVALID,
                "hardlinked marker was accepted");
        rejects_protocol([&] { (void)request_abort(hardlink_marker); },
                         "hardlinked marker request was accepted");

        const fs::path altered_marker = base / "altered-marker";
        make_private_tree(altered_marker);
        require(request_abort(altered_marker) == AbortRequestResult::REQUESTED,
                "altered-marker setup failed");
        {
            std::ofstream output(altered_marker / "control" / "cancel.request",
                                 std::ios::binary | std::ios::trunc);
            output << "BRUCE_WORKER_CANCEL_V0\n";
        }
        require(probe_abort(altered_marker) == AbortProbeResult::INVALID,
                "altered marker bytes were accepted");
        rejects_protocol([&] { (void)request_abort(altered_marker); },
                         "altered marker request was accepted");

        const fs::path extra_marker = base / "extra-marker";
        make_private_tree(extra_marker);
        require(request_abort(extra_marker) == AbortRequestResult::REQUESTED,
                "extra-marker setup failed");
        {
            std::ofstream output(extra_marker / "control" / "cancel.request",
                                 std::ios::binary | std::ios::trunc);
            output << kMarker << "extra";
        }
        require(probe_abort(extra_marker) == AbortProbeResult::INVALID,
                "extra marker bytes were accepted");
        rejects_protocol([&] { (void)request_abort(extra_marker); },
                         "extra marker request was accepted");

        const fs::path ctime_marker = base / "ctime-marker";
        make_private_tree(ctime_marker);
        require(request_abort(ctime_marker) == AbortRequestResult::REQUESTED,
                "ctime-marker setup failed");
        const auto ctime_path = ctime_marker / "control" / "cancel.request";
        const auto ctime_stat = lstat_path(ctime_path).value;
        mutation_device = ctime_stat.st_dev;
        mutation_inode = ctime_stat.st_ino;
        mutate_during_probe = true;
        require(probe_abort(ctime_marker) == AbortProbeResult::INVALID,
                "same-mtime marker mutation was accepted");
        require(!mutate_during_probe, "ctime mutation hook did not run");
    } catch (...) {
        fs::remove_all(base);
        throw;
    }
    fs::remove_all(base);
}
