#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {
enum class NanoCellOpCodeV1 { bit_and, bit_or, bit_xor, bit_not, rotl, shl, shr };

struct NanoCellByteOpV1 {
    NanoCellOpCodeV1 code;
    std::uint8_t operand{};
};

std::uint8_t and8(std::uint8_t a, std::uint8_t b);
std::uint8_t or8(std::uint8_t a, std::uint8_t b);
std::uint8_t xor8(std::uint8_t a, std::uint8_t b);
std::uint8_t apply_nanocell_byte_op_v1(std::uint8_t state, const NanoCellByteOpV1& op);

struct NanoNeuronCoreV1 {
    std::array<std::uint8_t,8> core_bytes{};
    std::uint8_t active_index{};
    std::string core_layout_version{"nanoneuron-core-v1"};
};

struct NanoCellContextV1 {
    std::string context_id;
    std::string context_bytes;
    std::string context_digest;
};

struct NanoCellTechniqueDefinitionV1 {
    std::string technique_id;
    std::string technique_version{"nanocell-technique-v1"};
    std::string required_context_digest;
    std::vector<NanoCellByteOpV1> program;
    std::uint8_t match_mask{};
    std::uint8_t match_value{};
};

enum class NanoCellReferenceTechniqueV1 { masked_match, rotate_match, context_conflict };
NanoCellTechniqueDefinitionV1 nanocell_reference_technique_v1(NanoCellReferenceTechniqueV1 technique);

enum class NanoCellStatusV1 { matched, no_match, context_rejected, incomplete };
enum class NanoCellTraceKindV1 { start, operation, terminal };

struct NanoCellTraceEntryV1 {
    NanoCellTraceKindV1 kind;
    std::uint64_t operations_used;
    std::uint8_t state;
};

struct NanoCellRequestV1 {
    std::string request_version{"nanocell-request-v1"};
    NanoNeuronCoreV1 core;
    NanoCellContextV1 context;
    NanoCellTechniqueDefinitionV1 technique;
    std::uint64_t max_operations{};
    std::uint64_t max_trace_entries{};
};

struct NanoCellAnswerPointV1 {
    std::string answer_version{"nanocell-answer-v1"};
    NanoCellStatusV1 status;
    std::optional<std::uint8_t> final_state;
    std::string technique_id;
    std::string technique_version;
    std::string core_layout_version;
    std::uint8_t active_index{};
    std::string core_digest;
    std::string context_id;
    std::string context_digest;
    std::string technique_digest;
    std::uint64_t operations_used{};
    std::vector<NanoCellTraceEntryV1> trace;
};

struct NanoCellControlV1 { std::function<bool()> cancelled; };
struct NanoCellRunV1 { NanoCellRequestV1 request; NanoCellAnswerPointV1 answer; };
NanoCellRunV1 execute_nanocell_v1(const NanoCellRequestV1& request, const NanoCellControlV1& control = {});
std::string pack_nanocell_run_v1(const NanoCellRunV1& run);
NanoCellRunV1 replay_nanocell_run_v1(std::string_view bundle);

struct NanoCellModRunV1 { NanoCellRunV1 nanocell; };
NanoCellModRunV1 execute_nanocell_mod_v1(const NanoCellRequestV1& request,
                                          const NanoCellControlV1& control = {});
std::string pack_nanocell_mod_run_v1(const NanoCellModRunV1& run);
NanoCellModRunV1 replay_nanocell_mod_run_v1(std::string_view bundle);
}
