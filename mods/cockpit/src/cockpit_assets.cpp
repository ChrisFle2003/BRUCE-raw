#include "cockpit_server.hpp"
#ifdef BRUCE_COCKPIT_LANGUAGE_DISPATCH
#include "cockpit_language_dispatch.hpp"
#endif

namespace bruce {
namespace {

constexpr char dashboard[] = R"HTML(<!doctype html>
<html lang="en">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>BRUCE Cockpit</title>
<style>
:root{font:16px system-ui,sans-serif;color:#17202a;background:#f4f6f8}
body{margin:0}.shell{max-width:1100px;margin:auto;padding:1rem}
header,.panel{background:#fff;border:1px solid #d8dee4;border-radius:.6rem;padding:1rem;margin-bottom:1rem}
header{display:flex;justify-content:space-between;gap:1rem;align-items:center}
h1,h2{margin:.1rem 0 .7rem}.muted{color:#5f6b76}.error{color:#9b1c1c;min-height:1.5rem}
button{border:1px solid #7b8794;border-radius:.35rem;background:#fff;padding:.45rem .7rem;cursor:pointer}
button:hover{background:#eef2f5}button:disabled{cursor:not-allowed;opacity:.5}
table{border-collapse:collapse;width:100%}th,td{text-align:left;padding:.45rem;border-bottom:1px solid #e4e8eb;vertical-align:top}
tr[data-job-id]{cursor:pointer}tr[data-job-id]:hover{background:#f7f9fa}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:1rem}
label{display:flex;flex-direction:column;gap:.3rem}input,textarea{font:inherit;padding:.45rem;border:1px solid #aab4be;border-radius:.35rem}
pre{background:#111820;color:#e8edf2;padding:.75rem;overflow:auto;white-space:pre-wrap;min-height:5rem}
.actions{display:flex;flex-wrap:wrap;gap:.5rem}.artifact{display:flex;justify-content:space-between;gap:1rem;border-bottom:1px solid #e4e8eb;padding:.4rem 0}
.import-item{display:flex;justify-content:space-between;gap:1rem;border-bottom:1px solid #e4e8eb;padding:.4rem 0;align-items:center}
</style></head>
<body><main class="shell">
<header><div><h1>BRUCE Cockpit</h1><p class="muted">Local management interface</p></div>
<div><span id="health">Service status: unknown</span> <button id="refresh">Refresh</button></div></header>
<p id="error" class="error" role="alert"></p>
<section class="panel" data-language-panel><h2>Natural language → draft</h2>
<p class="muted">German text and exactly two inputs. Input text is hashed as UTF-8; the response shows the server-side draft.</p>
<p data-language-availability>__BRUCE_LANGUAGE_AVAILABILITY__</p>
<p data-language-dispatch-availability>__BRUCE_LANGUAGE_DISPATCH_AVAILABILITY__</p>
<fieldset data-language-draft-controls __BRUCE_LANGUAGE_DRAFTS_DISABLED__>
<button data-language-load-drafts type="button">Load drafts</button>
<p class="muted">Drafts are retained only until the server stops. Loading only reads them; selecting an alternative and CORE actions require separate clicks.</p>
<div data-language-drafts aria-live="polite">No drafts loaded yet.</div></fieldset>
<form data-language-form><fieldset __BRUCE_LANGUAGE_DISABLED__><label>German text
<textarea data-language-text rows="3" required>Analysiere @train anhand @holdout auf rationale Beziehungen.</textarea></label>
<div class="grid"><div><label>Alias 1<input data-language-alias="1" value="train" required></label>
<label>Inputbytes 1 (UTF-8)<textarea data-language-bytes="1" rows="3">0,0
</textarea></label></div><div><label>Alias 2<input data-language-alias="2" value="holdout" required></label>
<label>Inputbytes 2 (UTF-8)<textarea data-language-bytes="2" rows="3">1,1
</textarea></label></div></div><button data-language-create type="submit">Create draft</button></fieldset></form>
<h3>Draft response</h3><pre data-language-result aria-live="polite">No draft created yet.</pre>
<div data-language-alternatives class="actions"></div>
<div data-language-dispatch-actions class="actions"></div>
<h3>CORE bundle and replay</h3>
<p class="muted">Exporting a bundle recalculates the stored draft. Replay verifies the bundle bytes; it does not contain the original language interpretation.</p>
<p data-language-bundle-status aria-live="polite">No bundle exported yet.</p><div data-language-bundle-download></div>
<fieldset data-language-bundle-controls __BRUCE_LANGUAGE_REPLAY_DISABLED__>
<button data-language-replay-current type="button" disabled>Replay exported bundle</button>
<label>CORE bundle file<input data-language-replay-file type="file" accept=".bruce,application/octet-stream"></label>
<button data-language-replay-selected type="button" disabled>Replay file</button></fieldset>
<pre data-language-replay-result aria-live="polite">No bundle replayed yet.</pre></section>
<section class="panel"><h2>Start job</h2><form id="job-form"><div class="grid"><label>Command
<input id="job-command" value="calculate" required></label><label>Arguments (one token per line)
<textarea id="job-args" rows="5">--op
add
--left
1
--right
2</textarea></label></div><button id="create-job" type="submit">Start job</button></form></section>
<section class="panel"><h2>Import .bruce</h2><form id="import-form"><label>File
<input id="import-file" type="file" accept=".bruce" required></label><button type="submit">Import</button></form>
<div id="imports" class="muted">No imports loaded.</div></section>
<section class="panel"><h2>Jobs</h2><table><thead><tr><th>ID</th><th>Status</th><th>Invocation</th><th>Exit</th></tr></thead>
<tbody id="jobs"><tr><td colspan="4" class="muted">No jobs loaded yet.</td></tr></tbody></table></section>
<section id="job-detail" class="panel"><h2>Job details</h2><p id="job-summary" class="muted">Select a job.</p>
<div class="actions"><button data-action="inspect" disabled>Inspect</button><button data-action="assess" disabled>Assess</button>
<button data-action="replay" disabled>Replay</button><button data-action="gate" disabled>Gate</button><button data-action="cancel" disabled>Cancel</button></div>
<h3>Artifacts</h3><div id="artifact-list" class="muted">No artifacts loaded.</div>
<div class="grid"><div><h3>stdout</h3><pre id="stdout"></pre></div><div><h3>stderr</h3><pre id="stderr"></pre></div></div></section>
</main>
<script>
(() => {
  const authToken = "__BRUCE_COCKPIT_TOKEN__";
  const authHeaders = () => ({Authorization: "Bearer " + authToken});
  const state = {job:null, source:null};
  let jobSelectionGeneration = 0;
  let refreshGeneration = 0;
  const $ = id => document.getElementById(id);
  const terminal = new Set(["SUCCEEDED","FAILED","CANCELLED","UNRESOLVED_AFTER_SERVER_EXIT"]);
  const showError = message => { $("error").textContent = message || ""; };
  const getJson = async path => { const response = await fetch(path, {headers:authHeaders()}); const data = await response.json();
    if (!response.ok) throw new Error(data.error || "HTTP " + response.status); return data; };
  const updateControls = () => { const job = state.job; const canCancel = Boolean(job && ["QUEUED","RUNNING"].includes(job.state));
    const canFollowUp = Boolean(job && terminal.has(job.state) && state.source); document.querySelectorAll("[data-action]").forEach(button => {
      button.disabled = button.dataset.action === "cancel" ? !canCancel : !canFollowUp; }); };
  const renderJobs = jobs => { const body = $("jobs"); body.textContent = "";
    if (!jobs.length) { body.innerHTML = '<tr><td colspan="4" class="muted">No jobs.</td></tr>'; return; }
    jobs.forEach(job => { const row = document.createElement("tr"); row.dataset.jobId = job.id;
      [job.id, job.state, job.argv.join(" "), job.return_code == null ? "—" : job.return_code].forEach(value => {
        const cell = document.createElement("td"); cell.textContent = String(value); row.appendChild(cell); });
      row.addEventListener("click", () => selectJob(job.id)); body.appendChild(row); }); };
  const renderArtifacts = artifacts => { const list = $("artifact-list"); list.textContent = "";
    if (!artifacts.length) { list.textContent = "No artifacts."; return; }
    artifacts.forEach(artifact => { const item = document.createElement("div"); item.className = "artifact";
      const label = document.createElement("span"); label.textContent = artifact.path + " (" + artifact.bytes + " Bytes)";
      item.appendChild(label); if (artifact.path.endsWith(".bruce")) { const button = document.createElement("button");
        button.textContent = "Choose source"; button.addEventListener("click", () => { state.source = artifact.path; updateControls(); }); item.appendChild(button); }
      list.appendChild(item); }); };
  const renderImports = imports => { const list = $("imports"); list.textContent = "";
    if (!imports.length) { list.textContent = "No imports."; return; }
    imports.forEach(imported => { const item = document.createElement("div"); item.className = "import-item";
      const label = document.createElement("span"); label.textContent = imported.name + " (" + imported.bytes + " Bytes, " + imported.sha256 + ")";
      item.appendChild(label); const button = document.createElement("button"); button.textContent = "Choose source";
      button.addEventListener("click", () => { state.source = imported.path; updateControls(); }); item.appendChild(button);
      const actions = document.createElement("div"); actions.className = "actions";
      [["inspect","Inspect"],["assess","Assess"],["replay","Replay"],["gate","Gate"]].forEach(([name, label]) => {
        const button = document.createElement("button"); button.textContent = label; button.dataset.importId = imported.id; button.dataset.importAction = name;
        button.addEventListener("click", () => importAction(button.dataset.importId, button.dataset.importAction)); actions.appendChild(button); });
      item.appendChild(actions); list.appendChild(item); }); };
  const languageJson = async (path, body) => {
    const response = await fetch(path, {method:"POST",headers:Object.assign(authHeaders(), {"Content-Type":"application/json"}),body:JSON.stringify(body)});
    const data = await response.json(); if (!response.ok) throw new Error(data.error || "HTTP " + response.status); return data;
  };
  const languageElement = selector => document.querySelector(selector);
  const languageEnabled = __BRUCE_LANGUAGE_ENABLED__;
  const languageDispatchEnabled = __BRUCE_LANGUAGE_DISPATCH_ENABLED__;
  const languageReplayMaxBytes = __BRUCE_LANGUAGE_REPLAY_MAX_BYTES__;
  let languagePending = false;
  let languageBundle = null;
  let languageBundleUrl = null;
  const updateLanguageReplayControls = () => {
    languageElement("[data-language-bundle-controls]").disabled = !languageDispatchEnabled || languagePending;
    languageElement("[data-language-replay-current]").disabled = !languageBundle || languagePending;
    languageElement("[data-language-replay-selected]").disabled = !languageElement("[data-language-replay-file]").files[0] || languagePending;
  };
  const setLanguagePending = pending => { languagePending = pending;
    document.querySelectorAll("[data-language-panel] button").forEach(button => { button.disabled = pending; });
    languageElement("[data-language-draft-controls]").disabled = !languageEnabled || pending;
    languageElement("[data-language-bundle-controls]").disabled = !languageDispatchEnabled || pending;
    updateLanguageReplayControls(); };
  const renderLanguageDraft = draft => {
    languageElement("[data-language-result]").textContent = JSON.stringify({
      id:draft.id, raw_text:draft.raw_text, normalized_text:draft.normalized_text,
      grammar_version:draft.grammar_version, status:draft.status, selected_id:draft.selected_id,
      alternatives:draft.alternatives, inputs:draft.inputs.map(input => ({alias:input.alias, sha256:input.sha256})),
      budget:draft.budget, max_bits:draft.max_bits, max_denominator:draft.max_denominator, max_trace_bytes:draft.max_trace_bytes
    }, null, 2);
    const list = languageElement("[data-language-alternatives]"); list.textContent = "";
    draft.alternatives.forEach(alternative => {
      const item = document.createElement("div"); const label = document.createElement("span");
      label.textContent = alternative.id + " · " + alternative.rule; item.appendChild(label);
      if (draft.status === "AMBIGUOUS" && !draft.selected_id) {
        const button = document.createElement("button"); button.type = "button";
        button.dataset.languageAlternative = alternative.id; button.textContent = "Choose alternative: " + alternative.id;
        button.disabled = languagePending;
        button.addEventListener("click", () => selectLanguageAlternative(draft.id, button.dataset.languageAlternative)); item.appendChild(button);
      }
      list.appendChild(item);
    });
    const actions = languageElement("[data-language-dispatch-actions]"); actions.textContent = "";
    if (languageDispatchEnabled && (draft.status === "UNDERSTOOD" ||
        (draft.status === "AMBIGUOUS" && Boolean(draft.selected_id)))) {
      const button = document.createElement("button"); button.type = "button";
      button.dataset.languageDispatch = draft.id; button.textContent = "Run CORE";
      button.disabled = languagePending;
      button.addEventListener("click", () => dispatchLanguageDraft(button.dataset.languageDispatch)); actions.appendChild(button);
      const exportButton = document.createElement("button"); exportButton.type = "button";
      exportButton.dataset.languageBundle = draft.id; exportButton.textContent = "Export CORE bundle";
      exportButton.disabled = languagePending;
      exportButton.addEventListener("click", () => exportLanguageBundle(exportButton.dataset.languageBundle)); actions.appendChild(exportButton);
    }
  };
  const renderLanguageDraftList = drafts => {
    const list = languageElement("[data-language-drafts]"); list.textContent = "";
    if (!drafts.length) { list.textContent = "No stored drafts."; return; }
    drafts.forEach(draft => {
      const item = document.createElement("div"); const label = document.createElement("span");
      label.textContent = draft.id + " · " + draft.status + " · " + draft.normalized_text;
      item.appendChild(label);
      const button = document.createElement("button"); button.type = "button";
      button.dataset.languageDraft = draft.id; button.textContent = "Show draft";
      button.disabled = languagePending;
      button.addEventListener("click", () => loadLanguageDraft(button.dataset.languageDraft));
      item.appendChild(button); list.appendChild(item);
    });
  };
  const loadLanguageDrafts = async () => { if (!languageEnabled || languagePending) return;
    setLanguagePending(true); showError("");
    try { renderLanguageDraftList(await getJson("/api/v1/language/drafts")); }
    catch (error) { showError(error.message); } finally { setLanguagePending(false); }
  };
  const loadLanguageDraft = async id => { if (!languageEnabled || languagePending) return;
    setLanguagePending(true); showError("");
    try { renderLanguageDraft(await getJson("/api/v1/language/drafts/" + encodeURIComponent(id))); }
    catch (error) { showError(error.message); } finally { setLanguagePending(false); }
  };
  const dispatchLanguageDraft = async id => { if (!languageDispatchEnabled || languagePending) return;
    setLanguagePending(true); showError("");
    try {
      const response = await fetch("/api/v1/language/drafts/" + encodeURIComponent(id) + "/dispatch",
        {method:"POST",headers:Object.assign(authHeaders(), {"Content-Type":"application/json"}),body:""});
      const data = await response.json(); if (!response.ok) throw new Error(data.error || "HTTP " + response.status);
      languageElement("[data-language-result]").textContent = JSON.stringify(data, null, 2);
    } catch (error) { showError(error.message); } finally { setLanguagePending(false); }
  };
  const exportLanguageBundle = async id => { if (!languageDispatchEnabled || languagePending) return;
    setLanguagePending(true); showError("");
    try {
      const response = await fetch("/api/v1/language/drafts/" + encodeURIComponent(id) + "/dispatch/bundle",
        {method:"POST",headers:Object.assign(authHeaders(), {"Content-Type":"application/json"}),body:""});
      if (!response.ok) { const data = await response.json(); throw new Error(data.error || "HTTP " + response.status); }
      const bundle = await response.blob();
      if (languageBundleUrl) URL.revokeObjectURL(languageBundleUrl);
      languageBundle = bundle; languageBundleUrl = URL.createObjectURL(bundle);
      const link = document.createElement("a"); link.href = languageBundleUrl; link.download = id + "-core.bruce";
      link.textContent = "Download bundle";
      const download = languageElement("[data-language-bundle-download]"); download.textContent = ""; download.appendChild(link);
      languageElement("[data-language-bundle-status]").textContent = "Exported " + id + ": " + bundle.size + " bytes (CORE).";
    } catch (error) { showError(error.message); } finally { setLanguagePending(false); }
  };
  const replayLanguageBundle = async selectedFile => { if (!languageDispatchEnabled || languagePending) return;
    const file = selectedFile ? languageElement("[data-language-replay-file]").files[0] : null;
    const bundle = selectedFile ? file : languageBundle; if (!bundle) return;
    if (file && file.size > languageReplayMaxBytes) { showError("ApplicationDispatchPayloadTooLarge"); return; }
    setLanguagePending(true); showError("");
    try {
      const response = await fetch("/api/v1/language/dispatch/replay",
        {method:"POST",headers:Object.assign(authHeaders(), {"Content-Type":"application/octet-stream"}),body:bundle});
      const data = await response.json(); if (!response.ok) throw new Error(data.error || "HTTP " + response.status);
      languageElement("[data-language-replay-result]").textContent = JSON.stringify(data, null, 2);
    } catch (error) { showError(error.message); } finally { setLanguagePending(false); }
  };
  const selectLanguageAlternative = async (id, alternative) => { if (languagePending) return;
    setLanguagePending(true); showError("");
    try { renderLanguageDraft(await languageJson("/api/v1/language/drafts/" + encodeURIComponent(id) + "/select", {alternative:alternative})); }
    catch (error) { showError(error.message); } finally { setLanguagePending(false); }
  };
  const createLanguageDraft = async event => { event.preventDefault(); if (languagePending) return;
    const text = languageElement("[data-language-text]").value;
    const inputs = ["1","2"].map(index => ({
      alias:languageElement('[data-language-alias="' + index + '"]').value,
      bytes:languageElement('[data-language-bytes="' + index + '"]').value
    }));
    setLanguagePending(true); showError("");
    try {
      if (!globalThis.crypto || !crypto.subtle) throw new Error("SHA-256 requires Web Crypto in a secure browser context.");
      await Promise.all(inputs.map(async input => {
        const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(input.bytes));
        input.sha256 = Array.from(new Uint8Array(digest), byte => byte.toString(16).padStart(2, "0")).join("");
      }));
      renderLanguageDraft(await languageJson("/api/v1/language/drafts", {text:text, inputs:inputs}));
    } catch (error) { showError(error.message); } finally { setLanguagePending(false); }
  };
  const selectJob = async id => { const generation = ++jobSelectionGeneration;
    try { const job = await getJson("/api/v1/jobs/" + encodeURIComponent(id));
      if (generation !== jobSelectionGeneration) return;
      state.job = job; state.source = null;
      $("job-summary").textContent = job.id + " · " + job.state + " · " + job.directory; $("stdout").textContent = job.stdout || ""; $("stderr").textContent = job.stderr || "";
      renderArtifacts([]); updateControls();
      const artifacts = await getJson("/api/v1/jobs/" + encodeURIComponent(id) + "/artifacts");
      if (generation !== jobSelectionGeneration) return;
      renderArtifacts(artifacts.artifacts); updateControls();
      const [stdout, stderr] = await Promise.all([fetch("/api/v1/jobs/" + encodeURIComponent(id) + "/logs/stdout", {headers:authHeaders()}), fetch("/api/v1/jobs/" + encodeURIComponent(id) + "/logs/stderr", {headers:authHeaders()})]);
      if (generation !== jobSelectionGeneration) return;
      for (const response of [stdout, stderr]) { if (!response.ok) {
        let data = null; try { data = await response.json(); } catch {}
        throw new Error((data && data.error) || "HTTP " + response.status);
      } }
      const [stdoutText, stderrText] = await Promise.all([stdout.text(), stderr.text()]);
      if (generation !== jobSelectionGeneration) return;
      $("stdout").textContent = stdoutText; $("stderr").textContent = stderrText;
    } catch (error) { if (generation === jobSelectionGeneration) showError(error.message); } };
  const refresh = async (expectedSelectionGeneration = jobSelectionGeneration) => { const generation = ++refreshGeneration;
    const selectionGeneration = jobSelectionGeneration;
    try { if (expectedSelectionGeneration === jobSelectionGeneration) showError("");
      const health = await getJson("/api/v1/health");
      if (generation !== refreshGeneration) return;
      $("health").textContent = "Service status: " + health.status;
      const [jobs, imports] = await Promise.all([getJson("/api/v1/jobs"), getJson("/api/v1/imports")]);
      if (generation !== refreshGeneration) return;
      renderJobs(jobs); renderImports(imports);
      if (expectedSelectionGeneration !== selectionGeneration) return null;
      if (selectionGeneration === jobSelectionGeneration && state.job) {
        const selection = selectJob(state.job.id);
        const refreshedSelectionGeneration = jobSelectionGeneration;
        await selection; return refreshedSelectionGeneration;
      }
      return selectionGeneration;
    } catch (error) { if (generation === refreshGeneration && expectedSelectionGeneration === jobSelectionGeneration) showError(error.message); } };
  const action = async name => { if (!state.job || (name !== "cancel" && !state.source)) return;
    const selectionGeneration = jobSelectionGeneration;
    const jobId = state.job.id;
    const source = state.source;
    try { const request = name === "cancel" ? {method:"POST",headers:authHeaders()} :
      {method:"POST",headers:Object.assign(authHeaders(), {"Content-Type":"application/json"}),body:JSON.stringify({path:source})}; const response = await fetch("/api/v1/jobs/" + encodeURIComponent(jobId) + "/" + name, request);
      const data = await response.json(); if (!response.ok) throw new Error(data.error || "HTTP " + response.status);
      const refreshedSelectionGeneration = await refresh(selectionGeneration);
      if (refreshedSelectionGeneration !== jobSelectionGeneration) return;
      await selectJob(data.id || jobId); }
      catch (error) { if (selectionGeneration === jobSelectionGeneration) showError(error.message); } };
  const importAction = async (id, name) => { const selectionGeneration = jobSelectionGeneration;
    try {
      const response = await fetch("/api/v1/imports/" + encodeURIComponent(id) + "/" + name,
        {method:"POST",headers:Object.assign(authHeaders(), {"Content-Type":"application/json"}),body:""});
      const data = await response.json(); if (response.status !== 201) throw new Error(data.error || "HTTP " + response.status);
      const refreshedSelectionGeneration = await refresh(selectionGeneration);
      if (refreshedSelectionGeneration !== jobSelectionGeneration) return;
      await selectJob(data.id); }
    catch (error) { if (selectionGeneration === jobSelectionGeneration) showError(error.message); } };
  const createJob = async event => { event.preventDefault(); const command = $("job-command").value.trim();
    const args = $("job-args").value.split(/\r?\n/).map(token => token.trim()).filter(Boolean); if (!command) return;
    const selectionGeneration = jobSelectionGeneration;
    try { const response = await fetch("/api/v1/jobs", {method:"POST",headers:Object.assign(authHeaders(), {"Content-Type":"application/json"}),body:JSON.stringify({argv:[command,...args]})});
      const data = await response.json(); if (!response.ok) throw new Error(data.error || "HTTP " + response.status);
      const refreshedSelectionGeneration = await refresh(selectionGeneration);
      if (refreshedSelectionGeneration !== jobSelectionGeneration) return;
      await selectJob(data.id); }
    catch (error) { if (selectionGeneration === jobSelectionGeneration) showError(error.message); } };
  const importFile = async event => { event.preventDefault(); const file = $("import-file").files[0]; if (!file) return;
    if (file.size > 8 * 1024 * 1024) { showError("CockpitImportTooLarge"); return; }
    const selectionGeneration = jobSelectionGeneration;
    try { const headers = Object.assign(authHeaders(), {"Content-Type":"application/octet-stream", "X-Bruce-Import-Name":file.name});
      const response = await fetch("/api/v1/imports", {method:"POST", headers, body:await file.arrayBuffer()});
      if (!response.ok) {
        let data = null; try { data = await response.json(); } catch {}
        throw new Error((data && data.error) || "HTTP " + response.status);
      }
      await response.json();
      if ($("import-file").files[0] === file) $("import-form").reset();
      await refresh(selectionGeneration); }
    catch (error) { if (selectionGeneration === jobSelectionGeneration) showError(error.message); } };
  $("refresh").addEventListener("click", () => refresh()); $("job-form").addEventListener("submit", createJob);
  $("import-form").addEventListener("submit", importFile);
  if (languageEnabled) {
    languageElement("[data-language-form]").addEventListener("submit", createLanguageDraft);
    languageElement("[data-language-load-drafts]").addEventListener("click", loadLanguageDrafts);
  }
  if (languageDispatchEnabled) {
    languageElement("[data-language-replay-current]").addEventListener("click", () => replayLanguageBundle(false));
    languageElement("[data-language-replay-selected]").addEventListener("click", () => replayLanguageBundle(true));
    languageElement("[data-language-replay-file]").addEventListener("change", updateLanguageReplayControls);
  }
  document.querySelectorAll("[data-action]").forEach(button => button.addEventListener("click", () => action(button.dataset.action))); refresh();
})();
</script></body>
</html>
)HTML";

} // namespace

const std::string& cockpit_dashboard_html() {
    static const std::string value = [] {
        std::string html(dashboard);
        const auto replace = [&html](const std::string& marker, const std::string& replacement) {
            html.replace(html.find(marker), marker.size(), replacement);
        };
#ifdef BRUCE_COCKPIT_LANGUAGE
        replace("__BRUCE_LANGUAGE_ENABLED__", "true");
        replace("__BRUCE_LANGUAGE_DISABLED__", "");
        replace("__BRUCE_LANGUAGE_DRAFTS_DISABLED__", "");
        replace("__BRUCE_LANGUAGE_AVAILABILITY__", "Language draft management is available.");
#else
        replace("__BRUCE_LANGUAGE_ENABLED__", "false");
        replace("__BRUCE_LANGUAGE_DISABLED__", "disabled");
        replace("__BRUCE_LANGUAGE_DRAFTS_DISABLED__", "disabled");
        replace("__BRUCE_LANGUAGE_AVAILABILITY__", "Language draft management is unavailable in this build.");
#endif
#if defined(BRUCE_COCKPIT_LANGUAGE) && defined(BRUCE_COCKPIT_LANGUAGE_DISPATCH)
        replace("__BRUCE_LANGUAGE_DISPATCH_ENABLED__", "true");
        replace("__BRUCE_LANGUAGE_DISPATCH_AVAILABILITY__", "CORE execution is available.");
        replace("__BRUCE_LANGUAGE_REPLAY_DISABLED__", "");
        replace("__BRUCE_LANGUAGE_REPLAY_MAX_BYTES__", std::to_string(cockpit_language_dispatch_max_request_bytes));
#else
        replace("__BRUCE_LANGUAGE_DISPATCH_ENABLED__", "false");
        replace("__BRUCE_LANGUAGE_DISPATCH_AVAILABILITY__", "CORE execution is unavailable in this build.");
        replace("__BRUCE_LANGUAGE_REPLAY_DISABLED__", "disabled");
        replace("__BRUCE_LANGUAGE_REPLAY_MAX_BYTES__", "0");
#endif
        return html;
    }();
    return value;
}

} // namespace bruce
