#include "bruce/artifact/artifact_store_v2.hpp"
#include "bruce/core/json.hpp"
#include "bruce/evidence/bundle.hpp"

#include <filesystem>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace bruce;
using namespace bruce::artifact_v1;
using namespace bruce::artifact_v2;
namespace fs = std::filesystem;

namespace {
enum class SyncFault { none, directory, publication_file };
SyncFault sync_fault = SyncFault::none;
unsigned regular_syncs = 0;
unsigned injected_sync_failures = 0;
}

extern "C" int __real_fsync(int fd);
extern "C" int __wrap_fsync(int fd) {
    struct stat status{};
    if (sync_fault != SyncFault::none && ::fstat(fd, &status) == 0) {
        const bool fail_directory = sync_fault == SyncFault::directory && S_ISDIR(status.st_mode);
        // The first regular fsync belongs to V2 private staging. The second
        // belongs to V1's publication temp file, before its renameat2 commit.
        const bool fail_file = sync_fault == SyncFault::publication_file &&
                               S_ISREG(status.st_mode) && regular_syncs++ != 0;
        if (fail_directory || fail_file) {
            ++injected_sync_failures;
            errno = EIO;
            return -1;
        }
    }
    return __real_fsync(fd);
}

namespace {
class SyncFaultScope {
public:
    explicit SyncFaultScope(SyncFault fault) {
        regular_syncs = 0;
        injected_sync_failures = 0;
        sync_fault = fault;
    }
    ~SyncFaultScope() { sync_fault = SyncFault::none; }
    SyncFaultScope(const SyncFaultScope&) = delete;
    SyncFaultScope& operator=(const SyncFaultScope&) = delete;
};

void require(bool value, const char* message) {
    if (!value) throw Error(message);
}

template<class F> void rejects(F&& action, const char* message) {
    try { action(); }
    catch (const Error& error) {
        require(std::string_view(error.what()) == "PROTOCOL_ERROR", "V2 leaked an internal error");
        return;
    }
    throw Error(message);
}

// Isolate potentially blocking filesystem calls and always reap the child.
// A regression fails within two seconds and cannot strand a blocked process.
template<class F> void rejects_promptly(F&& action, std::string_view category, const char* message) {
    const auto child = ::fork();
    require(child >= 0, "FIFO regression fork failed");
    if (child == 0) {
        try { action(); }
        catch (const Error& error) {
            ::_exit(std::string_view(error.what()) == category ? 0 : 12);
        } catch (...) { ::_exit(13); }
        ::_exit(14);
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    int status{};
    for (;;) {
        const auto waited = ::waitpid(child, &status, WNOHANG);
        if (waited == child) {
            require(WIFEXITED(status) && WEXITSTATUS(status) == 0, message);
            return;
        }
        if (waited < 0 && errno == EINTR) continue;
        if (waited < 0 || std::chrono::steady_clock::now() >= deadline) {
            ::kill(child, SIGKILL);
            while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {}
            throw Error(message);
        }
        ::usleep(1000);
    }
}

void mode(const fs::path& path, mode_t value) {
    require(::chmod(path.c_str(), value) == 0, "chmod fixture failed");
}

void write_file(const fs::path& path, std::string_view bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(stream), "write fixture failed");
    stream.close();
    mode(path, 0600);
}

fs::path object_path(const fs::path& root, const ArtifactIdV1& id) {
    return root / "objects" / id.sha256.substr(0, 2) / (id.sha256 + ".blob");
}

std::string configuration(const fs::path& root) {
    return "{\"contract\":\"bruce-artifact-store-providers-v2\",\"providers\":[{\"id\":\"mirror-a\",\"root\":" +
           json_string(root.string()) + "}]}\n";
}

// Content hashes make silent replacement of an existing object observable.
std::map<std::string, std::string> snapshot(const fs::path& root) {
    std::map<std::string, std::string> result;
    if (!fs::exists(root)) return result;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        const auto name = entry.path().lexically_relative(root).string();
        if (entry.is_regular_file()) {
            std::ifstream input(entry.path(), std::ios::binary);
            const std::string bytes((std::istreambuf_iterator<char>(input)), {});
            result.emplace(name, sha256(bytes));
        } else result.emplace(name, "directory");
    }
    return result;
}

