# Phase-DAG V2 NanoCell Adapter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Einen versionsgetrennten, seriellen und replaybaren Phase-DAG-V2-Kern mit dem einzigen statisch gelinkten Adapter `nanocell-phase-v1` bereitstellen.

**Architecture:** `bruce_core` enthält V2-Modelle, Validator, seriellen Scheduler und kanonischen V2-Codec, kennt aber NanoCell nicht. `bruce_phase_dag_nanocell` hängt von `bruce_core` und `bruce_mod_nanocell` ab und liefert einen unveränderlichen Ein-Adapter-Katalog; damit bleibt die bestehende Abhängigkeitsrichtung erhalten. V1 und dessen zwei privaten Fixture-Adapter bleiben unangetastet.

**Tech Stack:** C++20, STL, vorhandenes `bruce::sha256`, NanoCell-Mod, CMake, Ninja und CTest.

**Spec:** `docs/superpowers/specs/2026-10-12-phase-dag-v2-nanocell-adapter-design.md`

## Global Constraints

- V1 bleibt byte- und verhaltenskompatibel; weder `include/bruce/phase_dag/phase_dag.hpp` noch `src/phase_dag/phase_dag.cpp` werden für V2 umgedeutet oder um verdrahtete NanoCell-Aufrufe erweitert.
- V2 verwendet ausschließlich `phase-dag-v2`, `phase-output-v2` und den Bundleheader `BRUCE_PHASE_DAG_RUN_V2`; V1-Bundles werden nie als V2 gelesen.
- IDs sind 1..64 ASCII-Zeichen aus `[A-Za-z0-9_.-]`; eine Spec enthält 1..16 sortierte, eindeutige Nodes, höchstens 15 Vorgänger je Node und höchstens 64 Kanten.
- Der Scheduler ist seriell, startet den lexikographisch kleinsten entscheidungsfähigen Node und hält `READY`, `FAILED`, `BLOCKED` und `CANCELLED` strikt von `COMPLETE`, `INCOMPLETE` und `REJECTED` getrennt.
- Ein lokaler NanoCell-Befund ist keine mathematische Wahrheit, kein Gate, keine Freigabe und keine Berechtigung. Es werden weder CLI, ApplicationTask, ExecutionSession, Gates, Experience, Publish/Learn, Cockpit, GPU, Parallelität noch allgemeine semantische Zyklen ergänzt.
- Der Produktionskatalog wird ausschließlich beim Build gelinkt: keine Laufzeitregistrierung, Shared-Library-Suche, Datei- oder Netzwerkzugriffe und kein zweiter Produktionsadapter.
- `nanocell-phase-v1` akzeptiert nur die drei vorhandenen Referenztechniken `masked-match-v1`, `rotate-match-v1` und `context-conflict-v1`, acht Corebytes, Index 0..7, höchstens 4096 Kontextbytes, höchstens 16 Operationen und 2..18 Traceeinträge.
- Evidenzbytes sind der unveränderte kanonische NanoCell-Mod-Envelope. Alle Digests sichern Integrität; sie autorisieren keine fachliche Aussage.
- Keine neuen Drittanbieterabhängigkeiten. Neue Produktionsziele und Tests bauen mit `-Wall -Wextra -Werror`.
- Ausführung und Abnahme erfolgen über `scripts/with-host-deps.sh` aus einer temporären Kopie ohne problematische Pfadzeichen; die lokalen, ignorierten Dependency-Artefakte bleiben unverändert.
- Die bereitgestellte Arbeitskopie enthält kein `.git`; bis ein Repository verfügbar ist, werden keine Commit-Schritte simuliert oder behauptet.

## Review Focus

- **Katalogverwechslung:** Eine unbekannte Adapter-ID, -Version oder ein nicht kanonischer Katalog muss vor jedem NanoCell-Aufruf die V2-Spec ablehnen; Task 1 testet diese Vorbedingung.
- **Statusverwechslung:** `CONTEXT_REJECTED` und `INCOMPLETE` bleiben NanoCell-Ausgaben mit `READY`-Receipt, während ungültige Requests zu `FAILED` führen; Task 3 prüft alle vier NanoCell-Endzustände.
- **Abhängigkeitsisolation:** Ein fehlgeschlagener NanoCell-Node darf nur seine Nachfolger blockieren; unabhängige Nodes müssen weiterhin in kanonischer Reihenfolge laufen; Task 2 prüft dies mit einem Testkatalog und Task 3 mit NanoCell.
- **Evidenzersetzung:** Ein V2-Output darf nur einen unveränderten NanoCell-Mod-Envelope enthalten; Task 3 replays sowohl den eingebetteten Envelope als auch das V2-Bundle.
- **Neu gehashte Manipulation:** Korrekte äußere Prüfsummen dürfen keine veränderten Spec-, Request-, InputRef-, Output-, Receipt- oder Tracebytes legitimieren; Task 2 und Task 3 enthalten je gezielte Mutationen.

