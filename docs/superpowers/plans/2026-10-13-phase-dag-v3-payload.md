# Phase-DAG V3 Payload Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Einen eigenständigen, portablen Phase-DAG-V3-Lauf mit begrenzten
Payload-Kanten, NanoCell-Erzeuger und konsumierendem Payload-Gate bereitstellen.

**Architecture:** V3 erhält eigene Kernmodelle, Serialisierung und Replay;
V2 bleibt byte- und API-kompatibel. Der NanoCell-Moduladapter veröffentlicht
einen kleinen fachlichen Payload, das Gate konsumiert direkte Payload-Inputs
und erzeugt einen Manifest-Payload. Eine separate V3-CLI parst ausschließlich
die zwei fest eingebauten Knotentypen und veröffentlicht/prüft V3-Bundles.

**Tech Stack:** C++20, CMake/Ninja/CTest, Python `unittest`, SHA-256 über die
vorhandenen Core-Hilfen, vorhandener `LocalFiles`-Veröffentlichungsvertrag.

**Spec:** [2026-10-13-phase-dag-v3-payload-design.md](../specs/2026-10-13-phase-dag-v3-payload-design.md)

## Global Constraints

- V2-Typen, V2-Bundles, V2-CLI und V2-Adapter bleiben unverändert.
- V3 verwendet `BRUCE_PHASE_DAG_RUN_V3` und `BRUCE_PHASE_DAG_SPEC_V3`.
- Nur `nanocell-phase-v3`/`v1` und `payload-gate-v1`/`v1` sind eingebaut.
- Direkte Payloads sind höchstens 65536 Bytes groß; Bundle- und Receiptgrenzen
  bleiben kontrollierte, kanonisch geprüfte Grenzen.
- Kein externer Payloadspeicher, keine Adapter-Plugins, keine Cockpit-V3-Route.
- Generierte CLI-Dateien nur mit `docs/cli/verify_examples.py` in einer
  vollständigen temporären Kopie aktualisieren.
- In diesem Checkout sind Git-Status, Diff und Commits nicht verfügbar; keine
  Commit-Schritte vortäuschen. Vor Abschluss nur die vorhandenen CMake-,
  CTest-, Generator- und Link-Prüfungen ausführen.

## Review Focus

- Eine 65537-Byte-Payload muss vor Veröffentlichung kontrolliert scheitern;
  Task 1 ergänzt die Grenztests.
- Vertauschte oder manipulierte Payloadbytes/-digests müssen Replay scheitern
  lassen; Task 1 ergänzt Rehash-Angriffe.
- Leere, `INCOMPLETE`- oder `REJECTED`-Inputs müssen beim Gate ein READY-
  Receipt mit `REJECTED` erzeugen; Task 2 testet jede Variante.
- Eine Kette aus zwei Gates muss den Manifest-Payload des ersten Gates wirklich
  als Input des zweiten Gates konsumieren; Task 2 testet dies end-to-end.
- V2 und das V2-only Cockpit dürfen V3 weder stillschweigend akzeptieren noch
  ihre bestehenden Verträge ändern; Tasks 3 und 4 führen die bestehenden V2-
  und Cockpit-Prüfungen aus.

---

### Task 1: Eigenständiger V3-DAG-Kern und kanonischer Bundlecodec

