#include "bruce/mods/nanocell/nanocell.hpp"
#include "bruce/evidence/bundle.hpp"
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<class Action> void require_error(Action action, const std::string& message) {
    try { action(); } catch (const bruce::Error&) { return; }
    throw std::runtime_error(message);
}

template<class Action> void require_error_containing(Action action, const std::string& expected) {
    try { action(); } catch (const bruce::Error& error) {
        require(std::string(error.what()).find(expected)!=std::string::npos,"wrong error: "+std::string(error.what()));
        return;
    }
    throw std::runtime_error("expected error containing "+expected);
}

bruce::NanoCellRequestV1 request_for(bruce::NanoCellReferenceTechniqueV1 reference, std::uint8_t active=0xA5) {
    bruce::NanoCellRequestV1 request;
    request.core.core_bytes={0x10,0x20,active,0x40,0x50,0x60,0x70,0x80};
    request.core.active_index=2;
    request.context.context_id="reference.context_1";
    request.context.context_bytes="nanocell-reference-context-v1";
    request.context.context_digest=bruce::sha256(request.context.context_bytes);
    request.technique=bruce::nanocell_reference_technique_v1(reference);
    request.max_operations=16;
    request.max_trace_entries=18;
    return request;
}

void require_trace(const bruce::NanoCellRunV1& run,
                   const std::vector<std::pair<bruce::NanoCellTraceKindV1,std::uint8_t>>& expected) {
    require(run.answer.trace.size()==expected.size(),"trace length");
    for (std::size_t i=0;i<expected.size();++i) {
        const auto& actual=run.answer.trace[i];
        require(actual.kind==expected[i].first && actual.state==expected[i].second,"trace kind/state");
        const auto expected_ops=actual.kind==bruce::NanoCellTraceKindV1::start ? 0u :
            actual.kind==bruce::NanoCellTraceKindV1::terminal ? run.answer.operations_used : static_cast<unsigned>(i);
        require(actual.operations_used==expected_ops,"trace operation count");
    }
}

void executor_outcomes_and_trace() {
    using namespace bruce;
    const auto matched=execute_nanocell_v1(request_for(NanoCellReferenceTechniqueV1::masked_match));
    require(matched.answer.answer_version=="nanocell-answer-v1","answer version");
    require(matched.answer.status==NanoCellStatusV1::matched,"masked match status");
    require(matched.answer.final_state==0xA0 && matched.answer.operations_used==1,"masked result");
    require_trace(matched,{{NanoCellTraceKindV1::start,0xA5},{NanoCellTraceKindV1::operation,0xA0},{NanoCellTraceKindV1::terminal,0xA0}});
    require(matched.request.core.core_bytes[2]==0xA5,"run preserves request");

    const auto no_match=execute_nanocell_v1(request_for(NanoCellReferenceTechniqueV1::masked_match,0xB5));
    require(no_match.answer.status==NanoCellStatusV1::no_match && no_match.answer.final_state==0xB0,"upper nibble no match");

    const auto rotated=execute_nanocell_v1(request_for(NanoCellReferenceTechniqueV1::rotate_match,0x82));
    require(rotated.answer.status==NanoCellStatusV1::matched && rotated.answer.final_state==0x05,"rotate match");
    require_trace(rotated,{{NanoCellTraceKindV1::start,0x82},{NanoCellTraceKindV1::operation,0x05},{NanoCellTraceKindV1::operation,0x05},{NanoCellTraceKindV1::terminal,0x05}});

    const auto rejected=execute_nanocell_v1(request_for(NanoCellReferenceTechniqueV1::context_conflict));
    require(rejected.answer.status==NanoCellStatusV1::context_rejected,"context rejection");
    require(!rejected.answer.final_state && rejected.answer.operations_used==0,"context rejection has no result");
    require_trace(rejected,{{NanoCellTraceKindV1::start,0xA5},{NanoCellTraceKindV1::terminal,0xA5}});
}