---

## File Structure

- Create: `include/bruce/phase_dag/phase_dag_v2.hpp` – öffentlicher V2-Graph-, Adapter-, Receipt- und Replayvertrag.
- Create: `src/phase_dag/phase_dag_v2.cpp` – private V2-Validierung, serieller Scheduler, kanonischer Codec und Replay.
- Create: `include/bruce/phase_dag/nanocell_adapter_v2.hpp` – öffentlicher, enger NanoCell-Phaserequest und der feste Katalogzugang.
- Create: `mods/nanocell/src/phase_dag_adapter_v2.cpp` – Requestdecoder, Referenztechnikzuordnung, NanoCell-Aufruf, Metadaten und Evidenz.
- Modify: `CMakeLists.txt` – V2-Quelle in `bruce_core`, V2-Testtarget und optionale Registrierung des Adaptertesttargets.
- Modify: `mods/nanocell/CMakeLists.txt` – statische Bibliothek `bruce_phase_dag_nanocell` mit eindeutiger Linkrichtung.
- Create: `tests/unit/phase_dag_v2_tests.cpp` – V2-Kernvertrag mit ausschließlich testlokalem statischem Adapterkatalog.
- Create: `tests/mods/nanocell/phase_dag_nanocell_tests.cpp` – reale NanoCell-Adapter-, Evidenz- und V2-Replaytests.
- Create: `docs/implementation/phase-dag-v2-nanocell-measurement.md` – nach Umsetzung gemessene Artefaktgrößen und tatsächliche Prüfbelege.
- Modify: `docs/implementation/README.md` – Link zum V2-Messprotokoll mit klarer Abgrenzung von einer Leistungsbehauptung.

### Task 1: V2-Vertrag, statischer Katalog und Validierung

**Files:**
- Create: `include/bruce/phase_dag/phase_dag_v2.hpp`
- Create: `src/phase_dag/phase_dag_v2.cpp`
- Create: `tests/unit/phase_dag_v2_tests.cpp`
- Modify: `CMakeLists.txt:11-64, 556-559`

**Interfaces:**

```cpp
namespace bruce {
struct PhaseNodeSpecV2 {
    std::string node_id, adapter_id, adapter_version;
    std::vector<std::string> dependencies;
    std::string request_bytes, request_digest;
};
struct PhaseDagSpecV2 {
    std::string dag_version{"phase-dag-v2"}, dag_id;
    std::vector<PhaseNodeSpecV2> nodes;
    std::uint64_t max_started_nodes{}, max_receipt_bytes{}, max_trace_entries{};
};
struct PhaseInputRefV2 {
    std::string producer_node_id, producer_adapter_id, producer_adapter_version;
    std::string producer_status, producer_output_digest, producer_evidence_digest;
};
enum class PhaseOutputStatusV2 { complete, incomplete, rejected };
struct PhaseOutputRefV2 {
    std::string output_version{"phase-output-v2"};
    PhaseOutputStatusV2 status{};
    std::string output_bytes, output_digest, evidence_bytes, evidence_digest, claim_scope;
};
struct PhaseAdapterV2 {
    std::string_view adapter_id, adapter_version;
    PhaseOutputRefV2 (*execute)(std::string_view, std::span<const PhaseInputRefV2>);
};
void validate_phase_dag_spec_v2(const PhaseDagSpecV2&, std::span<const PhaseAdapterV2> catalog);
}
```

- [ ] **Step 1: Add failing V2 validation tests with a test-local immutable catalog**

  In `tests/unit/phase_dag_v2_tests.cpp`, declare a `constexpr`-style one-entry test catalog whose executor only increments a counter if called. Assert acceptance of the smallest valid V2 spec. Assert rejection before that counter changes for: wrong `dag_version`, invalid or unsorted/duplicate IDs and dependencies, self-edge, unknown predecessor, cycle, incorrect request digest, node/edge limits, unknown adapter ID, wrong adapter version, duplicate catalog keys and unsorted catalog keys.

