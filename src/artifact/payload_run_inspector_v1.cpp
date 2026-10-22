#include "bruce/artifact/payload_run_inspector_v1.hpp"
#include "payload_run_stream_inspector_v1.hpp"
#include "bruce/worker/worker_boundary_v1.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <memory>
#include <openssl/evp.h>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
namespace bruce::artifact_v1 {
namespace {
[[noreturn]] void fail() { throw Error("PROTOCOL_ERROR"); }
struct InspectJsonValue {
    enum class Kind { string, number, boolean, null_value, object, array } kind{};
    std::string text;
    std::uint64_t number{};
    bool boolean{};
    std::vector<std::pair<std::string, InspectJsonValue>> object;
    std::vector<InspectJsonValue> array;
};

class InspectJsonParser {
public:
    explicit InspectJsonParser(std::string_view source) : source_(source) {}
    InspectJsonValue parse() {
        auto value = value_at();
        if (cursor_ != source_.size()) throw Error("PROTOCOL_ERROR");
        return value;
    }
private:
    [[noreturn]] static void reject() { throw Error("PROTOCOL_ERROR"); }
    std::string string_at() {
        if (cursor_ >= source_.size() || source_[cursor_++] != '"') reject();
        std::string value;
        while (cursor_ < source_.size()) {
            const char c = source_[cursor_++];
            if (c == '"') return value;
            if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) >= 0x7f || c == '\\') reject();
            value.push_back(c);
        }
        reject();
    }
    std::uint64_t number_at() {
        const auto begin = cursor_;
        while (cursor_ < source_.size() && source_[cursor_] >= '0' && source_[cursor_] <= '9') ++cursor_;
        if (begin == cursor_ || (cursor_ - begin > 1 && source_[begin] == '0')) reject();
        try { return bruce::integer<std::uint64_t>(source_.substr(begin, cursor_ - begin)); }
        catch (...) { reject(); }
    }
    InspectJsonValue value_at() {
        if (cursor_ >= source_.size()) reject();
        if (source_[cursor_] == '"') {
            InspectJsonValue value;
            value.kind = InspectJsonValue::Kind::string;
            value.text = string_at();
            return value;
        }
        if (source_[cursor_] == '{') {
            if (++depth_ > max_depth) reject();
            ++cursor_;
            InspectJsonValue value; value.kind = InspectJsonValue::Kind::object;
            if (cursor_ < source_.size() && source_[cursor_] == '}') { ++cursor_; --depth_; return value; }
            while (true) {
                if (cursor_ >= source_.size() || source_[cursor_] != '"') reject();
                auto key = string_at();
                if (std::any_of(value.object.begin(), value.object.end(),
                                [&](const auto& item) { return item.first == key; })) reject();
                if (cursor_ >= source_.size() || source_[cursor_++] != ':') reject();
                if (value.object.size() >= 64) reject();
                value.object.emplace_back(std::move(key), value_at());
                if (cursor_ >= source_.size()) reject();
                if (source_[cursor_] == '}') { ++cursor_; --depth_; return value; }
                if (source_[cursor_++] != ',') reject();
            }
        }
        if (source_[cursor_] == '[') {
            if (++depth_ > max_depth) reject();
            ++cursor_;
            InspectJsonValue value; value.kind = InspectJsonValue::Kind::array;
            if (cursor_ < source_.size() && source_[cursor_] == ']') { ++cursor_; --depth_; return value; }
            while (true) {
                if (value.array.size() >= 4096) reject();
                value.array.push_back(value_at());
                if (cursor_ >= source_.size()) reject();
                if (source_[cursor_] == ']') { ++cursor_; --depth_; return value; }
                if (source_[cursor_++] != ',') reject();
            }
        }
        if (source_.substr(cursor_, 4) == "true") {
            cursor_ += 4; InspectJsonValue value; value.kind = InspectJsonValue::Kind::boolean;
            value.boolean = true; return value;
        }
        if (source_.substr(cursor_, 5) == "false") {
            cursor_ += 5; InspectJsonValue value; value.kind = InspectJsonValue::Kind::boolean;
            value.boolean = false; return value;
        }
        if (source_.substr(cursor_, 4) == "null") {
            cursor_ += 4; InspectJsonValue value; value.kind = InspectJsonValue::Kind::null_value;
            return value;
        }
        InspectJsonValue value; value.kind = InspectJsonValue::Kind::number;
        value.number = number_at(); return value;
    }
    std::string_view source_;
    std::size_t cursor_{};
    static constexpr std::size_t max_depth = 64;
    std::size_t depth_{};
};

