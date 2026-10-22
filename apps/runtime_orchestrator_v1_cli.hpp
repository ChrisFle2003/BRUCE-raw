#pragma once

#include "logic_mod_artifact_dag_v6_cli.hpp"
#include "bruce/mod/logic_mod_artifact_dag_v6_plan.hpp"
#include "bruce/runtime/admission_diagnostics_v1.hpp"
#include "bruce/runtime/runtime_orchestrator_v1.hpp"
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <charconv>
#include <cerrno>
#include <map>
#include <chrono>
#include <thread>

namespace bruce {
namespace runtime_orchestrator_v1_cli_detail {
inline constexpr std::size_t line_limit = 65536;
inline constexpr std::string_view contract = "runtime-orchestrator-v1";

[[noreturn]] inline void protocol_error() { throw Error("RuntimeOrchestratorProtocolError"); }
inline std::uint64_t number(const logic_mod_dag_cli_detail::Json& value) {
    if (value.kind != logic_mod_dag_cli_detail::Json::number || value.text.empty()) protocol_error();
    std::uint64_t out{};
    const auto parsed = std::from_chars(value.text.data(), value.text.data() + value.text.size(), out);
    if (parsed.ec != std::errc{} || parsed.ptr != value.text.data() + value.text.size()) protocol_error();
    return out;
}
inline std::string string(const logic_mod_dag_cli_detail::Json& value) {
    if (value.kind != logic_mod_dag_cli_detail::Json::string || value.text.empty() || value.text.size() > 4096) protocol_error();
    return value.text;
}
inline const logic_mod_dag_cli_detail::Json& value_for(const logic_mod_dag_cli_detail::Json& object, std::string_view key) {
    if (object.kind != logic_mod_dag_cli_detail::Json::object) protocol_error();
    const auto found = object.fields.find(std::string(key));
    if (found == object.fields.end()) protocol_error();
    return found->second;
}
inline const logic_mod_dag_cli_detail::Json* optional(const logic_mod_dag_cli_detail::Json& object, std::string_view key) {
    if (object.kind != logic_mod_dag_cli_detail::Json::object) protocol_error();
    const auto found = object.fields.find(std::string(key));
    return found == object.fields.end() ? nullptr : &found->second;
}
inline void exact_keys(const logic_mod_dag_cli_detail::Json& object, std::initializer_list<std::string_view> required,
    std::initializer_list<std::string_view> allowed = {}) {
    if (object.kind != logic_mod_dag_cli_detail::Json::object) protocol_error();
    for (const auto key : required) (void)field(object, key);
    for (const auto& [key, ignored] : object.fields) {
        (void)ignored;
        if (std::find(required.begin(), required.end(), key) == required.end() &&
            std::find(allowed.begin(), allowed.end(), key) == allowed.end()) protocol_error();
    }
}
inline bool opaque_id(std::string_view value) {
    return !value.empty() && value.size() <= 64 && std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    });
}
inline std::string error_reply(std::string_view code) {
    return "{\"contract\":\"runtime-orchestrator-v1\",\"ok\":false,\"error\":" + json_string(code) + "}";
}
inline std::string status_reply(std::string_view action, const RuntimeOrchestratorStatusV1& status) {
    return "{\"contract\":\"runtime-orchestrator-v1\",\"ok\":true,\"action\":" + json_string(action) +
        ",\"status\":" + runtime_orchestrator_status_v1(status) + "}";
}
inline std::string admission_status_reply(const RuntimeOrchestratorStatusV1& status) {
    return "{\"contract\":\"runtime-orchestrator-v1\",\"ok\":true,\"action\":\"admission-status\",\"admission_status\":" +
        render_admission_diagnostics_v1(admission_diagnostics_v1(status)) + "}";
}
inline RuntimeOrchestratorLimitsV1 parse_limits(int argc, char** argv) {
    if (argc != 13 || std::string_view(argv[2]) != "serve") throw Error("InvalidRuntimeOrchestratorArguments");
    const std::array<std::string_view, 5> names{"--max-queued-runs", "--max-active-runs", "--max-reserved-parallel-nodes", "--max-reserved-started-workers", "--max-reserved-payload-bytes"};
    std::map<std::string_view, std::uint64_t> values;
    for (int index = 3; index < argc; index += 2) {
        const std::string_view name(argv[index]), value(argv[index + 1]);
        if (std::find(names.begin(), names.end(), name) == names.end() || !values.emplace(name, 0).second ||
            value.empty() || (value.size() > 1 && value.front() == '0')) throw Error("InvalidRuntimeOrchestratorArguments");
        const auto result = std::from_chars(value.data(), value.data() + value.size(), values.at(name));
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || !values.at(name)) throw Error("InvalidRuntimeOrchestratorArguments");
    }
    if (values.size() != names.size()) throw Error("InvalidRuntimeOrchestratorArguments");
    return {values.at(names[0]), values.at(names[1]), values.at(names[2]), values.at(names[3]), values.at(names[4])};
}