struct Fixture {
    fs::path base;
    fs::path mirror;
    fs::path providers;
    fs::path destination;
    Fixture() {
        std::string pattern = (fs::temp_directory_path() / "bruce-v2-test-XXXXXX").string();
        const char* created = ::mkdtemp(pattern.data());
        require(created != nullptr, "mkdtemp fixture failed");
        base = created;
        mirror = base / "mirror";
        providers = base / "providers.json";
        destination = base / "local";
        fs::create_directory(mirror);
        mode(mirror, 0700);
        write_file(providers, configuration(mirror));
    }
    ~Fixture() { std::error_code ignored; fs::remove_all(base, ignored); }
    ArtifactIdV1 remote(std::string_view bytes) {
        auto store = ArtifactStoreV1::open(mirror);
        return store.put_bytes(bytes);
    }
    ProviderConfigV2 config() const { return read_provider_config_v2(providers); }
};

void canonical_configuration() {
    Fixture f;
    const auto parsed = f.config();
    require(parsed.providers.size() == 1 && parsed.providers[0].id == "mirror-a" &&
            parsed.providers[0].root == f.mirror, "canonical provider configuration mismatch");
    // Parsing cannot depend on mirror availability or create the local store.
    fs::rename(f.mirror, f.base / "offline");
    (void)f.config();
    require(!fs::exists(f.destination), "configuration opened a store");
    const std::string root = json_string(f.mirror.string());
    const std::string prefix = "{\"contract\":\"bruce-artifact-store-providers-v2\",\"providers\":[";
    const std::string a = "{\"id\":\"a\",\"root\":" + root + "}";
    const std::string b = "{\"id\":\"b\",\"root\":" + root + "}";
    write_file(f.providers, prefix + a + "," + b + "]}\n");
    require(f.config().providers.size() == 2, "sorted providers rejected");
    write_file(f.providers, prefix + "]}\n");
    require(f.config().providers.empty(), "empty allowlist rejected");
    for (const auto& invalid : {
             prefix + a + "," + a + "]}\n", // duplicate ID
             prefix + b + "," + a + "]}\n", // unordered IDs
             prefix + "{\"root\":" + root + ",\"id\":\"a\"}]}\n",
             prefix + "{\"id\":\"a\",\"id\":\"b\",\"root\":" + root + "}]}\n",
             prefix + "{\"id\":\"a\",\"root\":" + root + ",\"url\":\"x\"}]}\n",
             prefix + "{\"id\":\"a\",\"root\":\"relative\"}]}\n",
             prefix + "{\"id\":\"a\",\"root\":\"/safe/../escape\"}]}\n",
             prefix + "{\"id\":\"a\",\"root\":\"/safe/./root\"}]}\n",
             prefix + "{\"id\":\"a\",\"root\":\"/safe//root\"}]}\n",
             prefix + "{\"id\":\"a\",\"root\":\"/safe/\"}]}\n",
             prefix + "{\"id\":\"a\",\"root\":\"/safe\\u0000tail\"}]}\n",
             prefix + "{\"id\":\"a\",\"root\":\"/safe/\\u00ff\"}]}\n",
             prefix + "{\"id\":\"../a\",\"root\":" + root + "}]}\n",
             prefix + "{\"id\":\"a/b\",\"root\":" + root + "}]}\n",
             prefix + "{\"id\":\"\",\"root\":" + root + "}]}\n",
             prefix + "{\"id\":" + json_string(std::string(129, 'a')) + ",\"root\":" + root + "}]}\n",
             prefix + "{\"id\":\"\\u0061\",\"root\":" + root + "}]}\n",
             prefix + "{\"id\":\"a\",\"root\":\"https://host/object\"}]}\n",
             std::string("{\"providers\":[],\"contract\":\"bruce-artifact-store-providers-v2\"}\n"),
             std::string("{\"contract\":\"other\",\"providers\":[]}\n"),
             std::string("{\"contract\":\"bruce-artifact-store-providers-v2\",\"providers\":[],\"extra\":0}\n"),
             " " + configuration(f.mirror), configuration(f.mirror) + "\n",
             configuration(f.mirror).substr(0, configuration(f.mirror).size() - 1)}) {
        write_file(f.providers, invalid);
        rejects([&] { (void)f.config(); }, "noncanonical provider configuration accepted");
    }
    // Canonically escaped quotes/backslashes and UTF-8 are legitimate root bytes.
    const auto unusual = f.base / "root-\"-\\-ä";
    write_file(f.providers, configuration(unusual));
    require(f.config().providers[0].root == unusual, "canonical JSON string escaping rejected");
}

