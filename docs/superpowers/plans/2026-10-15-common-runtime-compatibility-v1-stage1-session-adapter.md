# Common Runtime Compatibility V1 — Stufe-1-Session-Adapter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Die sechs bestehenden generischen `execute_cli`-Routen akzeptieren
eine optionale, read-only `--compatibility-family session`-Bestätigung, ohne
Legacy-Ausgaben, Sessionsteuerung oder Bundles zu verändern.

**Architecture:** Der CLI-Einstieg validiert den optionalen Familienwert direkt
nach dem jeweiligen `options(...)`-Aufruf über den bestehenden
`CommonRuntimeCompatibilityV1`-Descriptor. Nur `session` wird akzeptiert; nach
der Prüfung läuft die existierende Route und anschließend `execute_cli`
unverändert weiter. Die Option ist keine neue Ausführungssteuerung und erzeugt
keinen Descriptor-Output.

**Tech Stack:** C++20 CLI in `apps/bruce_cli.cpp`, bestehende
`CommonRuntimeCompatibilityV1`-API, Python-Unittest-Integration, CMake/CTest
und `docs/cli/verify_examples.py`.

**Spec:** `docs/superpowers/specs/2026-10-15-common-runtime-compatibility-v1-stage1-session-adapter-design.md`

## Global Constraints

- Der einzige gültige Familienwert ist exakt `session`.
- Die Familienprüfung liegt unmittelbar nach `options(...)` und vor jedem
  Datei-, Bundle- oder Storezugriff.
- `--verification replay` ist nicht erforderlich und wird durch die Option
  weder aktiviert noch deaktiviert.
- Legacy-stdout, stderr, Exit-Codes, Bundle-/Runbytes, Sessionversionen,
  Checkpointreihenfolge und Exportgrenzen bleiben unverändert.
- Die Option wird nur für `compare`, `grid`, `grid-batch`, `grid-resume`,
  `research` und `compatibility` akzeptiert.
- `replay`, `inspect-run`, `assess-run`, `gate-run`, Transfer, Management,
  `task-spec`, `language` und `dialogue` bleiben außerhalb des Scopes.
- Kein Descriptor wird ausgegeben und keine gemeinsame Runtime eingeführt.
- Generierte CLI-Artefakte werden ausschließlich über den bestehenden
  Verifier in einer vollständigen temporären Kopie geprüft oder aktualisiert.

## Review Focus

- CSV-, Exact- und Grid-Research müssen denselben optionalen Familienvertrag
  verwenden. Test: `test_explicit_session_covers_research_variants` in Task 1.
- `grid-resume` darf das Parent-Bundle erst nach der Familienprüfung lesen.
  Test: `test_family_validation_precedes_file_or_bundle_access` in Task 1.
- Die Option darf ohne Replay und mit Replay/Budget/Cancellation identisch
  funktionieren. Tests: `test_session_confirmation_does_not_require_replay`
  und `test_existing_session_controls_remain_combinable` in Task 1.
- Die generische Route `compatibility` darf nicht mit den separaten
  Transferrouten verwechselt werden. Test:
  `test_non_target_routes_reject_the_option` in Task 1.
- Ein gültiger Familienwert darf keinen Descriptor oder Wrapper erzeugen.
  Test: `test_explicit_session_does_not_emit_descriptor` in Task 1.

---

### Task 1: Session-CLI-Adapter und TDD-Integrationstest

**Files:**
- Modify: `apps/bruce_cli.cpp` in den sechs generischen Dispatchzweigen, den
  Helpzeilen und neben dem bestehenden Transfer-Familienhelper.
- Create: `tests/integration/test_common_runtime_compatibility_v1_stage1_session.py`.
- Modify: `CMakeLists.txt` zur Registrierung von
  `common_runtime_compatibility_v1_stage1_session`.
- Reference: `include/bruce/application/common_runtime_compatibility_v1.hpp`
  und `src/application/common_runtime_compatibility_v1.cpp`.