void fixed_digest_contract_vector() {
    using namespace bruce;
    // Independently calculated from the V1 binary field stream, not from a production encoder.
    const auto request=request_for(NanoCellReferenceTechniqueV1::masked_match);
    const auto run=execute_nanocell_v1(request);
    constexpr std::string_view core_digest="df6c4838075217518af6bd509dba5f4dfef027f3fcf767557f47c9111062f024";
    constexpr std::string_view technique_digest="8520761a8f085f128fd6d484133f3b9ef5325d9d7af3186bd190e1061b7c12bb";
    require(run.answer.core_digest==core_digest,"fixed core digest vector");
    require(run.answer.technique_digest==technique_digest,"fixed technique digest vector");
    const auto bundle=pack_nanocell_run_v1(run);
    const auto replayed=replay_nanocell_run_v1(bundle);
    require(replayed.answer.core_digest==core_digest,"replayed core digest vector");
    require(replayed.answer.technique_digest==technique_digest,"replayed technique digest vector");
    require(pack_nanocell_run_v1(replayed)==bundle,"fixed digest vector bundle round trip");
}

void executor_budget_and_cancel() {
    using namespace bruce;
    auto request=request_for(NanoCellReferenceTechniqueV1::rotate_match,0x82);
    request.max_operations=0;
    auto run=execute_nanocell_v1(request);
    require(run.answer.status==NanoCellStatusV1::incomplete && run.answer.final_state==0x82 && run.answer.operations_used==0,"zero operations");
    require_trace(run,{{NanoCellTraceKindV1::start,0x82},{NanoCellTraceKindV1::terminal,0x82}});
    request.max_operations=1;
    run=execute_nanocell_v1(request);
    require(run.answer.status==NanoCellStatusV1::incomplete && run.answer.final_state==0x05 && run.answer.operations_used==1,"mid-program operation stop");
    require_trace(run,{{NanoCellTraceKindV1::start,0x82},{NanoCellTraceKindV1::operation,0x05},{NanoCellTraceKindV1::terminal,0x05}});
    request.max_operations=16;
    request.max_trace_entries=3;
    run=execute_nanocell_v1(request);
    require(run.answer.status==NanoCellStatusV1::incomplete && run.answer.operations_used==1,"trace budget stops second operation");
    require_trace(run,{{NanoCellTraceKindV1::start,0x82},{NanoCellTraceKindV1::operation,0x05},{NanoCellTraceKindV1::terminal,0x05}});
    request.max_trace_entries=2;
    run=execute_nanocell_v1(request);
    require(run.answer.status==NanoCellStatusV1::incomplete && run.answer.operations_used==0,"trace budget stops first operation");
    require_trace(run,{{NanoCellTraceKindV1::start,0x82},{NanoCellTraceKindV1::terminal,0x82}});
    request.max_trace_entries=4;
    run=execute_nanocell_v1(request);
    require(run.answer.status==NanoCellStatusV1::matched && run.answer.operations_used==2,"exact rotate trace capacity");
    // V1 has no valid 16-op technique; its documented accounting is start + 16 results + terminal.
    require(1u+16u+1u==18u && 17u-2u==15u,"16-op trace capacity arithmetic");

    request.max_operations=0;
    bool called=false;
    require_error_containing([&] { (void)execute_nanocell_v1(request,{[&] { called=true; return true; }}); },"NanoCellCancelled");
    require(called,"cancellation before first operation");
    called=false;
    const auto rejected=execute_nanocell_v1(request_for(NanoCellReferenceTechniqueV1::context_conflict),{[&] { called=true; return true; }});
    require(rejected.answer.status==NanoCellStatusV1::context_rejected && !called,"callback on context rejection");
}

