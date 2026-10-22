# Worker Run Inspector / Recovery V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax where implementation begins.

**Goal:** Einen lokalen, ausschließlich lesenden Inspector für bestehende
V5-Run-Bäume bauen. Er validiert JSONL-Verlauf, Attempt-Zustände und
referenzierte Artefakte und liefert einen metadata-only Bericht, ohne Worker,
Adapter oder Schreibpfade zu verwenden.

**Architecture:** Ein neuer `WorkerRunInspectorV1` liest den vorhandenen
`worker-run-log-v1`-Vertrag und die privaten Attempt-Dateien. Die V5-CLI bindet
`inspect BUNDLE` ausschließlich an den Seitensatz `BUNDLE.run/`. Der Inspector
verändert weder `WorkerRunLog::append` noch die bestehende Execute-Strecke.

**Tech Stack:** C++20, vorhandene Worker-/V5-Typen, Linux `openat`/`O_NOFOLLOW`
und `fstat`, bestehendes SHA-256, CMake/CTest und Python-Integrationstests.

**Spec:**
[2026-10-15-worker-run-inspector-recovery-v1-design.md](../specs/2026-10-15-worker-run-inspector-recovery-v1-design.md)

## Global Constraints

- Die neue öffentliche Syntax lautet exakt
  `bruce dag-scheduler-v5 inspect BUNDLE`.
- `inspect` akzeptiert keine Optionen, keine Spec-Datei, keinen alternativen
  Run-Root und keinen Worker-/Adapterpfad.
- `BUNDLE` bindet ausschließlich an `BUNDLE.run/`; ein freier Pfad aus einer
  Logzeile darf den Prüfroot nicht verändern.
- Der Inspector startet keinen Prozess, ruft keinen Adapter auf und schreibt
  keine Datei. Auch temporäre Dateien, Reparaturdateien und Log-Truncation sind
  verboten.
- Vollständige, strukturell gültige Runs liefern Exit 0; gültige Runs ohne
  `RUN_FINISHED` liefern Exit 1 mit `INCOMPLETE`; korrupte oder unsichere Daten
  liefern Exit 2 mit dem generischen `PROTOCOL_ERROR`.
- Eine regulär mit `FAILED`, `BLOCKED` oder `CANCELLED` beendete Run-Struktur
  ist gültig und bleibt inhaltlich fehlgeschlagen.
- JSONL- und Artefaktgrößen bleiben durch die bestehenden Dateilimits begrenzt;
  keine Allokation darf aus unvalidierten Längen entstehen.
- Jede Artefaktreferenz wird relativ zum geöffneten Run-Root aufgelöst.
  Absolute Pfade, `..`, `.`, Backslashes, Symlinks, Mehrfach-Hardlinks und
  Pfadauflösung außerhalb des Roots werden abgewiesen.
- Die Artefaktprüfung nutzt Größe und SHA-256. Payloadbytes erscheinen weder
  im Inspect-Bericht noch in Fehlerausgaben.
- Der bestehende V5-Execute-Bericht und die V2-, V3- und V4-Verträge bleiben
  bytegleich und unverändert.
- Es wird kein Artifact Store, kein GC, kein Payload-Download, kein Netzwerk,
  kein GPU-Adapter, kein dynamisches Plugin und keine allgemeine Orchestrierung
  eingeführt.
- Generierte CLI-Artefakte werden ausschließlich über
  `docs/cli/verify_examples.py` in einer temporären nativen Kopie geprüft oder
  erzeugt.
- Wegen des leeren `.git`-Verzeichnisses werden keine erfundenen Commits oder
  Git-Diffs behauptet. Zwischenstände und Testausgaben gehören unter `runs/`.

## Review Focus

- **Read-only:** Eine kontrollierte Sentinel-Datei und Dateibaum-Hashes zeigen,
  dass `inspect` keine Worker-/Adapterausführung und keine Änderung auslöst.
- **Truncated logs:** Fehlende abschließende LF, Teilobjekte, doppelte oder
  nichtkanonische Felder ergeben `PROTOCOL_ERROR`; ein vollständiger Verlauf
  ohne `RUN_FINISHED` ergibt dagegen `INCOMPLETE`.
- **Event state:** Mehrere unabhängige V5-Attempts dürfen global verschachtelt
  sein, aber jede per-Attempt-Sequenz und `RUN_FINISHED` müssen korrekt sein.
- **Path safety:** Symlink, Hardlink, absolute Referenz, `..`, Root-Escape,
  reguläre Datei- und Größenprüfung müssen mit Tests belegt werden.
- **Digest safety:** Geänderte oder verkürzte Artefakte werden vor einem
  `VALID`-/`INCOMPLETE`-Bericht abgewiesen.
- **No payload leakage:** Bekannte Request-/Payload-Marker, absolute Pfade,
  `attempts/...`, Kommandozeilen und Stderr dürfen nicht im stdout/stderr-
  Bericht erscheinen.

## Task 1: Read-only Run-Inspector-Kern und Artefaktprüfung

**Files:**