- [ ] **Step 2: Register the V2 core source and red test target**

  Add `src/phase_dag/phase_dag_v2.cpp` to `bruce_core`. Add executable `bruce_phase_dag_v2_tests`, link it only to `bruce_core`, compile with `-Wall -Wextra -Werror`, and register `phase_dag_v2_contracts`. Configure and build the target from a temporary source copy.

  Run: `scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-phase-dag-v2-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release` followed by `scripts/with-host-deps.sh cmake --build /tmp/bruce-phase-dag-v2-build --target bruce_phase_dag_v2_tests`.

  Expected: the target initially fails because V2 types and APIs do not exist.

- [ ] **Step 3: Implement the public V2 models and pre-execution validator**

  Define all V2 types independently from V1. In `phase_dag_v2.cpp`, keep identifier, digest, ordering, limit, graph and catalog validation private. Require a strictly sorted, unique `(adapter_id, adapter_version)` catalog and resolve every node against it during `validate_phase_dag_spec_v2`; never invoke an adapter while validating.

- [ ] **Step 4: Prove the V2 validation contract**

  Run: `scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-phase-dag-v2-build -R '^phase_dag_v2_contracts$' --output-on-failure`.

  Expected: PASS; malformed graph or catalog data is rejected with no test-adapter execution.

### Task 2: V2-Scheduler, Receiptcodec und Replay

**Files:**
- Modify: `include/bruce/phase_dag/phase_dag_v2.hpp`
- Modify: `src/phase_dag/phase_dag_v2.cpp`
- Modify: `tests/unit/phase_dag_v2_tests.cpp`

**Interfaces:**

```cpp
namespace bruce {
enum class PhaseReceiptOutcomeV2 { ready, blocked, failed, cancelled };
struct PhaseReceiptV2 {
    std::string node_id, adapter_id, adapter_version, request_digest;
    std::vector<PhaseInputRefV2> input_refs;
    PhaseReceiptOutcomeV2 outcome{};
    std::optional<PhaseOutputRefV2> output;
    std::string failure_code;
};
enum class PhaseTraceEventV2 { validated, started, ready, blocked, failed, cancelled, completed };
struct PhaseTraceEntryV2 { PhaseTraceEventV2 event; std::string node_id; };
struct PhaseDagControlV2 { std::function<bool()> cancelled; };
struct PhaseDagRunV2 {
    PhaseDagSpecV2 spec;
    std::vector<PhaseReceiptV2> receipts;
    std::vector<PhaseTraceEntryV2> trace;
};
PhaseDagRunV2 execute_phase_dag_v2(const PhaseDagSpecV2&, std::span<const PhaseAdapterV2>,
                                   const PhaseDagControlV2& = {});
std::string pack_phase_dag_run_v2(const PhaseDagRunV2&);
PhaseDagRunV2 replay_phase_dag_run_v2(std::string_view, std::span<const PhaseAdapterV2>);
}
```

- [ ] **Step 1: Add failing scheduler tests using the test-local adapter**

  Extend `phase_dag_v2_tests.cpp` with a deterministic test adapter: request `C:<bytes>` returns `COMPLETE`, `I:<bytes>` returns `INCOMPLETE`, `R:<bytes>` returns `REJECTED`, and `F` throws `Error("TestPhaseAdapterFailure")`. Test lexicographic start order, ordered predecessor input refs, one receipt per node, the distinct `READY`/output-status semantics, local failure with only dependent nodes `BLOCKED`, independent progress, receipt-size failure without exposed output, start-budget cancellation and control cancellation.

- [ ] **Step 2: Add failing canonical pack/replay and rehashed-mutation tests**

  Pack a successful two-node V2 run and require `pack_phase_dag_run_v2(replay_phase_dag_run_v2(bundle, catalog)) == bundle`. Reject a changed header, trailing bytes and a rehashed mutation of each represented semantic class: node request, input-ref digest, output bytes, receipt outcome and trace event. Include an adapter-version mutation that remains syntactically valid but resolves to no catalog member.