void executor_uses_request_snapshot_across_callback() {
    using namespace bruce;
    auto caller=request_for(NanoCellReferenceTechniqueV1::masked_match);
    const auto original=caller;
    unsigned callback_calls=0;
    const auto run=execute_nanocell_v1(caller,{[&] {
        ++callback_calls;
        caller.technique.program[0]={NanoCellOpCodeV1::bit_or,0};
        caller.context.context_bytes="changed after validation";
        caller.max_operations=0;
        caller.max_trace_entries=2;
        return false;
    }});
    require(callback_calls==1,"snapshot callback count");
    require(caller.technique.program[0].code==NanoCellOpCodeV1::bit_or &&
            caller.context.context_bytes=="changed after validation" &&
            caller.max_operations==0 && caller.max_trace_entries==2,"caller mutation occurred");
    require(run.request.technique.program[0].code==original.technique.program[0].code &&
            run.request.technique.program[0].operand==original.technique.program[0].operand &&
            run.request.context.context_bytes==original.context.context_bytes &&
            run.request.context.context_digest==original.context.context_digest &&
            run.request.max_operations==original.max_operations &&
            run.request.max_trace_entries==original.max_trace_entries,"run request is validated snapshot");
    require(run.answer.status==NanoCellStatusV1::matched && run.answer.final_state==0xA0 &&
            run.answer.operations_used==1,"snapshot execution ignores caller mutation");
    require_trace(run,{{NanoCellTraceKindV1::start,0xA5},{NanoCellTraceKindV1::operation,0xA0},{NanoCellTraceKindV1::terminal,0xA0}});
    const auto replayed=execute_nanocell_v1(run.request);
    require(replayed.answer.status==run.answer.status && replayed.answer.final_state==run.answer.final_state &&
            replayed.answer.operations_used==run.answer.operations_used &&
            replayed.answer.core_digest==run.answer.core_digest &&
            replayed.answer.context_digest==run.answer.context_digest &&
            replayed.answer.technique_digest==run.answer.technique_digest,"snapshot can be reexecuted consistently");
}

void executor_validation() {
    using namespace bruce;
    auto base=request_for(NanoCellReferenceTechniqueV1::masked_match);
    auto invalid=base;
    invalid.request_version="wrong"; require_error([&] { (void)execute_nanocell_v1(invalid); },"request version");
    invalid=base; invalid.core.core_layout_version="wrong"; require_error([&] { (void)execute_nanocell_v1(invalid); },"core version");
    invalid=base; invalid.core.active_index=8; require_error([&] { (void)execute_nanocell_v1(invalid); },"active index");
    for (const auto& id : {std::string{},std::string("bad id"),std::string("ü"),std::string(65,'a')}) {
        invalid=base; invalid.context.context_id=id; require_error([&] { (void)execute_nanocell_v1(invalid); },"context id");
    }
    invalid=base; invalid.context.context_bytes=std::string(4097,'x'); invalid.context.context_digest=sha256(invalid.context.context_bytes);
    require_error([&] { (void)execute_nanocell_v1(invalid); },"context size");
    invalid=base; invalid.context.context_bytes=std::string(4096,'x'); invalid.context.context_digest=sha256(invalid.context.context_bytes);
    require(execute_nanocell_v1(invalid).answer.status==NanoCellStatusV1::context_rejected,"4096-byte context boundary");
    invalid=base; invalid.context.context_digest=std::string(64,'0'); require_error([&] { (void)execute_nanocell_v1(invalid); },"context digest");
    invalid=base; invalid.max_operations=17; require_error([&] { (void)execute_nanocell_v1(invalid); },"operation budget");
    for (auto limit : {0u,1u,19u}) {
        invalid=base; invalid.max_trace_entries=limit; require_error([&] { (void)execute_nanocell_v1(invalid); },"trace budget");
    }
    invalid=base; invalid.technique.technique_id="forged"; require_error([&] { (void)execute_nanocell_v1(invalid); },"technique identity");
    for (const auto& id : {std::string{},std::string("bad id"),std::string(65,'a')}) {
        invalid=base; invalid.technique.technique_id=id; require_error([&] { (void)execute_nanocell_v1(invalid); },"technique id syntax");
    }
    invalid=base; invalid.technique.technique_version="wrong"; require_error([&] { (void)execute_nanocell_v1(invalid); },"technique version");
    invalid=base; invalid.technique.required_context_digest=std::string(64,'0'); require_error([&] { (void)execute_nanocell_v1(invalid); },"technique context digest");
    invalid=base; invalid.technique.program.clear(); require_error([&] { (void)execute_nanocell_v1(invalid); },"empty program");
    invalid=base; invalid.technique.program.resize(17,{NanoCellOpCodeV1::bit_and,0xF0}); require_error([&] { (void)execute_nanocell_v1(invalid); },"17-op program");
    invalid=base; invalid.technique.program.resize(16,{NanoCellOpCodeV1::bit_and,0xF0}); require_error([&] { (void)execute_nanocell_v1(invalid); },"custom 16-op technique");
    invalid=base; invalid.technique.program[0].code=static_cast<NanoCellOpCodeV1>(255); require_error([&] { (void)execute_nanocell_v1(invalid); },"unknown opcode");
    invalid=base; invalid.technique.program[0]={NanoCellOpCodeV1::bit_not,1}; require_error([&] { (void)execute_nanocell_v1(invalid); },"invalid NOT operand");
    invalid=base; invalid.technique.program[0]={NanoCellOpCodeV1::rotl,8}; require_error([&] { (void)execute_nanocell_v1(invalid); },"invalid rotate width");
    invalid=request_for(NanoCellReferenceTechniqueV1::masked_match,0xA0);
    invalid.technique.program[0]={NanoCellOpCodeV1::bit_or,0};
    require_error([&] { (void)execute_nanocell_v1(invalid); },"same-byte forged program");
    invalid=base; invalid.technique.match_mask=0xFF; require_error([&] { (void)execute_nanocell_v1(invalid); },"forged match mask");
    invalid=base; invalid.technique.match_value=0xA1; require_error([&] { (void)execute_nanocell_v1(invalid); },"forged match value");
}