- Create: `include/bruce/worker/worker_run_inspector_v1.hpp`
- Create: `src/worker/worker_run_inspector_v1.cpp`
- Create: `tests/unit/worker_run_inspector_v1_tests.cpp`
- Modify: `CMakeLists.txt`
- Optional, only to share existing validation helpers without changing the
  public append contract: `include/bruce/worker/worker_run_log_v1.hpp` and
  `src/worker/worker_run_log_v1.cpp`

**Interfaces:**

- Produces `WorkerRunInspectionV1 inspect_worker_run_v1(const
  std::filesystem::path& run_root)`.
- Produces bounded inspection data containing `run_id`, `VALID`/`INCOMPLETE`,
  `run_finished`, `run_status`, `last_seq` and per-Attempt metadata. The model
  may retain digest, size, Usage, event count and `artifact_verified`, but it
  must not retain payload bytes or local path strings for rendering.
- Reuses the existing `WorkerLogEvent`, `WorkerUsage`, identifier/status,
  reference and JSONL canonicalization rules. Do not create a permissive
  second JSON contract.

- [ ] **Step 1: Write failing unit tests for valid complete and incomplete
  runs.** Build temporary run roots through `WorkerRunLog::create` and
  `append`. Cover a complete single-attempt success, a parallel-looking
  interleaving of two Attempts, a valid terminal `FAILED` run and a complete
  retry history. Cover a well-formed log without `RUN_FINISHED` and assert
  `INCOMPLETE`, no continuation and no writes.

- [ ] **Step 2: Add failing corruption tests.** Cover empty log, missing or
  duplicated `RUN_CREATED`, `RUN_FINISHED` in the middle, duplicate/nonmonotone
  sequence numbers, an unknown event, an attempt event before
  `ATTEMPT_STARTED`, `PROCESS_EXITED` without `RESULT_RECEIVED`, acceptance
  without a matching result, retry after acceptance and a second result for
  one Attempt. Cover a final partial JSON object and a missing final LF as
  `PROTOCOL_ERROR`.

- [ ] **Step 3: Add failing file-safety tests.** Cover missing referenced
  output, wrong size, wrong digest, oversized file, symlink at every path
  component, a symlink that resolves inside the root, a symlink that escapes,
  and a second hardlink to the same artifact. Assert all fail closed without
  exposing the path. Assert valid regular owner-only files pass.

- [ ] **Step 4: Run the focused unit target and record the RED state.** Use a
  native temporary CMake build and run
  `ctest --test-dir /tmp/bruce-run-inspector-build -R
  '^worker_run_inspector_v1_contracts$' --output-on-failure` before the
  implementation exists.

- [ ] **Step 5: Implement bounded full-run validation.** Read the existing
  JSONL under the file limit, require complete lines, validate canonical
  fields, retain only metadata, and apply the per-Attempt/run state machine.
  Permit interleaving from independent workers while preserving each
  Attempt's local order. Treat absent final `RUN_FINISHED` as `INCOMPLETE`.

- [ ] **Step 6: Implement descriptor-relative artefact verification.** Open
  the run root and each directory component with no-follow semantics; use
  `fstat` to require a regular file and `st_nlink == 1`; stream the bounded
  file through SHA-256 and compare exact size and digest. Never canonicalize a
  symlink into acceptance and never return filesystem paths in the model.

- [ ] **Step 7: Run the focused target GREEN and regress the existing log
  target.** Run `worker_run_inspector_v1_contracts` and
  `worker_run_log_v1_contracts`. Existing append behavior and V5-produced logs
  must remain accepted.

- [ ] **Step 8: Request a task-scoped review.** The reviewer checks the
  malformed/truncated log boundary, interleaved Attempt state machine,
  no-follow path handling and absence of payload retention. Save the report
  under `runs/bruce-worker-run-inspector-v1/`.

## Task 2: V5 `inspect` CLI and executable workflow

**Files:**

- Modify: `apps/dag_scheduler_v5_cli.hpp`
- Modify: `tests/unit/dag_scheduler_v5_cli_tests.cpp`
- Create or modify: `tests/integration/test_dag_scheduler_v5_inspector.py`
- Modify: `CMakeLists.txt`

**Interfaces:**

- Extends only the V5 route with
  `bruce dag-scheduler-v5 inspect BUNDLE`.
- Produces exactly the bounded metadata report
  `dag-scheduler-v5-inspect-v1` on stdout for valid or incomplete runs.
- Produces only the compact `PROTOCOL_ERROR` JSON on stderr for invalid run
  data. Usage errors use the existing V5 usage error shape and exit 2.

- [ ] **Step 1: Add failing parser tests.** Assert exact acceptance of
  `inspect BUNDLE`, rejection of missing BUNDLE, options, extra arguments,
  `inspect BUNDLE.run`, an output option and a worker executable. Keep the
  existing Execute parser tests and defaults unchanged.

- [ ] **Step 2: Add failing real-binary tests.** Execute a small literal,
  append/join dependency graph through the existing V5 route, then inspect its
  output BUNDLE. Assert `VALID`, run ID, last sequence, attempt metadata,
  artifact verification and no payload marker, absolute path or
  `attempts/...` string in either public output or diagnostics. Invoke inspect
  twice and require byte-identical output.