- [ ] **Step 3: Implement the serial scheduler and V2 codec**

  Start only the lexicographically first ready node, assemble predecessor refs in dependency order, and validate every returned output's version, digests, claim scope and receipt size before publishing `READY`. Translate adapter exceptions into a `FAILED` receipt with no output, then block only descendants. Give control and start budget priority before each start. Encode spec, receipts and trace in length-prefixed canonical fields behind exactly `BRUCE_PHASE_DAG_RUN_V2\n` plus a 64-character outer SHA-256; replay must revalidate, reexecute without external control, compare bytewise and require canonical repacking.

- [ ] **Step 4: Prove scheduler and replay behavior**

  Run: `scripts/with-host-deps.sh cmake --build /tmp/bruce-phase-dag-v2-build --target bruce_phase_dag_v2_tests` and the focused CTest command from Task 1.

  Expected: PASS; all status, dependency, cancellation and rehashed-tamper invariants hold.

### Task 3: Statischer NanoCell-Adapter und seine Evidenz

**Files:**
- Create: `include/bruce/phase_dag/nanocell_adapter_v2.hpp`
- Create: `mods/nanocell/src/phase_dag_adapter_v2.cpp`
- Modify: `mods/nanocell/CMakeLists.txt`
- Modify: `CMakeLists.txt:176-183`
- Create: `tests/mods/nanocell/phase_dag_nanocell_tests.cpp`

**Interfaces:**

```cpp
namespace bruce {
struct NanoCellPhaseRequestV1 {
    std::string core_layout_version;
    std::array<std::uint8_t, 8> core_bytes{};
    std::uint8_t active_index{};
    std::string context_id, context_bytes, context_digest, technique_id;
    std::uint64_t max_operations{}, max_trace_entries{};
};
struct NanoCellPhaseMetadataV1 {
    std::string metadata_version{"nanocell-phase-output-v1"};
    std::string nanocell_status, technique_id, technique_version, core_layout_version;
    std::uint8_t active_index{};
    std::string core_digest, context_id, context_digest, technique_digest;
    std::uint64_t operations_used{}, trace_entries{};
};
std::string pack_nanocell_phase_request_v1(const NanoCellPhaseRequestV1&);
NanoCellPhaseRequestV1 unpack_nanocell_phase_request_v1(std::string_view);
std::string pack_nanocell_phase_metadata_v1(const NanoCellPhaseMetadataV1&);
NanoCellPhaseMetadataV1 unpack_nanocell_phase_metadata_v1(std::string_view);
std::span<const PhaseAdapterV2> phase_dag_nanocell_catalog_v2();
}
```

- [ ] **Step 1: Add failing real-NanoCell adapter tests**

  In `phase_dag_nanocell_tests.cpp`, construct requests with `pack_nanocell_phase_request_v1` and execute V2 with `phase_dag_nanocell_catalog_v2()`. Prove all four terminal NanoCell cases: `MATCHED` and `NO_MATCH` yield `READY` + `COMPLETE`; insufficient operation/trace budget yields `READY` + `INCOMPLETE`; `context-conflict-v1` with the normal reference context yields `READY` + `REJECTED`. Decode output metadata only through its public V2 adapter codec, check status, IDs, digests, operation count and trace count, then call `replay_nanocell_mod_run_v1` on the exact `evidence_bytes`.

- [ ] **Step 2: Add failing adapter-rejection and integration tests**

  Reject malformed length prefixes, leading-zero numeric fields, a non-eight-byte core, invalid index, altered context digest, an unrecognised technique ID, direct technique-program injection, values beyond NanoCell limits and changed request digests. Test an invalid NanoCell adapter request followed by a dependent node and an independent node: outcomes are `FAILED`, `BLOCKED`, `READY`. Pack and replay each successful terminal state through `replay_phase_dag_run_v2`; reject a rehashed mutation of NanoCell request, metadata or the embedded Mod-envelope bytes.

- [ ] **Step 3: Implement the narrow request and static adapter catalog**

  Implement `nanocell-phase-request-v1` as the specified ten ASCII length-prefixed fields with canonical decimal numbers. Decode it into `NanoCellRequestV1`, derive the technique exclusively by mapping the three permitted IDs through `nanocell_reference_technique_v1`, and call `execute_nanocell_mod_v1` without a new control callback. Build metadata under a dedicated `nanocell-phase-output-v1` codec from NanoCell's returned status, technique/core/context IDs and digests, operation count and trace count. Set V2 output status exactly as specified, attach `pack_nanocell_mod_run_v1(run)` unchanged as evidence and use `nanocell-phase-v1` as claim scope. Export a one-entry `std::span` over an internal static adapter record; do not add registration APIs.