void executor_provenance() {
    using namespace bruce;
    const auto base=request_for(NanoCellReferenceTechniqueV1::masked_match);
    const auto first=execute_nanocell_v1(base);
    const auto again=execute_nanocell_v1(base);
    require(first.answer.core_digest==again.answer.core_digest && first.answer.technique_digest==again.answer.technique_digest,"deterministic digests");
    require(first.answer.core_digest.size()==64 && first.answer.technique_digest.size()==64,"SHA-256 digest width");
    require(first.answer.context_digest==base.context.context_digest,"actual context digest in answer");
    auto modified=base;
    modified.core.core_bytes[0]^=1;
    require(execute_nanocell_v1(modified).answer.core_digest!=first.answer.core_digest,"inactive core byte in digest");
    modified=request_for(NanoCellReferenceTechniqueV1::context_conflict);
    modified.context.context_id="other.context";
    modified.context.context_bytes="nanocell-reference-context-conflict-v1";
    modified.context.context_digest=sha256(modified.context.context_bytes);
    const auto second=execute_nanocell_v1(modified);
    require(first.answer.final_state==second.answer.final_state && first.answer.status==second.answer.status,"same byte/status fixture");
    require(first.answer.technique_id!=second.answer.technique_id && first.answer.technique_digest!=second.answer.technique_digest,"distinct technique provenance");
    require(first.answer.context_id!=second.answer.context_id && first.answer.context_digest!=second.answer.context_digest,"distinct context provenance");
}

// Independent test-side framing permits checksum-renewed attacks without a public decoder.
std::string fields(const std::vector<std::string>& values) {
    std::string bytes;
    for (const auto& value : values) bytes+=std::to_string(value.size())+":"+value;
    return bytes;
}

std::vector<std::string> split_fields(std::string_view bytes) {
    std::vector<std::string> values;
    while (!bytes.empty()) {
        const auto colon=bytes.find(':');
        require(colon!=std::string_view::npos,"test fixture field separator");
        const auto length=std::stoull(std::string(bytes.substr(0,colon)));
        require(length<=bytes.size()-colon-1,"test fixture field length");
        values.emplace_back(bytes.substr(colon+1,length));
        bytes.remove_prefix(colon+1+length);
    }
    return values;
}

constexpr std::string_view bundle_header="BRUCE_NANOCELL_RUN_V1\n";
std::string frame_body(const std::string& body) {
    return std::string(bundle_header)+bruce::sha256(body)+"\n"+body;
}

