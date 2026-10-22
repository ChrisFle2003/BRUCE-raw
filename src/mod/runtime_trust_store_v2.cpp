#include "bruce/mod/runtime_trust_store_v2.hpp"

#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/mod/runtime_package.hpp"
#include "bruce/io/local_files.hpp"
#include "runtime_mod_store_internal.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <limits>
#include <set>
#include <tuple>
#include <array>
#include <cerrno>
#include <charconv>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace bruce {
namespace {
constexpr std::size_t max_metadata_bytes = 65536;
constexpr std::size_t max_state_bytes = 65536;
[[noreturn]] void invalid() { throw Error("InvalidRuntimeTrustV2"); }
bool lowercase_hex(std::string_view text, std::size_t length) {
    return text.size() == length && std::all_of(text.begin(), text.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
bool generation_valid(std::string_view text) {
    return text.size() == 20 &&
        std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; }) &&
        text != "00000000000000000000";
}
bool identifier(std::string_view text) {
    return !text.empty() && text.size() <= 128 && text.front() != '.' &&
        std::all_of(text.begin(), text.end(), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        });
}
bool well_formed_utf8(std::string_view text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i]);
        if (first < 0x80) { ++i; continue; }
        const std::size_t length = first >= 0xc2 && first <= 0xdf ? 2 :
            first >= 0xe0 && first <= 0xef ? 3 :
            first >= 0xf0 && first <= 0xf4 ? 4 : 0;
        if (length == 0 || length > text.size() - i) return false;
        for (std::size_t j = 1; j < length; ++j) {
            const auto next = static_cast<unsigned char>(text[i + j]);
            if (next < 0x80 || next > 0xbf) return false;
        }
        const auto second = static_cast<unsigned char>(text[i + 1]);
        if ((first == 0xe0 && second < 0xa0) || (first == 0xed && second >= 0xa0) ||
            (first == 0xf0 && second < 0x90) || (first == 0xf4 && second >= 0x90)) return false;
        i += length;
    }
    return true;
}
unsigned decimal(std::string_view text) {
    unsigned out = 0;
    for (char c : text) {
        if (c < '0' || c > '9') invalid();
        out = out * 10 + static_cast<unsigned>(c - '0');
    }
    return out;
}
bool utc_timestamp_valid(std::string_view text) {
    if (text.size() != 20 || text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':' || text[19] != 'Z') return false;
    for (const auto index : {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18})
        if (text[index] < '0' || text[index] > '9') return false;
    const auto year = static_cast<int>(decimal(text.substr(0, 4)));
    const auto month = decimal(text.substr(5, 2));
    const auto day = decimal(text.substr(8, 2));
    const auto date = std::chrono::year_month_day{
        std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day}};
    return date.ok() && decimal(text.substr(11, 2)) < 24 &&
        decimal(text.substr(14, 2)) < 60 && decimal(text.substr(17, 2)) < 60;
}
void check_trust(std::string_view bytes) {
    // The V1 parser enforces sorted, unique IDs and lowercase 32-byte keys.
    const auto parsed = read_runtime_mod_truststore_v1(bytes);
    std::string canonical = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n";
    for (const auto& [id, key] : parsed.public_keys()) canonical += id + ':' + key + '\n';
    if (bytes != canonical) invalid();
}
void check_generation(const RuntimeTrustGenerationV2& value) {
    if (!generation_valid(value.generation) || !utc_timestamp_valid(value.created_at) ||
        !lowercase_hex(value.trust_sha256, 64) ||
        (value.comment && (value.comment->size() > 4096 || !well_formed_utf8(*value.comment)))) invalid();
    check_trust(value.trust_bytes);
    if (sha256(value.trust_bytes) != value.trust_sha256) invalid();
}
void check_reference(const RuntimeTrustReferenceV2& value) {
    if (!generation_valid(value.generation) || !lowercase_hex(value.trust_sha256, 64)) invalid();
}
void check_state(const RuntimeTrustStateV2& value) {
    std::set<std::string> generations;
    for (const auto* ref : {&value.prepared, &value.active, &value.previous}) {
        if (!*ref) continue;
        check_reference(**ref);
        if (!generations.insert((*ref)->generation).second) invalid();
    }
    if (!value.active && value.previous) invalid();
}
class CanonicalReader {
public:
    explicit CanonicalReader(std::string_view bytes) : bytes_(bytes) {}
    void literal(std::string_view expected) {
        if (!bytes_.substr(offset_).starts_with(expected)) invalid();
        offset_ += expected.size();
    }
    bool has(std::string_view expected) const { return bytes_.substr(offset_).starts_with(expected); }
    std::string string() {
        const auto start = offset_;
        literal("\"");
        std::string value;
        while (offset_ < bytes_.size()) {
            const unsigned char c = static_cast<unsigned char>(bytes_[offset_++]);
            if (c == '"') {
                if (json_string(value) != bytes_.substr(start, offset_ - start)) invalid();
                return value;
            }
            if (c < 32) invalid();
            if (c != '\\') { value.push_back(static_cast<char>(c)); continue; }
            if (offset_ >= bytes_.size()) invalid();
            const char escaped = bytes_[offset_++];
            if (escaped == '"' || escaped == '\\') { value.push_back(escaped); continue; }
            if (escaped != 'u' || bytes_.size() - offset_ < 4 ||
                bytes_.substr(offset_, 2) != "00" ||
                !lowercase_hex(bytes_.substr(offset_ + 2, 2), 2)) invalid();
            const auto hi = bytes_[offset_ + 2];
            const auto lo = bytes_[offset_ + 3];
            const auto nibble = [](char x) { return static_cast<unsigned>(x <= '9' ? x - '0' : x - 'a' + 10); };
            const auto decoded = (nibble(hi) << 4) | nibble(lo);
            if (decoded >= 32) invalid();
            value.push_back(static_cast<char>(decoded));
            offset_ += 4;
        }
        invalid();
    }
    void finish() { if (offset_ != bytes_.size()) invalid(); }
private:
    std::string_view bytes_;
    std::size_t offset_ = 0;
};
std::string reference_json(const std::optional<RuntimeTrustReferenceV2>& reference) {
    if (!reference) return "null";
    return "{\"generation\":" + json_string(reference->generation) +
        ",\"trust_sha256\":" + json_string(reference->trust_sha256) + '}';
}
std::optional<RuntimeTrustReferenceV2> read_reference(CanonicalReader& reader) {
    if (reader.has("null")) { reader.literal("null"); return std::nullopt; }
    reader.literal("{\"generation\":");
    auto generation = reader.string();
    reader.literal(",\"trust_sha256\":");
    auto digest = reader.string();
    reader.literal("}");
    RuntimeTrustReferenceV2 result{std::move(generation), std::move(digest)};
    check_reference(result);
    return result;
}
} // namespace

RuntimeTrustGenerationV2 make_runtime_trust_generation_v2(
    std::string_view generation, std::string_view trust_bytes,
    const RuntimeTrustClockV2& clock, std::optional<std::string> comment) {
    RuntimeTrustGenerationV2 result{std::string(generation), std::string(trust_bytes),
                                    sha256(trust_bytes), clock.now_utc(), std::move(comment)};
    check_generation(result);
    return result;
}