inline const InspectJsonValue& inspect_field(const InspectJsonValue& object, std::string_view key) {
    if (object.kind != InspectJsonValue::Kind::object) throw Error("PROTOCOL_ERROR");
    for (const auto& item : object.object) if (item.first == key) return item.second;
    throw Error("PROTOCOL_ERROR");
}

inline void inspect_exact_keys(const InspectJsonValue& object,
                               std::initializer_list<std::string_view> keys) {
    if (object.kind != InspectJsonValue::Kind::object || object.object.size() != keys.size())
        throw Error("PROTOCOL_ERROR");
    std::size_t index = 0;
    for (const auto key : keys) {
        if (object.object[index].first != key) throw Error("PROTOCOL_ERROR");
        ++index;
    }
}

inline std::string inspect_string(const InspectJsonValue& value) {
    if (value.kind != InspectJsonValue::Kind::string) throw Error("PROTOCOL_ERROR");
    return value.text;
}
inline std::uint64_t inspect_number(const InspectJsonValue& value) {
    if (value.kind != InspectJsonValue::Kind::number) throw Error("PROTOCOL_ERROR");
    return value.number;
}
inline bool inspect_boolean(const InspectJsonValue& value) {
    if (value.kind != InspectJsonValue::Kind::boolean) throw Error("PROTOCOL_ERROR");
    return value.boolean;
}
inline std::optional<std::string> inspect_optional_string(const InspectJsonValue& value) {
    if (value.kind == InspectJsonValue::Kind::null_value) return std::nullopt;
    return inspect_string(value);
}