**Interfaces:**
- Consumes: `Options`, `options`, `get`, `required`,
  `control_family_compatibility_v1(std::string_view)` und
  `require_control_family_compatibility_v1(const CommonRuntimeCompatibilityV1&)`.
- Produces: eine interne Prüfung, die ohne Familienoption nichts tut, bei
  `session` fortfährt und sonst die bestehenden Descriptorfehler verwendet
  beziehungsweise `CommonRuntimeCompatibilityV1FamilyMismatch` wirft.
  Es entsteht keine neue öffentliche C++-API.

- [ ] **Step 1: Write the failing integration tests**

  Erzeuge temporäre CSV-, Exact-, Grid-, Regel-, Kandidaten- und
  Research-Run-Dateien sowie ein unvollständiges Grid-Bundle. Implementiere
  mindestens diese Testmethoden:

  - `test_explicit_session_is_byte_identical_across_six_routes`: führe für
    `compare`, `grid`, `grid-batch`, `grid-resume`, `research` und
    `compatibility` jeweils Legacy- und expliziten Aufruf aus; vergleiche
    Returncode, stdout, stderr und alle erzeugten Run-/Bundlebytes.
  - `test_explicit_session_covers_research_variants`: prüfe CSV-Research,
    Exact-Research mit `--domain bigint|rational` und Grid-Research mit
    `--grid-train`; der Familienwert darf bei allen Varianten gleich behandelt
    werden.
  - `test_compatibility_route_covers_candidate_and_run_inputs`: prüfe die
    `--candidates`- und `--from-run --at`-Form der generischen
    `compatibility`-Route.
  - `test_session_confirmation_does_not_require_replay`: führe mindestens
    `grid` und `research` mit `--compatibility-family session` ohne
    `--verification replay` aus und erwarte Erfolg sowie Legacybytes.
  - `test_existing_session_controls_remain_combinable`: kombiniere die
    Bestätigung mit `--verification replay`,
    `--max-session-checkpoints` und einer bereits routegültigen
    `--verification-cancellation`; prüfe unveränderte Session-Usage und
    Replayfelder.
  - `test_family_validation_precedes_file_or_bundle_access`: verwende für
    `research`, `grid-resume` und `compatibility` fehlende Eingaben mit den
    Familien `gate`, `transfer`, `management` und `unknown`; erwarte Exit 2,
    leeres stdout, den jeweiligen Descriptorfehler und keinen Output.
  - `test_duplicate_missing_and_foreign_options_keep_route_contract`: prüfe
    doppelte und fehlende Familienoption sowie eine Transferoption auf jeder
    relevanten Zielroute; bestehende Parser-/Usagefehler müssen bleiben.
  - `test_non_target_routes_reject_the_option`: prüfe `replay`, `inspect-run`,
    `assess-run`, `gate-run`, `compatibility-plan`, `compatibility-apply`,
    `experience`, `rules-status`, `task-spec`, `language` und `dialogue` mit
    `--compatibility-family session`; keine Route darf die Option stillschweigend
    akzeptieren.
  - `test_explicit_session_does_not_emit_descriptor`: suche in allen
    Erfolgsantworten nicht nach `common-runtime-compatibility`, einer neuen
    Sessionhülle oder einem zusätzlichen JSON-Feld und prüfe unveränderte
    Eingabedateien.

- [ ] **Step 2: Run the focused test before implementation**

  Run:

  ```sh
  ctest --test-dir /tmp/bruce-v14-core-build -R '^common_runtime_compatibility_v1_stage1_session$' --output-on-failure
  ```

  Expected: FAIL, weil `--compatibility-family` in den sechs Optionslisten
  noch unbekannt ist und der neue CTest nicht registriert ist.

- [ ] **Step 3: Add a session-family validation helper**

  Ergänze neben `validate_transfer_compatibility` einen internen Helper wie
  `validate_session_compatibility(const Options& args)` in
  `apps/bruce_cli.cpp`. Der Helper soll bei fehlender Option sofort
  zurückkehren, sonst den Wert über den bestehenden Descriptor laden, mit
  `require_control_family_compatibility_v1` prüfen und anschließend exakt
  `family == "session"` verlangen. Für `gate`, `transfer` und `management`
  muss `CommonRuntimeCompatibilityV1FamilyMismatch` entstehen; unbekannte
  Werte müssen `UnknownCommonRuntimeCompatibilityV1Family` behalten.