**Files:**
- Create: `include/bruce/phase_dag/phase_dag_v3.hpp`
- Create: `src/phase_dag/phase_dag_v3.cpp`
- Create: `tests/unit/phase_dag_v3_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `PhaseNodeSpecV3`, `PhaseDagSpecV3`, `PhaseInputV3`,
  `PhaseOutputRefV3`, `PhaseAdapterV3`, `PhaseDagRunV3`,
  `execute_phase_dag_v3`, `pack_phase_dag_run_v3` and
  `replay_phase_dag_run_v3` in namespace `bruce`.
- `PhaseOutputRefV3` contains `output_bytes/output_digest`,
  `evidence_bytes/evidence_digest`, `payload_bytes/payload_digest`, status
  and claim scope. `PhaseInputV3` contains producer identity, status, all
  three digest pairs and the producer payload bytes.
- Task 2 consumes the V3 adapter signature
  `PhaseOutputRefV3 (*execute)(std::string_view, std::span<const PhaseInputV3>)`.

- [ ] **Step 1: Write failing core-contract tests**

  Add a small literal fake V3 adapter in `tests/unit/phase_dag_v3_tests.cpp`.
  Assert a two-node dependency transports the exact named payload, 65536 bytes
  serialize/replay, 65537 bytes are rejected, and changing one serialized
  payload byte or payload digest (then recomputing the outer digest) makes
  replay fail.

- [ ] **Step 2: Run the new test to verify RED**

  Run: `scripts/with-host-deps.sh cmake --build /tmp/bruce-build --target bruce_phase_dag_v3_tests && scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build -R phase_dag_v3_contracts --output-on-failure`

  Expected: the target is absent before CMake registration, then the desired
  V3 API symbols are absent after registering only the test target.

- [ ] **Step 3: Implement V3 types, validation, execution and codec**

  Add the V3 source to `bruce_core` and a `phase_dag_v3_contracts` CTest
  target. Copy the established V2 deterministic scheduling and canonical
  framing pattern, but use a distinct V3 header and separate V3 enum/types.
  Validate all three digest pairs, cap individual payloads at 65536 bytes and
  include payload bytes/digests in receipts, encoding and replay comparison.

- [ ] **Step 4: Run the V3 core contract test to verify GREEN**

  Run the command from Step 2.

  Expected: `phase_dag_v3_contracts` passes with the transport, size and
  forged-bundle cases.

### Task 2: NanoCell-V3 producer and Payload-Gate consumer

**Files:**
- Create: `include/bruce/phase_dag/nanocell_adapter_v3.hpp`
- Create: `include/bruce/phase_dag/payload_gate_adapter_v3.hpp`
- Create: `mods/nanocell/src/phase_dag_adapter_v3.cpp`
- Create: `tests/mods/nanocell/phase_dag_payload_v3_tests.cpp`
- Modify: `mods/nanocell/CMakeLists.txt`

**Interfaces:**
- Consumes Task-1 V3 adapter API and existing `NanoCellPhaseRequestV1` codec.
- Produces `pack_nanocell_phase_payload_v1`,
  `unpack_nanocell_phase_payload_v1`, payload-gate request/metadata codecs,
  and `phase_dag_nanocell_catalog_v3()`.
- Task 3 consumes this catalog when executing and replaying V3 specs.

- [ ] **Step 1: Write failing adapter-contract tests**

  Add literal NanoCell requests and V3 nodes. Assert a `COMPLETE` NanoCell
  payload is consumed by `payload-gate-v1`; each of empty payload,
  `INCOMPLETE`, and `REJECTED` input produces READY + `REJECTED`; malformed
  direct input fails; and a two-gate chain consumes the first gate manifest.
  Assert V3 replay is byte-identical.

- [ ] **Step 2: Run the adapter contract to verify RED**

  Run: `scripts/with-host-deps.sh cmake --build /tmp/bruce-build --target bruce_phase_dag_payload_v3_tests && scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build -R phase_dag_payload_v3_contracts --output-on-failure`

  Expected: fail because no V3 adapter catalog or payload codecs exist.

- [ ] **Step 3: Implement the two fixed V3 adapters**

  Add the V3 adapter source to `bruce_phase_dag_nanocell`. Reuse V2 NanoCell
  execution semantics but encode only `nanocell-status`, a final-state-presence
  marker and an optional final state in `nanocell-phase-payload-v1`.
  Implement `payload-gate-v1` as a parameterless request: require at least one
  nonempty, digest-correct, COMPLETE input for COMPLETE; otherwise generate a
  valid REJECTED output. Emit a canonical manifest payload and input-reference
  evidence without copying raw input payloads into evidence.

- [ ] **Step 4: Run V3 adapter and existing V2 NanoCell contracts**

  Run: `scripts/with-host-deps.sh cmake --build /tmp/bruce-build --target bruce_phase_dag_payload_v3_tests bruce_phase_dag_nanocell_tests && scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build -R 'phase_dag_(payload_v3|nanocell)_contracts' --output-on-failure`

  Expected: both V3 payload behavior and unmodified V2 contracts pass.

### Task 3: Strikte Phase-DAG-V3-CLI

**Files:**
- Create: `apps/phase_dag_v3_cli.hpp`
- Modify: `apps/bruce_cli.cpp`
- Create: `tests/mods/nanocell/test_phase_dag_v3_cli.py`

**Interfaces:**
- Consumes `PhaseDagSpecV3`, V3 execution/replay functions and
  `phase_dag_nanocell_catalog_v3()` from Tasks 1–2.
- Produces the commands `bruce phase-dag-v3 execute SPEC --output BUNDLE`,
  `inspect BUNDLE` and `replay BUNDLE`, guarded by
  `BRUCE_BUILD_MOD_NANOCELL`.

- [ ] **Step 1: Write failing CLI workflow tests**

  Create a V3 Spec fixture with a NanoCell node and `payload-gate-v1` node.
  Assert execute produces the metadata-only JSON report and a V3 bundle;
  removing the Spec/context files leaves inspect and replay successful;
  malformed Gate fields, oversized payload behavior and a V2 bundle supplied
  to V3 inspect are rejected without raw payload leakage. Add the NanoCell-off
  build assertion that V3 help and routing are absent.

- [ ] **Step 2: Run the CLI workflow test to verify RED**

  Run: `python3 tests/mods/nanocell/test_phase_dag_v3_cli.py /tmp/bruce-build/bruce`

  Expected: fail because `phase-dag-v3` is unknown and the V3 parser is absent.

- [ ] **Step 3: Implement strict V3 parsing and command routing**

  Model argument validation, bounded readers, atomic publication and JSON
  report rendering on `phase_dag_v2_cli.hpp`, but accept only the V3 header,
  fixed NanoCell-V3 node form and exact three-field Payload-Gate node form.
  Route/help must be compiled only with NanoCell enabled. Do not add Cockpit
  routes or expose payload bytes in reports.

- [ ] **Step 4: Run V3 CLI tests and existing V2 CLI tests**

  Run: `python3 tests/mods/nanocell/test_phase_dag_v3_cli.py /tmp/bruce-build/bruce && python3 tests/mods/nanocell/test_phase_dag_v2_cli.py /tmp/bruce-build/bruce`

  Expected: V3 behavior passes and V2 remains unchanged.

### Task 4: Dokumentation, gemessene Beispiele und Integrationsprüfung

**Files:**
- Modify: `docs/cli/README.md`
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/cli/results-and-control.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/cli/verify_examples.py`
- Regenerate only: `docs/cli/source-index.md`, `docs/cli/examples.json`, `docs/cli/validation.md`

