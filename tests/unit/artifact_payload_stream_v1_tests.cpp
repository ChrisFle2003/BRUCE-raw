#include "bruce/artifact/artifact_payload_stream_v1.hpp"
#include "bruce/artifact/artifact_payload_external_v1.hpp"

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace bruce;
using namespace bruce::artifact_v1;
namespace fs = std::filesystem;

// Linker wrappers affect this test binary only. All successful operations use
// the real filesystem; only an explicitly armed late operation returns EIO.
static int failing_source = -1;
static int short_source = -1;
static int source_reads = 0;
static int chunk_writes = 0;
static bool fail_writes = false;
static std::string mutate_path;
static int observed_source = -1;
static std::vector<std::size_t> read_requests;
static fs::path nested_injection_root;
static fs::path external_cleanup_target;
static int nested_write_count = 0;
static bool nested_injected = false;
extern "C" ssize_t __real_read(int, void*, size_t);
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" ssize_t __wrap_read(int fd, void* data, size_t size) {
    if (fd == observed_source) read_requests.push_back(size);
    if (fd == failing_source && ++source_reads == 2) { errno = EIO; return -1; }
    if (size == 1 && !mutate_path.empty()) {
        const int target = ::open(mutate_path.c_str(), O_WRONLY | O_NOFOLLOW | O_CLOEXEC);
        mutate_path.clear();
        if (target >= 0) { const char changed = 'z'; __real_write(target, &changed, 1); ::close(target); }
    }
    return __real_read(fd, data, fd == short_source && size > 113 ? 113 : size);
}
extern "C" ssize_t __wrap_write(int fd, const void* data, size_t size) {
    if (size == 65536 && !nested_injection_root.empty() && ++nested_write_count == 2) {
        const auto root = nested_injection_root;
        nested_injection_root.clear();
        for (const auto& entry : fs::directory_iterator(root)) {
            if (!entry.path().filename().string().starts_with(".stream-staging-")) continue;
            const auto nested = entry.path() / "unexpected" / "deeper";
            fs::create_directories(nested);
            const int file = ::open((nested / "file").c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
            if (file >= 0) { __real_write(file, "nested", 6); ::close(file); }
            fs::create_directory_symlink(external_cleanup_target, nested / "outside");
            nested_injected = fs::is_regular_file(nested / "file");
        }
        errno = EIO; return -1;
    }
    if (fail_writes && size == 65536 && ++chunk_writes == 2) { errno = EIO; return -1; }
    return __real_write(fd, data, size);
}

namespace {
void require(bool value, const char* message) { if (!value) throw Error(message); }
template<class F> void rejects(F action, ArtifactPayloadStreamErrorCodeV15 code) {
    try { action(); } catch (const ArtifactPayloadStreamErrorV15& e) {
        require(e.code() == code, "wrong stream error category"); return;
    }
    throw Error("invalid stream accepted");
}
struct Temporary {
    fs::path root;
    Temporary() {
        std::string pattern = "/tmp/bruce-stream-XXXXXX";
        std::vector<char> bytes(pattern.begin(), pattern.end()); bytes.push_back(0);
        require(::mkdtemp(bytes.data()) != nullptr, "mkdtemp failed"); root = bytes.data();
    }
    ~Temporary() { std::error_code ec; fs::remove_all(root, ec); }
};
void put(const fs::path& path, std::string_view bytes) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(bytes.data(), static_cast<std::streamsize>(bytes.size())); f.close();
    require(static_cast<bool>(f), "test write failed"); require(::chmod(path.c_str(), 0600) == 0, "chmod failed");
}
struct Source {
    int fd;
    Source(const fs::path& path, std::string_view bytes) {
        put(path, bytes); fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
        require(fd >= 0, "source open failed");
    }
    ~Source() { ::close(fd); }
};
auto deadline() { return std::chrono::steady_clock::now() + std::chrono::seconds(10); }
ArtifactIdV1 id(std::string_view bytes) { return {digest_external_inputs_v14(bytes), bytes.size()}; }
ArtifactPayloadStreamV15 create(const fs::path& root, std::string_view bytes) {
    Temporary sources; Source source(sources.root / "source", bytes);
    const auto result = write_payload_stream_v15(source.fd, root, "input-1", id(bytes), bytes.size(), deadline());
    return result;
}
fs::path chunk(const fs::path& directory) {
    for (const auto& entry : fs::directory_iterator(directory))
        if (entry.path().extension() == ".bin") return entry.path();
    throw Error("missing chunk");
}
void boundary_payloads() {
    for (const std::size_t size : {0U, 1U, 65536U, 131089U}) {
        Temporary t; std::string bytes(size, 'a');
        const auto stream = create(t.root, bytes);
        require(stream.sha256 == id(bytes).sha256 && stream.size_bytes == size, "wrong digest or size");
        if (size == 0) require(stream.sha256 == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "empty SHA256");
        if (size == 1) require(stream.sha256 == "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb", "one-byte SHA256");
        if (size == 65536) require(stream.sha256 == "bf718b6f653bebc184e1479f1935b8da974d701b893afcf49e701f3e2f9f9c5a", "full-chunk SHA256");
        if (size == 131089) require(stream.sha256 == "c5e33faedb41ecc23507197af1d853212ff36641279638ebc1288c90f417404f", "multi-chunk SHA256");
        require(stream.chunk_bytes == 65536 && stream.chunk_count == (size + 65535) / 65536, "wrong chunks");
        require(!fs::path(stream.reference).is_absolute() && stream.reference.find("..") == std::string::npos, "unsafe reference");
        verify_payload_stream_v15(stream, t.root);
        std::string reconstructed;
        for (const auto& e : fs::directory_iterator(t.root / stream.reference)) {
            if (e.path().extension() == ".bin") {
                const auto n = fs::file_size(e.path()); require(n > 0 && n <= 65536, "bad chunk length");
                reconstructed.append(n, 'a');
            }
        }
        require(reconstructed == bytes, "missing payload bytes");
    }
}
void short_reads_and_binary_bytes() {
    Temporary t, sources;
    std::string bytes(65555, '\0');
    for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<char>(i % 256);
    Source source(sources.root / "source", bytes); short_source = source.fd;
    const auto stream = write_payload_stream_v15(source.fd, t.root, "short", id(bytes), bytes.size(), deadline());
    short_source = -1;
    require(stream.chunk_count == 2, "short reads changed chunk partition");
    verify_payload_stream_v15(stream, t.root);
    const auto dir = t.root / stream.reference;
    std::string actual;
    for (const char* name : {"chunk-00000000000000000000.bin", "chunk-00000000000000000001.bin"}) {
        std::ifstream f(dir / name, std::ios::binary);
        require(static_cast<bool>(f), "missing canonical chunk filename");
        actual.append(std::istreambuf_iterator<char>(f), {});
    }
    require(actual == bytes, "binary payload bytes or ordering changed");
}
void reads_respect_remaining_budget() {
    for (const std::uint64_t budget : {0ULL, 1ULL, 65536ULL, 65537ULL}) {
        Temporary t, sources;
        const std::string bytes(131072, 'b'); Source source(sources.root / "source", bytes);
        const auto prefix = std::string_view(bytes).substr(0, budget);
        observed_source = source.fd; read_requests.clear();
        rejects([&] { write_payload_stream_v15(source.fd, t.root, "bounded", id(prefix), budget, deadline()); },
                ArtifactPayloadStreamErrorCodeV15::limit_exceeded);
        observed_source = -1;
        require(::lseek(source.fd, 0, SEEK_CUR) == static_cast<off_t>(budget), "source advanced beyond byte budget");
        std::vector<std::size_t> expected;
        if (budget == 1) expected = {1};
        if (budget == 65536) expected = {65536};
        if (budget == 65537) expected = {65536, 1};
        require(read_requests == expected, "read request exceeded remaining budget");
        require(fs::is_empty(t.root), "budget failure left accepted stream or staging");
    }
}
void restrictive_umask_cleanup() {
    Temporary t, sources;
    Source source(sources.root / "source", "test");
    struct RestoreUmask {
        mode_t previous;
        ~RestoreUmask() { ::umask(previous); }
    };
    {
        RestoreUmask restore{::umask(0777)};
        rejects([&] { write_payload_stream_v15(source.fd, t.root, "restricted", id("test"), 4, deadline()); },
                ArtifactPayloadStreamErrorCodeV15::invalid_destination);
    }
    const bool empty_after_failure = fs::is_empty(t.root);
    // Restore access only for fixture cleanup if this regression fails; the
    // assertion uses the observed state before any test-side intervention.
    if (!empty_after_failure)
        for (const auto& entry : fs::directory_iterator(t.root)) ::chmod(entry.path().c_str(), 0700);
    require(empty_after_failure, "restrictive umask left empty staging directory");
}
void nested_staging_cleanup() {
    Temporary t, sources, outside;
    put(outside.root / "sentinel", "preserve external bytes");
    Source source(sources.root / "source", std::string(131072, 'a'));
    nested_injection_root = t.root; external_cleanup_target = outside.root;
    nested_write_count = 0; nested_injected = false;
    rejects([&] { write_payload_stream_v15(source.fd, t.root, "nested", id(std::string(131072, 'a')), 131072, deadline()); },
            ArtifactPayloadStreamErrorCodeV15::seal_failure);
    nested_injection_root.clear();
    require(nested_injected, "nested directory mutation was not injected");
    require(fs::is_empty(t.root), "nested staging directories survived failure");
    std::ifstream file(outside.root / "sentinel");
    const std::string bytes((std::istreambuf_iterator<char>(file)), {});
    require(bytes == "preserve external bytes", "cleanup followed external directory symlink");
    require(std::distance(fs::directory_iterator(outside.root), fs::directory_iterator{}) == 1, "external directory changed");
}
void failures_leave_nothing() {
    Temporary t, source_root; const std::string bytes(131072, 'b'); Source source(source_root.root / "source", bytes);
    auto attempt = [&](ArtifactIdV1 expected, std::uint64_t limit, auto time, auto code) {
        require(::lseek(source.fd, 0, SEEK_SET) == 0, "rewind failed");
        rejects([&] { write_payload_stream_v15(source.fd, t.root, "failed", expected, limit, time); }, code);
        require(fs::is_empty(t.root), "failed stream or staging left behind");
    };
    attempt(id(bytes), bytes.size() - 1, deadline(), ArtifactPayloadStreamErrorCodeV15::limit_exceeded);
    attempt(id(bytes), bytes.size(), std::chrono::steady_clock::now() - std::chrono::seconds(1), ArtifactPayloadStreamErrorCodeV15::deadline_exceeded);
    auto bad = id(bytes); bad.sha256[0] = bad.sha256[0] == '0' ? '1' : '0';
    attempt(bad, bytes.size(), deadline(), ArtifactPayloadStreamErrorCodeV15::digest_size_mismatch);
    bad = id(bytes); --bad.size_bytes;
    attempt(bad, bytes.size(), deadline(), ArtifactPayloadStreamErrorCodeV15::digest_size_mismatch);
    // A dishonest expected size must not bypass the running byte budget.
    bad = id(bytes); bad.size_bytes = bytes.size() - 1;
    attempt(bad, bytes.size() - 1, deadline(), ArtifactPayloadStreamErrorCodeV15::limit_exceeded);
    failing_source = source.fd; source_reads = 0;
    attempt(id(bytes), bytes.size(), deadline(), ArtifactPayloadStreamErrorCodeV15::invalid_source);
    failing_source = -1; fail_writes = true; chunk_writes = 0;
    attempt(id(bytes), bytes.size(), deadline(), ArtifactPayloadStreamErrorCodeV15::seal_failure);
    fail_writes = false;
    require(::fcntl(source.fd, F_GETFD) >= 0, "borrowed source closed");
    require(::chmod((source_root.root / "source").c_str(), 0644) == 0, "chmod failed");
    attempt(id(bytes), bytes.size(), deadline(), ArtifactPayloadStreamErrorCodeV15::invalid_source);
    require(::chmod((source_root.root / "source").c_str(), 0600) == 0, "chmod failed");
    fs::create_hard_link(source_root.root / "source", source_root.root / "hardlink");
    attempt(id(bytes), bytes.size(), deadline(), ArtifactPayloadStreamErrorCodeV15::invalid_source);
}
void tampered_files() {
    for (int scenario = 0; scenario < 16; ++scenario) {
        Temporary t; auto stream = create(t.root, "test"); const auto dir = t.root / stream.reference;
        const auto seal = dir / "seal.json"; const auto payload = chunk(dir);
        auto code = ArtifactPayloadStreamErrorCodeV15::mutation;
        switch (scenario) {
        case 0: put(seal, "{}"); code = ArtifactPayloadStreamErrorCodeV15::seal_failure; break;
        case 1: { std::ifstream f(seal); std::string s((std::istreambuf_iterator<char>(f)), {}); put(seal, " " + s); code = ArtifactPayloadStreamErrorCodeV15::seal_failure; break; }
        case 2: fs::remove(seal); fs::create_symlink(payload.filename(), seal); break;
        case 3: fs::create_hard_link(seal, t.root / "extra"); break;
        case 4: require(::chmod(seal.c_str(), 0644) == 0, "chmod failed"); break;
        case 5: fs::remove(seal); require(::mkfifo(seal.c_str(), 0600) == 0, "mkfifo failed"); break;
        case 6: put(payload, "evil"); break;
        case 7: fs::create_hard_link(payload, t.root / "extra"); break;
        case 8: fs::remove(payload); fs::create_symlink("header.jsonl", payload); break;
        case 9: put(dir / "extra", "surprise"); break;
        case 10: put(dir / "header.jsonl", "{}\n"); break;
        case 11: { std::ifstream f(seal); std::string s((std::istreambuf_iterator<char>(f)), {}); s.insert(s.size() - 1, ",\"unknown\":0"); put(seal, s); code = ArtifactPayloadStreamErrorCodeV15::seal_failure; break; }
        case 12: put(seal, "{\"size_bytes\":4,\"sha256\":\"" + stream.sha256 + "\",\"chunk_bytes\":65536,\"chunk_count\":1}"); code = ArtifactPayloadStreamErrorCodeV15::seal_failure; break;
        case 13: fs::remove(payload); fs::create_directory(payload); break;
        case 14: require(::chmod(payload.c_str(), 0644) == 0, "chmod failed"); break;
        case 15: put(seal, "{\"sha256\":\"" + stream.sha256 + "\",\"size_bytes\":04,\"chunk_bytes\":65536,\"chunk_count\":1}"); code = ArtifactPayloadStreamErrorCodeV15::seal_failure; break;
        }
        rejects([&] { verify_payload_stream_v15(stream, t.root); }, code);
    }
}
void concurrent_chunk_mutation() {
    Temporary t; const auto stream = create(t.root, std::string(65537, 'a'));
    const auto dir = t.root / stream.reference;
    for (const auto& e : fs::directory_iterator(dir))
        if (e.path().extension() == ".bin" && fs::file_size(e.path()) == 65536) mutate_path = e.path().string();
    require(!mutate_path.empty(), "missing first chunk");
    try {
        verify_payload_stream_v15(stream, t.root);
    } catch (const ArtifactPayloadStreamErrorV15& e) {
        require(e.code() == ArtifactPayloadStreamErrorCodeV15::mutation, "wrong mutation code"); return;
    }
    require(mutate_path.empty(), "mutation hook did not fire");
    throw Error("concurrent mutation accepted after hook fired");
}
void isolation_and_destinations() {
    Temporary a, b; const auto one = create(a.root, "same"); const auto two = create(b.root, "same");
    require(one.sha256 == two.sha256 && !fs::equivalent(a.root / one.reference, b.root / two.reference), "attempts shared files");
    verify_payload_stream_v15(one, a.root); verify_payload_stream_v15(two, b.root);
    Temporary source_root; Source source(source_root.root / "source", "same");
    rejects([&] { write_payload_stream_v15(source.fd, a.root, "../bad", id("same"), 4, deadline()); }, ArtifactPayloadStreamErrorCodeV15::invalid_destination);
    rejects([&] { write_payload_stream_v15(source.fd, a.root, "input-1", id("same"), 4, deadline()); }, ArtifactPayloadStreamErrorCodeV15::invalid_destination);
    verify_payload_stream_v15(one, a.root);
    require(std::distance(fs::directory_iterator(a.root), fs::directory_iterator{}) == 1, "duplicate left staging");
    auto bad = one; bad.reference = "../escape";
    rejects([&] { verify_payload_stream_v15(bad, a.root); }, ArtifactPayloadStreamErrorCodeV15::invalid_destination);
    fs::create_directory_symlink(a.root, source_root.root / "link");
    rejects([&] { write_payload_stream_v15(source.fd, source_root.root / "link", "x", id("same"), 4, deadline()); }, ArtifactPayloadStreamErrorCodeV15::invalid_destination);
    const int directory_source = ::open(source_root.root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    require(directory_source >= 0, "open directory failed");
    rejects([&] { write_payload_stream_v15(directory_source, a.root, "dir", id(""), 0, deadline()); }, ArtifactPayloadStreamErrorCodeV15::invalid_source);
    ::close(directory_source);
    require(::chmod(b.root.c_str(), 0755) == 0, "chmod failed");
    rejects([&] { verify_payload_stream_v15(two, b.root); }, ArtifactPayloadStreamErrorCodeV15::invalid_destination);
}
}
int main() {
    const char* stage = "boundary payloads";
    try {
        boundary_payloads(); stage = "short reads and binary bytes";
        short_reads_and_binary_bytes(); stage = "failure cleanup";
        failures_leave_nothing(); stage = "bounded read requests";
        reads_respect_remaining_budget(); stage = "nested staging cleanup";
        nested_staging_cleanup(); stage = "restrictive umask cleanup";
        restrictive_umask_cleanup(); stage = "tampered files";
        tampered_files(); stage = "concurrent mutation";
        concurrent_chunk_mutation(); stage = "isolation and destinations";
        isolation_and_destinations();
    }
    catch (const std::exception& e) { std::cerr << stage << ": " << e.what() << '\n'; return 1; }
    std::cout << "artifact payload stream V1.5 contracts passed\n";
}
