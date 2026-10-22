#include "bruce/dag_scheduler/dag_scheduler_v2.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace bruce {
namespace {
bool identifier(std::string_view id) {
    return !id.empty() && id.size() <= 64 &&
        std::all_of(id.begin(), id.end(), [](unsigned char c) {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_';
        });
}
void require_identifier(std::string_view id, std::string_view affected = {}) {
    if (!identifier(id)) throw Error("InvalidDagSchedulerV2Spec" +
        (identifier(affected) ? ": " + std::string(affected) : std::string{}));
}
const DagSchedulerCallbackEntryV2* callback(
    DagSchedulerCallbackRegistryV2 registry, std::string_view id) {
    const auto entry = std::lower_bound(registry.begin(), registry.end(), id,
        [](const auto& candidate, std::string_view key) { return candidate.callback_id < key; });
    return entry != registry.end() && entry->callback_id == id ? &*entry : nullptr;
}
}

void validate_dag_scheduler_spec_v2(
    const DagSchedulerSpecV2& spec, DagSchedulerCallbackRegistryV2 registry) {
    if (spec.scheduler_version != "dag-scheduler-v2")
        throw Error("InvalidDagSchedulerV2Spec" +
            (identifier(spec.dag_id) ? ": " + spec.dag_id : std::string{}));
    require_identifier(spec.dag_id);
    if (registry.empty()) throw Error("InvalidDagSchedulerV2CallbackRegistry");
    std::string_view previous;
    for (const auto& entry : registry) {
        if (!identifier(entry.callback_id)) throw Error("InvalidDagSchedulerV2CallbackRegistry");
        if (!entry.execute || (!previous.empty() && entry.callback_id <= previous))
            throw Error("InvalidDagSchedulerV2CallbackRegistry: " + std::string(entry.callback_id));
        previous = entry.callback_id;
    }
    if (spec.nodes.empty() || spec.nodes.size() > 64)
        throw Error("InvalidDagSchedulerV2Spec: " + spec.dag_id);
    if (spec.max_started_nodes == 0 || spec.max_started_nodes > 64 ||
        spec.max_total_receipt_bytes == 0 || spec.max_total_receipt_bytes > 4194304)
        throw Error("InvalidDagSchedulerV2Spec: " + spec.dag_id);

    std::map<std::string, const DagSchedulerNodeSpecV2*> nodes;
    for (const auto& node : spec.nodes) {
        require_identifier(node.node_id, spec.dag_id);
        require_identifier(node.callback_id, node.node_id);
        if (node.request_bytes.size() > 65536 || node.request_digest != sha256(node.request_bytes))
            throw Error("InvalidDagSchedulerV2Spec: " + node.node_id);
        if (!nodes.emplace(node.node_id, &node).second)
            throw Error("DuplicateDagSchedulerV2Node: " + node.node_id);
        if (!callback(registry, node.callback_id))
            throw Error("UnknownDagSchedulerV2Callback: " + node.callback_id);
    }
    std::map<std::string, std::size_t> remaining;
    std::map<std::string, std::vector<std::string>> successors;
    std::set<std::string> ready;
    for (const auto& [id, node] : nodes) {
        if (node->dependencies.size() > 15)
            throw Error("InvalidDagSchedulerV2Dependency: " + id);
        std::set<std::string> unique;
        for (const auto& dependency : node->dependencies) {
            require_identifier(dependency, id);
            if (dependency == id || !unique.insert(dependency).second)
                throw Error("InvalidDagSchedulerV2Dependency: " + id);
            if (!nodes.contains(dependency))
                throw Error("UnknownDagSchedulerV2Dependency: " + dependency);
            successors[dependency].push_back(id);
        }
        remaining[id] = node->dependencies.size();
        if (node->dependencies.empty()) ready.insert(id);
    }
    std::size_t visited = 0;
    while (!ready.empty()) {
        const auto id = *ready.begin();
        ready.erase(ready.begin());
        ++visited;
        for (const auto& successor : successors[id]) {
            if (--remaining.at(successor) == 0) ready.insert(successor);
        }
    }
    if (visited != nodes.size()) {
        const auto unresolved = std::find_if(remaining.begin(), remaining.end(),
            [](const auto& entry) { return entry.second != 0; });
        throw Error("DagSchedulerV2Cycle: " + unresolved->first);
    }
}