void unsafe_configuration_files() {
    Fixture f;
    mode(f.providers, 0644);
    rejects([&] { (void)f.config(); }, "world-readable config accepted");
    mode(f.providers, 0660);
    rejects([&] { (void)f.config(); }, "group-writable config accepted");
    mode(f.providers, 0600);
    fs::create_hard_link(f.providers, f.base / "config-link");
    rejects([&] { (void)f.config(); }, "hardlinked config accepted");
    fs::remove(f.base / "config-link");
    fs::rename(f.providers, f.base / "original");
    fs::create_symlink(f.base / "original", f.providers);
    rejects([&] { (void)f.config(); }, "symlink config accepted");
    fs::remove(f.providers);
    require(::mkfifo(f.providers.c_str(), 0600) == 0, "config FIFO fixture failed");
    rejects([&] { (void)f.config(); }, "FIFO config accepted");
    fs::remove(f.providers);
    fs::create_directory(f.providers);
    rejects([&] { (void)f.config(); }, "directory config accepted");
    fs::remove(f.providers);
    write_file(f.providers, "");
    fs::resize_file(f.providers, max_file_bytes + 1);
    rejects([&] { (void)f.config(); }, "oversized config accepted");
    fs::create_directory_symlink(f.base, f.base / "alias");
    rejects([&] { (void)read_provider_config_v2(f.base / "alias" / "original"); },
            "symlink config ancestor accepted");
    rejects([&] { (void)read_provider_config_v2(f.base / ".." / f.base.filename() / "original"); },
            "config parent traversal accepted");
    rejects([&] { (void)read_provider_config_v2(f.base / "missing"); }, "missing config accepted");
}

void successful_fetches() {
    Fixture f;
    const auto config = f.config();
    for (const auto& bytes : {std::string{}, std::string("small\0payload", 13),
                              std::string(max_file_bytes, 'm')}) {
        const auto id = f.remote(bytes);
        require(fetch_to_store_v2(config, "mirror-a", id, f.destination) == id,
                "valid fetch identity mismatch");
        auto local = ArtifactStoreV1::open_existing(f.destination);
        local.verify(id);
        const auto before = snapshot(f.destination);
        require(fetch_to_store_v2(config, "mirror-a", id, f.destination) == id,
                "repeated fetch identity mismatch");
        require(snapshot(f.destination) == before, "idempotent fetch changed store");
        struct stat status{};
        require(::lstat(object_path(f.destination, id).c_str(), &status) == 0 &&
                status.st_nlink == 1 && (status.st_mode & 0077) == 0,
                "fetched object does not satisfy V1 ownership contract");
    }
}

void failure_preserves_store() {
    Fixture f;
    const auto config = f.config();
    auto local = ArtifactStoreV1::open(f.destination);
    const auto retained = local.put_bytes("retained-local-object");
    const auto before = snapshot(f.destination);
    const auto real = f.remote("untrusted-mirror-content");
    const ArtifactIdV1 wrong_digest{sha256("other-expected-content"), real.size_bytes};
    fs::create_directories(object_path(f.mirror, wrong_digest).parent_path());
    mode(object_path(f.mirror, wrong_digest).parent_path(), 0700);
    fs::rename(object_path(f.mirror, real), object_path(f.mirror, wrong_digest));
    rejects([&] { (void)fetch_to_store_v2(config, "mirror-a", wrong_digest, f.destination); },
            "wrong digest accepted");
    require(snapshot(f.destination) == before, "digest failure changed local tree");
    fs::rename(object_path(f.mirror, wrong_digest), object_path(f.mirror, real));
    for (const auto size : {real.size_bytes - 1, real.size_bytes + 1}) {
        rejects([&] { (void)fetch_to_store_v2(config, "mirror-a", {real.sha256, size}, f.destination); },
                "wrong size accepted");
        require(snapshot(f.destination) == before, "size failure changed local tree");
    }
    const auto absent = f.base / "absent-store";
    rejects([&] { (void)fetch_to_store_v2(config, "mirror-a", wrong_digest, absent); },
            "missing mirror object accepted");
    require(!fs::exists(absent), "failure created destination store");
    local.verify(retained);
    const auto conflict = object_path(f.destination, real);
    fs::create_directories(conflict.parent_path());
    mode(conflict.parent_path(), 0700);
    write_file(conflict, "existing-corrupt-object");
    const auto conflicting_tree = snapshot(f.destination);
    rejects([&] { (void)fetch_to_store_v2(config, "mirror-a", real, f.destination); },
            "local conflict accepted");
    require(snapshot(f.destination) == conflicting_tree, "local conflict replaced existing object");
}

