#include "bruce/dag_scheduler/dag_scheduler_v1.hpp"
#include "bruce/core/types.hpp"

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
void require_identifier(std::string_view id) {
    if (!identifier(id)) throw Error("InvalidDagSchedulerIdentifier");
}
const DagSchedulerCallbackEntryV1* callback(
    DagSchedulerCallbackRegistryV1 registry, std::string_view id) {
    const auto entry = std::lower_bound(registry.begin(), registry.end(), id,
        [](const auto& candidate, std::string_view key) { return candidate.callback_id < key; });
    return entry != registry.end() && entry->callback_id == id ? &*entry : nullptr;
}
}

void validate_dag_scheduler_spec_v1(
    const DagSchedulerSpecV1& spec, DagSchedulerCallbackRegistryV1 registry) {
    if (spec.scheduler_version != "dag-scheduler-v1")
        throw Error("InvalidDagSchedulerIdentifier");
    require_identifier(spec.dag_id);
    if (registry.empty()) throw Error("InvalidDagSchedulerCallbackRegistry");
    std::string_view previous;
    for (const auto& entry : registry) {
        if (!identifier(entry.callback_id)) throw Error("InvalidDagSchedulerCallbackRegistry");
        if (!entry.execute || (!previous.empty() && entry.callback_id <= previous))
            throw Error("InvalidDagSchedulerCallbackRegistry: " + std::string(entry.callback_id));
        previous = entry.callback_id;
    }
    if (spec.nodes.empty() || spec.nodes.size() > 64) throw Error("InvalidDagSchedulerSpec");
    if (spec.max_started_nodes == 0 || spec.max_started_nodes > 64)
        throw Error("InvalidDagSchedulerStartBudget");

    std::map<std::string, const DagSchedulerNodeSpecV1*> nodes;
    for (const auto& node : spec.nodes) {
        require_identifier(node.node_id);
        require_identifier(node.callback_id);
        if (!nodes.emplace(node.node_id, &node).second)
            throw Error("DuplicateDagSchedulerNode: " + node.node_id);
        if (!callback(registry, node.callback_id))
            throw Error("UnknownDagSchedulerCallback: " + node.callback_id);
    }
    std::map<std::string, std::size_t> remaining;
    std::map<std::string, std::vector<std::string>> successors;
    std::set<std::string> ready;
    for (const auto& [id, node] : nodes) {
        if (node->dependencies.size() > 15)
            throw Error("InvalidDagSchedulerDependency: " + id);
        std::set<std::string> unique;
        for (const auto& dependency : node->dependencies) {
            require_identifier(dependency);
            if (dependency == id || !unique.insert(dependency).second)
                throw Error("InvalidDagSchedulerDependency: " + id);
            if (!nodes.contains(dependency))
                throw Error("UnknownDagSchedulerDependency: " + dependency);
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
        throw Error("DagSchedulerCycle: " + unresolved->first);
    }
}

DagSchedulerRunV1 execute_dag_scheduler_v1(
    const DagSchedulerSpecV1& spec, DagSchedulerCallbackRegistryV1 registry,
    const DagSchedulerControlV1& control) {
    validate_dag_scheduler_spec_v1(spec, registry);
    DagSchedulerRunV1 run{spec, {}, {{DagSchedulerTraceEventV1::validated, ""}}};
    std::map<std::string, const DagSchedulerNodeSpecV1*> nodes;
    std::map<std::string, std::optional<DagSchedulerReceiptV1>> terminal;
    for (const auto& node : spec.nodes) {
        nodes.emplace(node.node_id, &node);
        terminal.emplace(node.node_id, std::nullopt);
    }
    std::size_t settled = 0;
    std::uint64_t started = 0;
    auto publish = [&](const DagSchedulerNodeSpecV1& node,
                       DagSchedulerReceiptOutcomeV1 outcome, DagSchedulerTraceEventV1 event,
                       std::string code, std::vector<DagSchedulerInputV1> inputs = {}) {
        terminal.at(node.node_id) = DagSchedulerReceiptV1{node.node_id, node.callback_id,
            std::move(inputs), outcome, std::move(code)};
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
                        return receipt && receipt->outcome != DagSchedulerReceiptOutcomeV1::ready;
                    });
                if (bad_predecessor) {
                    publish(*node, DagSchedulerReceiptOutcomeV1::blocked,
                        DagSchedulerTraceEventV1::blocked, "DEPENDENCY_NOT_READY");
                    blocked = true;
                }
            }
        } while (blocked);
        if (settled == nodes.size()) break;

        const bool cancelled = control.cancelled && control.cancelled();
        if (cancelled || started >= spec.max_started_nodes) {
            for (const auto& [id, node] : nodes) {
                if (!terminal.at(id))
                    publish(*node, DagSchedulerReceiptOutcomeV1::cancelled,
                        DagSchedulerTraceEventV1::cancelled,
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
                    return receipt && receipt->outcome == DagSchedulerReceiptOutcomeV1::ready;
                })) ready.insert(id);
        }
        const auto& node = *nodes.at(*ready.begin());
        std::vector<DagSchedulerInputV1> inputs;
        for (const auto& dependency : node.dependencies) {
            const auto& receipt = *terminal.at(dependency);
            inputs.push_back({dependency, receipt.terminal_code});
        }
        std::sort(inputs.begin(), inputs.end(), [](const auto& left, const auto& right) {
            return left.producer_node_id < right.producer_node_id;
        });
        run.trace.push_back({DagSchedulerTraceEventV1::started, node.node_id});
        ++started;
        const auto result = callback(registry, node.callback_id)->execute(inputs);
        if (!identifier(result.result_code) ||
            (result.status != DagSchedulerCallbackStatusV1::ready &&
             result.status != DagSchedulerCallbackStatusV1::failed))
            throw Error("InvalidDagSchedulerCallbackResult: " + node.node_id);
        const bool successful = result.status == DagSchedulerCallbackStatusV1::ready;
        publish(node, successful ? DagSchedulerReceiptOutcomeV1::ready : DagSchedulerReceiptOutcomeV1::failed,
            successful ? DagSchedulerTraceEventV1::ready : DagSchedulerTraceEventV1::failed,
            result.result_code, std::move(inputs));
    }
    for (auto& [id, receipt] : terminal) {
        (void)id;
        run.receipts.push_back(std::move(*receipt));
    }
    run.trace.push_back({DagSchedulerTraceEventV1::completed, ""});
    return run;
}
}