inline bool ascii_identifier_char(unsigned char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9');
}
inline void inspect_identifier(std::string_view value) {
    if (value.empty() || value.size() > 128 ||
        !ascii_identifier_char(static_cast<unsigned char>(value.front()))) throw Error("PROTOCOL_ERROR");
    for (const char c : value.substr(1))
        if (!ascii_identifier_char(static_cast<unsigned char>(c)) && c != '.' && c != '_' && c != ':' && c != '-')
            throw Error("PROTOCOL_ERROR");
}
inline void inspect_digest(std::string_view value) {
    if (value.size() != 64 || !std::all_of(value.begin(), value.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        })) throw Error("PROTOCOL_ERROR");
}
inline void inspect_public_reference(std::string_view value) {
    if (!value.starts_with("artifact/") || value.size() <= 9 || value.find('/') != 8 ||
        value.find('\\') != std::string_view::npos || value.find("..") != std::string_view::npos)
        throw Error("PROTOCOL_ERROR");
    inspect_identifier(value.substr(9));
}
inline worker_v1::WorkerUsage inspect_usage_value(const InspectJsonValue& value) {
    inspect_exact_keys(value, {"wall_ms", "input_bytes", "output_bytes", "operations", "peak_memory_bytes"});
    worker_v1::WorkerUsage usage;
    usage.wall_ms = inspect_number(value.object[0].second);
    usage.input_bytes = inspect_number(value.object[1].second);
    usage.output_bytes = inspect_number(value.object[2].second);
    usage.operations = inspect_number(value.object[3].second);
    usage.peak_memory_bytes = inspect_number(value.object[4].second);
    return usage;
}
struct InspectBudget {
    std::uint64_t max_wall_ms{};
    std::uint64_t max_input_bytes{};
    std::uint64_t max_output_bytes{};
    std::uint64_t max_operations{};
    std::uint64_t max_memory_bytes{};
};
inline InspectBudget inspect_budget_value(const InspectJsonValue& value) {
    inspect_exact_keys(value, {"max_wall_ms", "max_input_bytes", "max_output_bytes", "max_operations", "max_memory_bytes"});
    InspectBudget budget;
    budget.max_wall_ms = inspect_number(value.object[0].second);
    budget.max_input_bytes = inspect_number(value.object[1].second);
    budget.max_output_bytes = inspect_number(value.object[2].second);
    budget.max_operations = inspect_number(value.object[3].second);
    budget.max_memory_bytes = inspect_number(value.object[4].second);
    if (!budget.max_wall_ms || budget.max_wall_ms > 600000 ||
        !budget.max_input_bytes || budget.max_input_bytes > 1048576 ||
        !budget.max_output_bytes || budget.max_output_bytes > 5242880 ||
        !budget.max_operations || budget.max_operations > 10000000 ||
        !budget.max_memory_bytes || budget.max_memory_bytes > 2147483648ULL)
        throw Error("PROTOCOL_ERROR");
    return budget;
}
inline void inspect_usage_bounds(const worker_v1::WorkerUsage& usage, const InspectBudget& budget) {
    if (usage.wall_ms > budget.max_wall_ms + 1000 || usage.input_bytes > budget.max_input_bytes ||
        usage.output_bytes > budget.max_output_bytes || usage.operations > budget.max_operations ||
        usage.peak_memory_bytes > budget.max_memory_bytes)
        throw Error("PROTOCOL_ERROR");
}
inline void inspect_status(std::string_view value) {
    if (value != "READY" && value != "FAILED" && value != "BLOCKED" && value != "CANCELLED")
        throw Error("PROTOCOL_ERROR");
}
inline void inspect_failure(std::string_view value) {
    static constexpr std::array<std::string_view, 15> values{
        "NONE", "READY", "FAILED", "BLOCKED", "INVALID_REQUEST", "PROTOCOL_ERROR",
        "PROCESS_START_FAILED", "PROCESS_CRASHED", "TIMEOUT", "CANCELLED",
        "INPUT_LIMIT_EXCEEDED", "OUTPUT_LIMIT_EXCEEDED", "OPERATION_BUDGET_EXCEEDED",
        "MEMORY_LIMIT_EXCEEDED", "DIGEST_MISMATCH"};
    if (std::find(values.begin(), values.end(), value) == values.end()) throw Error("PROTOCOL_ERROR");
}
inline void inspect_result_code(std::string_view value) {
    inspect_identifier(value);
}
inline std::string_view expected_result_code(std::string_view adapter_id) {
    if (adapter_id == "literal-v1") return "LITERAL_READY";
    if (adapter_id == "append-v1") return "APPENDED_READY";
    if (adapter_id == "join-v1") return "JOINED_READY";
    throw Error("PROTOCOL_ERROR");
}
using Json = InspectJsonValue;
using Clock = std::chrono::steady_clock;
using Usage = worker_v1::WorkerUsage;
void ensure(bool condition) { if (!condition) fail(); }
const Json& field(const Json& j, std::string_view k) { return inspect_field(j,k); }
std::string str(const Json& j,std::string_view k) { return inspect_string(field(j,k)); }
std::uint64_t num(const Json& j,std::string_view k) { return inspect_number(field(j,k)); }
const Json* optional(const Json& j,std::string_view k) { for(const auto& x:j.object)if(x.first==k)return &x.second;return nullptr; }
void keys(const Json& j, const std::vector<std::string_view>& expected) {
    ensure(j.kind==Json::Kind::object && j.object.size()==expected.size());
    for(std::size_t i=0;i<expected.size();++i)ensure(j.object[i].first==expected[i]);
}
bool equal_usage(const Usage& a,const Usage& b) {
    return a.wall_ms==b.wall_ms && a.input_bytes==b.input_bytes && a.output_bytes==b.output_bytes &&
        a.operations==b.operations && a.peak_memory_bytes==b.peak_memory_bytes;
}
struct Fd {
    int value{-1}; explicit Fd(int fd):value(fd){ensure(fd>=0);} ~Fd(){if(value>=0)::close(value);}
    Fd(const Fd&)=delete; Fd& operator=(const Fd&)=delete;
    Fd(Fd&& other) noexcept:value(other.value){other.value=-1;}
};
bool same(const struct stat& a,const struct stat& b) {
    return a.st_dev==b.st_dev && a.st_ino==b.st_ino && a.st_mode==b.st_mode && a.st_uid==b.st_uid &&
        a.st_gid==b.st_gid && a.st_nlink==b.st_nlink && a.st_size==b.st_size &&
        a.st_mtim.tv_sec==b.st_mtim.tv_sec && a.st_mtim.tv_nsec==b.st_mtim.tv_nsec &&
        a.st_ctim.tv_sec==b.st_ctim.tv_sec && a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;
}
struct Snapshot { std::filesystem::path path; struct stat st{}; bool directory{}; std::string digest; std::set<std::string> entries; bool metadata_file{}; };
struct Reader {
    Clock::time_point deadline=Clock::now()+std::chrono::milliseconds(30000);
    std::vector<Snapshot> snapshots;
    void tick() const {ensure(Clock::now()<deadline);}
    Fd open(const std::filesystem::path& path,bool directory) {
        tick();ensure(!path.empty());
        Fd current(::open(path.is_absolute()?"/":".",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        std::vector<std::string> parts;
        for(const auto& component:path){const auto s=component.string();if(s=="/")continue;ensure(!s.empty()&&s!="."&&s!=".."&&s.find('\0')==std::string::npos);parts.push_back(s);}
        ensure(!parts.empty());
        for(std::size_t i=0;i<parts.size();++i){tick();int flags=O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK;if(i+1<parts.size()||directory)flags|=O_DIRECTORY;Fd next(::openat(current.value,parts[i].c_str(),flags));std::swap(current.value,next.value);}
        return current;
    }
    struct stat stat(int fd,bool directory) {tick();struct ::stat s{};ensure(::fstat(fd,&s)==0 && s.st_uid==::geteuid());if(directory)ensure(S_ISDIR(s.st_mode)&&(s.st_mode&0777)==0700);else ensure(S_ISREG(s.st_mode)&&s.st_nlink==1&&(s.st_mode&0077)==0&&s.st_size>=0&&static_cast<std::uint64_t>(s.st_size)<=max_file_bytes);return s;}
    std::set<std::string> entries(int fd) {
        tick();Fd copy(::openat(fd,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
        DIR* raw=::fdopendir(copy.value);ensure(raw);copy.value=-1;
        std::unique_ptr<DIR,int(*)(DIR*)> d(raw,::closedir);std::set<std::string> result;
        while(true){tick();errno=0;auto* e=::readdir(d.get());if(!e){ensure(errno==0);break;}std::string n=e->d_name;if(n=="."||n=="..")continue;ensure(result.size()<65536);result.insert(n);}
        return result;
    }
    void directory(const std::filesystem::path& path) {
        tick();for(const auto& s:snapshots)if(s.directory&&s.path==path)return;
        auto fd=open(path,true);auto before=stat(fd.value,true);auto names=entries(fd.value);ensure(same(before,stat(fd.value,true)));snapshots.push_back({path,before,true,{},std::move(names)});
    }
    std::string digest(int fd,std::uint64_t size,std::string* bytes=nullptr) {
        tick();auto* raw=EVP_MD_CTX_new();ensure(raw);std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> ctx(raw,EVP_MD_CTX_free);ensure(EVP_DigestInit_ex(raw,EVP_sha256(),nullptr)==1);
        std::array<char,65536> buffer{};std::uint64_t total{};if(bytes)bytes->reserve(static_cast<std::size_t>(size));
        while(total<size){tick();auto n=::read(fd,buffer.data(),static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(),size-total)));if(n<0&&errno==EINTR)continue;ensure(n>0);tick();ensure(EVP_DigestUpdate(raw,buffer.data(),static_cast<std::size_t>(n))==1);if(bytes)bytes->append(buffer.data(),static_cast<std::size_t>(n));total+=static_cast<std::uint64_t>(n);}
        tick();unsigned char hash[EVP_MAX_MD_SIZE]{};unsigned length{};ensure(EVP_DigestFinal_ex(raw,hash,&length)==1&&length==32);std::string out;for(unsigned i=0;i<length;++i){out.push_back("0123456789abcdef"[hash[i]>>4]);out.push_back("0123456789abcdef"[hash[i]&15]);}return out;
    }
    std::string file(const std::filesystem::path& path,std::uint64_t cap,std::optional<std::string> expected={},std::optional<std::uint64_t> expected_size={},bool collect=false) {
        auto fd=open(path,false);auto before=stat(fd.value,false);ensure(static_cast<std::uint64_t>(before.st_size)<=cap);if(expected_size)ensure(static_cast<std::uint64_t>(before.st_size)==*expected_size);std::string bytes;auto hash=digest(fd.value,static_cast<std::uint64_t>(before.st_size),collect?&bytes:nullptr);ensure(same(before,stat(fd.value,false)));if(expected)ensure(hash==*expected);snapshots.push_back({path,before,false,hash,{},collect});return bytes;
    }
    void finish() {
        // Potentially lengthy payload reads precede the report/log byte checks.
        // A final whole-set metadata/membership pass catches cross-file changes
        // during those reads. This is detection, not an immutable FS snapshot.
        for(bool metadata_file:{false,true})for(const auto& s:snapshots){
            if(s.directory||s.metadata_file!=metadata_file)continue;
            tick();auto fd=open(s.path,false);ensure(same(s.st,stat(fd.value,false)));
            ensure(digest(fd.value,static_cast<std::uint64_t>(s.st.st_size))==s.digest);
            ensure(same(s.st,stat(fd.value,false)));
        }
        for(const auto& s:snapshots){
            tick();auto fd=open(s.path,s.directory);ensure(same(s.st,stat(fd.value,s.directory)));
            if(s.directory)ensure(entries(fd.value)==s.entries);
            ensure(same(s.st,stat(fd.value,s.directory)));
        }
        tick();
    }
};
struct Attempt {
    std::string id,status,reference,digest;
    std::uint64_t size{};
    std::uint64_t started_seq{},result_seq{};
    bool exited{},result{},accepted{},cancelled{};
    std::string rejection_reason;
    std::optional<Usage> usage,exit_usage;
};
struct Log {
    std::string run_id,status;
    std::uint64_t last_seq{};
    bool finished{},cancelled{};
    std::map<std::string,Attempt> attempts;
    std::vector<std::string> order;
};
Log parse_log(std::string_view bytes,Reader& reader,const std::filesystem::path& root,const InspectBudget& budget) {
    ensure(!bytes.empty()&&bytes.back()=='\n');Log log;std::size_t start{};
    while(start<bytes.size()){
        reader.tick();auto end=bytes.find('\n',start);ensure(end!=std::string_view::npos&&end>start);auto j=InspectJsonParser(bytes.substr(start,end-start)).parse();
        std::vector<std::string_view> expected{"schema","run_id"};if(optional(j,"attempt_id"))expected.push_back("attempt_id");
        expected.insert(expected.end(),{"seq","ts_unix_ms","event","status"});for(auto key:{"digest","reference","size_bytes","usage"})if(optional(j,key))expected.push_back(key);keys(j,expected);
        ensure(str(j,"schema")=="worker-run-log-v1");auto run=str(j,"run_id");inspect_identifier(run);if(log.run_id.empty())log.run_id=run;ensure(log.run_id==run);
        const auto seq=num(j,"seq");ensure(seq>log.last_seq);log.last_seq=seq;(void)num(j,"ts_unix_ms");auto event=str(j,"event"),status=str(j,"status");ensure(!log.finished);
        const auto* aid=optional(j,"attempt_id");const auto* ref=optional(j,"reference");const auto* dg=optional(j,"digest");const auto* sz=optional(j,"size_bytes");
        ensure((ref!=nullptr)==(dg!=nullptr)&&(ref!=nullptr)==(sz!=nullptr));
        std::optional<Usage> usage;if(auto u=optional(j,"usage")){usage=inspect_usage_value(*u);inspect_usage_bounds(*usage,budget);}
        if(event=="RUN_CREATED"){ensure(start==0&&!aid&&!ref&&!usage&&status=="READY");}
        else {
            ensure(start!=0);
            if(event=="RUN_FINISHED"){ensure(!aid&&!ref&&!usage&&(status=="READY"||status=="FAILED"||status=="CANCELLED")&&end+1==bytes.size());log.finished=true;log.status=status;}
            else {
                ensure(aid);auto id=inspect_string(*aid);inspect_identifier(id);
                if(event=="ATTEMPT_STARTED") {ensure((status=="READY"||status=="RUNNING")&&!ref&&!usage&&!log.cancelled&&!log.attempts.contains(id)&&log.attempts.size()<256);Attempt a;a.id=id;a.started_seq=seq;log.attempts.emplace(id,std::move(a));log.order.push_back(id);reader.directory(root/"attempts");reader.directory(root/"attempts"/id);reader.directory(root/"attempts"/id/"output");}
                else {
                    auto it=log.attempts.find(id);ensure(it!=log.attempts.end());auto& a=it->second;
                    if(event=="STATUS")ensure(!a.exited&&!a.result&&!a.cancelled&&!ref&&!usage&&status=="RUNNING");
                    else if(event=="CANCEL_REQUESTED") {
                        ensure(!a.accepted&&!a.cancelled&&!ref&&!usage&&status=="CANCELLED"&&
                               (a.rejection_reason.empty()||a.rejection_reason=="TIMEOUT"));
                        a.cancelled=true;log.cancelled=true;
                        if(a.result){a.status="CANCELLED";a.reference.clear();a.digest.clear();a.size=0;}
                    }
                    else if(event=="PROCESS_EXITED"){inspect_status(status);ensure(!a.exited&&!a.result&&!ref);a.exited=true;a.status=status;a.exit_usage=usage;}
                    else if(event=="RESULT_RECEIVED"){
                        inspect_status(status);ensure(a.exited&&!a.result&&a.status==status&&usage);a.result=true;a.result_seq=seq;a.usage=usage;if(a.exit_usage)ensure(equal_usage(*usage,*a.exit_usage));
                        ensure((status=="READY")==static_cast<bool>(ref));
                        if(ref){a.reference=inspect_string(*ref);a.digest=inspect_string(*dg);inspect_digest(a.digest);a.size=inspect_number(*sz);ensure(a.reference=="attempts/"+id+"/output/result.bin"&&a.size<=budget.max_output_bytes&&usage->output_bytes==a.size);reader.file(root/a.reference,budget.max_output_bytes,a.digest,a.size);}
                    }else if(event=="RESULT_REJECTED") {
                        static constexpr std::array<std::string_view,6> reasons{
                            "OUTPUT_LIMIT_EXCEEDED","TIMEOUT","ARTIFACT_REFERENCE_INVALID",
                            "ARTIFACT_MANIFEST_INVALID","ARTIFACT_DIGEST_MISMATCH","ARTIFACT_STORE_VIOLATION"};
                        ensure(a.result&&a.status=="READY"&&!a.accepted&&!a.cancelled&&!log.cancelled&&
                               a.rejection_reason.empty()&&!ref&&!usage&&
                               std::find(reasons.begin(),reasons.end(),status)!=reasons.end());
                        a.status="FAILED";a.rejection_reason=status;a.reference.clear();a.digest.clear();a.size=0;
                    }else if(event=="ARTIFACT_ACCEPTED"){
                        ensure(a.result&&a.status=="READY"&&!a.accepted&&!a.cancelled&&!log.cancelled&&
                               a.rejection_reason.empty()&&status=="READY"&&ref&&usage&&a.usage&&equal_usage(*usage,*a.usage));ensure(inspect_string(*ref)==a.reference&&inspect_string(*dg)==a.digest&&inspect_number(*sz)==a.size);a.accepted=true;
                    }else fail();
                }
            }
        }
        start=end+1;
    }
    for(const auto& [id,a]:log.attempts){(void)id;ensure(a.result);}
    return log;
}
bool host_code(std::string_view s,bool external) {
    static constexpr std::array<std::string_view,10> codes{"ARTIFACT_REFERENCE_INVALID","ARTIFACT_MANIFEST_INVALID","ARTIFACT_DIGEST_MISMATCH","ARTIFACT_STORE_VIOLATION","INPUT_LIMIT_EXCEEDED","TIMEOUT","EXTERNAL_MANIFEST_INVALID","EXTERNAL_OBJECT_INVALID","EXTERNAL_INPUT_LIMIT_EXCEEDED","EXTERNAL_INPUT_TIMEOUT"};
    auto last=codes.begin()+(external?10:6);return std::find(codes.begin(),last,s)!=last;
}
struct Result {
    std::string status,failure,code;
    std::optional<std::string> digest,reference;
    std::uint64_t size{};
    Usage usage;
};
Result result(const Json& j,const InspectBudget& budget) {
    Result r;r.status=str(j,"status");inspect_status(r.status);r.failure=str(j,"failure_code");inspect_failure(r.failure);r.code=str(j,"result_code");inspect_result_code(r.code);r.digest=inspect_optional_string(field(j,"payload_digest"));if(r.digest)inspect_digest(*r.digest);r.size=num(j,"payload_size");ensure(r.size<=budget.max_output_bytes);r.usage=inspect_usage_value(field(j,"usage"));inspect_usage_bounds(r.usage,budget);if(auto ref=optional(j,"reference"))r.reference=inspect_optional_string(*ref);
    if(r.status!="READY")ensure(!r.digest&&!r.reference&&r.size==0);
    return r;
}
bool same_result(const Result& a,const Result& b) {return a.status==b.status&&a.failure==b.failure&&a.code==b.code&&a.digest==b.digest&&a.size==b.size&&equal_usage(a.usage,b.usage);}
bool host_failure(const Result& r,bool external) {return r.status=="FAILED"&&r.failure=="FAILED"&&host_code(r.code,external)&&!r.digest&&!r.reference&&r.size==0&&equal_usage(r.usage,Usage{});}
std::string attempt_id(std::size_t node,std::size_t attempt,bool single){if(single&&attempt==0)return "a-1";return "a-"+std::to_string(node+1)+"-"+std::to_string(attempt+1);}
void verify_worker_result(const Result& r,const Attempt& a,std::string_view adapter) {
    if(a.accepted){ensure(r.status=="READY"&&r.failure=="READY"&&r.code==expected_result_code(adapter)&&r.digest&&*r.digest==a.digest&&r.size==a.size&&a.usage&&equal_usage(r.usage,*a.usage));}
    else if(a.cancelled&&a.result&&a.status=="CANCELLED") {
        ensure(r.status=="CANCELLED"&&r.failure=="CANCELLED"&&r.code=="CANCELLED"&&a.usage&&equal_usage(r.usage,*a.usage));
    }
    else if(!a.rejection_reason.empty()) {
        const auto failure=a.rejection_reason=="TIMEOUT"?"TIMEOUT":
            (a.rejection_reason=="OUTPUT_LIMIT_EXCEEDED"?"OUTPUT_LIMIT_EXCEEDED":"FAILED");
        ensure(r.status=="FAILED"&&r.failure==failure&&r.code==a.rejection_reason&&
               !r.digest&&!r.reference&&r.size==0&&a.usage&&equal_usage(r.usage,*a.usage));
    }
    else if(a.status=="READY")ensure(host_failure(r,false));
    else {ensure(r.status==a.status&&r.failure!="NONE"&&r.failure!="READY"&&r.failure!="BLOCKED"&&r.code==r.failure&&a.usage&&equal_usage(r.usage,*a.usage));ensure((r.status=="CANCELLED")== (r.failure=="CANCELLED"));}
}
PayloadRunInspectionV1 inspect(const std::filesystem::path& bundle,const PayloadRunInspectOptionsV1& options) {
    Reader reader;const auto run_root=std::filesystem::path(bundle.string()+".run");
    auto bytes=reader.file(bundle,max_file_bytes,{}, {},true);ensure(!bytes.empty()&&bytes.back()=='\n');
    auto report=InspectJsonParser(std::string_view(bytes).substr(0,bytes.size()-1)).parse();PayloadRunInspectionV1 out;
    out.source_contract=str(report,"contract");const bool external=out.source_contract=="dag-scheduler-v5-payload-v14";ensure(external||out.source_contract=="dag-scheduler-v5-payload-v13");ensure(str(report,"action")=="payload");inspect_digest(str(report,"spec_digest"));inspect_identifier(str(report,"dag_id"));
    const bool single=optional(report,"node_id");
    std::vector<std::string_view> report_keys{"contract","action","spec_digest","dag_id"};
    if(single)report_keys.insert(report_keys.end(),{"node_id","adapter_id","request_digest"});
    report_keys.insert(report_keys.end(),{"retry","workers","budget"});if(external)report_keys.push_back("external_inputs_digest");report_keys.push_back("status");
    if(single){report_keys.insert(report_keys.end(),{"failure_code","result_code","reference","payload_digest","payload_size","usage"});if(external)report_keys.insert(report_keys.end(),{"external_input_count","external_input_bytes"});report_keys.push_back("attempts");}else report_keys.push_back("receipts");keys(report,report_keys);
    const auto retry=num(report,"retry"),workers=num(report,"workers");ensure(retry<=3&&workers>=1&&workers<=4);const auto budget=inspect_budget_value(field(report,"budget"));if(external)inspect_digest(str(report,"external_inputs_digest"));
    reader.directory(run_root);reader.directory(run_root/"inputs");reader.directory(run_root/"control");
    // No attempt directory is produced when every node fails before worker start.
    { auto rootfd=reader.open(run_root,true); struct stat st{}; reader.tick();
      if(::fstatat(rootfd.value,"attempts",&st,AT_SYMLINK_NOFOLLOW)==0)reader.directory(run_root/"attempts");
      else ensure(errno==ENOENT); }
    const auto log_bytes=reader.file(run_root/"control/run-log.jsonl",max_file_bytes,{}, {},true);auto log=parse_log(log_bytes,reader,run_root,budget);
    std::vector<const Json*> nodes;if(single)nodes.push_back(&report);else{const auto& receipts=field(report,"receipts");ensure(receipts.kind==Json::Kind::array&&!receipts.array.empty()&&receipts.array.size()<=64);for(const auto& j:receipts.array)nodes.push_back(&j);}
    std::set<std::string> node_ids,seen;bool all_ready=true;
    for(std::size_t index=0;index<nodes.size();++index){
        reader.tick();const auto& node=*nodes[index];
        if(!single){std::vector<std::string_view> k{"node_id","callback_id","request_digest","attempt_id","status","failure_code","result_code","reference","payload_digest","payload_size","usage"};if(external)k.insert(k.end(),{"external_input_count","external_input_bytes"});k.push_back("attempts");keys(node,k);}
        auto node_id=str(node,"node_id");inspect_identifier(node_id);ensure(node_ids.insert(node_id).second);auto adapter=str(node,single?"adapter_id":"callback_id");(void)expected_result_code(adapter);const auto request_digest=str(node,"request_digest");inspect_digest(request_digest);reader.file(run_root/"inputs"/(node_id+".bin"),65536,request_digest);
        const auto receipt=result(node,budget);all_ready=all_ready&&receipt.status=="READY";
        const bool control_failure=single&&log.finished&&log.status=="FAILED"&&
            receipt.status=="FAILED"&&receipt.failure=="PROTOCOL_ERROR"&&receipt.code=="PROTOCOL_ERROR";
        ensure(receipt.reference==(receipt.status=="READY"?std::optional<std::string>("artifact/"+node_id):std::nullopt));
        if(external){const auto count=num(node,"external_input_count"),size=num(node,"external_input_bytes");ensure(count<=15&&size<=budget.max_input_bytes&&(count!=0||size==0));if(!host_failure(receipt,true)&&receipt.status!="CANCELLED"&&!control_failure)ensure(size<=receipt.usage.input_bytes);}
        const auto& history=field(node,"attempts");ensure(history.kind==Json::Kind::array&&history.array.size()<=retry+1);
        std::optional<Result> last;std::string last_id;std::uint64_t preceding_result_seq{};
        for(std::size_t ordinal=0;ordinal<history.array.size();++ordinal){
            const auto& h=history.array[ordinal];std::vector<std::string_view> k{"attempt_id","status","failure_code","result_code","accepted"};if(single)k.push_back("reference");k.insert(k.end(),{"payload_digest","payload_size","usage"});keys(h,k);
            const auto id=str(h,"attempt_id");ensure(id==attempt_id(index,ordinal,single)&&seen.insert(id).second);const auto it=log.attempts.find(id);ensure(it!=log.attempts.end());const auto& a=it->second;auto r=result(h,budget);ensure(inspect_boolean(field(h,"accepted"))==a.accepted);verify_worker_result(r,a,adapter);
            ensure(preceding_result_seq<a.started_seq);preceding_result_seq=a.result_seq;
            if(single)ensure(r.reference==(a.accepted?std::optional<std::string>("artifact/"+node_id):std::nullopt));
            if(ordinal+1<history.array.size())ensure(!a.accepted&&r.status=="FAILED"&&(r.failure=="TIMEOUT"||r.failure=="PROCESS_CRASHED"||r.failure=="PROTOCOL_ERROR"));
            if(a.accepted)++out.accepted_artifact_count;
            last=r;last_id=id;
        }
        std::optional<std::string> reported_id;if(!single)reported_id=inspect_optional_string(field(node,"attempt_id"));else if(last)reported_id=last_id;
        const bool reserved=!single&&reported_id&&*reported_id!=last_id;
        const bool preflight=host_failure(receipt,external)&&(reserved||!last||!same_result(receipt,*last));
        if(control_failure&&(!last||last->status=="CANCELLED")) {
            ensure(equal_usage(receipt.usage,last?last->usage:Usage{}));
            if(last)ensure(reported_id&&*reported_id==last_id&&!log.attempts.at(last_id).accepted);
        }
        else if(preflight){ensure(history.array.size()<=retry);if(!single)ensure(reported_id&&*reported_id==attempt_id(index,history.array.size(),single)&&!log.attempts.contains(*reported_id));if(last)ensure(last->status=="FAILED"&&(last->failure=="TIMEOUT"||last->failure=="PROCESS_CRASHED"||last->failure=="PROTOCOL_ERROR"));}
        else if(last){ensure(reported_id&&*reported_id==last_id&&same_result(receipt,*last));ensure((receipt.status=="READY")==log.attempts.at(last_id).accepted);}
        else {ensure(!reported_id&&equal_usage(receipt.usage,Usage{})&&((receipt.status=="BLOCKED"&&receipt.failure=="BLOCKED"&&receipt.code=="BLOCKED")||(receipt.status=="FAILED"&&receipt.failure=="FAILED"&&receipt.code=="START_BUDGET_EXCEEDED")||(receipt.status=="CANCELLED"&&receipt.failure=="CANCELLED"&&receipt.code=="CANCELLED")));}
    }
    ensure(seen.size()==log.attempts.size());const auto status=str(report,"status");
    const bool cancelled=status=="CANCELLED";
    if(cancelled)ensure(!all_ready);
    if(!single&&!cancelled)ensure(status==(all_ready?"READY":"FAILED"));
    if(log.finished)ensure(log.status==(cancelled?"CANCELLED":(all_ready?"READY":"FAILED")));
    detail::PayloadStreamInspectionV1 streams;
    if(options.verify_streams_v15)streams=detail::inspect_payload_run_streams_v1(run_root,log.order,budget.max_input_bytes,reader.deadline);
    // Complete long stream/input content passes before the core's final
    // report/log reads. Afterwards check metadata/identity/membership across
    // both evidence sets; do not start another long read after that boundary.
    if(streams.validate_snapshot)streams.validate_snapshot();
    reader.finish();
    if(streams.validate_metadata)streams.validate_metadata();
    out.streams_requested=options.verify_streams_v15;out.verified_stream_count=streams.count;out.verified_stream_bytes=streams.bytes;
    out.run_finished=log.finished;out.run_status=log.finished?log.status:"INCOMPLETE";out.run_id=log.run_id;out.last_seq=log.last_seq;out.node_count=nodes.size();out.attempt_count=log.attempts.size();return out;
}
} // namespace
PayloadRunInspectionV1 inspect_payload_run_v1(const std::filesystem::path& bundle,const PayloadRunInspectOptionsV1& options) {
    try {return inspect(bundle,options);}catch(...){throw Error("PROTOCOL_ERROR");}
}
} // namespace bruce::artifact_v1