**Interfaces:**
- Documents Task-3 commands, fixed V3 adapters, 64-KiB payload bound, replay,
  V2 compatibility and the explicit V2-only Cockpit boundary.

- [ ] **Step 1: Write failing documentation assertions**

  Extend the V3 Python documentation test with literals for the V3 commands,
  `BRUCE_PHASE_DAG_SPEC_V3`, `payload-gate-v1`, the 65536-byte limit,
  no raw-payload report and the V2-only Cockpit boundary.

- [ ] **Step 2: Run the documentation assertion to verify RED**

  Run: `python3 tests/mods/nanocell/test_phase_dag_v3_cli.py /tmp/bruce-build/bruce PhaseDagV3CliDocumentationTests`

  Expected: fail because user documentation and measured examples do not yet
  name V3.

- [ ] **Step 3: Document V3 and generate evidence in a full temporary copy**

  Update only human-authored documents and `verify_examples.py`. Copy the
  complete repository to a native temporary path, build its `bruce` binary,
  run `python3 docs/cli/verify_examples.py ABSOLUTE_BINARY_PATH` there, then
  copy back exactly the three generated CLI artifacts. Never hand-edit them.

- [ ] **Step 4: Verify docs and regressions**

  Run: `python3 tests/mods/nanocell/test_phase_dag_v3_cli.py /tmp/bruce-build/bruce PhaseDagV3CliDocumentationTests && python3 scripts/check_local_doc_links.py && scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build -R 'phase_dag_v2_cli_workflows|phase_dag_v3_cli_workflows|cockpit_inspector_contracts' --output-on-failure`

  Expected: documentation assertions and local links pass; V2 CLI and
  V2-only Cockpit contract remain green alongside V3.

## Plan Self-Review

- **Spec coverage:** Tasks 1–3 implement the V3 core, fixed adapters, portable
  bundles, CLI, limits and rejection semantics. Task 4 documents and proves
  generated evidence. No spec requirement is unassigned.
- **Type consistency:** Task 1 introduces all `*V3` core types; Task 2's
  catalog matches Task 3's single catalog consumer. No V2 type is a V3
  parameter.
- **Review focus coverage:** Task 1 owns payload size and bundle tampering;
  Task 2 owns gate input states and gate chains; Tasks 3–4 own V2/Cockpit
  boundaries.
- **Scope:** No external persistence, Cockpit V3 work, plugin API or scheduler
  change has been included.
