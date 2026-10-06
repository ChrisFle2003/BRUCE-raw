#pragma once
#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/mods/nanocell/nanocell.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace bruce {
namespace nanocell_cli_detail {
using Options=std::map<std::string,std::string>;

inline Options parse_options(int argc,char** argv) {
    static const std::set<std::string> allowed={"--core","--active-index","--context-id",
        "--context-file","--technique","--max-operations","--max-trace-entries","--output"};
    Options result;
    for(int i=3;i<argc;i+=2) {
        const std::string key=argv[i];
        if(!allowed.contains(key)) throw Error("UnknownOption: "+key);
        if(i+1>=argc) throw Error("MissingOptionValue: "+key);
        if(!result.emplace(key,argv[i+1]).second) throw Error("DuplicateOption: "+key);
    }
    return result;
}

inline std::string required(const Options& options,const std::string& key) {
    const auto it=options.find(key);
    if(it==options.end() || it->second.empty()) throw Error("MissingOption: "+key);
    return it->second;
}

inline std::string optional(const Options& options,const std::string& key,std::string fallback) {
    const auto it=options.find(key);
    return it==options.end() ? fallback : it->second;
}

inline unsigned hex_digit(char value) {
    if(value>='0' && value<='9') return static_cast<unsigned>(value-'0');
    if(value>='a' && value<='f') return static_cast<unsigned>(value-'a'+10);
    if(value>='A' && value<='F') return static_cast<unsigned>(value-'A'+10);
    throw Error("InvalidNanoCellCoreHex");
}

inline std::array<std::uint8_t,8> parse_core(std::string_view hex) {
    if(hex.size()!=16) throw Error("InvalidNanoCellCoreHex");
    std::array<std::uint8_t,8> bytes{};
    for(std::size_t i=0;i<bytes.size();++i)
        bytes[i]=static_cast<std::uint8_t>((hex_digit(hex[2*i])<<4)|hex_digit(hex[2*i+1]));
    return bytes;
}

inline NanoCellReferenceTechniqueV1 technique(std::string_view id) {
    if(id=="masked-match-v1") return NanoCellReferenceTechniqueV1::masked_match;
    if(id=="rotate-match-v1") return NanoCellReferenceTechniqueV1::rotate_match;
    if(id=="context-conflict-v1") return NanoCellReferenceTechniqueV1::context_conflict;
    throw Error("UnknownNanoCellTechnique");
}

inline std::string_view status(NanoCellStatusV1 value) {
    switch(value) {
    case NanoCellStatusV1::matched: return "MATCHED";
    case NanoCellStatusV1::no_match: return "NO_MATCH";
    case NanoCellStatusV1::context_rejected: return "CONTEXT_REJECTED";
    case NanoCellStatusV1::incomplete: return "INCOMPLETE";
    }
    throw Error("InvalidNanoCellStatus");
}

inline std::string_view trace_kind(NanoCellTraceKindV1 value) {
    switch(value) {
    case NanoCellTraceKindV1::start: return "START";
    case NanoCellTraceKindV1::operation: return "OPERATION";
    case NanoCellTraceKindV1::terminal: return "TERMINAL";
    }
    throw Error("InvalidNanoCellTraceKind");
}

inline std::string render(const NanoCellRunV1& run) {
    const auto& answer=run.answer;
    std::string result="{\"contract\":\"nanocell-cli-v1\",\"status\":"+json_string(status(answer.status))+
        ",\"final_state\":"+(answer.final_state ? std::to_string(*answer.final_state) : "null")+
        ",\"technique_id\":"+json_string(answer.technique_id)+
        ",\"technique_digest\":"+json_string(answer.technique_digest)+
        ",\"core_digest\":"+json_string(answer.core_digest)+
        ",\"context_id\":"+json_string(answer.context_id)+
        ",\"context_digest\":"+json_string(answer.context_digest)+
        ",\"active_index\":"+std::to_string(answer.active_index)+
        ",\"operations_used\":"+std::to_string(answer.operations_used)+
        ",\"max_operations\":"+std::to_string(run.request.max_operations)+
        ",\"trace_entries_used\":"+std::to_string(answer.trace.size())+
        ",\"max_trace_entries\":"+std::to_string(run.request.max_trace_entries)+
        ",\"trace\":[";
    for(std::size_t i=0;i<answer.trace.size();++i) {
        if(i) result+=',';
        const auto& entry=answer.trace[i];
        result+="{\"kind\":"+json_string(trace_kind(entry.kind))+
            ",\"operations_used\":"+std::to_string(entry.operations_used)+
            ",\"state\":"+std::to_string(entry.state)+"}";
    }
    return result+"]}";
}
}

inline int nanocell_cli(int argc,char** argv) {
    if(argc<3) throw Error("Usage: bruce nanocell run|replay ...");
    const std::string command=argv[2];
    if(command=="replay") {
        if(argc!=4) throw Error("Usage: bruce nanocell replay FILE");
        // Direct V1 remains capped at its established 5703-byte ceiling. The
        // outer Mod envelope also contains a canonical V1 request (at most
        // 4800 bytes), so 12 KiB admits both bounded V1 components and fixed
        // manifest/digest metadata without inheriting Core's broad envelope cap.
        constexpr std::size_t direct_v1_run_limit=5703;
        constexpr std::size_t mod_run_limit=12*1024;
        constexpr std::string_view direct_v1_header="BRUCE_NANOCELL_RUN_V1\n";
        const auto bundle=read_bounded(argv[3],mod_run_limit);
        const auto run=[&]() {
            if(bundle.starts_with(direct_v1_header)) {
                if(bundle.size()>direct_v1_run_limit) throw Error("NanoCell direct V1 run exceeds byte limit");
                return replay_nanocell_run_v1(bundle);
            }
            return replay_nanocell_mod_run_v1(bundle).nanocell;
        }();
        std::cout<<nanocell_cli_detail::render(run)<<'\n';
        return 0;
    }
    if(command!="run") throw Error("UnknownNanoCellCommand: "+command);
    const auto args=nanocell_cli_detail::parse_options(argc,argv);
    NanoCellRequestV1 request;
    request.core.core_bytes=nanocell_cli_detail::parse_core(nanocell_cli_detail::required(args,"--core"));
    const auto active_index=integer<std::uint64_t>(nanocell_cli_detail::required(args,"--active-index"));
    if(active_index>=request.core.core_bytes.size()) throw Error("Invalid NanoCell active index");
    request.core.active_index=static_cast<std::uint8_t>(active_index);
    request.context.context_id=nanocell_cli_detail::required(args,"--context-id");
    request.context.context_bytes=read_bounded(nanocell_cli_detail::required(args,"--context-file"),4096);
    request.context.context_digest=sha256(request.context.context_bytes);
    request.technique=nanocell_reference_technique_v1(
        nanocell_cli_detail::technique(nanocell_cli_detail::required(args,"--technique")));
    request.max_operations=integer<std::uint64_t>(nanocell_cli_detail::optional(args,"--max-operations","16"));
    request.max_trace_entries=integer<std::uint64_t>(nanocell_cli_detail::optional(args,"--max-trace-entries","18"));
    const auto mod_run=execute_nanocell_mod_v1(request);
    const auto bundle=pack_nanocell_mod_run_v1(mod_run);
    if(args.contains("--output")) LocalFiles::publish_new(nanocell_cli_detail::required(args,"--output"),bundle);
    std::cout<<nanocell_cli_detail::render(mod_run.nanocell)<<'\n';
    return 0;
}
}
