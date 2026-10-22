# DAG-Scheduler V4 Cockpit Inspector Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Den lokalen, nur lesenden Cockpit-Inspector um eine explizite V4-Bundleansicht erweitern, die kanonische DAG-Scheduler-V4-Bundles ohne Reexecution erklärt.

**Architecture:** Ein schlanker Cockpit-Adapter ruft ausschließlich die vorhandene V4-Inspect-Schnittstelle auf und projiziert ihr V2-Run-Modell zu sicheren Metadaten. Der bestehende Loopback-Server bekommt eine neue feste Route; die bestehende HTML-Seite erhält V4 als dritte explizite Auswahl. V2 und V3 bleiben getrennte Verträge.

**Tech Stack:** C++20, bestehende V2/V4-Scheduler-Typen, cpp-httplib, eingebettetes HTML/JavaScript, CMake/CTest und Python-HTTP-/DOM-Tests.

**Spec:** [2026-10-14-dag-scheduler-v4-cockpit-inspector-design.md](../specs/2026-10-14-dag-scheduler-v4-cockpit-inspector-design.md)

## Global Constraints

- Die neue Route heißt exakt `POST /api/dag-scheduler-v4/inspect`; es gibt keine automatische Formaterkennung.
- Der V4-Upload ist auf 5 MiB begrenzt; V2 behält seine eigene 1-MiB-Prüfung und V3 seine bestehende 5-MiB-Prüfung.
- Die V4-Route ruft nur `inspect_dag_scheduler_run_v4(std::string_view, DagSchedulerCallbackRegistryV2)` auf. Sie darf weder `replay_dag_scheduler_run_v4` noch `execute_dag_scheduler_v2` aufrufen.
- Nutze als vertrauenswürdigen festen Registry-Parameter nur die bestehende V3-Registry mit `literal-v1`, `append-v1` und `join-v1`; Bundlebytes führen keinen Code ein.
- JSON und Browser-DOM zeigen ausschließlich IDs, Status, Callback-IDs, Abhängigkeiten, Budgets, Größen, Digests und Trace. Request-, Input- und Output-Payloadbytes, Dateipfade und interne Fehlertexte bleiben ausgeschlossen.
- Der Server bleibt an `127.0.0.1` gebunden, speichert oder loggt keine Uploads und erhält keine Schreib-, Replay- oder Ausführungsroute.
- C++20, vorhandene CMake-/CTest-Abläufe und `-Wall -Wextra -Werror` bleiben maßgeblich. Keine neuen Paketabhängigkeiten.
- Die generierten CLI-Artefakte `docs/cli/source-index.md`, `docs/cli/examples.json` und `docs/cli/validation.md` werden nur über `docs/cli/verify_examples.py` in einer vollständigen temporären Kopie an einem nativen Pfad geprüft oder aktualisiert.
- Dieser Checkout hat keine nutzbaren Git-Metadaten. Tasks dokumentieren deshalb ihre Dateischnappschüsse, Befehle und Ergebnisse unter `runs/`; sie erstellen keine erfundenen Commits oder Git-Diffs.
- Die vorhandene, sandboxbedingte Socket-Einschränkung des Cockpit-HTTP-Tests ist getrennt von Produktfehlern zu erfassen; die passende Abnahme erfolgt in einer erlaubten nativen Umgebung.

## Review Focus

- **V4 ohne Replay:** Ein Bundle, das strukturell gültig und kanonisch ist, aber andere zulässige Receipt-/Trace-Werte trägt, muss Inspect nach V4-Kernvertrag akzeptieren können; kein Cockpit-Pfad darf unbemerkt reexecuten. Task 1 beweist das mit einer kontrollierten Registry-Sentinel- oder strukturellen Fixture.
- **Rohdatenleck:** Eingebettete Request-, Input- und Output-Payloadbytes sowie ein Pfad/Script-String dürfen weder im C++-JSON noch in einer absichtlich erweiterten Browserantwort erscheinen. Task 1 und 2 besitzen diese Negativtests.
- **Versionsverwechslung:** V2- und V3-Bundles an der V4-Route sowie V4-Bundles an bestehenden Routen müssen generisch `REJECTED` ergeben. Task 2 besitzt die HTTP-Matrix.
- **Grenzwerte:** Ein Transport über 5 MiB muss HTTP 413 liefern; ein V2-Upload über 1 MiB darf nicht durch die nun gemeinsame 5-MiB-Servergrenze fallen. Task 2 besitzt die Grenztests.
- **UI-Zustandswechsel:** Auswahlwechsel oder Reset während eines ausstehenden V4-Requests darf keine alten Daten rendern und muss die gewählte Datei freigeben. Task 2 erweitert den bestehenden asynchronen DOM-Harness.

