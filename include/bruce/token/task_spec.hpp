#pragma once
#include "bruce/application/exact_research.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {
inline constexpr std::string_view task_spec_version_v1="tokenized-task-spec-v1";
inline constexpr std::string_view token_normalization_version_v1="ascii-whitespace-case-v1";

struct TokenEntryV1 { std::string id,visible_text,normalized_text; std::uint64_t ordinal=0; };
struct TokenTableV1 { std::string version; std::vector<TokenEntryV1> entries; };
struct TokenizedAssetV1 { std::string alias,bytes,digest; };
struct TokenizedRoleV1 { std::string role,token_id,asset_alias; };
struct TokenizedAlternativeV1 { std::string id; std::vector<TokenizedRoleV1> roles; };
struct TokenizedTaskSpecV1 {
    std::string task_spec_version=std::string(task_spec_version_v1);
    std::string normalization_version=std::string(token_normalization_version_v1);
    TokenTableV1 table;
    std::string table_digest;
    std::vector<std::string> ordered_token_ids;
    std::vector<TokenizedAssetV1> assets;
    std::vector<TokenizedRoleV1> roles;
    std::vector<TokenizedAlternativeV1> alternatives;
    std::string selected_id;
    std::string route_id,domain;
    std::string minimum="-8",maximum="8";
    Budget budget;
    unsigned max_bits=4096,max_denominator=8;
    unsigned max_power=4,max_degree=3;
    std::uint64_t max_trace_bytes=1048576;
};

TokenizedTaskSpecV1 make_tokenized_task_spec(TokenizedTaskSpecV1 spec);
std::string pack_tokenized_task_spec(const TokenizedTaskSpecV1& spec);
TokenizedTaskSpecV1 unpack_tokenized_task_spec(std::string_view bytes);
ExactResearchTask compile_tokenized_task_spec(const TokenizedTaskSpecV1& spec);
}
