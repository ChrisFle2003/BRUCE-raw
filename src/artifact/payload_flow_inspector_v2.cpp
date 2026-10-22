#include "bruce/artifact/payload_flow_inspector_v2.hpp"

#include "bruce/artifact/payload_flow_v2.hpp"
#include "bruce/artifact/v5_manifest_id_v1.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <charconv>
#include <cctype>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <memory>
#include <optional>
#include <openssl/evp.h>
#include <set>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>

namespace bruce::artifact_v1 {
namespace {

std::string debug_stage; [[noreturn]] void fail() { if (std::getenv("BRUCE_DEBUG_FLOW")) std::fprintf(stderr, "FLOWFAIL %s\n", debug_stage.c_str()); throw Error("PROTOCOL_ERROR"); }
void ensure(bool value) { if (!value) fail(); }

struct Json {
    enum class Kind { string, number, boolean, null_value, object, array } kind{};
    std::string text;
    std::uint64_t number{};
    bool boolean{};
    std::vector<std::pair<std::string, Json>> object;
    std::vector<Json> array;
};

class Parser {
public:
    explicit Parser(std::string_view value) : source_(value) {}
    Json parse() { auto value = value_at(); ensure(cursor_ == source_.size()); return value; }
private:
    std::string string_at() {
        ensure(cursor_ < source_.size() && source_[cursor_++] == '"');
        std::string value;
        while (cursor_ < source_.size()) {
            const auto c = source_[cursor_++];
            if (c == '"') return value;
            ensure(static_cast<unsigned char>(c) >= 0x20 && static_cast<unsigned char>(c) < 0x7f && c != '\\');
            value.push_back(c);
        }
        fail();
    }
    std::uint64_t number_at() {
        const auto begin = cursor_;
        while (cursor_ < source_.size() && source_[cursor_] >= '0' && source_[cursor_] <= '9') ++cursor_;
        ensure(begin != cursor_ && !(cursor_ - begin > 1 && source_[begin] == '0'));
        std::uint64_t value{};
        const auto result = std::from_chars(source_.data() + begin, source_.data() + cursor_, value);
        ensure(result.ec == std::errc{} && result.ptr == source_.data() + cursor_);
        return value;
    }
    Json value_at() {
        ensure(cursor_ < source_.size());
        if (source_[cursor_] == '"') { Json value; value.kind = Json::Kind::string; value.text = string_at(); return value; }
        if (source_[cursor_] == '{') {
            ensure(++depth_ <= 64); ++cursor_; Json value; value.kind = Json::Kind::object;
            if (cursor_ < source_.size() && source_[cursor_] == '}') { ++cursor_; --depth_; return value; }
            while (true) {
                ensure(cursor_ < source_.size() && source_[cursor_] == '"');
                auto key = string_at(); ensure(value.object.size() < 64);
                ensure(std::none_of(value.object.begin(), value.object.end(), [&](const auto& pair) { return pair.first == key; }));
                ensure(cursor_ < source_.size() && source_[cursor_++] == ':');
                value.object.emplace_back(std::move(key), value_at());
                ensure(cursor_ < source_.size());
                if (source_[cursor_] == '}') { ++cursor_; --depth_; return value; }
                ensure(source_[cursor_++] == ',');
            }
        }
        if (source_[cursor_] == '[') {
            ensure(++depth_ <= 64); ++cursor_; Json value; value.kind = Json::Kind::array;
            if (cursor_ < source_.size() && source_[cursor_] == ']') { ++cursor_; --depth_; return value; }
            while (true) {
                ensure(value.array.size() < payload_flow_v2_max_attempts);
                value.array.push_back(value_at()); ensure(cursor_ < source_.size());
                if (source_[cursor_] == ']') { ++cursor_; --depth_; return value; }
                ensure(source_[cursor_++] == ',');
            }
        }
        if (source_.substr(cursor_, 4) == "true") { cursor_ += 4; Json value; value.kind = Json::Kind::boolean; value.boolean = true; return value; }
        if (source_.substr(cursor_, 5) == "false") { cursor_ += 5; Json value; value.kind = Json::Kind::boolean; value.boolean = false; return value; }
        if (source_.substr(cursor_, 4) == "null") { cursor_ += 4; Json value; value.kind = Json::Kind::null_value; return value; }
        Json value; value.kind = Json::Kind::number; value.number = number_at(); return value;
    }
    std::string_view source_; std::size_t cursor_{}; std::size_t depth_{};
};

const Json& field(const Json& value, std::string_view key) {
    ensure(value.kind == Json::Kind::object);
    for (const auto& pair : value.object) if (pair.first == key) return pair.second;
    fail();
}
const Json* optional(const Json& value, std::string_view key) {
    ensure(value.kind == Json::Kind::object);
    for (const auto& pair : value.object) if (pair.first == key) return &pair.second;
    return nullptr;
}
void keys(const Json& value, std::initializer_list<std::string_view> expected) {
    ensure(value.kind == Json::Kind::object && value.object.size() == expected.size());
    std::size_t index = 0; for (const auto key : expected) ensure(value.object[index++].first == key);
}
std::string str(const Json& value, std::string_view key) { const auto& item = field(value, key); ensure(item.kind == Json::Kind::string); return item.text; }
std::uint64_t num(const Json& value, std::string_view key) { const auto& item = field(value, key); ensure(item.kind == Json::Kind::number); return item.number; }
bool boolean(const Json& value, std::string_view key) { const auto& item = field(value, key); ensure(item.kind == Json::Kind::boolean); return item.boolean; }
void digest(std::string_view value) { ensure(value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); })); }
void identifier(std::string_view value) { ensure(!value.empty() && value.size() <= 128 && std::isalnum(static_cast<unsigned char>(value.front()))); for (const char c : value) ensure(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.' || c == ':'); }

struct Fd { int value{-1}; explicit Fd(int fd) : value(fd) { ensure(fd >= 0); } ~Fd() { if (value >= 0) ::close(value); } Fd(const Fd&) = delete; Fd& operator=(const Fd&) = delete; Fd(Fd&& other) noexcept : value(other.value) { other.value = -1; } };
bool same_stat(const struct stat& a, const struct stat& b) {
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_mode == b.st_mode && a.st_uid == b.st_uid &&
        a.st_nlink == b.st_nlink && a.st_size == b.st_size && a.st_mtim.tv_sec == b.st_mtim.tv_sec &&
        a.st_mtim.tv_nsec == b.st_mtim.tv_nsec && a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}
struct Snapshot { std::filesystem::path path; struct stat metadata{}; bool directory{}; std::set<std::string> members; std::string hash; };

class Reader {
public:
    Reader() : deadline_(std::chrono::steady_clock::now() + std::chrono::seconds(30)) {}
    void tick() const { ensure(std::chrono::steady_clock::now() < deadline_); }
    Fd open(const std::filesystem::path& input, bool directory) const {
        tick(); const auto path = std::filesystem::absolute(input).lexically_normal(); ensure(!path.empty());
        Fd current(::open("/", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
        std::vector<std::string> parts; for (const auto& part : path.relative_path()) parts.push_back(part.string()); ensure(!parts.empty());
        for (std::size_t index = 0; index < parts.size(); ++index) {
            const auto& name = parts[index];
            ensure(!name.empty() && name != "." && name != ".." && name.find('\0') == std::string::npos);
            int flags = O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK; if (directory || index + 1 < parts.size()) flags |= O_DIRECTORY;
            Fd next(::openat(current.value, name.c_str(), flags)); ::close(current.value); current.value = next.value; next.value = -1; tick();
        }
        return current;
    }
    struct stat stat_file(int fd, bool directory) const {
        tick(); struct stat value{}; ensure(::fstat(fd, &value) == 0 && value.st_uid == ::geteuid());
        if (directory) ensure(S_ISDIR(value.st_mode) && (value.st_mode & 0777) == 0700);
        else ensure(S_ISREG(value.st_mode) && value.st_nlink == 1 && (value.st_mode & 0777) == 0600 && value.st_size >= 0 && static_cast<std::uint64_t>(value.st_size) <= max_file_bytes);
        return value;
    }
    std::set<std::string> entries(int fd) const {
        Fd copy(::openat(fd, ".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)); DIR* raw = ::fdopendir(copy.value); ensure(raw); copy.value = -1;
        std::unique_ptr<DIR, int(*)(DIR*)> dir(raw, ::closedir); std::set<std::string> values;
        while (true) { tick(); errno = 0; auto* item = ::readdir(dir.get()); if (!item) { ensure(errno == 0); break; } std::string name = item->d_name; if (name != "." && name != "..") { ensure(values.size() < 65536); values.insert(std::move(name)); } }
        return values;
    }
    void directory(const std::filesystem::path& path) {
        auto fd = open(path, true); auto before = stat_file(fd.value, true); auto names = entries(fd.value); ensure(same_stat(before, stat_file(fd.value, true))); snapshots_.push_back({path, before, true, std::move(names), {}});
    }
    std::string file(const std::filesystem::path& path, std::uint64_t cap, bool collect = false,
                     std::optional<std::uint64_t> expected_size = {}) {
        auto fd = open(path, false); auto before = stat_file(fd.value, false); ensure(static_cast<std::uint64_t>(before.st_size) <= cap);
        if (expected_size) ensure(static_cast<std::uint64_t>(before.st_size) == *expected_size);
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free); ensure(context && EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) == 1);
        std::array<char, 65536> buffer{}; std::string bytes; if (collect) bytes.reserve(static_cast<std::size_t>(before.st_size)); std::uint64_t total{};
        while (total < static_cast<std::uint64_t>(before.st_size)) { tick(); auto count = ::read(fd.value, buffer.data(), static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), static_cast<std::uint64_t>(before.st_size) - total))); if (count < 0 && errno == EINTR) continue; ensure(count > 0); ensure(EVP_DigestUpdate(context.get(), buffer.data(), static_cast<std::size_t>(count)) == 1); if (collect) bytes.append(buffer.data(), static_cast<std::size_t>(count)); total += static_cast<std::uint64_t>(count); }
        unsigned char hash[EVP_MAX_MD_SIZE]{}; unsigned length{}; ensure(EVP_DigestFinal_ex(context.get(), hash, &length) == 1 && length == 32); std::string result; for (unsigned i = 0; i < length; ++i) { result.push_back("0123456789abcdef"[hash[i] >> 4]); result.push_back("0123456789abcdef"[hash[i] & 15]); }
        ensure(same_stat(before, stat_file(fd.value, false))); snapshots_.push_back({path, before, false, {}, result}); return collect ? bytes : result;
    }
    void finish() {
        const auto initial_count = snapshots_.size();
        for (std::size_t index = 0; index < initial_count; ++index) { const Snapshot snapshot = snapshots_[index]; tick(); if (!snapshot.directory) ensure(file(snapshot.path, static_cast<std::uint64_t>(snapshot.metadata.st_size)) == snapshot.hash); }
        for (const auto& snapshot : snapshots_) { auto fd = open(snapshot.path, snapshot.directory); ensure(same_stat(snapshot.metadata, stat_file(fd.value, snapshot.directory))); if (snapshot.directory) ensure(entries(fd.value) == snapshot.members); ensure(same_stat(snapshot.metadata, stat_file(fd.value, snapshot.directory))); }
    }
private:
    std::chrono::steady_clock::time_point deadline_; std::vector<Snapshot> snapshots_;
};

struct LogAttempt {
    std::uint64_t started{}, accepted{};
    bool exited{}, result{};
    std::string result_status;
    std::string digest;
    std::uint64_t size{};
};
struct Log {
    std::string run_id;
    std::uint64_t last_seq{};
    bool finished{};
    std::string status;
    std::map<std::string, LogAttempt> attempts;
    std::vector<std::string> started_order;
};

struct ReceiptAttempt {
    std::string id, status, failure, result;
    bool accepted{};
    std::optional<std::string> digest;
    std::uint64_t size{};
};
struct Receipt {
    std::string node, callback, request, status, failure, result;
    std::optional<std::string> attempt;
    std::optional<std::string> digest;
    std::uint64_t size{};
    std::vector<ReceiptAttempt> attempts;
};

std::optional<std::string> nullable_string(const Json& value, std::string_view key) {
    const auto& item = field(value, key);
    if (item.kind == Json::Kind::null_value) return std::nullopt;
    ensure(item.kind == Json::Kind::string);
    return item.text;
}

PayloadFlowInspectionV2 inspect(const std::filesystem::path& bundle) {
    debug_stage="report-read"; Reader reader; const auto report_bytes = reader.file(bundle, max_file_bytes, true); const auto sidecar_path = std::filesystem::path(bundle.string() + ".flow-v2.json"); const auto sidecar_bytes = reader.file(sidecar_path, max_file_bytes, true);
    debug_stage="report-parse"; ensure(!report_bytes.empty() && report_bytes.back() == '\n'); auto report = Parser(std::string_view(report_bytes).substr(0, report_bytes.size() - 1)).parse();
    const bool single = optional(report, "node_id") != nullptr;
    if (single) keys(report, {"contract","action","spec_digest","dag_id","node_id","adapter_id","request_digest","retry","workers","budget","status","failure_code","result_code","reference","payload_digest","payload_size","usage","attempts"});
    else keys(report, {"contract","action","spec_digest","dag_id","retry","workers","budget","status","receipts"});
    ensure(str(report,"contract") == "dag-scheduler-v5-payload-v13" && str(report,"action") == "payload"); digest(str(report,"spec_digest"));
    std::vector<const Json*> receipt_values;
    if (single) receipt_values.push_back(&report); else { const auto& receipts = field(report,"receipts"); ensure(receipts.kind == Json::Kind::array); for (const auto& value : receipts.array) receipt_values.push_back(&value); }
    ensure(!receipt_values.empty() && receipt_values.size() <= payload_flow_v2_max_nodes); std::vector<Receipt> parsed; parsed.reserve(receipt_values.size());
    reader.directory(bundle.string() + ".run"); reader.directory(bundle.string() + ".run/inputs"); reader.directory(bundle.string() + ".run/control");
    const auto log_path = std::filesystem::path(bundle.string() + ".run/control/run-log.jsonl"); const auto log_bytes = reader.file(log_path, max_file_bytes, true);
    debug_stage="log-parse"; Log log; // Parse the log inline so all reads share this Reader's deadline.
    ensure(!log_bytes.empty() && log_bytes.back() == '\n'); std::size_t log_cursor{}; std::uint64_t previous_seq{};
    bool saw_created = false;
    while (log_cursor < log_bytes.size()) {
        const auto end = log_bytes.find('\n', log_cursor); ensure(end != std::string::npos && end > log_cursor);
        const auto value = Parser(std::string_view(log_bytes).substr(log_cursor, end - log_cursor)).parse();
        const auto event = str(value,"event"); const auto seq = num(value,"seq");
        ensure(seq > previous_seq); previous_seq = seq;
        ensure(str(value,"schema") == "worker-run-log-v1");
        const auto run = str(value,"run_id"); identifier(run);
        if(log.run_id.empty()) log.run_id=run;
        ensure(run==log.run_id); (void)num(value,"ts_unix_ms");
        const auto* attempt = optional(value,"attempt_id");
        if (!saw_created) {
            ensure(event == "RUN_CREATED" && !attempt && seq == 1);
            keys(value,{"schema","run_id","seq","ts_unix_ms","event","status"});
            ensure(str(value,"status") == "READY");
            saw_created = true;
        } else if (event == "RUN_CREATED") {
            fail();
        } else if (event == "RUN_FINISHED") {
            ensure(!attempt && !log.finished && end + 1 == log_bytes.size());
            keys(value,{"schema","run_id","seq","ts_unix_ms","event","status"});
            log.finished=true; log.status=str(value,"status");
            ensure(log.status == "READY" || log.status == "FAILED");
        } else {
            ensure(!log.finished && attempt && attempt->kind==Json::Kind::string);
            const auto id=attempt->text; identifier(id);
            auto& record=log.attempts[id];
            if(event=="ATTEMPT_STARTED") {
                ensure(!record.started && !record.result && !record.exited && !record.accepted);
                ensure(str(value,"status")=="READY");
                record.started=seq; log.started_order.push_back(id);
            } else {
                ensure(record.started);
                if(event=="RESULT_RECEIVED") {
                    ensure(record.exited && !record.result);
                    record.result=true; record.result_status=str(value,"status");
                    const auto* digest_value = optional(value,"digest");
                    const auto* reference_value = optional(value,"reference");
                    const auto* size_value = optional(value,"size_bytes");
                    if (digest_value || reference_value || size_value) {
                        ensure(digest_value && reference_value && size_value &&
                              digest_value->kind == Json::Kind::string &&
                              reference_value->kind == Json::Kind::string &&
                              size_value->kind == Json::Kind::number);
                        record.digest=digest_value->text; digest(record.digest);
                        record.size=size_value->number;
                        ensure(reference_value->text=="attempts/"+id+"/output/result.bin");
                        ensure(record.result_status == "READY");
                    } else ensure(record.result_status != "READY");
                } else if(event=="ARTIFACT_ACCEPTED") {
                    ensure(record.result && !record.accepted && !record.digest.empty());
                    record.accepted=seq;
                    ensure(str(value,"status")=="READY" &&
                           str(value,"digest")==record.digest &&
                           num(value,"size_bytes")==record.size &&
                           str(value,"reference")=="attempts/"+id+"/output/result.bin");
                } else if(event=="PROCESS_EXITED") {
                    ensure(!record.exited && !record.result); record.exited=true;
                } else ensure(event=="STATUS"||event=="CANCEL_REQUESTED");
            }
        }
        log_cursor=end+1;
    }
    debug_stage="log-check";
    ensure(saw_created && previous_seq != 0);
    for(const auto& id:log.started_order) {
        const auto& record=log.attempts.at(id);
        ensure(!record.exited || record.result);
        if (record.result) ensure((record.result_status == "READY") == (record.accepted != 0));
        if (log.finished) ensure(record.exited && record.result);
    }
    debug_stage="flow-decode"; const auto flow = decode_payload_flow_v2(sidecar_bytes); ensure(flow.spec_digest == str(report,"spec_digest") && flow.report_digest == sha256(report_bytes) && flow.run_id == log.run_id); ensure(flow.nodes.size() == receipt_values.size());
    debug_stage="receipts"; std::map<std::string,std::size_t> ordinals; std::vector<Receipt> node_receipts;
    for(std::size_t index=0;index<receipt_values.size();++index){
        const auto& value=*receipt_values[index];
        debug_stage="receipt-keys";
        if(!single)keys(value,{"node_id","callback_id","request_digest","attempt_id","status","failure_code","result_code","reference","payload_digest","payload_size","usage","attempts"});
        Receipt item;
        debug_stage="receipt-node"; item.node=str(value,"node_id");
        debug_stage="receipt-callback"; item.callback=single ? str(value,"adapter_id") : str(value,"callback_id");
        debug_stage="receipt-request"; item.request=str(value,"request_digest");
        debug_stage="receipt-status"; item.status=str(value,"status"); item.failure=str(value,"failure_code"); item.result=str(value,"result_code");
        debug_stage="receipt-attempt-id";
        item.attempt=single ? std::nullopt : nullable_string(value,"attempt_id");
        item.digest=nullable_string(value,"payload_digest"); item.size=num(value,"payload_size");
        debug_stage="receipt-fields"; identifier(item.node); identifier(item.callback); digest(item.request);
        if(item.digest) digest(*item.digest);
        const auto reference=nullable_string(value,"reference");
        ensure(ordinals.emplace(item.node,index).second);
        debug_stage="receipt-input"; reader.file(bundle.string()+".run/inputs/"+item.node+".bin",max_file_bytes);
        ensure(reader.file(bundle.string()+".run/inputs/"+item.node+".bin",max_file_bytes)==item.request);
        debug_stage="receipt-history";
        const auto& history=field(value,"attempts");
        ensure(history.kind==Json::Kind::array&&history.array.size()<=3+num(report,"retry"));
        for(const auto& attempt_value:history.array){
            debug_stage="receipt-attempt";
            if(single) keys(attempt_value,{"attempt_id","status","failure_code","result_code","accepted","reference","payload_digest","payload_size","usage"});
            else keys(attempt_value,{"attempt_id","status","failure_code","result_code","accepted","payload_digest","payload_size","usage"});
            ReceiptAttempt attempt_record;
            attempt_record.id=str(attempt_value,"attempt_id"); identifier(attempt_record.id);
            attempt_record.status=str(attempt_value,"status"); attempt_record.failure=str(attempt_value,"failure_code");
            attempt_record.result=str(attempt_value,"result_code"); attempt_record.accepted=boolean(attempt_value,"accepted");
            attempt_record.digest=nullable_string(attempt_value,"payload_digest");
            attempt_record.size=num(attempt_value,"payload_size");
            if(single) ensure(nullable_string(attempt_value,"reference") ==
                              (attempt_record.accepted ? std::optional<std::string>("artifact/"+item.node) : std::nullopt));
            ensure(std::none_of(item.attempts.begin(),item.attempts.end(),[&](const auto& prior){return prior.id==attempt_record.id;}));
            item.attempts.push_back(std::move(attempt_record));
        }
        if (single && !item.attempts.empty()) item.attempt = item.attempts.back().id;
        // The report's terminal receipt is a summary of its last accepted or
        // failed state. Null attempt IDs are meaningful for BLOCKED nodes and
        // failures that never reached ATTEMPT_STARTED.
        if (item.status == "READY") {
            ensure(item.attempt && reference && reference->empty() == false &&
                   reference.value() == "artifact/" + item.node && item.digest);
            ensure(item.failure == "READY" && !item.result.empty());
            ensure(!item.attempts.empty() && item.attempts.back().id == *item.attempt);
            ensure(log.attempts.contains(*item.attempt));
            const auto& accepted = log.attempts.at(*item.attempt);
            ensure(accepted.accepted && !accepted.digest.empty() &&
                   accepted.digest == *item.digest && accepted.size == item.size);
        } else if (item.status == "BLOCKED") {
            ensure(!item.attempt && !reference && !item.digest && item.size == 0 &&
                   item.failure == "BLOCKED" && item.result == "BLOCKED" && item.attempts.empty());
        } else if (item.status == "FAILED" || item.status == "CANCELLED") {
            ensure(!reference && !item.digest && item.size == 0 &&
                   !item.result.empty() && item.failure != "READY" && item.failure != "BLOCKED");
            if (item.attempt) ensure(!item.attempts.empty() && item.attempts.back().id == *item.attempt);
        } else fail();
        if (!item.attempts.empty()) {
            const auto& terminal = item.attempts.back();
            ensure(item.attempt && *item.attempt == terminal.id);
            if (terminal.accepted) {
                ensure(item.status == "READY" && item.failure == terminal.failure &&
                       item.result == terminal.result && reference &&
                       reference.value() == "artifact/" + item.node && item.digest &&
                       terminal.digest && *item.digest == *terminal.digest &&
                       item.size == terminal.size);
            } else {
                ensure(item.status == terminal.status && item.failure == terminal.failure &&
                       item.result == terminal.result && !reference && !item.digest &&
                       item.size == 0);
            }
        }
        node_receipts.push_back(std::move(item));
    }
    if (log.finished) ensure(str(report,"status") == log.status);
    debug_stage="graph"; ensure(flow.nodes.size()==node_receipts.size()); for(std::size_t i=0;i<flow.nodes.size();++i){ensure(flow.nodes[i].node_id==node_receipts[i].node&&flow.nodes[i].callback_id==node_receipts[i].callback&&flow.nodes[i].request_digest==node_receipts[i].request);for(const auto& dependency:flow.nodes[i].dependencies)ensure(ordinals.contains(dependency));}
    // Detect dependency cycles before accepting producer-before-consumer claims.
    std::function<void(std::size_t,std::set<std::size_t>&,std::set<std::size_t>&)> visit=[&](std::size_t node,std::set<std::size_t>& active,std::set<std::size_t>& done){ensure(!active.contains(node));if(done.contains(node))return;active.insert(node);for(const auto& dependency:flow.nodes[node].dependencies)visit(ordinals.at(dependency),active,done);active.erase(node);done.insert(node);}; std::set<std::size_t> active,done;for(std::size_t i=0;i<flow.nodes.size();++i)visit(i,active,done);
    debug_stage="attempt-completeness";
    // The sidecar order is the immutable ATTEMPT_STARTED order.  Check both
    // directions: no started log attempt may disappear from the sidecar and
    // no report/sidecar attempt may be a reserved or foreign identifier.
    ensure(flow.attempts.size() == log.started_order.size());
    for (std::size_t index = 0; index < flow.attempts.size(); ++index)
        ensure(flow.attempts[index].consumer_attempt_id == log.started_order[index]);
    std::map<std::string, std::vector<std::string>> expected_history_order;
    for (const auto& attempt : flow.attempts)
        expected_history_order[attempt.consumer_node_id].push_back(attempt.consumer_attempt_id);
    std::set<std::string> report_attempts;
    for (const auto& receipt : node_receipts) {
        const auto expected = expected_history_order[receipt.node];
        ensure(receipt.attempts.size() == expected.size());
        for (std::size_t index = 0; index < expected.size(); ++index)
            ensure(receipt.attempts[index].id == expected[index]);
        for (const auto& history : receipt.attempts) {
            ensure(report_attempts.insert(history.id).second);
            ensure(log.attempts.contains(history.id));
            const auto& record = log.attempts.at(history.id);
            ensure(record.result && history.status == record.result_status);
            ensure(history.accepted == (record.accepted != 0));
            if (history.accepted) {
                ensure(history.status == "READY" && history.digest &&
                       *history.digest == record.digest && history.size == record.size);
            } else {
                ensure(history.status != "READY" && !history.digest && history.size == 0);
            }
        }
    }
    for (const auto& id : log.started_order) ensure(report_attempts.contains(id));
    ensure(report_attempts.size() == log.started_order.size());
    debug_stage="bindings"; std::set<std::string> sidecar_attempts; std::uint64_t bindings{}; std::uint64_t verified_bytes{}; std::set<std::string> expected_flat, expected_stream_attempts;
    const auto has_history = [](const Receipt& receipt, std::string_view id) {
        return std::any_of(receipt.attempts.begin(), receipt.attempts.end(),
                           [&](const ReceiptAttempt& value) { return value.id == id; });
    };
    if (flow.stream_v15) for (const auto& attempt : flow.attempts) expected_stream_attempts.insert(attempt.consumer_attempt_id);
    for(const auto& attempt:flow.attempts){ensure(sidecar_attempts.insert(attempt.consumer_attempt_id).second);ensure(log.attempts.contains(attempt.consumer_attempt_id));const auto& consumer_log=log.attempts.at(attempt.consumer_attempt_id);ensure(consumer_log.started);const auto consumer_node=ordinals.at(attempt.consumer_node_id);ensure(has_history(node_receipts[consumer_node], attempt.consumer_attempt_id));ensure(consumer_log.started>0);const auto& node=flow.nodes[consumer_node];ensure(attempt.inputs.size()==node.dependencies.size());for(std::size_t index=0;index<attempt.inputs.size();++index){const auto& input=attempt.inputs[index];ensure(input.producer_node_id==node.dependencies[index]&&ordinals.contains(input.producer_node_id));const auto producer_id=input.producer_attempt_id;ensure(log.attempts.contains(producer_id));const auto& producer_log=log.attempts.at(producer_id);ensure(producer_log.accepted&&producer_log.started<consumer_log.started);ensure(input.manifest_id==v5_manifest_id_v1(flow.run_id,input.producer_node_id,producer_id));const auto producer_node=ordinals.at(input.producer_node_id);ensure(node_receipts[producer_node].digest && *node_receipts[producer_node].digest==input.sha256&&node_receipts[producer_node].size==input.size_bytes);const auto flat="n"+std::to_string(consumer_node)+"-"+attempt.consumer_attempt_id+"-"+std::to_string(index)+".bin";if (flow.stream_v15) {
            const auto input_root = std::filesystem::path(bundle.string() + ".run/payload-inputs") /
                attempt.consumer_attempt_id / ("input-" + std::to_string(index));
            reader.directory(input_root);
            const auto members = reader.entries(reader.open(input_root, true).value);
            ensure(members == std::set<std::string>{"input.bin", "stream-payload"});
            const auto stream_root = input_root / "stream-payload";
            reader.directory(stream_root);
            const auto header = reader.file(stream_root / "header.jsonl", 1024, true);
            const auto seal = reader.file(stream_root / "seal.json", 1024, true);
            ensure(header.starts_with("{\"stream_id\":\"payload\",\"sha256\":\""));
            const auto hpos = header.find("\",\"size_bytes\":");
            ensure(hpos != std::string::npos);
            const auto hhash = header.substr(33, 64);
            digest(hhash);
            const auto sizepos = hpos + 15;
            const auto comma = header.find(',', sizepos);
            ensure(comma != std::string::npos);
            std::uint64_t stream_size{};
            const auto parsed_size = std::from_chars(header.data() + sizepos,
                header.data() + comma, stream_size);
            ensure(parsed_size.ec == std::errc{} && parsed_size.ptr == header.data() + comma);
            ensure(stream_size == input.size_bytes);
            const auto count = stream_size / 65536 + (stream_size % 65536 != 0);
            const auto expected_header = "{\"stream_id\":\"payload\",\"sha256\":\"" + hhash +
                "\",\"size_bytes\":" + std::to_string(stream_size) +
                ",\"chunk_bytes\":65536,\"chunk_count\":" + std::to_string(count) + "}\n";
            const auto expected_seal = "{\"sha256\":\"" + hhash + "\",\"size_bytes\":" +
                std::to_string(stream_size) + ",\"chunk_bytes\":65536,\"chunk_count\":" +
                std::to_string(count) + "}";
            ensure(header == expected_header && seal == expected_seal);
            const auto chunk_dir_members = reader.entries(reader.open(stream_root, true).value);
            std::set<std::string> expected{"header.jsonl", "seal.json"};
            std::string payload;
            for (std::uint64_t chunk = 0; chunk < count; ++chunk) {
                auto digits = std::to_string(chunk);
                ensure(digits.size() <= 20);
                const auto name = "chunk-" + std::string(20 - digits.size(), '0') + digits + ".bin";
                expected.insert(name);
                const auto bytes = reader.file(stream_root / name, 65536, true);
                payload += bytes;
            }
            ensure(chunk_dir_members == expected && sha256(payload) == input.sha256 &&
                   hhash == input.sha256 &&
                   sha256(reader.file(input_root / "input.bin", max_file_bytes, true, input.size_bytes)) == input.sha256);
        } else {
            expected_flat.insert(flat);
            ensure(reader.file(std::filesystem::path(bundle.string() + ".run/payload-inputs") / flat,
                               max_file_bytes, false, input.size_bytes) == input.sha256);
        }ensure(verified_bytes<=std::numeric_limits<std::uint64_t>::max()-input.size_bytes);verified_bytes+=input.size_bytes;++bindings;}}
    debug_stage="payload-root"; const auto payload_root=std::filesystem::path(bundle.string()+".run/payload-inputs");ensure(std::filesystem::exists(payload_root));reader.directory(payload_root);if(flow.stream_v15){ensure(reader.entries(reader.open(payload_root,true).value)==expected_stream_attempts);}else{ensure(reader.entries(reader.open(payload_root,true).value)==expected_flat);}
    debug_stage="outputs"; // Every worker result and every started attempt has a private output file.
    for(const auto& pair:log.attempts){const auto& record=pair.second;const auto output=std::filesystem::path(bundle.string()+".run/attempts")/pair.first/"output"/"result.bin";reader.directory(output.parent_path());if(record.result&&record.digest.empty())continue;ensure(reader.file(output,max_file_bytes,false,record.size)==record.digest);}
    debug_stage="finish"; reader.finish(); PayloadFlowInspectionV2 result;result.status=log.finished?"VALID":"INCOMPLETE";result.run_status=log.finished?log.status:"INCOMPLETE";result.run_id=flow.run_id;result.node_count=flow.nodes.size();result.attempt_count=flow.attempts.size();result.binding_count=bindings;result.verified_input_bytes=verified_bytes;result.stream_v15=flow.stream_v15;result.input_completeness=log.finished?"VERIFIED":"NOT_PROVABLE";return result;
}

} // namespace

PayloadFlowInspectionV2 inspect_payload_flow_v2(const std::filesystem::path& bundle) {
    try { return inspect(bundle); } catch (...) { throw Error("PROTOCOL_ERROR"); }
}

} // namespace bruce::artifact_v1