void replay_outcomes_and_binary_fields() {
    using namespace bruce;
    std::vector<NanoCellRequestV1> requests{
        request_for(NanoCellReferenceTechniqueV1::masked_match),
        request_for(NanoCellReferenceTechniqueV1::masked_match,0xB5),
        request_for(NanoCellReferenceTechniqueV1::context_conflict),
        request_for(NanoCellReferenceTechniqueV1::rotate_match,0x82)};
    requests.back().max_operations=1;
    const std::vector<NanoCellStatusV1> statuses{NanoCellStatusV1::matched,NanoCellStatusV1::no_match,
        NanoCellStatusV1::context_rejected,NanoCellStatusV1::incomplete};
    for (std::size_t i=0;i<requests.size();++i) {
        const auto bundle=pack_nanocell_run_v1(execute_nanocell_v1(requests[i]));
        require(bundle.starts_with(bundle_header),"bundle header");
        const auto replayed=replay_nanocell_run_v1(bundle);
        require(replayed.answer.status==statuses[i],"replayed status");
        require(replayed.answer.final_state.has_value()==(i!=2),"replayed final-state presence");
        require(pack_nanocell_run_v1(replayed)==bundle,"byte-identical replay");
        require(pack_nanocell_run_v1(execute_nanocell_v1(requests[i]))==bundle,"independent deterministic execution");
        const auto body=std::string_view(bundle).substr(bundle_header.size()+65);
        require(bundle.substr(bundle_header.size(),64)==sha256(body),"outer digest binds body");
        const auto outer=split_fields(body);
        require(outer.size()==4 && outer[1]==sha256(outer[0]),"four fields and request digest");
        const auto request=split_fields(outer[0]);
        require(request.size()==15 && request[2].size()==8,"all request fields and raw core bytes");
        require(request[10]==std::string(i==3 ? "\x04\x01\x00\x0f" : "\x00\xf0",i==3 ? 4 : 2),"raw opcode/operand pairs");
    }
    for (std::size_t length : {0u,4096u}) {
        auto request=requests[0];
        request.context.context_bytes.resize(length);
        for (std::size_t i=0;i<length;++i) request.context.context_bytes[i]=static_cast<char>(i%256);
        request.context.context_digest=sha256(request.context.context_bytes);
        const auto bundle=pack_nanocell_run_v1(execute_nanocell_v1(request));
        const auto replayed=replay_nanocell_run_v1(bundle);
        require(replayed.request.context.context_bytes==request.context.context_bytes,"raw binary context preserved");
        require(pack_nanocell_run_v1(replayed)==bundle,"boundary context replay");
    }
    std::optional<std::string> cancelled_bundle;
    require_error_containing([&] { cancelled_bundle=pack_nanocell_run_v1(execute_nanocell_v1(requests[0],{[] { return true; }})); },"NanoCellCancelled");
    require(!cancelled_bundle,"cancellation produces no bundle");
}