DagSchedulerRunV2 execute_dag_scheduler_v2(
    const DagSchedulerSpecV2& spec, DagSchedulerCallbackRegistryV2 registry,
    const DagSchedulerControlV2& control) {
    validate_dag_scheduler_spec_v2(spec, registry);
    DagSchedulerRunV2 run{spec, {}, {{DagSchedulerTraceEventV2::validated, ""}}};
    std::map<std::string, const DagSchedulerNodeSpecV2*> nodes;
    std::map<std::string, std::optional<DagSchedulerReceiptV2>> terminal;
    for (const auto& node : spec.nodes) {
        nodes.emplace(node.node_id, &node);
        terminal.emplace(node.node_id, std::nullopt);
    }
    std::size_t settled = 0;
    std::uint64_t started = 0, retained_bytes = 0;
    auto publish = [&](const DagSchedulerNodeSpecV2& node,
                       DagSchedulerReceiptOutcomeV2 outcome, DagSchedulerTraceEventV2 event,
                       std::string failure_code, std::vector<DagSchedulerInputV2> inputs = {},
                       std::optional<DagSchedulerOutputRefV2> output = {}) {
        // Account for every retained copy before publishing any part of this receipt.
        std::uint64_t bytes = output ? output->payload_bytes.size() : 0;
        for (const auto& input : inputs) bytes += input.payload_bytes.size();
        if (bytes > spec.max_total_receipt_bytes - retained_bytes)
            throw Error("DagSchedulerV2ReceiptBudgetExceeded: " + node.node_id);
        retained_bytes += bytes;
        terminal.at(node.node_id) = DagSchedulerReceiptV2{node.node_id, node.callback_id,
            node.request_digest, std::move(inputs), outcome, std::move(output), std::move(failure_code)};
        run.trace.push_back({event, node.node_id});
        ++settled;
    };
    while (settled != nodes.size()) {
        // Finish every transitive failure consequence before consulting controls.
        bool blocked;
        do {
            blocked = false;
            for (const auto& [id, node] : nodes) {
                if (terminal.at(id)) continue;
                const bool bad_predecessor = std::any_of(node->dependencies.begin(),
                    node->dependencies.end(), [&](const auto& dependency) {
                        const auto& receipt = terminal.at(dependency);
                        return receipt && receipt->outcome != DagSchedulerReceiptOutcomeV2::ready;
                    });
                if (bad_predecessor) {
                    publish(*node, DagSchedulerReceiptOutcomeV2::blocked,
                        DagSchedulerTraceEventV2::blocked, "DEPENDENCY_NOT_READY");
                    blocked = true;
                }
            }
        } while (blocked);
        if (settled == nodes.size()) break;

        const bool cancelled = control.cancelled && control.cancelled();
        if (cancelled || started >= spec.max_started_nodes) {
            for (const auto& [id, node] : nodes) {
                if (!terminal.at(id))
                    publish(*node, DagSchedulerReceiptOutcomeV2::cancelled,
                        DagSchedulerTraceEventV2::cancelled,
                        cancelled ? "CONTROLLED_CANCELLATION" : "START_BUDGET_EXHAUSTED");
            }
            break;
        }

        std::set<std::string> ready;
        for (const auto& [id, node] : nodes) {
            if (terminal.at(id)) continue;
            if (std::all_of(node->dependencies.begin(), node->dependencies.end(),
                [&](const auto& dependency) {
                    const auto& receipt = terminal.at(dependency);
                    return receipt && receipt->outcome == DagSchedulerReceiptOutcomeV2::ready;
                })) ready.insert(id);
        }
        const auto& node = *nodes.at(*ready.begin());
        std::vector<DagSchedulerInputV2> inputs;
        for (const auto& dependency : node.dependencies) {
            const auto& receipt = *terminal.at(dependency);
            const auto& output = *receipt.output;
            inputs.push_back({dependency, output.result_code, output.payload_bytes, output.payload_digest});
        }
        std::sort(inputs.begin(), inputs.end(), [](const auto& left, const auto& right) {
            return left.producer_node_id < right.producer_node_id;
        });
        run.trace.push_back({DagSchedulerTraceEventV2::started, node.node_id});
        ++started;
        auto result = callback(registry, node.callback_id)->execute(node.request_bytes, inputs);
        if (!identifier(result.result_code) ||
            (result.status != DagSchedulerCallbackStatusV2::ready &&
             result.status != DagSchedulerCallbackStatusV2::failed))
            throw Error("InvalidDagSchedulerV2CallbackResult: " + node.node_id);
        if (result.status == DagSchedulerCallbackStatusV2::ready) {
            if (result.payload_bytes.size() > 65536 || result.payload_digest != sha256(result.payload_bytes))
                throw Error("InvalidDagSchedulerV2CallbackResult: " + node.node_id);
            publish(node, DagSchedulerReceiptOutcomeV2::ready, DagSchedulerTraceEventV2::ready,
                "", std::move(inputs), DagSchedulerOutputRefV2{"dag-scheduler-output-v2",
                    std::move(result.result_code), std::move(result.payload_bytes), std::move(result.payload_digest)});
        } else {
            if (!result.payload_bytes.empty() || !result.payload_digest.empty())
                throw Error("InvalidDagSchedulerV2CallbackResult: " + node.node_id);
            publish(node, DagSchedulerReceiptOutcomeV2::failed, DagSchedulerTraceEventV2::failed,
                std::move(result.result_code));
        }
    }
    for (auto& [id, receipt] : terminal) {
        (void)id;
        run.receipts.push_back(std::move(*receipt));
    }
    run.trace.push_back({DagSchedulerTraceEventV2::completed, ""});
    return run;
}
}
