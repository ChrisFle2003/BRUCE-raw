#include "bruce/mods/nanocell/nanocell.hpp"
#include "nanocell_wire.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include <string_view>
#include <tuple>
#include <utility>

namespace bruce {
std::uint8_t and8(std::uint8_t a, std::uint8_t b) {
    return static_cast<std::uint8_t>(static_cast<unsigned>(a)&static_cast<unsigned>(b));
}

std::uint8_t or8(std::uint8_t a, std::uint8_t b) {
    return static_cast<std::uint8_t>(static_cast<unsigned>(a)|static_cast<unsigned>(b));
}

std::uint8_t xor8(std::uint8_t a, std::uint8_t b) {
    return static_cast<std::uint8_t>(static_cast<unsigned>(a)^static_cast<unsigned>(b));
}

std::uint8_t apply_nanocell_byte_op_v1(std::uint8_t state, const NanoCellByteOpV1& op) {
    const auto value=static_cast<unsigned>(state);
    const auto width=static_cast<unsigned>(op.operand);
    switch (op.code) {
    case NanoCellOpCodeV1::bit_and: return and8(state,op.operand);
    case NanoCellOpCodeV1::bit_or: return or8(state,op.operand);
    case NanoCellOpCodeV1::bit_xor: return xor8(state,op.operand);
    case NanoCellOpCodeV1::bit_not:
        if (op.operand!=0) throw Error("NanoCell NOT operand must be zero");
        return static_cast<std::uint8_t>(~value);
    case NanoCellOpCodeV1::rotl:
        if (width>7) throw Error("NanoCell ROTL width must be 0..7");
        if (width==0) return state;
        return static_cast<std::uint8_t>((value<<width)|(value>>(8u-width)));
    case NanoCellOpCodeV1::shl:
        if (width>7) throw Error("NanoCell SHL width must be 0..7");
        return static_cast<std::uint8_t>(value<<width);
    case NanoCellOpCodeV1::shr:
        if (width>7) throw Error("NanoCell SHR width must be 0..7");
        return static_cast<std::uint8_t>(value>>width);
    }
    throw Error("Unknown NanoCell byte opcode");
}

NanoCellTechniqueDefinitionV1 nanocell_reference_technique_v1(NanoCellReferenceTechniqueV1 technique) {
    NanoCellTechniqueDefinitionV1 definition;
    switch (technique) {
    case NanoCellReferenceTechniqueV1::masked_match:
        definition.technique_id="masked-match-v1";
        definition.required_context_digest=sha256("nanocell-reference-context-v1");
        definition.program={{NanoCellOpCodeV1::bit_and,0xF0}};
        definition.match_mask=0xF0;
        definition.match_value=0xA0;
        return definition;
    case NanoCellReferenceTechniqueV1::rotate_match:
        definition.technique_id="rotate-match-v1";
        definition.required_context_digest=sha256("nanocell-reference-context-v1");
        definition.program={{NanoCellOpCodeV1::rotl,1},{NanoCellOpCodeV1::bit_and,0x0F}};
        definition.match_mask=0x0F;
        definition.match_value=0x05;
        return definition;
    case NanoCellReferenceTechniqueV1::context_conflict:
        definition.technique_id="context-conflict-v1";
        definition.required_context_digest=sha256("nanocell-reference-context-conflict-v1");
        definition.program={{NanoCellOpCodeV1::bit_and,0xF0}};
        definition.match_mask=0xF0;
        definition.match_value=0xA0;
        return definition;
    }
    throw Error("Unknown NanoCell reference technique");
}

namespace {
bool valid_id(std::string_view id) {
    if (id.empty() || id.size()>64) return false;
    for (const unsigned char ch : id) {
        if (!((ch>='A' && ch<='Z') || (ch>='a' && ch<='z') ||
              (ch>='0' && ch<='9') || ch=='_' || ch=='.' || ch=='-')) return false;
    }
    return true;
}

void append_field(std::string& out, std::string_view field) {
    const auto size=static_cast<std::uint64_t>(field.size());
    for (int shift=56;shift>=0;shift-=8)
        out.push_back(static_cast<char>((size>>shift)&0xffu));
    out.append(field);
}

void append_byte_field(std::string& out, std::uint8_t byte) {
    const char value=static_cast<char>(byte);
    append_field(out,std::string_view(&value,1));
}

std::string encode_core(const NanoNeuronCoreV1& core) {
    std::string out;
    append_field(out,"nanocell-core-digest-v1");
    append_field(out,core.core_layout_version);
    append_byte_field(out,core.active_index);
    append_field(out,std::string_view(reinterpret_cast<const char*>(core.core_bytes.data()),core.core_bytes.size()));
    return out;
}

std::string encode_technique(const NanoCellTechniqueDefinitionV1& technique) {
    std::string out;
    append_field(out,"nanocell-technique-digest-v1");
    append_field(out,technique.technique_id);
    append_field(out,technique.technique_version);
    append_field(out,technique.required_context_digest);
    std::string program;
    program.reserve(technique.program.size()*2);
    for (const auto& op : technique.program) {
        program.push_back(static_cast<char>(op.code));
        program.push_back(static_cast<char>(op.operand));
    }
    append_field(out,program);
    append_byte_field(out,technique.match_mask);
    append_byte_field(out,technique.match_value);
    return out;
}

void validate_op(const NanoCellByteOpV1& op) {
    switch (op.code) {
    case NanoCellOpCodeV1::bit_and:
    case NanoCellOpCodeV1::bit_or:
    case NanoCellOpCodeV1::bit_xor: return;
    case NanoCellOpCodeV1::bit_not:
        if (op.operand==0) return;
        throw Error("NanoCell NOT operand must be zero");
    case NanoCellOpCodeV1::rotl:
    case NanoCellOpCodeV1::shl:
    case NanoCellOpCodeV1::shr:
        if (op.operand<=7) return;
        throw Error("NanoCell shift/rotation width must be 0..7");
    }
    throw Error("Unknown NanoCell byte opcode");
}

void validate_request(const NanoCellRequestV1& request) {
    if (request.request_version!="nanocell-request-v1") throw Error("Invalid NanoCell request version");
    if (request.core.core_layout_version!="nanoneuron-core-v1") throw Error("Invalid NanoCell core layout version");
    if (request.core.active_index>=request.core.core_bytes.size()) throw Error("Invalid NanoCell active index");
    if (!valid_id(request.context.context_id)) throw Error("Invalid NanoCell context id");
    if (request.context.context_bytes.size()>4096) throw Error("NanoCell context too large");
    if (request.context.context_digest!=sha256(request.context.context_bytes)) throw Error("Invalid NanoCell context digest");
    if (request.max_operations>16) throw Error("Invalid NanoCell operation budget");
    if (request.max_trace_entries<2 || request.max_trace_entries>18) throw Error("Invalid NanoCell trace budget");

    const auto& technique=request.technique;
    if (!valid_id(technique.technique_id)) throw Error("Invalid NanoCell technique id");
    if (technique.technique_version!="nanocell-technique-v1") throw Error("Invalid NanoCell technique version");
    if (technique.program.empty() || technique.program.size()>16) throw Error("Invalid NanoCell program length");
    for (const auto& op : technique.program) validate_op(op);

    NanoCellReferenceTechniqueV1 reference;
    if (technique.technique_id=="masked-match-v1") reference=NanoCellReferenceTechniqueV1::masked_match;
    else if (technique.technique_id=="rotate-match-v1") reference=NanoCellReferenceTechniqueV1::rotate_match;
    else if (technique.technique_id=="context-conflict-v1") reference=NanoCellReferenceTechniqueV1::context_conflict;
    else throw Error("Unknown NanoCell technique id");
    const auto expected=nanocell_reference_technique_v1(reference);
    if (technique.required_context_digest!=expected.required_context_digest ||
        technique.match_mask!=expected.match_mask || technique.match_value!=expected.match_value ||
        technique.program.size()!=expected.program.size()) throw Error("Forged NanoCell technique");
    for (std::size_t i=0;i<technique.program.size();++i) {
        if (technique.program[i].code!=expected.program[i].code ||
            technique.program[i].operand!=expected.program[i].operand) throw Error("Forged NanoCell technique");
    }
}
}

NanoCellRunV1 execute_nanocell_v1(const NanoCellRequestV1& caller_request, const NanoCellControlV1& control) {
    const NanoCellRequestV1 request=caller_request;
    validate_request(request);
    NanoCellAnswerPointV1 answer;
    answer.technique_id=request.technique.technique_id;
    answer.technique_version=request.technique.technique_version;
    answer.core_layout_version=request.core.core_layout_version;
    answer.active_index=request.core.active_index;
    answer.core_digest=sha256(encode_core(request.core));
    answer.context_id=request.context.context_id;
    answer.context_digest=request.context.context_digest;
    answer.technique_digest=sha256(encode_technique(request.technique));
    auto state=request.core.core_bytes[request.core.active_index];
    answer.trace.push_back({NanoCellTraceKindV1::start,0,state});
    if (request.context.context_digest!=request.technique.required_context_digest) {
        answer.status=NanoCellStatusV1::context_rejected;
        answer.trace.push_back({NanoCellTraceKindV1::terminal,0,state});
        return {request,std::move(answer)};
    }
    for (const auto& op : request.technique.program) {
        if (control.cancelled && control.cancelled()) throw Error("NanoCellCancelled");
        if (answer.operations_used>=request.max_operations || answer.trace.size()+2>request.max_trace_entries) {
            answer.status=NanoCellStatusV1::incomplete;
            answer.final_state=state;
            answer.trace.push_back({NanoCellTraceKindV1::terminal,answer.operations_used,state});
            return {request,std::move(answer)};
        }
        state=apply_nanocell_byte_op_v1(state,op);
        ++answer.operations_used;
        answer.trace.push_back({NanoCellTraceKindV1::operation,answer.operations_used,state});
    }
    answer.final_state=state;
    answer.status=(state&request.technique.match_mask)==request.technique.match_value ?
        NanoCellStatusV1::matched : NanoCellStatusV1::no_match;
    answer.trace.push_back({NanoCellTraceKindV1::terminal,answer.operations_used,state});
    return {request,std::move(answer)};
}

namespace {
constexpr std::string_view run_header="BRUCE_NANOCELL_RUN_V1\n";
// Rounded ceilings from V1: request includes 4096 context bytes, two 64-byte IDs,
// two digests, 8 core bytes, 32 program bytes, versions and decimal scalar fields.
// Answer includes two IDs, three digests and version/scalar fields. Trace is at
// most 4 + 18*(3+4+5) = 220 bytes. The four outer prefixes take at most 16 bytes.
constexpr std::size_t max_request_bytes=4800;
constexpr std::size_t max_answer_bytes=512;
constexpr std::size_t max_trace_bytes=224;
constexpr std::size_t max_run_bytes=run_header.size()+65+16+max_request_bytes+64+max_answer_bytes+max_trace_bytes;

void put_field(std::string& out, std::string_view value) {
    out+=std::to_string(value.size());
    out.push_back(':');
    out.append(value);
}

void put_number(std::string& out, std::uint64_t value) { put_field(out,std::to_string(value)); }

std::uint64_t parse_number(std::string_view value, std::uint64_t limit) {
    if (value.empty() || (value.size()>1 && value.front()=='0')) throw Error("Noncanonical NanoCell number");
    std::uint64_t result=0;
    for (const unsigned char ch : value) {
        if (ch<'0' || ch>'9') throw Error("Invalid NanoCell number");
        const auto digit=static_cast<unsigned>(ch-'0');
        if (result>limit/10 || (result==limit/10 && digit>limit%10)) throw Error("NanoCell number exceeds limit");
        result=result*10+digit;
    }
    return result;
}

class RunReader {
public:
    explicit RunReader(std::string_view bytes) : remaining_(bytes) {}
    std::string_view field(std::size_t limit) {
        const auto colon=remaining_.find(':');
        if (colon==std::string_view::npos) throw Error("Missing NanoCell length separator");
        const auto size=static_cast<std::size_t>(parse_number(remaining_.substr(0,colon),limit));
        remaining_.remove_prefix(colon+1);
        if (size>remaining_.size()) throw Error("Truncated NanoCell field");
        const auto value=remaining_.substr(0,size);
        remaining_.remove_prefix(size);
        return value;
    }
    std::uint64_t number(std::uint64_t limit) { return parse_number(field(20),limit); }
    std::string digest() {
        const auto value=field(64);
        if (value.size()!=64) throw Error("Invalid NanoCell digest length");
        for (const char ch : value)
            if (!((ch>='0' && ch<='9') || (ch>='a' && ch<='f'))) throw Error("Invalid NanoCell digest encoding");
        return std::string(value);
    }
    void finish() const { if (!remaining_.empty()) throw Error("Trailing NanoCell bytes"); }
private:
    std::string_view remaining_;
};

std::string encode_run_request(const NanoCellRequestV1& request) {
    std::string out;
    put_field(out,request.request_version);
    put_field(out,request.core.core_layout_version);
    put_field(out,std::string_view(reinterpret_cast<const char*>(request.core.core_bytes.data()),8));
    put_number(out,request.core.active_index);
    put_field(out,request.context.context_id);
    put_field(out,request.context.context_bytes);
    put_field(out,request.context.context_digest);
    put_field(out,request.technique.technique_id);
    put_field(out,request.technique.technique_version);
    put_field(out,request.technique.required_context_digest);
    std::string program;
    for (const auto& op : request.technique.program) {
        program.push_back(static_cast<char>(op.code));
        program.push_back(static_cast<char>(op.operand));
    }
    put_field(out,program);
    put_number(out,request.technique.match_mask);
    put_number(out,request.technique.match_value);
    put_number(out,request.max_operations);
    put_number(out,request.max_trace_entries);
    return out;
}

NanoCellRequestV1 decode_run_request(std::string_view bytes) {
    RunReader reader(bytes);
    NanoCellRequestV1 request;
    request.request_version=reader.field(19);
    request.core.core_layout_version=reader.field(19);
    const auto core=reader.field(8);
    if (core.size()!=8) throw Error("Invalid NanoCell core length");
    for (std::size_t i=0;i<8;++i) request.core.core_bytes[i]=static_cast<std::uint8_t>(core[i]);
    request.core.active_index=static_cast<std::uint8_t>(reader.number(7));
    request.context.context_id=reader.field(64);
    request.context.context_bytes=reader.field(4096);
    request.context.context_digest=reader.digest();
    request.technique.technique_id=reader.field(64);
    request.technique.technique_version=reader.field(21);
    request.technique.required_context_digest=reader.digest();
    const auto program=reader.field(32);
    if (program.empty() || program.size()%2!=0) throw Error("Invalid NanoCell program bytes");
    for (std::size_t i=0;i<program.size();i+=2) {
        const auto code=static_cast<unsigned char>(program[i]);
        if (code>static_cast<unsigned>(NanoCellOpCodeV1::shr)) throw Error("Unknown NanoCell encoded opcode");
        request.technique.program.push_back({static_cast<NanoCellOpCodeV1>(code),static_cast<std::uint8_t>(program[i+1])});
    }
    request.technique.match_mask=static_cast<std::uint8_t>(reader.number(255));
    request.technique.match_value=static_cast<std::uint8_t>(reader.number(255));
    request.max_operations=reader.number(16);
    request.max_trace_entries=reader.number(18);
    reader.finish();
    validate_request(request);
    if (encode_run_request(request)!=bytes) throw Error("Noncanonical NanoCell request");
    return request;
}

std::string encode_run_answer(const NanoCellAnswerPointV1& answer) {
    std::string out;
    put_field(out,answer.answer_version);
    put_number(out,static_cast<unsigned>(answer.status));
    put_number(out,answer.final_state.has_value());
    put_field(out,answer.final_state ? std::to_string(*answer.final_state) : "");
    put_field(out,answer.technique_id);
    put_field(out,answer.technique_version);
    put_field(out,answer.core_layout_version);
    put_number(out,answer.active_index);
    put_field(out,answer.core_digest);
    put_field(out,answer.context_id);
    put_field(out,answer.context_digest);
    put_field(out,answer.technique_digest);
    put_number(out,answer.operations_used);
    return out;
}

NanoCellAnswerPointV1 decode_run_answer(std::string_view bytes) {
    RunReader reader(bytes);
    NanoCellAnswerPointV1 answer;
    answer.answer_version=reader.field(19);
    if (answer.answer_version!="nanocell-answer-v1") throw Error("Invalid NanoCell answer version");
    answer.status=static_cast<NanoCellStatusV1>(reader.number(3));
    const auto present=reader.number(1);
    const auto final=reader.field(3);
    if (present) answer.final_state=static_cast<std::uint8_t>(parse_number(final,255));
    else if (!final.empty()) throw Error("Unexpected NanoCell final state");
    answer.technique_id=reader.field(64);
    answer.technique_version=reader.field(21);
    answer.core_layout_version=reader.field(19);
    answer.active_index=static_cast<std::uint8_t>(reader.number(7));
    answer.core_digest=reader.digest();
    answer.context_id=reader.field(64);
    answer.context_digest=reader.digest();
    answer.technique_digest=reader.digest();
    answer.operations_used=reader.number(16);
    reader.finish();
    return answer;
}

std::string encode_run_trace(const std::vector<NanoCellTraceEntryV1>& trace) {
    std::string out;
    put_number(out,trace.size());
    for (const auto& entry : trace) {
        put_number(out,static_cast<unsigned>(entry.kind));
        put_number(out,entry.operations_used);
        put_number(out,entry.state);
    }
    return out;
}

std::vector<NanoCellTraceEntryV1> decode_run_trace(std::string_view bytes) {
    RunReader reader(bytes);
    const auto count=reader.number(18);
    if (count<2) throw Error("Invalid NanoCell trace count");
    std::vector<NanoCellTraceEntryV1> trace;
    for (std::uint64_t i=0;i<count;++i) {
        const auto kind=static_cast<NanoCellTraceKindV1>(reader.number(2));
        const auto used=reader.number(16);
        const auto state=static_cast<std::uint8_t>(reader.number(255));
        trace.push_back({kind,used,state});
    }
    reader.finish();
    return trace;
}

bool equal_answer(const NanoCellAnswerPointV1& a, const NanoCellAnswerPointV1& b) {
    if (std::tie(a.answer_version,a.status,a.final_state,a.technique_id,a.technique_version,
                 a.core_layout_version,a.active_index,a.core_digest,a.context_id,a.context_digest,
                 a.technique_digest,a.operations_used)!=
        std::tie(b.answer_version,b.status,b.final_state,b.technique_id,b.technique_version,
                 b.core_layout_version,b.active_index,b.core_digest,b.context_id,b.context_digest,
                 b.technique_digest,b.operations_used) || a.trace.size()!=b.trace.size()) return false;
    for (std::size_t i=0;i<a.trace.size();++i)
        if (a.trace[i].kind!=b.trace[i].kind || a.trace[i].operations_used!=b.trace[i].operations_used ||
            a.trace[i].state!=b.trace[i].state) return false;
    return true;
}
}

std::string pack_nanocell_run_v1(const NanoCellRunV1& run) {
    const auto expected=execute_nanocell_v1(run.request);
    if (!equal_answer(run.answer,expected.answer)) throw Error("Inconsistent NanoCell run");
    const auto request=encode_run_request(run.request);
    std::string body;
    put_field(body,request);
    put_field(body,sha256(request));
    put_field(body,encode_run_answer(run.answer));
    put_field(body,encode_run_trace(run.answer.trace));
    return std::string(run_header)+sha256(body)+"\n"+body;
}

NanoCellRunV1 replay_nanocell_run_v1(std::string_view bundle) {
    if (bundle.size()>max_run_bytes || bundle.size()<run_header.size()+65 || !bundle.starts_with(run_header))
        throw Error("Invalid NanoCell bundle framing or size");
    const auto digest=bundle.substr(run_header.size(),64);
    if (bundle[run_header.size()+64]!='\n') throw Error("Invalid NanoCell digest separator");
    const auto body=bundle.substr(run_header.size()+65);
    if (digest!=sha256(body)) throw Error("NanoCell bundle digest mismatch");
    RunReader reader(body);
    const auto request_bytes=reader.field(max_request_bytes);
    const auto request_digest=reader.digest();
    if (request_digest!=sha256(request_bytes)) throw Error("NanoCell request digest mismatch");
    const auto answer_bytes=reader.field(max_answer_bytes);
    const auto trace_bytes=reader.field(max_trace_bytes);
    reader.finish();
    const auto request=decode_run_request(request_bytes);
    auto answer=decode_run_answer(answer_bytes);
    answer.trace=decode_run_trace(trace_bytes);
    auto replayed=execute_nanocell_v1(request);
    if (!equal_answer(answer,replayed.answer)) throw Error("NanoCell replay result mismatch");
    if (pack_nanocell_run_v1(replayed)!=bundle) throw Error("Noncanonical NanoCell bundle");
    return replayed;
}

namespace nanocell_detail {

std::string pack_nanocell_request_v1(const NanoCellRequestV1& request) {
    return encode_run_request(execute_nanocell_v1(request).request);
}

NanoCellRequestV1 unpack_nanocell_request_v1(const std::string_view bytes) {
    return decode_run_request(bytes);
}

} // namespace nanocell_detail
}