inline LogicModArtifactDagV6Plan make_plan(const logic_mod_dag_cli_detail::Json& input,
    RuntimeOrchestratorReservationV1& reservation) {
    exact_keys(input, {"action", "spec", "store", "record", "report", "max_parallel_nodes", "max_total_wall_ms",
        "max_total_payload_bytes", "max_started_workers", "reservation_max_parallel_nodes",
        "reservation_max_total_wall_ms", "reservation_max_total_payload_bytes", "reservation_max_started_workers"},
        {"cancel_after_ms", "remote_restore"});
    const auto spec_path = string(field(input, "spec"));
    const auto store = string(field(input, "store"));
    const auto record = string(field(input, "record"));
    LogicModArtifactDagControlV6 control;
    control.report_root = string(field(input, "report"));
    const auto parallel = number(field(input, "max_parallel_nodes"));
    const auto wall = number(field(input, "max_total_wall_ms"));
    const auto payload = number(field(input, "max_total_payload_bytes"));
    const auto workers = number(field(input, "max_started_workers"));
    if (!parallel || parallel > 4 || !wall || wall > 60000 || payload > 8388608 || !workers || workers > 16) protocol_error();
    control.max_parallel_nodes = static_cast<std::uint32_t>(parallel);
    control.max_total_wall_ms = wall;
    control.max_total_payload_bytes = payload;
    control.max_started_workers = static_cast<std::uint32_t>(workers);
    if (const auto* cancel = optional(input, "cancel_after_ms")) {
        const auto value = number(*cancel); if (!value || value > 60000) protocol_error(); control.cancel_after_ms = value;
    }
    if (const auto* remote = optional(input, "remote_restore")) control.remote_restore = string(*remote);
    reservation = {number(field(input, "reservation_max_parallel_nodes")), number(field(input, "reservation_max_total_wall_ms")),
        number(field(input, "reservation_max_started_workers")), number(field(input, "reservation_max_total_payload_bytes"))};
    if (reservation.max_parallel_nodes != parallel || reservation.max_total_wall_ms != wall ||
        reservation.max_started_workers != workers || reservation.max_total_payload_bytes != payload) protocol_error();

    // All command structure and numeric ranges above are checked before any submitted path is opened.
    const auto spec = logic_mod_dag_cli_detail::parse_spec(logic_mod_chain_cli_detail::read_file(spec_path, logic_mod_dag_cli_detail::spec_limit));
    auto session = std::make_shared<LogicModArtifactDagV6PlanSession>();
    for (const auto& node : spec.nodes) {
        const auto trust = read_runtime_mod_truststore_v1(logic_mod_chain_cli_detail::read_file(node.trust, 65536));
        const auto package = unpack_logic_mod_package_v1(logic_mod_chain_cli_detail::read_file(node.package, 1024 * 1024));
        verify_logic_mod_package_v1(package, trust);
        auto directory = std::make_shared<logic_mod_cli_detail::PackageDirectory>();
        auto registry = std::make_shared<LogicModRegistryV1>();
        logic_mod_chain_cli_detail::activate(*registry, package, trust, *directory);
        const auto registry_index = session->registries.size();
        session->registries.push_back(registry);
        session->keepalive.push_back(directory);
        LogicModArtifactDagV6PlanNode prepared;
        prepared.registry_index = registry_index;
        prepared.id = node.id; prepared.mod_id = package.manifest().mod_id;
        prepared.version = package.manifest().version; prepared.template_id = "math.rational.power.v1";
        prepared.input = node.input; prepared.predecessors = node.predecessors;
        if (node.internal) prepared.input_artifact = LogicModOutputV1{store, *node.internal};
        prepared.output_artifact = LogicModOutputV1{store, node.output}; prepared.budgets = node.budgets;
        session->nodes.push_back(std::move(prepared));
    }
    return prepare_logic_mod_artifact_dag_v6_plan({std::move(session), store, record, spec.digest,
        [record](std::string_view bytes) { logic_mod_dag_cli_detail::publish_record(record, bytes); }, std::move(control)});
}