- [ ] **Step 4: Add the adapter library and test target with an acyclic CMake graph**

  In `mods/nanocell/CMakeLists.txt`, create `bruce_phase_dag_nanocell` from `phase_dag_adapter_v2.cpp`, publish `include`, link it publicly to `bruce_mod_nanocell` and `bruce_core`, and apply `-Wall -Wextra -Werror`. Under the existing NanoCell test gate in root `CMakeLists.txt`, add `bruce_phase_dag_nanocell_tests`, link it to `bruce_phase_dag_nanocell`, and register `phase_dag_nanocell_contracts`. Do not link `bruce_core` back to NanoCell or alter `bruce_mod_catalog`.

- [ ] **Step 5: Prove the NanoCell/V2 boundary contract**

  Run: `scripts/with-host-deps.sh cmake --build /tmp/bruce-phase-dag-v2-build --target bruce_phase_dag_nanocell_tests` followed by `scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-phase-dag-v2-build -R '^(phase_dag_v2_contracts|phase_dag_nanocell_contracts|nanocell_contracts|nanocell_mod_contracts|phase_dag_contracts)$' --output-on-failure`.

  Expected: PASS; V1, NanoCell and V2 preserve their independent contracts, while the only production V2 adapter is the static NanoCell adapter.

### Task 4: Reproducible acceptance record and full verification

**Files:**
- Create: `docs/implementation/phase-dag-v2-nanocell-measurement.md`
- Modify: `docs/implementation/README.md`

**Interfaces:**

- Consumes: built bundles from Task 3 and the actual CMake/CTest output.
- Produces: a dated, source-backed measurement record; it is evidence of the recorded run and never a performance claim.

- [ ] **Step 1: Write the acceptance record only after successful measurements**

  In `phase-dag-v2-nanocell-measurement.md`, record the exact temporary-copy configure/build/test commands, the actual focused and full CTest totals, V2 bundle byte sizes for the four NanoCell terminal states, the outer digest algorithm and the evidence that V1 remained unchanged. Mark the document **IMPLEMENTIERT UND BELEGT** only when all stated commands passed; otherwise state the exact incomplete verification status. Link it from `docs/implementation/README.md` as a focused serialization measurement, not a performance result.

- [ ] **Step 2: Execute focused and full verification from a fresh temporary source copy**

  Copy the repository to `/tmp/bruce-phase-dag-v2-source`, configure it into `/tmp/bruce-phase-dag-v2-build`, then run:

  ```sh
  scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-phase-dag-v2-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
  scripts/with-host-deps.sh cmake --build /tmp/bruce-phase-dag-v2-build
  scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-phase-dag-v2-build -R '^(phase_dag_contracts|phase_dag_v2_contracts|phase_dag_nanocell_contracts|nanocell_contracts|nanocell_mod_contracts)$' --output-on-failure
  scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-phase-dag-v2-build --output-on-failure --parallel 8
  ```

  Expected: every configured target builds; focused contracts and the complete available test suite pass. Report command, exit code, passed/failed/skipped tests and any environmental limitation exactly.

- [ ] **Step 3: Perform final read-only hygiene checks**

  Verify local Markdown links to the new record and spec. Run `git diff --check` and `git status --short` only when `.git` exists; otherwise document that the supplied workspace lacks Git metadata and list the changed paths from the implementation session without fabricating Git output.

## Plan Self-Review

- **Spec coverage:** Tasks 1–2 implement the independent V2 contract, canonical scheduling and replay; Task 3 realizes exactly the NanoCell adapter, evidence and link boundary; Task 4 records reproducible verification. V1 compatibility, non-goals and all four NanoCell statuses are explicitly covered.
- **Step scan:** Each task begins with executable failing tests, then names the exact public interface and implementation boundary before its focused verification.
- **Type consistency:** `PhaseAdapterV2`, `PhaseDagSpecV2`, `PhaseOutputRefV2`, `PhaseDagRunV2` and the `std::span<const PhaseAdapterV2>` catalog are declared in Task 1 and used unchanged in Tasks 2–3.
- **Review focus:** Each of the five listed risks has a named owning task and concrete test condition.
- **Proportion:** The plan fixes interfaces, encodings and test evidence without prescribing implementation bodies beyond the canonical formats required by the approved specification.