std::string render_runtime_trust_metadata_v2(const RuntimeTrustGenerationV2& generation) {
    check_generation(generation);
    std::string bytes = "{";
    if (generation.comment) bytes += "\"comment\":" + json_string(*generation.comment) + ',';
    bytes += "\"created_at\":" + json_string(generation.created_at) +
        ",\"generation\":" + json_string(generation.generation) +
        ",\"trust_sha256\":" + json_string(generation.trust_sha256) + "}\n";
    if (bytes.size() > max_metadata_bytes) invalid();
    return bytes;
}

RuntimeTrustGenerationV2 parse_runtime_trust_metadata_v2(std::string_view metadata,
                                                          std::string_view trust_bytes) {
    if (metadata.size() > max_metadata_bytes) invalid();
    CanonicalReader reader(metadata);
    reader.literal("{");
    std::optional<std::string> comment;
    if (reader.has("\"comment\":")) {
        reader.literal("\"comment\":");
        comment = reader.string();
        reader.literal(",");
    }
    reader.literal("\"created_at\":");
    auto created_at = reader.string();
    reader.literal(",\"generation\":");
    auto generation = reader.string();
    reader.literal(",\"trust_sha256\":");
    auto digest = reader.string();
    reader.literal("}\n");
    reader.finish();
    RuntimeTrustGenerationV2 result{std::move(generation), std::string(trust_bytes),
                                    std::move(digest), std::move(created_at), std::move(comment)};
    check_generation(result);
    return result;
}

bool same_runtime_trust_identity_v2(const RuntimeTrustGenerationV2& left,
                                    const RuntimeTrustGenerationV2& right) {
    check_generation(left); check_generation(right);
    return left.trust_sha256 == right.trust_sha256 && left.trust_bytes == right.trust_bytes;
}

std::string render_runtime_trust_state_v2(const RuntimeTrustStateV2& state) {
    check_state(state);
    std::string bytes = "{\"active\":" + reference_json(state.active) +
        ",\"prepared\":" + reference_json(state.prepared) +
        ",\"previous\":" + reference_json(state.previous) + "}\n";
    if (bytes.size() > max_state_bytes) invalid();
    return bytes;
}

RuntimeTrustStateV2 parse_runtime_trust_state_v2(std::string_view state) {
    if (state.size() > max_state_bytes) invalid();
    CanonicalReader reader(state);
    reader.literal("{\"active\":");
    auto active = read_reference(reader);
    reader.literal(",\"prepared\":");
    auto prepared = read_reference(reader);
    reader.literal(",\"previous\":");
    auto previous = read_reference(reader);
    reader.literal("}\n");
    reader.finish();
    RuntimeTrustStateV2 result{std::move(prepared), std::move(active), std::move(previous)};
    check_state(result);
    return result;
}

std::vector<RuntimeAffectedModV2> runtime_mods_losing_trust_v2(
    const RuntimeTrustGenerationV2& current, const RuntimeTrustGenerationV2& candidate,
    std::span<const RuntimeActiveInstalledModV2> installed) {
    check_generation(current); check_generation(candidate);
    const auto old_keys = read_runtime_mod_truststore_v1(current.trust_bytes).public_keys();
    const auto new_keys = read_runtime_mod_truststore_v1(candidate.trust_bytes).public_keys();
    std::vector<RuntimeAffectedModV2> affected;
    std::set<std::tuple<RuntimeInstalledModKindV2, std::string, std::string>> seen;
    for (const auto& mod : installed) {
        if (!mod.active) continue;
        if (!identifier(mod.mod_id) || !identifier(mod.version) || !identifier(mod.signing_key_id)) invalid();
        if (!seen.emplace(mod.kind, mod.mod_id, mod.version).second) invalid();
        const auto old_key = old_keys.find(mod.signing_key_id);
        if (old_key == old_keys.end()) invalid();
        const auto new_key = new_keys.find(mod.signing_key_id);
        if (new_key == new_keys.end() || new_key->second != old_key->second)
            affected.push_back({mod.kind, mod.mod_id, mod.version, mod.signing_key_id});
    }
    std::sort(affected.begin(), affected.end(), [](const auto& a, const auto& b) {
        return std::tie(a.kind, a.mod_id, a.version, a.signing_key_id) <
            std::tie(b.kind, b.mod_id, b.version, b.signing_key_id);
    });
    return affected;
}