---

### Task 1: V4-Inspect-Projektion und C++-Vertrag

**Files:**
- Modify: `mods/cockpit/src/cockpit_inspector.hpp`
- Modify: `mods/cockpit/src/cockpit_inspector.cpp`
- Modify: `tests/mods/cockpit/cockpit_inspector_tests.cpp`

**Interfaces:**
- Consumes: `DagSchedulerRunV2 inspect_dag_scheduler_run_v4(std::string_view, DagSchedulerCallbackRegistryV2)` from `include/bruce/dag_scheduler/dag_scheduler_v4.hpp` and the fixed V3 registry from `apps/dag_scheduler_v3_cli.hpp` or a narrowly exposed equivalent that does not change V3 CLI behavior.
- Produces: `CockpitInspectionResultV1 inspect_dag_scheduler_v4_bundle(std::string_view bytes)` in `cockpit_inspector.hpp`.
- Produces on success a JSON object with `status`, `code`, `scheduler_version`, `dag_id`, `max_started_nodes`, `max_total_receipt_bytes`, `nodes`, `receipts` and `trace`; on every expected V4 error it returns exactly `{"status":"REJECTED","code":"REJECTED"}`.

- [ ] **Step 1: Write failing C++ projection tests in `tests/mods/cockpit/cockpit_inspector_tests.cpp`.**

  Add a small fixed V2 scheduler run using only `literal-v1`, `append-v1` and `join-v1`, pack it with `pack_dag_scheduler_run_v4`, and assert a future V4 Cockpit result has the exact public identity/budget fields, node callback IDs/dependencies, receipt outcomes, request sizes/digests, input and output payload sizes/digests, plus trace events. Assert every fixture request/payload marker, the V4 header and field names `request_bytes`/`payload_bytes` are absent. Add a `--write-valid-dag-scheduler-v4-bundle` helper mode for the later real HTTP test.

- [ ] **Step 2: Add failing V4 rejection and no-reexecution tests.**

  Assert header/digest mutation, oversize input, empty bytes, and V2/V3 fixtures all produce the exact fixed rejection. Add one test that uses a registry callback sentinel which throws if invoked and proves the V4 Cockpit adapter only calls V4 inspect; it must not call a callback or V4 replay. Keep V2 and V3 contract tests present and unchanged.

- [ ] **Step 3: Run the focused contract target and record the expected RED state.**

  Run in a complete native temporary copy:
  `scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-v4-cockpit-core -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release`,
  then build `bruce_cockpit_inspector_tests` and run `ctest --test-dir /tmp/bruce-v4-cockpit-core -R '^cockpit_inspector_contracts$' --output-on-failure`.
  Expected: compilation or contract failure because the V4 Cockpit function and projection do not yet exist.

- [ ] **Step 4: Implement the smallest V4 metadata-only adapter.**

  Include the V4 header and reuse only established `CockpitInspectionStatusV1`/result handling. Add V4 enum-to-string renderers for `DagSchedulerReceiptOutcomeV2` and `DagSchedulerTraceEventV2`, then render V4 nodes, inputs, outputs, receipts and trace directly from the inspected `DagSchedulerRunV2`. Compute request size from the matching Spec node. Catch `Error` and normalize it to the established fixed rejection. Do not serialize any byte string or call replay/execute.

- [ ] **Step 5: Run core regression contracts GREEN.**

  Rebuild the same target, then run `cockpit_inspector_contracts`, `dag_scheduler_v4_contracts`, `dag_scheduler_v3_cli_workflows`, and `dag_scheduler_v2_cli_workflows`. Record each test count and exit code in `runs/dag-scheduler-v4-cockpit-sdd/reports/task-1.md`.