void publication_sync_failures_have_unambiguous_results() {
    Fixture f;
    const auto config = f.config();
    const auto id = f.remote("post-rename-directory-sync-failure");
    auto local = ArtifactStoreV1::open(f.destination);
    const auto retained = local.put_bytes("unrelated-existing-object");
    const auto retained_path = object_path(f.destination, retained);
    struct stat retained_before{};
    require(::lstat(retained_path.c_str(), &retained_before) == 0, "retained object stat failed");
    {
        SyncFaultScope fault(SyncFault::directory);
        require(fetch_to_store_v2(config, "mirror-a", id, f.destination) == id,
                "verified post-rename object did not resolve to success");
        require(injected_sync_failures == 1, "post-rename directory fsync fault did not fire");
    }
    local.verify(id);
    local.verify(retained);
    struct stat retained_after{};
    require(::lstat(retained_path.c_str(), &retained_after) == 0 &&
            retained_after.st_ino == retained_before.st_ino,
            "post-rename recovery replaced unrelated object");
    const auto committed = snapshot(f.destination);
    struct stat committed_before{};
    require(::lstat(object_path(f.destination, id).c_str(), &committed_before) == 0,
            "committed object stat failed");
    {
        SyncFaultScope fault(SyncFault::directory);
        require(fetch_to_store_v2(config, "mirror-a", id, f.destination) == id,
                "existing correct object rejected during directory fault");
    }
    require(snapshot(f.destination) == committed, "existing correct object changed");
    struct stat committed_after{};
    require(::lstat(object_path(f.destination, id).c_str(), &committed_after) == 0 &&
            committed_before.st_ino == committed_after.st_ino,
            "repeated fetch replaced a preexisting inode");
    {
        SyncFaultScope fault(SyncFault::publication_file);
        require(fetch_to_store_v2(config, "mirror-a", id, f.destination) == id,
                "already verified local object lost during pre-rename sync failure");
        require(injected_sync_failures == 1, "pre-rename sync fault did not fire");
    }
    require(snapshot(f.destination) == committed, "preexisting-object recovery changed store");

    const auto absent = f.remote("new-object-before-rename-sync-failure");
    {
        SyncFaultScope fault(SyncFault::publication_file);
        rejects([&] { (void)fetch_to_store_v2(config, "mirror-a", absent, f.destination); },
                "pre-rename sync failure falsely reported success");
        require(injected_sync_failures == 1, "new-object pre-rename sync fault did not fire");
    }
    require(snapshot(f.destination) == committed, "pre-rename failure changed destination tree");
    require(!fs::exists(object_path(f.destination, absent)), "pre-rename failure published an object");

    const auto corrupt = f.remote("preexisting-corrupt-target");
    const auto target = object_path(f.destination, corrupt);
    fs::create_directories(target.parent_path());
    mode(target.parent_path(), 0700);
    write_file(target, "corrupt-existing-bytes");
    const auto corrupt_before = snapshot(f.destination);
    struct stat conflict_before{};
    require(::lstat(target.c_str(), &conflict_before) == 0, "conflict object stat failed");
    for (const auto injected : {SyncFault::directory, SyncFault::publication_file}) {
        SyncFaultScope fault(injected);
        rejects([&] { (void)fetch_to_store_v2(config, "mirror-a", corrupt, f.destination); },
                "sync recovery accepted a preexisting corrupt object");
        require(snapshot(f.destination) == corrupt_before, "sync recovery modified preexisting conflict");
        struct stat conflict_after{};
        require(::lstat(target.c_str(), &conflict_after) == 0 &&
                conflict_after.st_ino == conflict_before.st_ino,
                "sync recovery replaced a preexisting conflict inode");
    }
}