namespace {
using runtime_mod_store_detail::check_private_directory;
using runtime_mod_store_detail::check_private_node;
using runtime_mod_store_detail::test_failpoint;
constexpr std::string_view coordinator_header = "BRUCE_RUNTIME_TRUST_JOURNAL_V2\n";
constexpr std::size_t trust_journal_limit = 32 * 1024 * 1024;
constexpr std::size_t trust_field_limit = 1024 * 1024;
constexpr std::size_t max_trust_generations = 4096;
thread_local std::set<std::string> coordinating_roots;
struct CoordinatorScope {
    std::string root;
    explicit CoordinatorScope(std::string path) : root(std::move(path)) {
        if (!coordinating_roots.insert(root).second) invalid();
    }
    ~CoordinatorScope() { coordinating_roots.erase(root); }
};
struct TrustFd {
    int fd;
    explicit TrustFd(int value) : fd(value) { if (fd < 0) throw Error("RuntimeTrustFileOpenFailed"); }
    ~TrustFd() { ::close(fd); }
};
bool trust_exists(const std::string& path) {
    struct stat st{};
    if (::lstat(path.c_str(), &st) == 0) return true;
    if (errno != ENOENT) throw Error("RuntimeTrustFileStatFailed");
    return false;
}
std::string trust_read(const std::string& path, std::size_t limit = trust_field_limit) {
    TrustFd file(::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOATIME));
    check_private_node(file.fd, false);
    std::string out;
    std::array<char, 8192> buffer{};
    for (;;) {
        const auto n = ::read(file.fd, buffer.data(), buffer.size());
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) throw Error("RuntimeTrustFileReadFailed");
        if (n == 0) return out;
        if (static_cast<std::size_t>(n) > limit - out.size()) invalid();
        out.append(buffer.data(), static_cast<std::size_t>(n));
    }
}
void trust_sync(const std::string& path) {
    TrustFd dir(::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    check_private_node(dir.fd, true);
    if (::fsync(dir.fd) != 0) throw Error("RuntimeTrustDurabilityUnknown");
}
void trust_replace(const std::string& path, std::string_view bytes) {
    if (trust_exists(path)) (void)trust_read(path, trust_journal_limit);
    LocalFiles::replace_atomically(path, bytes);
}
void trust_append(const std::string& path, std::string_view bytes) {
    TrustFd fd(::open(path.c_str(), O_WRONLY | O_APPEND | O_NOFOLLOW | O_CLOEXEC));
    check_private_node(fd.fd, false);
    struct stat st{};
    if (::fstat(fd.fd, &st) != 0 || st.st_size < 0 ||
        static_cast<std::uint64_t>(st.st_size) + bytes.size() > trust_journal_limit) invalid();
    for (std::size_t pos = 0; pos < bytes.size();) {
        const auto n = ::write(fd.fd, bytes.data() + pos, bytes.size() - pos);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) throw Error("RuntimeTrustJournalWriteFailed");
        pos += static_cast<std::size_t>(n);
    }
    if (::fsync(fd.fd) != 0) throw Error("RuntimeTrustDurabilityUnknown");
}
void tf(std::string& bytes, std::string_view field) { bytes += std::to_string(field.size()) + ':'; bytes += field; }
struct TornTrustRecord {};
struct TrustReader {
    std::string_view bytes;
    std::size_t pos = 0;
    std::string field() {
        const auto colon = bytes.find(':', pos);
        if (colon == std::string_view::npos) {
            const auto rest = bytes.substr(pos);
            if (rest.size() <= 7 && std::all_of(rest.begin(), rest.end(), [](char c) { return c >= '0' && c <= '9'; })) throw TornTrustRecord{};
            invalid();
        }
        const auto digits = bytes.substr(pos, colon - pos);
        std::size_t n = 0;
        const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), n);
        if (digits.empty() || (digits.size() > 1 && digits[0] == '0') || parsed.ec != std::errc{} ||
            parsed.ptr != digits.data() + digits.size() || n > trust_field_limit) invalid();
        pos = colon + 1;
        if (n > bytes.size() - pos) throw TornTrustRecord{};
        auto result = std::string(bytes.substr(pos, n)); pos += n; return result;
    }
};
struct RawRegistryState {
    std::uint64_t generation = 0;
    std::vector<std::vector<std::string>> records;
};
std::uint64_t canonical_number(std::string_view text) {
    std::uint64_t out = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), out);
    if (text.empty() || (text.size() > 1 && text.front() == '0') || parsed.ec != std::errc{} ||
        parsed.ptr != text.data() + text.size()) invalid();
    return out;
}
RawRegistryState parse_raw_registry(std::string_view bytes, std::string_view header,
                                    const std::size_t fields_per_record) {
    if (!bytes.starts_with(header) || !bytes.ends_with('\n')) invalid();
    TrustReader reader{bytes, header.size()};
    RawRegistryState state;
    try {
        state.generation = canonical_number(reader.field());
        const auto count = canonical_number(reader.field());
        if (count > 512) invalid();
        for (std::uint64_t index = 0; index < count; ++index) {
            std::vector<std::string> fields;
            for (std::size_t field = 0; field < fields_per_record; ++field)
                fields.push_back(reader.field());
            state.records.push_back(std::move(fields));
        }
    } catch (const TornTrustRecord&) { invalid(); }
    if (reader.pos + 1 != bytes.size()) invalid();
    std::string canonical(header);
    tf(canonical, std::to_string(state.generation));
    tf(canonical, std::to_string(state.records.size()));
    for (const auto& record : state.records)
        for (const auto& field : record) tf(canonical, field);
    canonical += '\n';
    if (canonical != bytes) invalid();
    return state;
}
void validate_registry_transition(std::string_view old_bytes, std::string_view next_bytes,
                                  const bool logic) {
    if (old_bytes == next_bytes) return;
    constexpr std::string_view data_header = "BRUCE_RUNTIME_MOD_REGISTRY_V1\n";
    constexpr std::string_view logic_header = "BRUCE_LOGIC_MOD_REGISTRY_V1\n";
    const auto header = logic ? logic_header : data_header;
    const auto fields = logic ? 5U : 4U;
    const auto old_state = parse_raw_registry(old_bytes, header, fields);
    const auto next_state = parse_raw_registry(next_bytes, header, fields);
    if (old_state.generation == std::numeric_limits<std::uint64_t>::max() ||
        next_state.generation != old_state.generation + 1 ||
        next_state.records.size() != old_state.records.size()) invalid();
    const std::string active = logic ? "4" : "1";
    const std::string disabled = logic ? "5" : "2";
    bool changed = false;
    for (std::size_t index = 0; index < old_state.records.size(); ++index) {
        const auto& old_record = old_state.records[index];
        const auto& next_record = next_state.records[index];
        if (!std::equal(old_record.begin(), old_record.end() - 1, next_record.begin())) invalid();
        if (old_record.back() == next_record.back()) continue;
        if (old_record.back() != active || next_record.back() != disabled) invalid();
        changed = true;
    }
    if (!changed) invalid();
}
void validate_exact_deactivation(std::string_view old_bytes, std::string_view next_bytes,
                                 const bool logic,
                                 const std::vector<RuntimeAffectedModV2>& affected) {
    constexpr std::string_view data_header = "BRUCE_RUNTIME_MOD_REGISTRY_V1\n";
    constexpr std::string_view logic_header = "BRUCE_LOGIC_MOD_REGISTRY_V1\n";
    const auto fields = logic ? 5U : 4U;
    const auto old_state = parse_raw_registry(old_bytes, logic ? logic_header : data_header, fields);
    const auto next_state = parse_raw_registry(next_bytes, logic ? logic_header : data_header, fields);
    std::set<std::pair<std::string, std::string>> changed;
    if (old_bytes != next_bytes) {
        validate_registry_transition(old_bytes, next_bytes, logic);
        for (std::size_t index = 0; index < old_state.records.size(); ++index)
            if (old_state.records[index].back() != next_state.records[index].back())
                changed.emplace(old_state.records[index][0], old_state.records[index][1]);
    }
    std::set<std::pair<std::string, std::string>> expected;
    const auto kind = logic ? RuntimeInstalledModKindV2::logic : RuntimeInstalledModKindV2::data;
    for (const auto& mod : affected) if (mod.kind == kind)
        expected.emplace(mod.mod_id, mod.version);
    if (changed != expected) invalid();
}
struct TrustTransaction {
    RuntimeTrustStateV2 old_state, next_state;
    std::string old_trust, next_trust, old_data, next_data, old_logic, next_logic;
    bool importing = false;
};
std::string transaction_bytes(const TrustTransaction& tx) {
    std::string out;
    tf(out, render_runtime_trust_state_v2(tx.old_state)); tf(out, render_runtime_trust_state_v2(tx.next_state));
    tf(out, tx.old_trust); tf(out, tx.next_trust);
    // Explicit digest fields bind root compatibility projections as well as bytes.
    tf(out, sha256(tx.old_trust)); tf(out, sha256(tx.next_trust));
    tf(out, tx.old_data); tf(out, tx.next_data); tf(out, tx.old_logic); tf(out, tx.next_logic);
    tf(out, tx.importing ? "import-v1" : "transition"); return out;
}
TrustTransaction transaction_parse(std::string_view bytes) {
    TrustReader r{bytes}; TrustTransaction tx;
    try {
        tx.old_state = parse_runtime_trust_state_v2(r.field()); tx.next_state = parse_runtime_trust_state_v2(r.field());
        tx.old_trust = r.field(); tx.next_trust = r.field();
        if (r.field() != sha256(tx.old_trust) || r.field() != sha256(tx.next_trust)) invalid();
        tx.old_data = r.field(); tx.next_data = r.field(); tx.old_logic = r.field(); tx.next_logic = r.field();
        const auto kind = r.field();
        if (kind != "import-v1" && kind != "transition") invalid();
        tx.importing = kind == "import-v1";
        if (r.pos != bytes.size() || !tx.next_state.active) invalid();
        validate_registry_transition(tx.old_data, tx.next_data, false);
        validate_registry_transition(tx.old_logic, tx.next_logic, true);
        check_trust(tx.old_trust); check_trust(tx.next_trust);
        if (tx.next_state.active->trust_sha256 != sha256(tx.next_trust) ||
            (tx.old_state.active && tx.old_state.active->trust_sha256 != sha256(tx.old_trust))) invalid();
        if (tx.importing != !tx.old_state.active || (tx.importing &&
            (tx.next_state.active->generation != "00000000000000000001" || tx.old_trust != tx.next_trust))) invalid();
    } catch (const TornTrustRecord&) { invalid(); }
    return tx;
}
struct CoordinatorLog {
    RuntimeTrustStateV2 state;
    std::string chain = sha256(coordinator_header);
    std::optional<TrustTransaction> pending;
    bool committed = false, projected = false;
    bool has_committed = false;
    std::map<std::string, std::string> committed_generations;
};
void remember_committed_generations(CoordinatorLog& log) {
    for (const auto* state : {&log.pending->old_state, &log.pending->next_state})
        for (const auto* ref : {&state->active, &state->prepared, &state->previous}) if (*ref) {
            const auto [found, inserted] = log.committed_generations.emplace((*ref)->generation, (*ref)->trust_sha256);
            if (!inserted && found->second != (*ref)->trust_sha256) invalid();
        }
}
struct CoordinatorRecord { std::string bytes, hash; };
CoordinatorRecord coordinator_record(std::string_view previous, std::string_view type, std::string_view payload) {
    if (payload.size() > trust_field_limit) invalid();
    std::string bytes; tf(bytes, type); tf(bytes, previous); tf(bytes, payload);
    const auto hash = sha256(bytes); tf(bytes, hash); bytes += '\n';
    return {std::move(bytes), hash};
}
void require_capacity(std::size_t used, std::size_t needed) {
    if (used > trust_journal_limit || needed > trust_journal_limit - used)
        throw Error("RuntimeTrustJournalCapacity");
}
std::size_t trust_file_size(const std::string& path) {
    TrustFd file(::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOATIME));
    check_private_node(file.fd, false); struct stat st{};
    if (::fstat(file.fd, &st) != 0 || st.st_size < 0 || static_cast<std::uint64_t>(st.st_size) > trust_journal_limit) invalid();
    return static_cast<std::size_t>(st.st_size);
}
void reserve_transaction(const std::string& root, const CoordinatorLog& log, const TrustTransaction& tx, bool existing = true) {
    const auto payload = transaction_bytes(tx), digest = sha256(payload);
    const auto prepare = coordinator_record(log.chain, "Prepare", payload);
    const auto commit = coordinator_record(prepare.hash, "Commit", digest);
    const auto projected = coordinator_record(commit.hash, "Projected", digest);
    const auto used = existing ? trust_file_size(root + "/trust/journal.v1") : coordinator_header.size();
    require_capacity(used, prepare.bytes.size() + commit.bytes.size() + projected.bytes.size());
}
void log_record(const std::string& root, CoordinatorLog& log, std::string_view type, std::string_view payload) {
    const auto record = coordinator_record(log.chain, type, payload);
    trust_append(root + "/trust/journal.v1", record.bytes); log.chain = record.hash;
}
CoordinatorLog read_log(const std::string& root, bool read_only, bool inspect_torn_tail = false) {
    const auto path = root + "/trust/journal.v1";
    const auto bytes = trust_read(path, trust_journal_limit);
    if (!bytes.starts_with(coordinator_header)) invalid();
    CoordinatorLog log; TrustReader r{bytes, coordinator_header.size()};
    while (r.pos < bytes.size()) {
        const auto start = r.pos;
        try {
            const auto type = r.field(), previous = r.field(), payload = r.field();
            const auto unsigned_end = r.pos;
            const auto hash = r.field();
            if (r.pos == bytes.size()) throw TornTrustRecord{};
            if (bytes[r.pos++] != '\n' || previous != log.chain || hash != sha256(std::string_view(bytes).substr(start, unsigned_end - start))) invalid();
            if (type == "Prepare") {
                if (log.pending && log.committed && !log.projected) invalid();
                auto tx = transaction_parse(payload);
                if (tx.old_state != log.state) invalid();
                log.pending = std::move(tx); log.committed = false; log.projected = false;
            } else if (type == "Commit") {
                if (!log.pending || log.committed || payload != sha256(transaction_bytes(*log.pending))) invalid();
                log.committed = true; log.has_committed = true; log.state = log.pending->next_state;
                remember_committed_generations(log);
            } else if (type == "Projected") {
                if (!log.pending || !log.committed || log.projected || payload != sha256(transaction_bytes(*log.pending))) invalid();
                log.projected = true;
            } else invalid();
            log.chain = hash;
        } catch (const TornTrustRecord&) {
            // Admission may inspect the complete committed prefix without
            // repairing it. Only a fully validated operation may recover later.
            if (inspect_torn_tail) break;
            if (read_only) throw Error("RuntimeTrustRecoveryRequired");
            TrustFd fd(::open(path.c_str(), O_WRONLY | O_NOFOLLOW | O_CLOEXEC)); check_private_node(fd.fd, false);
            if (::ftruncate(fd.fd, static_cast<off_t>(start)) != 0 || ::fsync(fd.fd) != 0) throw Error("RuntimeTrustRecoveryFailed");
            break;
        }
    }
    return log;
}
RuntimeTrustGenerationV2 read_generation(const std::string& root, const RuntimeTrustReferenceV2& ref) {
    check_reference(ref);
    check_private_directory(root + "/trust/generations", false);
    const auto dir = root + "/trust/generations/" + ref.generation;
    check_private_directory(dir, false);
    auto result = parse_runtime_trust_metadata_v2(trust_read(dir + "/metadata.json", max_metadata_bytes), trust_read(dir + "/truststore.pub", 65536));
    if (result.generation != ref.generation || result.trust_sha256 != ref.trust_sha256) invalid();
    return result;
}
void validate_committed_generations(const std::string& root, const CoordinatorLog& log) {
    for (const auto& [generation, digest] : log.committed_generations)
        (void)read_generation(root, {generation, digest});
}
void write_generation(const std::string& root, const RuntimeTrustGenerationV2& generation) {
    const auto parent = root + "/trust/generations";
    const auto dir = parent + '/' + generation.generation;
    // Never overwrite or adopt an orphaned generation: collisions fail closed.
    if (trust_exists(dir)) throw Error("RuntimeTrustGenerationCollision");
    check_private_directory(dir, true);
    LocalFiles::publish_new_atomically(dir + "/truststore.pub", generation.trust_bytes);
    LocalFiles::publish_new_atomically(dir + "/metadata.json", render_runtime_trust_metadata_v2(generation));
    trust_sync(dir); trust_sync(parent); test_failpoint("trust.after_generation");
}
std::vector<RuntimeTrustGenerationV2> generations(const std::string& root) {
    check_private_directory(root + "/trust/generations", false);
    std::vector<RuntimeTrustGenerationV2> out;
    for (const auto& entry : std::filesystem::directory_iterator(root + "/trust/generations")) {
        const auto id = entry.path().filename().string();
        if (!generation_valid(id)) invalid();
        check_private_directory(entry.path().string(), false);
        const auto bytes = trust_read(entry.path().string() + "/truststore.pub", 65536);
        auto g = parse_runtime_trust_metadata_v2(trust_read(entry.path().string() + "/metadata.json", max_metadata_bytes), bytes);
        if (g.generation != id) invalid();
        out.push_back(std::move(g));
        if (out.size() > max_trust_generations) invalid();
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.generation < b.generation; });
    return out;
}
void validate_references(const std::string& root, const RuntimeTrustStateV2& state) {
    for (const auto* ref : {&state.active, &state.prepared, &state.previous}) if (*ref) (void)read_generation(root, **ref);
}
std::string registry_record(const std::string& previous, const std::string& snapshot) {
    std::string unsigned_record;
    tf(unsigned_record, previous); tf(unsigned_record, snapshot);
    auto record = unsigned_record;
    tf(record, sha256(unsigned_record)); record += '\n';
    return record;
}
std::string registry_chain(const std::string& journal, std::string_view header) {
    if (!journal.starts_with(header)) invalid();
    std::string chain = sha256(header);
    TrustReader reader{journal, header.size()};
    while (reader.pos < journal.size()) {
        if (std::string_view(journal).substr(reader.pos).starts_with(runtime_trust_store_detail::migration_barrier)) {
            reader.pos += runtime_trust_store_detail::migration_barrier.size();
            continue;
        }
        try {
            const auto previous = reader.field();
            const auto snapshot = reader.field();
            const auto hash = reader.field();
            if (reader.pos == journal.size() || journal[reader.pos++] != '\n') invalid();
            std::string unsigned_record; tf(unsigned_record, previous); tf(unsigned_record, snapshot);
            if (previous != chain || hash != sha256(unsigned_record)) invalid();
            chain = hash;
        } catch (const TornTrustRecord&) { invalid(); }
    }
    return chain;
}
void reserve_registry_projection(const std::string& root, const std::string& journal_name,
                                 std::string_view header, const std::string& old_snapshot,
                                 const std::string& next_snapshot) {
    if (old_snapshot == next_snapshot) return;
    const auto journal = trust_read(root + '/' + journal_name, trust_journal_limit);
    const auto record = registry_record(registry_chain(journal, header), next_snapshot);
    require_capacity(journal.size(), record.size());
}
void project_registry(const std::string& root, const std::string& journal_name,
                      const std::string& snapshot_name, std::string_view header,
                      const std::string& old_snapshot, const std::string& next_snapshot,
                      std::string_view journal_failpoint, std::string_view snapshot_failpoint) {
    if (old_snapshot == next_snapshot) return;
    const auto current = snapshot_name == "registry.v1"
        ? runtime_trust_store_detail::data_snapshot_locked(root, false)
        : runtime_trust_store_detail::logic_snapshot_locked(root, false);
    if (current == next_snapshot) return;
    if (current != old_snapshot) invalid();
    const auto journal = trust_read(root + '/' + journal_name, trust_journal_limit);
    const auto record = registry_record(registry_chain(journal, header), next_snapshot);
    require_capacity(journal.size(), record.size());
    trust_append(root + '/' + journal_name, record);
    test_failpoint(journal_failpoint);
    trust_replace(root + '/' + snapshot_name, next_snapshot);
    test_failpoint(snapshot_failpoint);
}
std::size_t torn_barrier_bytes(std::string_view bytes) {
    const auto barrier = runtime_trust_store_detail::migration_barrier;
    for (std::size_t n = barrier.size() - 1; n > 0; --n)
        if (bytes.ends_with(barrier.substr(0, n))) return n;
    return 0;
}
void reserve_barriers(const std::string& root) {
    const auto barrier = runtime_trust_store_detail::migration_barrier;
    for (const auto& name : {"journal.v1", "logic-journal.v1"}) {
        const auto path = root + '/' + name;
        const auto bytes = trust_exists(path) ? trust_read(path, trust_journal_limit) :
            std::string(name == std::string_view("journal.v1") ? "BRUCE_RUNTIME_MOD_JOURNAL_V1\n" : "BRUCE_LOGIC_MOD_JOURNAL_V1\n");
        if (bytes.find(barrier) == std::string::npos)
            require_capacity(bytes.size() - torn_barrier_bytes(bytes), barrier.size());
    }
}
void ensure_barrier(const std::string& root, const std::string& name) {
    const auto path = root + '/' + name;
    if (!trust_exists(path)) {
        if (name == "journal.v1") {
            check_private_directory(root + "/packages", true);
            LocalFiles::publish_new_atomically(path, "BRUCE_RUNTIME_MOD_JOURNAL_V1\n");
        } else {
            check_private_directory(root + "/logic-packages", true);
            check_private_directory(root + "/logic-workers", true);
            LocalFiles::publish_new_atomically(path, "BRUCE_LOGIC_MOD_JOURNAL_V1\n");
        }
        trust_sync(root);
    }
    const auto bytes = trust_read(path, trust_journal_limit);
    const auto barrier = runtime_trust_store_detail::migration_barrier;
    if (bytes.find(barrier) != std::string::npos) return;
    // Only migration writes occur after the legacy snapshot was captured and
    // while the sentinel fences both readers. Repair a torn barrier suffix,
    // never an arbitrary malformed journal record.
    const auto n = torn_barrier_bytes(bytes);
    require_capacity(bytes.size() - n, barrier.size());
    if (n != 0) {
        TrustFd fd(::open(path.c_str(), O_WRONLY | O_NOFOLLOW | O_CLOEXEC)); check_private_node(fd.fd, false);
        if (::ftruncate(fd.fd, static_cast<off_t>(bytes.size() - n)) != 0 || ::fsync(fd.fd) != 0) throw Error("RuntimeTrustRecoveryFailed");
    }
    trust_append(path, barrier);
}
void barriers(const std::string& root) {
    reserve_barriers(root);
    ensure_barrier(root, "journal.v1"); test_failpoint("trust.after_data_barrier");
    ensure_barrier(root, "logic-journal.v1"); test_failpoint("trust.after_logic_barrier");
}
bool later_generation_evidence(const std::string& root) {
    const auto path = root + "/trust/generations";
    if (!trust_exists(path)) return false;
    check_private_directory(path, false);
    for (const auto& entry : std::filesystem::directory_iterator(path))
        if (entry.path().filename() != "00000000000000000001") return true;
    return false;
}
bool established_migration_evidence(const std::string& root) {
    return trust_exists(root + "/trust/state.json") || later_generation_evidence(root);
}
void project(const std::string& root, CoordinatorLog& log) {
    const auto& tx = *log.pending;
    validate_references(root, tx.next_state);
    // Recovery may only finish the transaction's own old/new projections. A
    // later live state cannot be overwritten by a truncated historical prefix.
    if (tx.importing && later_generation_evidence(root)) invalid();
    if (trust_exists(root + "/trust/state.json")) {
        const auto state = trust_read(root + "/trust/state.json", max_state_bytes);
        if (state != render_runtime_trust_state_v2(tx.old_state) &&
            state != render_runtime_trust_state_v2(tx.next_state)) invalid();
    }
    const auto compatibility = trust_read(root + "/truststore.pub", 65536);
    if (compatibility != tx.old_trust && compatibility != tx.next_trust &&
        !(tx.importing && compatibility == runtime_trust_store_detail::migration_fence)) invalid();
    const auto data = runtime_trust_store_detail::data_snapshot_locked(root, false);
    const auto logic = runtime_trust_store_detail::logic_snapshot_locked(root, false);
    if (data != tx.old_data && data != tx.next_data) invalid();
    if (logic != tx.old_logic && logic != tx.next_logic) invalid();
    if (tx.importing) barriers(root);
    project_registry(root, "journal.v1", "registry.v1", "BRUCE_RUNTIME_MOD_JOURNAL_V1\n",
                     tx.old_data, tx.next_data, "trust.after_data_journal",
                     "trust.after_data_projection");
    project_registry(root, "logic-journal.v1", "logic-registry.v1",
                     "BRUCE_LOGIC_MOD_JOURNAL_V1\n", tx.old_logic, tx.next_logic,
                     "trust.after_logic_journal", "trust.after_logic_projection");
    trust_replace(root + "/trust/state.json", render_runtime_trust_state_v2(tx.next_state));
    trust_replace(root + "/truststore.pub", tx.next_trust);
    test_failpoint("trust.after_projection");
    log_record(root, log, "Projected", sha256(transaction_bytes(tx)));
    log.projected = true;
}
bool migration_evidence(const std::string& root) {
    if (established_migration_evidence(root) ||
        trust_read(root + "/truststore.pub", 65536) == runtime_trust_store_detail::migration_fence) return true;
    for (const auto& name : {"journal.v1", "logic-journal.v1"}) {
        const auto path = root + '/' + name;
        if (trust_exists(path) && trust_read(path, trust_journal_limit).find(runtime_trust_store_detail::migration_barrier) != std::string::npos) return true;
    }
    return false;
}
std::string validated_pre_fence_trust(const std::string& root, const CoordinatorLog* log = nullptr) {
    check_private_directory(root + "/trust", false);
    for (const auto& entry : std::filesystem::directory_iterator(root + "/trust")) {
        const auto name = entry.path().filename();
        if (name != "generations" && !(log && (name == "journal.v1" || name == "pending-state.json"))) invalid();
    }
    const auto legacy = trust_read(root + "/truststore.pub", 65536);
    check_trust(legacy);
    if (log) {
        if (log->has_committed || log->state != RuntimeTrustStateV2{}) invalid();
        if (log->pending) {
            const auto& tx = *log->pending;
            const RuntimeTrustStateV2 initial{std::nullopt,
                RuntimeTrustReferenceV2{"00000000000000000001", sha256(legacy)}, std::nullopt};
            if (!tx.importing || tx.old_state != RuntimeTrustStateV2{} ||
                tx.next_state != initial || tx.old_trust != legacy || tx.next_trust != legacy ||
                tx.old_data != tx.next_data || tx.old_logic != tx.next_logic) invalid();
            validate_references(root, tx.next_state);
        }
        if (trust_exists(root + "/trust/pending-state.json") &&
            (!log->pending || trust_read(root + "/trust/pending-state.json", max_state_bytes) !=
                render_runtime_trust_state_v2(log->pending->next_state))) invalid();
    }
    if (trust_exists(root + "/trust/generations")) {
        const auto orphaned = generations(root);
        if (orphaned.size() > 1 || (!orphaned.empty() &&
            (orphaned.front().generation != "00000000000000000001" ||
             orphaned.front().trust_bytes != legacy))) invalid();
    }
    return legacy;
}
CoordinatorLog recover_coordinator(const std::string& root, bool read_only, bool inspect_torn_tail = false) {
    check_private_directory(root + "/trust", false);
    auto log = read_log(root, read_only, inspect_torn_tail);
    // Earlier committed references retain authority after they leave the live
    // active/prepared/previous triple. Self-consistent replacement metadata is
    // not a substitute for the identity bound by that committed journal.
    validate_committed_generations(root, log);
    // A live snapshot or a generation beyond the initial import proves this
    // cannot be a genuine pre-Commit import, even if its sentinel was restored.
    if (!log.has_committed && established_migration_evidence(root)) invalid();
    const auto trust = trust_read(root + "/truststore.pub", 65536);
    if (log.pending && log.pending->importing && !log.committed && trust == runtime_trust_store_detail::migration_fence) {
        if (read_only) throw Error("RuntimeTrustRecoveryRequired");
        validate_references(root, log.pending->next_state);
        // The single rootwide fence is irreversible. Both barriers precede Commit.
        barriers(root);
        log_record(root, log, "Commit", sha256(transaction_bytes(*log.pending)));
        log.committed = true; log.has_committed = true; log.state = log.pending->next_state;
        remember_committed_generations(log);
    }
    if (log.committed && !log.projected) {
        if (read_only) throw Error("RuntimeTrustRecoveryRequired");
        project(root, log);
    }
    if (log.has_committed) {
        validate_references(root, log.state);
        if (parse_runtime_trust_state_v2(trust_read(root + "/trust/state.json", max_state_bytes)) != log.state ||
            trust_read(root + "/truststore.pub", 65536) != read_generation(root, *log.state.active).trust_bytes) invalid();
        for (const auto& name : {"journal.v1", "logic-journal.v1"})
            if (trust_read(root + '/' + name, trust_journal_limit).find(runtime_trust_store_detail::migration_barrier) == std::string::npos) invalid();
    } else {
        if (migration_evidence(root)) invalid();
        (void)validated_pre_fence_trust(root, &log);
    }
    return log;
}
TrustTransaction new_transaction(const std::string& root, const RuntimeTrustStateV2& state,
                                const RuntimeTrustStateV2& next, std::string old_trust,
                                std::string next_trust, bool importing = false,
                                std::optional<std::string> next_data = std::nullopt,
                                std::optional<std::string> next_logic = std::nullopt) {
    const auto data = runtime_trust_store_detail::data_snapshot_locked(root, false);
    const auto logic = runtime_trust_store_detail::logic_snapshot_locked(root, false);
    return {state, next, std::move(old_trust), std::move(next_trust), data,
        next_data ? std::move(*next_data) : data, logic,
        next_logic ? std::move(*next_logic) : logic, importing};
}
void transact(const std::string& root, CoordinatorLog& log, const TrustTransaction& tx) {
    // registry.lock reserves this exact budget against every cooperating writer.
    // Admission includes Projected, not just the records before linearization.
    reserve_transaction(root, log, tx);
    if (tx.importing) reserve_barriers(root);
    reserve_registry_projection(root, "journal.v1", "BRUCE_RUNTIME_MOD_JOURNAL_V1\n",
                                tx.old_data, tx.next_data);
    reserve_registry_projection(root, "logic-journal.v1", "BRUCE_LOGIC_MOD_JOURNAL_V1\n",
                                tx.old_logic, tx.next_logic);
    log_record(root, log, "Prepare", transaction_bytes(tx)); log.pending = tx; log.committed = false; log.projected = false;
    test_failpoint("trust.after_prepare");
    // Staging a snapshot is not publishing state.json. Commit remains authority.
    trust_replace(root + "/trust/pending-state.json", render_runtime_trust_state_v2(tx.next_state));
    test_failpoint("trust.after_snapshot");
    if (tx.importing) {
        trust_replace(root + "/truststore.pub", runtime_trust_store_detail::migration_fence);
        test_failpoint("trust.after_fence"); barriers(root);
    }
    log_record(root, log, "Commit", sha256(transaction_bytes(tx))); log.committed = true; log.has_committed = true; log.state = tx.next_state;
    remember_committed_generations(log);
    test_failpoint("trust.after_commit"); project(root, log);
}
RuntimeTrustVerificationV2 verify_locked(const std::string& root, const CoordinatorLog& log) {
    if (!log.has_committed) throw Error("RuntimeTrustNotImported");
    const auto history = generations(root);
    auto installed = runtime_trust_store_detail::data_packages_locked(root, history);
    auto logic = runtime_trust_store_detail::logic_packages_locked(root, history);
    installed.insert(installed.end(), logic.begin(), logic.end());
    RuntimeTrustVerificationV2 report{log.state, {}, {}};
    const auto active = read_runtime_mod_truststore_v1(read_generation(root, *log.state.active).trust_bytes);
    std::optional<RuntimeModTrustStoreV1> prepared;
    if (log.state.prepared) prepared = read_runtime_mod_truststore_v1(read_generation(root, *log.state.prepared).trust_bytes);
    for (const auto& r : installed) {
        const auto supports = [&](const RuntimeModTrustStoreV1& keys) {
            // The registry bridges report the actual key identity as ID:hex,
            // preventing changed bytes under the same ID from inheriting trust.
            const auto colon = r.signing_key_id.find(':');
            const auto id = r.signing_key_id.substr(0, colon);
            const auto key = keys.public_keys().find(id);
            return colon != std::string::npos && key != keys.public_keys().end() && key->second == r.signing_key_id.substr(colon + 1);
        };
        const auto id = r.signing_key_id.substr(0, r.signing_key_id.find(':'));
        if (!supports(active)) report.active_incompatible.push_back({r.kind, r.mod_id, r.version, id});
        if (prepared && !supports(*prepared)) report.prepared_incompatible.push_back({r.kind, r.mod_id, r.version, id});
    }
    return report;
}
runtime_trust_store_detail::TransitionPlanV2 transition_plan(
    const std::string& root, const CoordinatorLog& log, const bool rollback) {
    if (!log.has_committed) throw Error("RuntimeTrustNotImported");
    const auto target = rollback ? log.state.previous : log.state.prepared;
    if (!target) throw Error("RuntimeTrustTransitionMissing");
    const auto history = generations(root);
    auto installed = runtime_trust_store_detail::data_packages_locked(root, history);
    auto logic = runtime_trust_store_detail::logic_packages_locked(root, history);
    installed.insert(installed.end(), logic.begin(), logic.end());
    const auto candidate = read_generation(root, *target);
    const auto keys = read_runtime_mod_truststore_v1(candidate.trust_bytes);
    std::vector<RuntimeAffectedModV2> affected;
    for (const auto& record : installed) if (record.active) {
        const auto colon = record.signing_key_id.find(':');
        const auto id = record.signing_key_id.substr(0, colon);
        const auto found = keys.public_keys().find(id);
        if (colon == std::string::npos || found == keys.public_keys().end() ||
            found->second != record.signing_key_id.substr(colon + 1))
            affected.push_back({record.kind, record.mod_id, record.version, id});
    }
    std::sort(affected.begin(), affected.end(), [](const auto& left, const auto& right) {
        return std::tie(left.kind, left.mod_id, left.version, left.signing_key_id) <
            std::tie(right.kind, right.mod_id, right.version, right.signing_key_id);
    });
    return {{std::nullopt, target, log.state.active}, candidate.trust_bytes, std::move(affected)};
}
void transition(const std::string& root, bool rollback) {
    CoordinatorScope scope(root); auto log = recover_coordinator(root, false);
    const auto plan = transition_plan(root, log, rollback);
    if (!plan.affected.empty()) throw Error("RuntimeTrustActiveModsIncompatible");
    const auto current = read_generation(root, *log.state.active);
    transact(root, log, new_transaction(root, log.state, plan.next_state,
                                        current.trust_bytes, plan.candidate_trust_bytes));
}
} // namespace