- [ ] **Step 6: Request a task-scoped review.**

  Save a before/after snapshot or unified file diff plus the test transcript under `runs/dag-scheduler-v4-cockpit-sdd/`, ask a fresh reviewer to check the no-reexecution guarantee and raw-byte boundary, and address any finding before Task 2.

### Task 2: Local V4 HTTP route and explicit Cockpit selection

**Files:**
- Modify: `mods/cockpit/src/cockpit_inspector_server.cpp`
- Modify: `mods/cockpit/src/cockpit_inspector_assets.cpp`
- Modify: `tests/mods/cockpit/test_cockpit_inspector.py`

**Interfaces:**
- Consumes: `inspect_dag_scheduler_v4_bundle(std::string_view)` from Task 1.
- Produces: `POST /api/dag-scheduler-v4/inspect`, HTTP 200 for `VALID`, HTTP 422 for a V4 semantic rejection, and existing HTTP 413/500 generic JSON behavior.
- Produces a third explicit page selection `dag-scheduler-v4` (the concrete option value is `v4`) with title `BRUCE Cockpit — DAG-Scheduler V4 Inspector`, route `/api/dag-scheduler-v4/inspect` and `maxBytes: 5 * 1024 * 1024`.

- [ ] **Step 1: Extend the Python HTTP tests before server changes.**

  Extend `canonical_bundle()` with the Task-1 writer mode. Add a V4 success test that sends real CLI-compatible V4 bytes to the new route and asserts only declared metadata; assert known fixture request/payload strings and `BRUCE_DAG_SCHEDULER_RUN_V4` are absent. Add the full route matrix: V2/V3 at V4 and V4 at V2/V3 each return HTTP 422 with the fixed response. Add V4 mutation and 5-MiB-plus-one HTTP 413 cases.

- [ ] **Step 2: Extend the browser/DOM test before UI changes.**

  Add `v4` to expected selector options. In the Node harness, choose `v4` and assert the page title, route, 5-MiB local threshold, overview fields `scheduler_version`, `dag_id`, `max_started_nodes`, `max_total_receipt_bytes`, V4 callback metadata and trace render through `textContent`. Inject unexpected `request_bytes`, `payload_bytes`, path and script strings into the API model and assert none reaches DOM. Verify scheduler explanation stays hidden/empty for V4; verify reset and a late V4 response cannot restore content.

- [ ] **Step 3: Run the selected future workflow cases and record RED.**

  Build the `bruce` target in a complete native temporary copy and run the new V4-named Python tests against it. Expected: route/UI selection failures because V4 is not wired yet; pre-existing V2/V3 tests still pass.

- [ ] **Step 4: Wire the server route.**

  Add only `server.Post("/api/dag-scheduler-v4/inspect", ...)`, set status from the established valid/rejected enum exactly as the existing routes do, and retain the global 5-MiB transport maximum, pre-routing method gate, generic 413/500 bodies and `127.0.0.1` bind. Do not add V4 health, GET, replay, write or auto-detect routes.

- [ ] **Step 5: Add V4 to the existing assets without duplicating the page.**

  Extend `configurations`, `selectedConfiguration`, the `<select>` and field projection helpers so V4 maps only declared Scheduler V4 fields. Render the validation sentence exactly as specified: `VALID: canonical bundle validation passed; not re-executed.` (or equivalent documented German/English copy that unambiguously states no reexecution). Keep `textContent`, revision cancellation, `ArrayBuffer` release, reset and V2/V3 behavior. Show the scheduler section only for V3.

- [ ] **Step 6: Run HTTP/UI and compatibility regressions GREEN.**

  Run `cockpit_inspector_workflows` in a permitted environment, plus `cockpit_inspector_contracts`, V2/V3 CLI workflow tests and V4 CLI workflows. In a Cockpit/NanoCell-OFF build, run V4 CLI workflows to prove V4 CLI remains independent. Capture socket policy failures separately from assertion failures in `runs/dag-scheduler-v4-cockpit-sdd/reports/task-2.md`.