void fifo_destination_rejects_without_blocking() {
    Fixture f;
    const auto config = f.config();
    constexpr std::string_view bytes = "existing-destination-fifo";
    const auto id = f.remote(bytes);
    auto local = ArtifactStoreV1::open(f.destination);
    const auto retained = local.put_bytes("retained-before-fifo-conflict");
    const auto fifo = object_path(f.destination, id);
    fs::create_directories(fifo.parent_path());
    mode(fifo.parent_path(), 0700);
    require(::mkfifo(fifo.c_str(), 0600) == 0, "destination FIFO fixture failed");
    const auto before = snapshot(f.destination);
    struct stat fifo_before{};
    require(::lstat(fifo.c_str(), &fifo_before) == 0 && S_ISFIFO(fifo_before.st_mode),
            "destination FIFO stat failed");

    rejects_promptly([&] { (void)fetch_to_store_v2(config, "mirror-a", id, f.destination); },
                     "PROTOCOL_ERROR", "V2 blocked on an existing destination FIFO");
    // A pre-rename sync failure invokes V2's fresh local verification path,
    // which must also reject the FIFO before any blocking read.
    rejects_promptly([&] {
        SyncFaultScope fault(SyncFault::publication_file);
        (void)fetch_to_store_v2(config, "mirror-a", id, f.destination);
    }, "PROTOCOL_ERROR", "V2 sync recovery blocked on an existing destination FIFO");
    rejects_promptly([&] { (void)local.put_bytes(bytes); }, "ArtifactObjectConflict",
                     "V1 publication blocked on an existing destination FIFO");
    rejects_promptly([&] { local.verify(id); }, "ArtifactObjectCorrupt",
                     "V1 verification blocked on an existing object FIFO");

    struct stat fifo_after{};
    require(::lstat(fifo.c_str(), &fifo_after) == 0 && S_ISFIFO(fifo_after.st_mode) &&
            fifo_after.st_ino == fifo_before.st_ino,
            "FIFO conflict changed the preexisting destination inode");
    require(snapshot(f.destination) == before, "FIFO conflict changed the destination tree");
    local.verify(retained);
}

void unsafe_mirrors_and_objects() {
    Fixture f;
    const auto config = f.config();
    const auto id = f.remote("mirror-object");
    const auto object = object_path(f.mirror, id);
    const auto reject_fetch = [&] {
        rejects([&] { (void)fetch_to_store_v2(config, "mirror-a", id, f.destination); },
                "unsafe mirror accepted");
        require(!fs::exists(f.destination), "unsafe mirror created local store");
    };
    rejects([&] { (void)fetch_to_store_v2(config, "unknown", id, f.destination); }, "unknown provider accepted");
    for (const auto& invalid_id : {ArtifactIdV1{"../escape", 1}, ArtifactIdV1{std::string(64, 'A'), 1},
                                   ArtifactIdV1{id.sha256, max_file_bytes + 1}}) {
        rejects([&] { (void)fetch_to_store_v2(config, "mirror-a", invalid_id, f.destination); },
                "invalid expected identity accepted");
    }
    auto changed = config;
    changed.providers[0].root = f.mirror / ".." / "mirror";
    rejects([&] { (void)fetch_to_store_v2(changed, "mirror-a", id, f.destination); },
            "programmatic root traversal accepted");
    changed = config;
    changed.providers.push_back(changed.providers[0]);
    rejects([&] { (void)fetch_to_store_v2(changed, "mirror-a", id, f.destination); },
            "programmatic duplicate provider accepted");
    for (const auto& directory : {f.mirror, f.mirror / "objects", object.parent_path()}) {
        mode(directory, 0755); reject_fetch(); mode(directory, 0700);
        const auto saved = directory.string() + "-saved";
        fs::rename(directory, saved);
        fs::create_directory_symlink(saved, directory);
        reject_fetch(); fs::remove(directory); fs::rename(saved, directory);
    }
    mode(object, 0644); reject_fetch(); mode(object, 0600);
    fs::create_hard_link(object, f.base / "hardlink");
    reject_fetch(); fs::remove(f.base / "hardlink");
    fs::rename(object, f.base / "saved-object");
    fs::create_symlink(f.base / "saved-object", object);
    reject_fetch(); fs::remove(object);
    require(::mkfifo(object.c_str(), 0600) == 0, "mirror FIFO fixture failed");
    reject_fetch(); fs::remove(object);
    fs::create_directory(object); mode(object, 0700);
    reject_fetch(); fs::remove(object);
    reject_fetch(); // missing object
    write_file(object, ""); fs::resize_file(object, max_file_bytes + 1);
    reject_fetch(); fs::remove(object);
    fs::rename(f.base / "saved-object", object);
    // A symlink in an ancestor above the allowlisted root must also fail.
    fs::create_directory_symlink(f.base, f.base / "alias");
    changed = config;
    changed.providers[0].root = f.base / "alias" / "mirror";
    rejects([&] { (void)fetch_to_store_v2(changed, "mirror-a", id, f.destination); },
            "mirror ancestor symlink accepted");
    fs::rename(f.mirror, f.base / "offline"); reject_fetch();
}
} // namespace

int main() {
    ::alarm(60); // FIFO regressions must fail instead of hanging the test runner.
    try {
        canonical_configuration();
        unsafe_configuration_files();
        successful_fetches();
        failure_preserves_store();
        publication_sync_failures_have_unambiguous_results();
        fifo_destination_rejects_without_blocking();
        unsafe_mirrors_and_objects();
        std::cout << "artifact store V2 contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