namespace runtime_trust_store_detail {
bool coordinator_required_locked(const std::string& root) {
    if (!trust_exists(root + "/trust")) return false;
    if (migration_evidence(root)) return true;
    if (trust_exists(root + "/trust/journal.v1")) {
        const auto log = read_log(root, true);
        if (log.has_committed) return true;
        (void)validated_pre_fence_trust(root, &log);
        return false;
    }
    (void)validated_pre_fence_trust(root);
    return false;
}
void recover_locked(const std::string& root, bool read_only) {
    if (coordinating_roots.contains(root) || !trust_exists(root + "/trust")) return;
    check_private_directory(root + "/trust", false);
    if (!trust_exists(root + "/trust/journal.v1")) {
        if (migration_evidence(root)) invalid();
        // Only the exact pre-fence import footprint is harmless. Unexpected
        // links/files or mismatched generation-1 bytes fail closed.
        (void)validated_pre_fence_trust(root);
        return;
    }
    CoordinatorScope scope(root); (void)recover_coordinator(root, read_only);
}
std::string active_trust_bytes_locked(const std::string& root) {
    if (!trust_exists(root + "/trust")) return trust_read(root + "/truststore.pub", 65536);
    if (!trust_exists(root + "/trust/journal.v1")) {
        if (migration_evidence(root)) invalid();
        return validated_pre_fence_trust(root);
    }
    CoordinatorScope scope(root);
    const auto log = recover_coordinator(root, false);
    if (!log.has_committed) return validated_pre_fence_trust(root, &log);
    if (!log.state.active) throw Error("RuntimeTrustNotImported");
    return read_generation(root, *log.state.active).trust_bytes;
}
TransitionPlanV2 plan_transition_locked(const std::string& root, const bool rollback,
                                       const bool read_only_admission) {
    CoordinatorScope scope(root);
    return transition_plan(root, recover_coordinator(root, read_only_admission), rollback);
}
RuntimeTrustVerificationV2 verify_for_host_locked(const std::string& root) {
    CoordinatorScope scope(root);
    if (!trust_exists(root + "/trust")) throw Error("RuntimeTrustNotImported");
    // A projected committed prefix can be verified entirely without writes,
    // even if its final append is torn. More complex recovery is conservative.
    const auto log = recover_coordinator(root, true, true);
    auto report = verify_locked(root, log);
    // All deterministic state/package checks passed under this same lock and
    // pinned root. Recovery now only removes the validated torn suffix.
    (void)recover_coordinator(root, false);
    return report;
}
void commit_transition_locked(const std::string& root, const bool rollback,
                              std::string next_data_snapshot,
                              std::string next_logic_snapshot) {
    CoordinatorScope scope(root);
    auto log = recover_coordinator(root, false);
    const auto plan = transition_plan(root, log, rollback);
    const auto old_data = data_snapshot_locked(root, false);
    const auto old_logic = logic_snapshot_locked(root, false);
    validate_exact_deactivation(old_data, next_data_snapshot, false, plan.affected);
    validate_exact_deactivation(old_logic, next_logic_snapshot, true, plan.affected);
    const auto current = read_generation(root, *log.state.active);
    transact(root, log, new_transaction(root, log.state, plan.next_state,
        current.trust_bytes, plan.candidate_trust_bytes, false,
        std::move(next_data_snapshot), std::move(next_logic_snapshot)));
}
} // namespace runtime_trust_store_detail

