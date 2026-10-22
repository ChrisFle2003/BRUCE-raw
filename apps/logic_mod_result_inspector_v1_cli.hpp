#pragma once

#include "bruce/core/types.hpp"
#include "bruce/mod/logic_mod_result_inspector_v1.hpp"
#include <iostream>
#include <span>
#include <vector>

namespace bruce {
namespace logic_mod_result_inspector_cli_detail {
struct Arguments { std::string record, store; };
inline Arguments parse(std::span<const std::string_view> args) {
    const auto path = [](std::string_view value) {
        return !value.empty() && !value.starts_with("--") &&
            value.find_first_not_of(" \t\r\n\v\f") != std::string_view::npos;
    };
    if (args.size() != 3 || !path(args[0]) || args[1] != "--store" || !path(args[2]))
        throw Error("InvalidLogicModResultInspectArguments");
    return {std::string(args[0]), std::string(args[2])};
}
}

inline int logic_mod_result_inspector_v1_cli(int argc, char** argv) {
    std::vector<std::string_view> args;
    for (int i = 2; i < argc; ++i) args.emplace_back(argv[i]);
    const auto options = logic_mod_result_inspector_cli_detail::parse(args);
    LogicModResultInspectionV1 report;
    try {
        report = inspect_logic_mod_result_v1(read_logic_mod_result_record_file_v1(options.record), options.store);
    } catch (const Error&) {
        // Unsafe or unreadable record files have the same sanitized response
        // as a malformed record. Never reflect file diagnostics or local paths.
    }
    std::cout << render_logic_mod_result_inspection_v1(report);
    return 0;
}
}