- [ ] **Step 4: Add the option to exactly six allow-lists**

  Ergänze `--compatibility-family` nur in den `options(...)`-Allow-Lists der
  Dispatchzweige `compare`, `grid`, `grid-batch`, `grid-resume`, `research`
  und `compatibility`. Rufe den Helper jeweils unmittelbar nach `options(...)`
  auf. Bei `grid-resume` muss das vor `read_bounded(required(args,
  "--from-run"))` liegen; bei `research` vor Experience-, CSV- und
  Grid-Source-Lesen; bei `compatibility` vor Regel-, Kandidaten- und
  Research-Run-Lesen.

- [ ] **Step 5: Preserve `execute_cli` and route semantics**

  Übergebe den validierten Aufruf weiterhin mit denselben Taskobjekten und
  `ExecutionControl`-Callbacks an `execute_cli`. Ändere weder die
  `execute_cli`-Validierung noch die Session-/Replaybudgets, Bundlepacker,
  Sessionversionen, Exportfunktionen oder Fehlerhüllen.

- [ ] **Step 6: Update the six public help forms**

  Ergänze die bestehenden Helpzeilen um den optionalen Suffix
  `[--compatibility-family session]` beziehungsweise die vorhandene
  Variantenbeschreibung. Bewahre die bisherigen Optionsnamen und
  Nichtzielrouten ohne Familienoption.

- [ ] **Step 7: Register and run the focused CTest**

  Registriere das Python-Integrationstestmodul mit `$<TARGET_FILE:bruce>`
  neben den bestehenden Stage-1-Tests. Führe danach aus:

  ```sh
  cmake --build /tmp/bruce-v14-core-build -j2
  ctest --test-dir /tmp/bruce-v14-core-build -R '^common_runtime_compatibility_v1_stage1_session$' --output-on-failure
  ```

  Expected: Build exit 0 und alle Session-Adaptertests grün.

### Task 2: Session-Dokumentation und CLI-Generierung

**Files:**
- Modify: `docs/cli/commands.md`.
- Modify: `docs/cli/bruce-cli-reference.md` und
  `docs/cli/bruce-cli-reference.json`.
- Modify: `docs/cli/results-and-control.md`.
- Modify: `docs/contracts/06-control-and-publication.md`.
- Modify: `docs/implementation/184-control-contract-matrix-usage.md`.
- Generated via verifier only: `docs/cli/source-index.md`,
  `docs/cli/examples.json`, `docs/cli/validation.md`.

**Interfaces:**
- Consumes: die sechs implementierten Helpformen und den Stage-1-
  Integrationstest.
- Produces: konsistente öffentliche Dokumentation mit dem Status
  **IMPLEMENTIERT UND BELEGT** erst nach erfolgreicher Task-1-Verifikation.

- [ ] **Step 1: Document the exact route scope**

  Dokumentiere `--compatibility-family session` für `compare`, `grid`,
  `grid-batch`, `grid-resume`, `research` und `compatibility`. Beschreibe die
  CSV-/Exact-/Grid-Researchvarianten sowie die beiden generischen
  Compatibility-Eingabeformen. Stelle ausdrücklich klar, dass
  `compatibility-plan` und `compatibility-apply` Transferrouten bleiben.

- [ ] **Step 2: Document validation order and replay semantics**

  Ergänze den Sessionvertrag um die Regel „direkt nach `options(...)` und vor
  Datei-/Bundlezugriff“, den einzigen Wert `session`, die bekannten
  Familienfehler, die unveränderte Legacy-Ausgabe und die Tatsache, dass
  `--verification replay` optional bleibt.