void replay_rejects_manipulation() {
    using namespace bruce;
    const auto run=execute_nanocell_v1(request_for(NanoCellReferenceTechniqueV1::masked_match));
    const auto bundle=pack_nanocell_run_v1(run);
    const auto body=bundle.substr(bundle_header.size()+65);
    const auto outer=split_fields(body);
    const auto rejects=[&](const std::string& candidate,const std::string& label) {
        require_error([&] { (void)replay_nanocell_run_v1(candidate); },"replay accepted "+label);
    };
    auto flipped=bundle; flipped.back()^=1; rejects(flipped,"byte flip");
    auto header=bundle; header[0]='X'; rejects(header,"unknown header");
    rejects(bundle+"x","trailing byte");
    rejects(frame_body(body+"0:"),"checksum-renewed trailing field");
    rejects(frame_body("0"+body),"leading-zero outer length");
    rejects(frame_body("999999999999999999999999999999:"),"oversized length");
    rejects(frame_body("-1:x"),"negative length");
    rejects(frame_body("+1:x"),"signed length");
    rejects(frame_body(":"),"empty length");
    rejects(frame_body(std::string(7000,'x')),"oversized bundle");
    for (std::size_t n=0;n<bundle.size();++n) rejects(bundle.substr(0,n),"truncation");
    auto changed=outer; changed[1]=std::string(64,'0'); rejects(frame_body(fields(changed)),"request hash");
    auto uppercase=bundle; uppercase[bundle_header.size()]='A'; rejects(uppercase,"noncanonical outer digest");

    // Request mutations also renew its hash, so deeper validation/re-execution must catch them.
    const std::vector<std::pair<std::size_t,std::string>> request_attacks{
        {0,"nanocell-request-v2"},{1,"nanoneuron-core-v2"},{2,std::string(7,'x')},
        {2,std::string(8,'x')},{3,"8"},{3,"02"},{3,"+2"},{3,"-2"},{3,""},
        {4,"other.context"},{4,std::string(65,'a')},{4,"bad id"},
        {5,"changed context"},{5,std::string(4097,'x')},{6,std::string(64,'0')},
        {7,"forged"},{8,"nanocell-technique-v2"},{9,std::string(64,'0')},
        {10,std::string("\x00\xef",2)},{10,std::string("\xff\xf0",2)},
        {10,""},{10,"x"},{10,std::string(34,'x')},{11,"256"},{12,"161"},
        {13,"0"},{13,"17"},{13,"18446744073709551616"},{14,"2"},{14,"19"}};
    for (const auto& [index,value] : request_attacks) {
        auto request=split_fields(outer[0]); request[index]=value;
        changed=outer; changed[0]=fields(request); changed[1]=sha256(changed[0]);
        rejects(frame_body(fields(changed)),"request field "+std::to_string(index));
    }
    auto request=split_fields(outer[0]); request[5]="changed context"; request[6]=sha256(request[5]);
    changed=outer; changed[0]=fields(request); changed[1]=sha256(changed[0]);
    rejects(frame_body(fields(changed)),"valid modified context with stale answer");
    for (const auto index : {0u,2u,3u}) {
        changed=outer; changed[index]="0"+changed[index];
        if (index==0) changed[1]=sha256(changed[0]);
        rejects(frame_body(fields(changed)),"nested noncanonical length");
        changed=outer; changed[index]+="0:";
        if (index==0) changed[1]=sha256(changed[0]);
        rejects(frame_body(fields(changed)),"nested trailing bytes");
    }
    const std::vector<std::pair<std::size_t,std::string>> answer_attacks{
        {0,"nanocell-answer-v2"},{1,"1"},{1,"4"},{2,"0"},{2,"2"},{3,"161"},{3,"256"},
        {4,"other"},{5,"nanocell-technique-v2"},{6,"nanoneuron-core-v2"},{7,"1"},
        {8,std::string(64,'0')},{9,"other.context"},{10,std::string(64,'0')},
        {11,std::string(64,'0')},{12,"0"},{12,"17"}};
    for (const auto& [index,value] : answer_attacks) {
        auto answer=split_fields(outer[2]); answer[index]=value;
        changed=outer; changed[2]=fields(answer);
        rejects(frame_body(fields(changed)),"answer field "+std::to_string(index));
    }
    const std::vector<std::pair<std::size_t,std::string>> trace_attacks{
        {0,"2"},{0,"19"},{0,"03"},{1,"3"},{2,"1"},{3,"164"},{6,"161"},{8,"0"},{9,"256"}};
    for (const auto& [index,value] : trace_attacks) {
        auto trace=split_fields(outer[3]); trace[index]=value;
        changed=outer; changed[3]=fields(trace);
        rejects(frame_body(fields(changed)),"trace field "+std::to_string(index));
    }
}

void binary_truth_tables() {
    for (unsigned a=0; a<256; ++a) {
        for (unsigned b=0; b<256; ++b) {
            const auto left=static_cast<std::uint8_t>(a);
            const auto right=static_cast<std::uint8_t>(b);
            require(bruce::and8(left,right)==static_cast<std::uint8_t>(a&b),"AND truth table");
            require(bruce::or8(left,right)==static_cast<std::uint8_t>(a|b),"OR truth table");
            require(bruce::xor8(left,right)==static_cast<std::uint8_t>(a^b),"XOR truth table");
            require(bruce::apply_nanocell_byte_op_v1(left,{bruce::NanoCellOpCodeV1::bit_and,right})==static_cast<std::uint8_t>(a&b),"program AND truth table");
            require(bruce::apply_nanocell_byte_op_v1(left,{bruce::NanoCellOpCodeV1::bit_or,right})==static_cast<std::uint8_t>(a|b),"program OR truth table");
            require(bruce::apply_nanocell_byte_op_v1(left,{bruce::NanoCellOpCodeV1::bit_xor,right})==static_cast<std::uint8_t>(a^b),"program XOR truth table");
        }
    }
}

void unary_truth_tables() {
    for (unsigned value=0; value<256; ++value) {
        const auto byte=static_cast<std::uint8_t>(value);
        require(bruce::apply_nanocell_byte_op_v1(byte,{bruce::NanoCellOpCodeV1::bit_not,0})==static_cast<std::uint8_t>(value^0xFFu),"NOT truth table");
    }
    require_error([] { (void)bruce::apply_nanocell_byte_op_v1(0x55,{bruce::NanoCellOpCodeV1::bit_not,1}); },"NOT accepted nonzero operand");
}