inline std::string dispatch(RuntimeOrchestratorV1& host, std::string_view line) {
    if (line.empty() || line.size() > line_limit) return error_reply("INVALID_COMMAND");
    try {
        const auto request = logic_mod_dag_cli_detail::Parser(line).parse();
        if (logic_mod_dag_cli_detail::canonical(request) != line) protocol_error();
        exact_keys(request, {"action"}, {"spec", "store", "record", "report", "max_parallel_nodes", "max_total_wall_ms",
            "max_total_payload_bytes", "max_started_workers", "reservation_max_parallel_nodes", "reservation_max_total_wall_ms",
            "reservation_max_total_payload_bytes", "reservation_max_started_workers", "cancel_after_ms", "remote_restore", "run_id"});
        const auto action = string(field(request, "action"));
        if (action == "submit") {
            // Reject a full queue before constructing a filesystem path or
            // preparing package/trust/session material from the request.
            if (!host.has_queue_capacity()) return error_reply("QUEUE_FULL");
            RuntimeOrchestratorReservationV1 reservation;
            auto plan = make_plan(request, reservation);
            const auto id = host.submit(std::move(plan), reservation);
            host.poll();
            return status_reply(action, host.status(id));
        }
        if (action == "status" || action == "cancel") {
            exact_keys(request, {"action", "run_id"});
            const auto id = string(field(request, "run_id"));
            if (!opaque_id(id)) protocol_error();
            if (action == "cancel" && !host.cancel(id)) return error_reply("RUN_NOT_FOUND");
            return status_reply(action, host.status(id));
        }
        if (action == "admission-status") {
            // The action is intentionally host.status-only: its exact request
            // shape contains no V6 plan or path field and it opens no file.
            exact_keys(request, {"action", "run_id"});
            const auto id = string(field(request, "run_id"));
            if (!opaque_id(id)) protocol_error();
            try {
                return admission_status_reply(host.status(id));
            } catch (const Error& error) {
                if (std::string_view(error.what()) == "RuntimeOrchestratorRunNotFound") return error_reply("RUN_NOT_FOUND");
                throw;
            }
        }
        return error_reply("UNKNOWN_ACTION");
    } catch (const Error& error) {
        if (std::string_view(error.what()) == "RuntimeOrchestratorQueueFull") return error_reply("QUEUE_FULL");
        return error_reply("INVALID_COMMAND");
    } catch (...) { return error_reply("INVALID_COMMAND"); }
}
inline bool all_terminal(const RuntimeOrchestratorV1& host) {
    for (const auto& status : host.statuses()) {
        if (status.state != RuntimeOrchestratorStateV1::SUCCEEDED && status.state != RuntimeOrchestratorStateV1::FAILED &&
            status.state != RuntimeOrchestratorStateV1::BLOCKED && status.state != RuntimeOrchestratorStateV1::CANCELLED) return false;
    }
    return true;
}
}

inline int runtime_orchestrator_v1_cli(int argc, char** argv) {
    using namespace runtime_orchestrator_v1_cli_detail;
    const auto limits = parse_limits(argc, argv);
    RuntimeOrchestratorV1 host(limits);
    const int previous = fcntl(STDIN_FILENO, F_GETFL);
    if (previous < 0 || fcntl(STDIN_FILENO, F_SETFL, previous | O_NONBLOCK) != 0) throw Error("RuntimeOrchestratorInputUnavailable");
    bool eof = false;
    bool discarding_oversized_line = false;
    std::string buffer;
    // Consume incrementally: buffer never exceeds line_limit, and a rejected
    // oversized line produces exactly one response before its delimiter is
    // discarded. This also prevents a tail in the same read from becoming a
    // fresh command.
    const auto consume = [&](char character) {
        if (discarding_oversized_line) {
            if (character == '\n') discarding_oversized_line = false;
            return;
        }
        if (character == '\n') {
            std::cout << dispatch(host, buffer) << '\n' << std::flush;
            buffer.clear();
            host.poll();
            return;
        }
        if (buffer.size() == line_limit) {
            std::cout << error_reply("INVALID_COMMAND") << '\n' << std::flush;
            buffer.clear();
            discarding_oversized_line = true;
            return;
        }
        buffer.push_back(character);
    };
    while (!eof || !all_terminal(host)) {
        host.poll();
        int ready = 0;
        pollfd descriptor{STDIN_FILENO, POLLIN | POLLHUP, 0};
        if (!eof) {
            ready = ::poll(&descriptor, 1, 10);
            if (ready < 0) { if (errno == EINTR) continue; throw Error("RuntimeOrchestratorInputUnavailable"); }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!eof && ready > 0 && (descriptor.revents & (POLLIN | POLLHUP))) {
            char bytes[4096];
            for (;;) {
                const auto read_count = ::read(STDIN_FILENO, bytes, sizeof(bytes));
                if (read_count > 0) {
                    for (ssize_t index = 0; index < read_count; ++index) consume(bytes[index]);
                    // A peer may stream a discarded oversized line forever;
                    // keep joining/completing V6 work between bounded reads.
                    host.poll();
                    continue;
                }
                if (!read_count) { eof = true; break; }
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                throw Error("RuntimeOrchestratorInputUnavailable");
            }
        }
        // EOF is a final bounded line delimiter. A prior oversized line has
        // already received its one closed error and stays discarded.
        if (eof && !discarding_oversized_line && !buffer.empty()) {
            std::cout << dispatch(host, buffer) << '\n' << std::flush;
            buffer.clear();
            host.poll();
        }
    }
    return 0;
}
}
