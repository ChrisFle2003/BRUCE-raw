#undef json_string
#include "bruce/core/json.hpp"
#include <new>

namespace bruce {
// The test target renames only the builder translation unit's json_string calls.
// Core package construction remains real; the second report field fails.
std::string json_string_report_fault(std::string_view value) {
    static unsigned calls = 0;
    if (++calls == 2) throw std::bad_alloc();
    return json_string(value);
}
}