- [ ] **Step 3: Synchronize the reference JSON**

  Aktualisiere die nicht generierte CLI-Referenz nach bestehender Konvention,
  parse sie mit `python3 -m json.tool` und prüfe, dass genau die sechs Routen
  die Familienoption nennen. Führe keine Handänderung an den drei
  verifier-eigenen Dateien durch.

- [ ] **Step 4: Regenerate verifier-owned artifacts**

  Erzeuge eine vollständige temporäre Kopie des Repositories und führe
  `docs/cli/verify_examples.py` mit der passenden CLI-Binärdatei aus. Übernimm
  nach erfolgreichem Lauf ausschließlich die exakten Ergebnisse für
  `source-index.md`, `examples.json` und `validation.md`. Prüfe anschließend
  lokale Markdown-Links und die generierten Diagnose-/Optionszahlen.

### Task 3: Vollverifikation und unabhängige Review

**Files:**
- Create: `runs/common-runtime-compatibility-v1-stage1-session-task1.md`.
- Review: alle Task-1- und Task-2-Dateien.

**Interfaces:**
- Consumes: fokussierten Session-CTest, bestehende Session-/Matrix-/CLI-
  Tests, CLI-Verifier, Linkcheck und Secret-Scan.
- Produces: reproduzierbaren Bericht mit Statuslabels, Exitcodes,
  Testzahlen, Generatorprovenienz, Dokumentationszahlen und
  umgebungsbedingten Einschränkungen.

- [ ] **Step 1: Run focused implementation and regression tests**

  ```sh
  cmake --build /tmp/bruce-v14-core-build -j2
  ctest --test-dir /tmp/bruce-v14-core-build -R \
    '^(common_runtime_compatibility_v1_contracts|common_runtime_matrix_binding|common_runtime_compatibility_v1_workflows|common_runtime_compatibility_v1_stage1_gate|common_runtime_compatibility_v1_stage1_management|common_runtime_compatibility_v1_stage1_transfer|common_runtime_compatibility_v1_stage1_session|execution_session|control_contract_matrix|control_contract_matrix_unit)$' \
    --output-on-failure
  ```

  Erwartet werden Build exit 0 und grüne ausgewählte Tests. Environment-
  sensitive Cockpit-/Socketfehler sind getrennt von Produktfehlern zu melden.

- [ ] **Step 2: Run documentation and safety checks**

  Führe den CLI-Verifier in der vollständigen temporären Kopie aus, parse die
  Referenz-JSON, starte `python3 scripts/check_local_doc_links.py` und prüfe
  geänderte Dateien auf hochkonfidente Geheimnisse. Erwartung: gültiges JSON,
  null defekte lokale Links, bytegleiche verifier-generierte Dateien und kein
  offengelegtes Secret.

- [ ] **Step 3: Inspect change boundaries**

  Führe `git diff --check` und Git-Status aus. Bestätige, dass nur der
  Session-Adapter, sein Test, CMake, die betroffene Dokumentation und der
  Bericht geändert wurden. Verändere keine unabhängigen Vorarbeiten.

- [ ] **Step 4: Independent review gate**

  Lass eine frische Review die sechs Routen, die Forschungsversionen, die
  Validierung-vor-I/O-Reihenfolge, fehlende Replaypflicht, Bytekompatibilität,
  Nichtzielrouten und generierte Artefakte prüfen. Konkrete Findings beheben
  und die relevanten Tests wiederholen, bevor der Adapter als
  **IMPLEMENTIERT UND BELEGT** bezeichnet wird.

## Verification summary

Die Umsetzung ist erst vollständig, wenn Task 1 den fokussierten CTest besteht,
die vorhandenen Session-/Matrix-/Transfer-/Managementtests grün bleiben, die
CLI-Dokumentation durch den Verifier bestätigt ist, lokale Links und JSON
gültig sind und die unabhängige Review keine offenen Scope- oder
Kompatibilitätsbefunde hinterlässt. Ein grüner Adaptertest belegt keine
gemeinsame Runtime und keine Zukunftsfunktion außerhalb dieser Spec.