- [ ] **Step 7: Request a task-scoped review.**

  A fresh reviewer checks route inventory, localhost/read-only invariants, size limits, UI projection and V2/V3 compatibility. Save the verdict and any remediation evidence under `runs/dag-scheduler-v4-cockpit-sdd/reviews/task-2.md`.

### Task 3: Documentation, generated references and integrated acceptance

**Files:**
- Modify: `README.md`
- Modify: `DEPENDENCIES.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/architecture/repository-structure.md`
- Modify: `docs/cli/README.md`
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/implementation/README.md`
- Modify: `tests/mods/cockpit/test_cockpit_inspector.py`
- Generate only through the verifier: `docs/cli/source-index.md`, `docs/cli/examples.json`, `docs/cli/validation.md`

**Interfaces:**
- Consumes: the implemented V4 Cockpit route and page contract from Tasks 1–2.
- Produces: user-facing documentation that labels the feature **IMPLEMENTIERT UND BELEGT** only after its tests pass, while retaining broader runtime/network/GPU/plugin/storage claims as **DOKUMENTIERT / KONZEPT**.

- [ ] **Step 1: Add failing documentation-contract assertions.**

  Extend `CockpitInspectorDocumentationTests` so every current Cockpit scope names DAG-Scheduler V4, `POST /api/dag-scheduler-v4/inspect`, explicit selection, 5 MiB, local `127.0.0.1`, metadata-only output and the no-reexecution/read-only boundary. Retain assertions for V2/V3 routes and their separate limits. Add explicit negative assertions against replay buttons/routes, persistent uploads, payload download, dynamic plugins and external access.

- [ ] **Step 2: Run documentation-only tests RED.**

  Run `python3 tests/mods/cockpit/test_cockpit_inspector.py --documentation-only /path/to/bruce` from the full temporary build. Expected: current docs omit V4 Cockpit support.

- [ ] **Step 3: Update human-authored documentation.**

  Update the eight listed prose files with the exact implemented Cockpit route, safe metadata fields, V4 confidentiality boundary, no-reexecution meaning and all non-goals. Do not portray free language, general proving, GPU/network execution, free code plugins, external payload storage or global runtime orchestration as implemented.

- [ ] **Step 4: Regenerate and compare CLI artifacts.**

  In a full native temporary copy without spaces, run `docs/cli/verify_examples.py`; copy back only the three generated artifacts if and only if their regenerated contents differ. Do not edit their contents manually. Record example/option/diagnostic counts.

- [ ] **Step 5: Run final integration checks.**

  In an independent native temporary build, run the AGENTS.md CMake configure/build/CTest path, targeted Cockpit/V4/V2/V3 test set, `python3 tests/integration/test_cli_docs.py`, local Markdown-link checking and `git diff --check` if Git becomes available. Report test numbers, warnings and any known sandbox-specific socket failure separately in `runs/dag-scheduler-v4-cockpit-sdd/reports/task-3.md`.

- [ ] **Step 6: Request the final whole-change review.**

  A fresh reviewer reads the spec, this plan, all changed product files and test results. The review must explicitly verify V4 is still a read-only local Inspector rather than a runtime orchestration claim. Save its verdict under `runs/dag-scheduler-v4-cockpit-sdd/reviews/final.md`.

## Plan Self-Review

- **Spec coverage:** Task 1 implements the V4 decoder/projection and no-execution boundary; Task 2 implements the fixed route, explicit selection, safe UI and HTTP behavior; Task 3 maps the local/read-only contract into documentation and runs full acceptance. No spec requirement is unowned.
- **Step quality:** Every test names an exact target behavior or fixture; every implementation step names the interface and boundary it creates. No task asks for a generic “appropriate” behavior.
- **Type consistency:** Task 1 supplies `inspect_dag_scheduler_v4_bundle`; Task 2 consumes exactly that result. Both retain `CockpitInspectionResultV1` and the existing V2 run data types instead of inventing a transport model.
- **Review focus:** Each listed failure mode is assigned to Task 1 or Task 2 and is covered by a concrete negative test.
- **Proportion:** Three independently reviewable tasks match the core adapter, public surface and documentation/acceptance boundaries without planning unrelated runtime features.
