// Statically linked adversarial fixture; no BRUCE implementation is linked.
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/mman.h>
#ifdef BRUCE_LOGIC_PAYLOAD_WORKER
#include <sys/stat.h>
#endif
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
char input[65536], output[65536];
// Parse by copying; length framing does not require separator bytes.
void field(char*& p, char* dst) { char* end; auto n = std::strtoul(p, &end, 10); if (*end != ':' || n > 16000) _exit(80); std::memcpy(dst, end + 1, n); dst[n] = 0; p = end + 1 + n; }
void add(char*& p, const char* s) { p += std::sprintf(p, "%zu:", std::strlen(s)); std::memcpy(p, s, std::strlen(s)); p += std::strlen(s); }
int main() {
#ifdef BRUCE_LOGIC_EXEC_ESCAPE_PATH
    // Regression: absolute paths ignore dirfd, even with AT_EMPTY_PATH. On an
    // unsafe filter this launches a different on-disk executable which emits
    // a perfectly valid result for the original sealed worker's identity.
    char name[] = "escaped-worker";
    char* const args[] = {name, nullptr}; char* const env[] = {nullptr};
    ::syscall(SYS_execveat, 3, BRUCE_LOGIC_EXEC_ESCAPE_PATH, args, env, AT_EMPTY_PATH);
    return 77;
#endif
    extern char** environ;
    if (environ[0]) return 98;
    char probe;
    for (int fd = 3; fd < 64; ++fd) {
#ifdef BRUCE_LOGIC_PAYLOAD_WORKER
        if (fd == 6) continue;
#endif
        if (::read(fd, &probe, 1) != -1) return 99;
    }
    std::size_t n = 0; ssize_t r; while ((r = ::read(0, input + n, sizeof(input) - n)) > 0) n += r;
    char* p = std::strchr(input, ':'); if (!p) return 80; ++p;
    if (std::strncmp(p, "BRUCE_LOGIC_REQUEST_V1\n", 23) != 0) return 81;
    p += 23;
    char identity[8192], digest[128], text[8192], budget[128]; field(p, identity); field(p, digest); field(p, text); field(p, budget);
#ifdef BRUCE_LOGIC_PAYLOAD_WORKER
    char present[8], manifest[256], index[32], hash[128], size[32], codec[64];
    field(p, present); field(p, manifest); field(p, index); field(p, hash); field(p, size); field(p, codec);
    if (std::strcmp(present, "1") || std::strcmp(codec, "artifact-bytes-v1") || std::strlen(hash) != 64 || *p) return 70;
    const auto expected = std::strtoul(size, nullptr, 10);
    struct stat st{};
    if (::fstat(6, &st) != 0 || static_cast<unsigned long>(st.st_size) != expected || ::lseek(6, 0, SEEK_SET) != 0) return 71;
    char bytes[4096]; unsigned long count = 0;
    while ((r = ::read(6, bytes, sizeof(bytes))) > 0) {
        for (ssize_t i = 0; i < r; ++i) if (bytes[i] != (expected == 3 ? "abc"[count + i] : 'x')) return 72;
        count += r;
    }
    if (r != 0 || count != expected || ::lseek(6, 0, SEEK_END) != static_cast<off_t>(expected)) return 73;
    if (!std::strcmp(text, "payload-write")) { ::write(6, "y", 1); return 74; }
    if (!std::strcmp(text, "payload-mmap")) { ::mmap(nullptr, 4096, PROT_READ, MAP_PRIVATE, 6, 0); return 75; }
    if (::close(6) != 0) return 76;
#endif
    if (!std::strcmp(text, "timeout")) for (;;) ::syscall(SYS_sched_yield);
    if (!std::strcmp(text, "crash")) ::abort();
    if (!std::strcmp(text, "memory")) { void* m = ::mmap(nullptr, 1024UL*1024*1024, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0); return m == MAP_FAILED ? 91 : 92; }
    if (!std::strcmp(text, "network")) { ::socket(AF_INET, SOCK_STREAM, 0); return 93; }
    if (!std::strcmp(text, "filesystem")) { ::open("/etc/passwd", O_RDONLY); return 94; }
    if (!std::strcmp(text, "fork")) { ::fork(); return 95; }
    if (!std::strcmp(text, "unknown-syscall")) { ::syscall(-1, 0); return 97; }
    if (!std::strcmp(text, "output") || !std::strcmp(text, "stderr")) { for (;;) if (::write(!std::strcmp(text, "stderr") ? 2 : 1, input, sizeof(input)) < 0) return 96; }
    if (!std::strcmp(text, "malformed")) { ::write(1, "xyz", 3); return 0; }
    if (!std::strcmp(text, "oversized")) { ::write(1, "99999999999999:", 15); return 0; }
    if (!std::strcmp(text, "mismatch")) identity[0] = 'X';
    const char* value = !std::strcmp(text, "wrong") ? "1" : "9/16";
    p = output; std::memcpy(p, "BRUCE_LOGIC_RESULT_V1\n", 22); p += 22;
    add(p, identity); add(p, digest); add(p, "candidate"); add(p, !std::strcmp(text, "codec") ? "proof" : "rational-v1"); add(p, value);
    add(p, !std::strcmp(text, "digest") ? "bad" : !std::strcmp(text, "wrong") ? "6b86b273ff34fce19d6b804eff5a3f5747ada4eaa22f1d49c01e52ddb7875b4b" : "31540b43fa20cb275095247f6784794cecee19c0117b76238fa153964a52af5b"); add(p, "");
    if (!std::strcmp(text, "claim")) add(p, "proven");
    char prefix[64]; const auto z = std::sprintf(prefix, "%zu:", static_cast<std::size_t>(p-output)); ::write(1, prefix, z); ::write(1, output, p-output);
    if (!std::strcmp(text, "trailing")) ::write(1, "x", 1);
    return 0;
}