- [ ] **Step 3: Add failing incomplete/corruption workflows.** Copy an
  executed `.run` tree to a temporary location, remove `RUN_FINISHED` to
  obtain `INCOMPLETE`, then separately truncate the JSONL, alter a digest,
  replace an output with a symlink and add a hardlink. Assert the documented
  exit/status/error matrix and that all source bytes remain unchanged.

- [ ] **Step 4: Implement the exact route dispatch.** Add a separate `inspect`
  branch before Execute option parsing. Require the BUNDLE metadata file and
  derive only `BUNDLE.run`; do not accept a user-supplied run root. Read both
  through bounded no-follow checks, call only
  `inspect_worker_run_v1`, render the metadata-only report and never invoke
  `start_attempt`, `poll_attempt`, `finish_attempt`, `request_cancel` or an
  adapter registry.

- [ ] **Step 5: Implement stable report rendering.** Render fields in one
  fixed order, sort Attempts by their first observed start sequence, preserve
  only the approved metadata, terminate with one LF and keep runtime timing
  diagnostics out of any semantic digest. Normalize all expected data errors
  to `PROTOCOL_ERROR` without leaking OS text.

- [ ] **Step 6: Run V5, worker, V2, V3 and V4 workflows GREEN.** The existing
  Execute behavior, V5 retry/multi-worker behavior and V2/V3/V4 outputs must
  remain unchanged. Record exact exit codes and test counts.

- [ ] **Step 7: Request a task-scoped review.** Review the CLI shape, no-write
  behavior, BUNDLE-to-sidecar binding, metadata-only report and execution
  exclusion. Save the verdict with the implementation evidence.

## Task 3: Documentation, generated references and final verification

**Files:**

- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/implementation/README.md` if its V5 boundary table needs the
  new read-only route
- Modify: `tests/integration/test_cli_docs.py` only for the new documented
  contract assertions
- Generate only through the verifier: `docs/cli/source-index.md`,
  `docs/cli/examples.json`, `docs/cli/validation.md`
- Create: `runs/bruce-worker-run-inspector-v1-<date>.md`

**Interfaces:**

- Documents `inspect` as **IMPLEMENTIERT UND BELEGT** only after Tasks 1–2
  pass.
- Keeps permanent Artifact Store, payload download, external references,
  dynamic plugins, GPU/network execution and broad orchestration labeled
  **DOKUMENTIERT / KONZEPT**.

- [ ] **Step 1: Add failing documentation assertions.** Require the exact
  V5 `inspect` syntax, `dag-scheduler-v5-inspect-v1`, valid/incomplete/error
  matrix, JSONL sequence rules, artifact size/digest checks, no-follow path
  policy and metadata-only boundary. Add negative assertions for replay,
  writes, payload download, external storage, plugins and network execution.

- [ ] **Step 2: Update human-authored CLI and architecture documentation.**
  Include the BUNDLE-to-`BUNDLE.run` binding, the difference between
  `INCOMPLETE` and `PROTOCOL_ERROR`, and the fact that inspect does not resume,
  reexecute or repair a run.

- [ ] **Step 3: Regenerate protected CLI artifacts.** In a complete temporary
  native copy run `docs/cli/verify_examples.py` against the built binary. Copy
  back only the verifier-produced `source-index.md`, `examples.json` and
  `validation.md` if repository workflow requires it; do not hand-edit them.

- [ ] **Step 4: Run focused and compatibility verification.** Run the worker
  inspector unit/integration targets, worker boundary/log/supervisor targets,
  V5 workflows, all V2/V3/V4 workflow tests and `test_cli_docs.py`.

- [ ] **Step 5: Run the full repository checks.** Use the AGENTS.md CMake,
  build and CTest sequence in a native temporary copy. Run local Markdown-link
  checks, a conservative secret scan, and `git diff --check`/status only when
  valid Git metadata exists. Report the known sandbox-only Cockpit socket
  limitation separately from product failures.

- [ ] **Step 6: Request the final whole-change review.** The reviewer checks
  that the new feature is a read-only V5 observer, not an Artifact Store,
  scheduler, plugin system or recovery executor, and that every spec boundary
  has a test and evidence label. Save the final verdict under
  `runs/bruce-worker-run-inspector-v1/`.

## Plan Self-Review

- **Spec coverage:** Task 1 owns JSONL parsing, sequence/event validation,
  incomplete-run classification and descriptor-relative artifact checks. Task
  2 owns the exact CLI route, report and no-execution workflow. Task 3 owns
  documentation and full verification.
- **Compatibility:** Execute remains the existing V5 route; V2, V3, V4,
  Worker Boundary V1 and the append-only log contract remain unchanged.
- **Failure coverage:** Truncated lines, missing final LF, malformed events,
  sequence errors, invalid transitions, digest/size mismatch, symlink,
  hardlink and root escape each have explicit tests.
- **Security boundary:** No path, payload, stderr, command line or filesystem
  error crosses the public report. No write or process start is reachable from
  `inspect`.
- **Proportion:** Three tasks implement one narrow read-only capability and
  deliberately defer durable storage, payload retrieval, plugins and global
  orchestration.