RuntimeTrustVerificationV2 RuntimeTrustStoreV2::read_only_status(const std::string& root) {
    if (root.empty()) throw Error("RuntimeModRootRequired");
    runtime_mod_store_detail::ReadOnlyRoot pinned(root);
    const auto path = pinned.anchored_path();
    runtime_mod_store_detail::RootLock lock(path, true);
    CoordinatorScope scope(path);
    if (!trust_exists(path + "/trust")) throw Error("RuntimeTrustNotImported");
    return verify_locked(path, recover_coordinator(path, true));
}
RuntimeTrustStoreV2 RuntimeTrustStoreV2::open_existing(const std::string& root) {
    if (root.empty()) throw Error("RuntimeModRootRequired");
    runtime_mod_store_detail::ReadOnlyRoot pinned(root);
    const auto path = pinned.anchored_path(); runtime_mod_store_detail::RootLock lock(path);
    CoordinatorScope scope(path); const auto log = recover_coordinator(path, false);
    if (!log.has_committed) throw Error("RuntimeTrustNotImported");
    return RuntimeTrustStoreV2(std::filesystem::absolute(root).string());
}
RuntimeTrustStoreV2 RuntimeTrustStoreV2::import_v1(const std::string& root, const RuntimeTrustClockV2& clock, std::optional<std::string> comment) {
    if (root.empty()) throw Error("RuntimeModRootRequired");
    runtime_mod_store_detail::ReadOnlyRoot pinned(root);
    const auto path = pinned.anchored_path(); runtime_mod_store_detail::RootLock lock(path);
    runtime_trust_store_detail::import_v1_locked(path, clock, std::move(comment), false);
    return RuntimeTrustStoreV2(std::filesystem::absolute(root).string());
}
void runtime_trust_store_detail::import_v1_locked(const std::string& path,
    const RuntimeTrustClockV2& clock, std::optional<std::string> comment,
    const bool read_only_admission) {
    CoordinatorScope scope(path);
    const bool retry = trust_exists(path + "/trust");
    CoordinatorLog log;
    if (retry) {
        // A committed/fenced root is never admitted as legacy, including a
        // truncated historical import prefix with surviving V2 evidence.
        if (migration_evidence(path)) throw Error("RuntimeTrustAlreadyImported");
        if (trust_exists(path + "/trust/journal.v1")) {
            log = read_log(path, true);
            if (log.has_committed) throw Error("RuntimeTrustAlreadyImported");
            (void)validated_pre_fence_trust(path, &log);
        } else (void)validated_pre_fence_trust(path);
    }
    const auto old_trust = trust_read(path + "/truststore.pub", 65536);
    auto generation = make_runtime_trust_generation_v2("00000000000000000001", old_trust, clock, std::move(comment));
    const bool generation_exists = retry && trust_exists(path + "/trust/generations/00000000000000000001");
    if (generation_exists) generation = read_generation(path, {generation.generation, generation.trust_sha256});
    // Validate both legacy snapshots before creating any V2 artifact.
    if (read_only_admission) {
        (void)data_snapshot_locked(path, true);
        (void)logic_snapshot_locked(path, true);
    }
    RuntimeTrustStateV2 next{std::nullopt, RuntimeTrustReferenceV2{generation.generation, generation.trust_sha256}, std::nullopt};
    auto tx = new_transaction(path, {}, next, old_trust, old_trust, true);
    const bool journal_exists = retry && trust_exists(path + "/trust/journal.v1");
    reserve_transaction(path, log, tx, journal_exists);
    reserve_barriers(path);
    check_private_directory(path + "/trust", true); trust_sync(path);
    check_private_directory(path + "/trust/generations", true); trust_sync(path + "/trust");
    if (!generation_exists) write_generation(path, generation);
    if (!journal_exists) LocalFiles::publish_new_atomically(path + "/trust/journal.v1", coordinator_header);
    transact(path, log, tx);
}
RuntimeTrustStateV2 RuntimeTrustStoreV2::state() const { return verify().state; }
RuntimeTrustVerificationV2 RuntimeTrustStoreV2::verify() const {
    runtime_mod_store_detail::ReadOnlyRoot pinned(root_);
    const auto path = pinned.anchored_path(); runtime_mod_store_detail::RootLock lock(path, true);
    CoordinatorScope scope(path); return verify_locked(path, recover_coordinator(path, true));
}
void RuntimeTrustStoreV2::prepare(const std::string& external, const RuntimeTrustClockV2& clock, std::optional<std::string> comment) {
    runtime_mod_store_detail::ReadOnlyRoot pinned(root_);
    const auto path = pinned.anchored_path(); runtime_mod_store_detail::RootLock lock(path);
    runtime_trust_store_detail::prepare_locked(path, external, clock, std::move(comment), false);
}
void runtime_trust_store_detail::prepare_locked(const std::string& path,
    const std::string& external, const RuntimeTrustClockV2& clock,
    std::optional<std::string> comment, const bool read_only_admission) {
    CoordinatorScope scope(path); auto log = recover_coordinator(path, read_only_admission);
    if (!log.has_committed) throw Error("RuntimeTrustNotImported");
    if (read_only_admission) (void)verify_locked(path, log);
    if (external.empty() || external.find('\0') != std::string::npos) invalid();
    const auto file = std::filesystem::absolute(external);
    runtime_mod_store_detail::ReadOnlyRoot external_parent(file.parent_path().string());
    const auto bytes = trust_read(external_parent.anchored_path() + '/' + file.filename().string(), 65536);
    const auto history = generations(path);
    if (history.size() >= max_trust_generations) throw Error("RuntimeTrustGenerationLimit");
    std::string id = history.empty() ? "00000000000000000000" : history.back().generation;
    auto pos = id.size();
    while (pos && id[pos - 1] == '9') id[--pos] = '0';
    if (!pos) throw Error("RuntimeTrustGenerationLimit");
    ++id[pos - 1];
    const auto generation = make_runtime_trust_generation_v2(id, bytes, clock, std::move(comment));
    auto next = log.state; next.prepared = RuntimeTrustReferenceV2{id, generation.trust_sha256};
    const auto active = read_generation(path, *log.state.active);
    const auto tx = new_transaction(path, log.state, next, active.trust_bytes, active.trust_bytes);
    reserve_transaction(path, log, tx);
    write_generation(path, generation);
    transact(path, log, tx);
}
void RuntimeTrustStoreV2::activate() {
    runtime_mod_store_detail::ReadOnlyRoot pinned(root_); const auto path = pinned.anchored_path();
    runtime_mod_store_detail::RootLock lock(path); transition(path, false);
}
void RuntimeTrustStoreV2::rollback() {
    runtime_mod_store_detail::ReadOnlyRoot pinned(root_); const auto path = pinned.anchored_path();
    runtime_mod_store_detail::RootLock lock(path); transition(path, true);
}
} // namespace bruce