void shift_rotation_truth_tables() {
    for (unsigned value=0; value<256; ++value) {
        for (unsigned width=0; width<8; ++width) {
            const auto byte=static_cast<std::uint8_t>(value);
            const auto shift=static_cast<std::uint8_t>(width);
            const auto expected_left=static_cast<std::uint8_t>((value<<width)&0xFFu);
            const auto expected_right=static_cast<std::uint8_t>(value>>width);
            const auto expected_rotate=static_cast<std::uint8_t>(((value<<width)|(value>>(8-width)))&0xFFu);
            require(bruce::apply_nanocell_byte_op_v1(byte,{bruce::NanoCellOpCodeV1::shl,shift})==expected_left,"SHL truth table");
            require(bruce::apply_nanocell_byte_op_v1(byte,{bruce::NanoCellOpCodeV1::shr,shift})==expected_right,"SHR truth table");
            require(bruce::apply_nanocell_byte_op_v1(byte,{bruce::NanoCellOpCodeV1::rotl,shift})==expected_rotate,"ROTL truth table");
        }
    }
    for (const auto code : {bruce::NanoCellOpCodeV1::shl,bruce::NanoCellOpCodeV1::shr,bruce::NanoCellOpCodeV1::rotl}) {
        for (unsigned invalid : {8u,255u}) {
            require_error([&] { (void)bruce::apply_nanocell_byte_op_v1(0xA5,{code,static_cast<std::uint8_t>(invalid)}); },"shift/rotate accepted width outside 0..7");
        }
    }
}

void reference_techniques() {
    using namespace bruce;
    const auto masked=nanocell_reference_technique_v1(NanoCellReferenceTechniqueV1::masked_match);
    const auto rotated=nanocell_reference_technique_v1(NanoCellReferenceTechniqueV1::rotate_match);
    const auto conflict=nanocell_reference_technique_v1(NanoCellReferenceTechniqueV1::context_conflict);
    require(masked.technique_id=="masked-match-v1" && masked.technique_version=="nanocell-technique-v1","masked identity");
    require(masked.program.size()==1 && masked.program[0].code==NanoCellOpCodeV1::bit_and && masked.program[0].operand==0xF0,"masked program");
    require(masked.match_mask==0xF0 && masked.match_value==0xA0,"masked match fields");
    require(rotated.technique_id=="rotate-match-v1" && rotated.technique_version=="nanocell-technique-v1","rotate identity");
    require(rotated.program.size()==2 && rotated.program[0].code==NanoCellOpCodeV1::rotl && rotated.program[0].operand==1 && rotated.program[1].code==NanoCellOpCodeV1::bit_and && rotated.program[1].operand==0x0F,"rotate program");
    require(rotated.match_mask==0x0F && rotated.match_value==0x05,"rotate match fields");
    require(conflict.technique_id=="context-conflict-v1" && conflict.technique_version=="nanocell-technique-v1","conflict identity");
    require(conflict.program.size()==1 && conflict.program[0].code==NanoCellOpCodeV1::bit_and && conflict.program[0].operand==0xF0 && conflict.match_mask==0xF0 && conflict.match_value==0xA0,"conflict program differs from masked");
    require(masked.required_context_digest==sha256("nanocell-reference-context-v1"),"masked context digest");
    require(rotated.required_context_digest==masked.required_context_digest,"rotate context digest");
    require(conflict.required_context_digest==sha256("nanocell-reference-context-conflict-v1"),"conflict context digest");
    require(conflict.required_context_digest!=masked.required_context_digest,"conflict context must differ");
    require_error([] { (void)nanocell_reference_technique_v1(static_cast<NanoCellReferenceTechniqueV1>(255)); },"unknown reference technique accepted");
}
}

int main() {
    try {
        binary_truth_tables();
        unary_truth_tables();
        shift_rotation_truth_tables();
        reference_techniques();
        fixed_digest_contract_vector();
        executor_outcomes_and_trace();
        executor_budget_and_cancel();
        executor_uses_request_snapshot_across_callback();
        executor_validation();
        executor_provenance();
        replay_outcomes_and_binary_fields();
        replay_rejects_manipulation();
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
