#include "mcp_inspector_assets.hpp"

namespace bruce {

const std::string& mcp_inspector_html() {
    static const std::string html = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>BRUCE MCP — Phase-DAG V2 Inspector</title>
<style>
:root { color-scheme: light; font-family: system-ui, sans-serif; color: #182735; background: #eef3f7; }
body { max-width: 1100px; margin: 0 auto; padding: 32px 20px; }
h1 { font-size: clamp(1.4rem, 3vw, 2rem); margin-bottom: 12px; }
p { line-height: 1.6; }
.controls, section { background: white; border: 1px solid #cad6df; border-radius: 12px; padding: 20px; }
.controls { display: flex; flex-wrap: wrap; gap: 12px; align-items: center; }
input, select { max-width: 100%; font: inherit; }
button { background: #145b91; color: white; border: 0; border-radius: 6px; padding: 10px 16px; font: inherit; cursor: pointer; }
button:disabled { opacity: .6; cursor: wait; }
button.secondary { background: #e2eaf0; color: #182735; }
button:focus-visible, input:focus-visible, select:focus-visible { outline: 3px solid #cc8500; outline-offset: 3px; }
#status { min-height: 1.6em; }
.views { display: grid; grid-template-columns: repeat(auto-fit, minmax(min(100%, 380px), 1fr)); gap: 16px; }
h2 { margin-top: 0; font-size: 1.15rem; }
pre { white-space: pre-wrap; overflow-wrap: anywhere; font-size: .9rem; line-height: 1.5; }
</style>
</head>
<body>
<h1 id="title">BRUCE MCP — Phase-DAG V2 Inspector</h1>
<p>Select a version and saved bundle to validate its canonical bytes and view recorded metadata. Maximum size: <span id="limit">1 MiB</span>. The bundle is held in memory only; payload contents are never displayed.</p>
<div class="controls">
<label for="version">Bundle version</label>
<select id="version">
<option value="v2">Phase-DAG V2</option>
<option value="v3">Phase-DAG V3</option>
<option value="v4">DAG-Scheduler V4</option>
</select>
<label for="bundle">Bundle file</label>
<input id="bundle" type="file">
<button id="inspect" type="button">Inspect bundle</button>
<button id="reset" type="button" class="secondary">Reset</button>
</div>
<p id="status" role="status" aria-live="polite"></p>
<div class="views">
<section><h2>Overview</h2><pre id="overview"></pre></section>
<section><h2>Graph</h2><pre id="graph"></pre></section>
<section><h2>Receipts</h2><pre id="receipts"></pre></section>
<section><h2>Trace</h2><pre id="trace"></pre></section>
<section><h2>Validation</h2><pre id="validation"></pre></section>
<section id="scheduler-section" hidden><h2>Scheduler explanation</h2><pre id="scheduler"></pre></section>
</div>
<script>
const versionChooser = document.getElementById('version');
const chooser = document.getElementById('bundle');
const inspectButton = document.getElementById('inspect');
const statusView = document.getElementById('status');
const views = ['overview', 'graph', 'receipts', 'trace', 'validation', 'scheduler'].map(id => document.getElementById(id));
const schedulerSection = document.getElementById('scheduler-section');
const configurations = {
    v2: {title:'BRUCE MCP — Phase-DAG V2 Inspector', route:'/api/v1/inspect', maxBytes:1024 * 1024, limit:'1 MiB'},
    v3: {title:'BRUCE MCP — Phase-DAG V3 Inspector', route:'/api/v3/inspect', maxBytes:5 * 1024 * 1024, limit:'5 MiB'},
    v4: {title:'BRUCE MCP — DAG-Scheduler V4 Inspector', route:'/api/dag-scheduler-v4/inspect', maxBytes:5 * 1024 * 1024, limit:'5 MiB'}
};
let revision = 0;
function selectedConfiguration() {
    if (versionChooser.value === 'v4') return configurations.v4;
    return versionChooser.value === 'v3' ? configurations.v3 : configurations.v2;
}
function updateSelection() {
    const config = selectedConfiguration();
    document.title = config.title;
    document.getElementById('title').textContent = config.title;
    document.getElementById('limit').textContent = config.limit;
    schedulerSection.hidden = versionChooser.value !== 'v3';
}
// Project only the declared metadata even if an unexpected response adds byte fields.
function nodeMetadata(node, isV4) {
    if (isV4) return {node_id:node.node_id, callback_id:node.callback_id, dependencies:node.dependencies,
        request_size:node.request_size, request_digest:node.request_digest};
    return {node_id:node.node_id, adapter_id:node.adapter_id, adapter_version:node.adapter_version, dependencies:node.dependencies};
}
function inputMetadata(input, isV3, isV4) {
    if (isV4) return {producer_node_id:input.producer_node_id, result_code:input.result_code,
        payload_size:input.payload_size, payload_digest:input.payload_digest};
    const metadata = {producer_node_id:input.producer_node_id, producer_adapter_id:input.producer_adapter_id,
        producer_adapter_version:input.producer_adapter_version, producer_status:input.producer_status,
        producer_output_digest:input.producer_output_digest, producer_evidence_digest:input.producer_evidence_digest};
    if (isV3) {
        metadata.producer_payload_digest = input.producer_payload_digest;
        metadata.payload_size = input.payload_size;
    }
    return metadata;
}
function outputMetadata(output, isV3, isV4) {
    if (!output) return null;
    if (isV4) return {output_version:output.output_version, result_code:output.result_code,
        payload_size:output.payload_size, payload_digest:output.payload_digest};
    const metadata = {output_version:output.output_version, status:output.status,
        output_digest:output.output_digest, evidence_digest:output.evidence_digest, claim_scope:output.claim_scope};
    if (isV3) {
        metadata.payload_digest = output.payload_digest;
        metadata.payload_size = output.payload_size;
    }
    return metadata;
}
function receiptMetadata(receipt, isV3, isV4) {
    if (isV4) return {node_id:receipt.node_id, callback_id:receipt.callback_id,
        request_size:receipt.request_size, request_digest:receipt.request_digest,
        input_refs:receipt.input_refs.map(input => inputMetadata(input, isV3, isV4)),
        outcome:receipt.outcome, output:outputMetadata(receipt.output, isV3, isV4), failure_code:receipt.failure_code};
    return {node_id:receipt.node_id, adapter_id:receipt.adapter_id, adapter_version:receipt.adapter_version,
        request_digest:receipt.request_digest, input_refs:receipt.input_refs.map(input => inputMetadata(input, isV3)),
        outcome:receipt.outcome, output:outputMetadata(receipt.output, isV3), failure_code:receipt.failure_code};
}
function schedulerExplanationMetadata(explanation) {
    if (!explanation || typeof explanation !== 'object' || Array.isArray(explanation) || !Array.isArray(explanation.nodes)) return null;
    const nodes = explanation.nodes.map(node => {
        if (!node || typeof node !== 'object' || Array.isArray(node)) return null;
        const blockers = Array.isArray(node.blocking_predecessors) ? node.blocking_predecessors : [];
        return {
            node_id:node.node_id,
            outcome:node.outcome,
            failure_code:node.failure_code,
            dependencies:Array.isArray(node.dependencies) ? node.dependencies : [],
            blocking_predecessors:blockers.filter(blocker => blocker && typeof blocker === 'object' && !Array.isArray(blocker)).map(blocker => ({
                node_id:blocker.node_id,
                outcome:blocker.outcome,
                failure_code:blocker.failure_code
            }))
        };
    }).filter(node => node !== null);
    return {trace_model:explanation.trace_model, parallelism_recorded:explanation.parallelism_recorded, nodes:nodes};
}
function renderSchedulerExplanation(explanation) {
    const metadata = schedulerExplanationMetadata(explanation);
    const view = views[5];
    if (!metadata) {
        view.textContent = 'No scheduler explanation is available for this V3 bundle.';
        return;
    }
    view.textContent = 'Canonical logical trace. Physical parallelism is not recorded in this bundle.\n' +
        JSON.stringify(metadata, null, 2);
}
function clearViews() {
    views.forEach(view => { view.textContent = ''; });
    statusView.textContent = '';
}
versionChooser.addEventListener('change', () => {
    revision += 1;
    chooser.value = '';
    inspectButton.disabled = false;
    clearViews();
    updateSelection();
});
updateSelection();
document.getElementById('reset').addEventListener('click', () => {
    revision += 1;
    chooser.value = '';
    inspectButton.disabled = false;
    clearViews();
});
inspectButton.addEventListener('click', async () => {
    let selected = chooser.files[0];
    let bytes = null;
    const config = selectedConfiguration();
    const isV3 = versionChooser.value === 'v3';
    const isV4 = versionChooser.value === 'v4';
    revision += 1;
    const current = revision;
    clearViews();
    if (!selected) {
        statusView.textContent = 'Select a bundle file first.';
        return;
    }
    if (selected.size > config.maxBytes) {
        statusView.textContent = 'REJECTED: bundle exceeds ' + config.limit + '.';
        views[4].textContent = 'REJECTED';
        return;
    }
    inspectButton.disabled = true;
    statusView.textContent = 'Inspecting bundle…';
    try {
        bytes = await selected.arrayBuffer();
        selected = null;
        if (current !== revision) return;
        chooser.value = '';
        const response = await fetch(config.route, {method:'POST', body:bytes});
        bytes = null;
        const model = await response.json();
        if (current !== revision) return;
        if (response.ok && model.status === 'VALID') {
            const overview = isV4 ? {status:model.status, scheduler_version:model.scheduler_version, dag_id:model.dag_id,
                max_started_nodes:model.max_started_nodes, max_total_receipt_bytes:model.max_total_receipt_bytes, node_count:model.nodes.length} :
                {status:model.status, dag_version:model.dag_version, dag_id:model.dag_id, node_count:model.nodes.length};
            views[0].textContent = JSON.stringify(overview, null, 2);
            views[1].textContent = JSON.stringify(model.nodes.map(node => nodeMetadata(node, isV4)), null, 2);
            views[2].textContent = JSON.stringify(model.receipts.map(receipt => receiptMetadata(receipt, isV3, isV4)), null, 2);
            views[3].textContent = JSON.stringify(model.trace.map(entry => ({event:entry.event, node_id:entry.node_id})), null, 2);
            views[4].textContent = isV4 ? 'VALID: canonical bundle validation passed; not re-executed.' :
                'VALID: canonical bundle validation passed.';
            if (isV3) renderSchedulerExplanation(model.scheduler_explanation);
            statusView.textContent = 'VALID';
        } else {
            const label = model.status === 'REJECTED' ? 'REJECTED' : 'ERROR';
            views[4].textContent = label;
            statusView.textContent = label;
        }
    } catch {
        if (current !== revision) return;
        views[4].textContent = 'ERROR';
        statusView.textContent = 'ERROR: inspection could not be completed.';
    } finally {
        bytes = null;
        selected = null;
        if (current === revision) {
            chooser.value = '';
            inspectButton.disabled = false;
        }
    }
});
</script>
</body>
</html>)HTML";
    return html;
}

const std::string& mcp_inspector_html_v1() {
    return mcp_inspector_html();
}

} // namespace bruce
