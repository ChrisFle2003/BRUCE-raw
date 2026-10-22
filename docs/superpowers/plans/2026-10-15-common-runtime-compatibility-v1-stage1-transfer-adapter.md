# Common Runtime Compatibility V1 — Stufe-1-Transfer-Adapter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Die sieben bestehenden Transfer-Routen akzeptieren eine optionale,
read-only `--compatibility-family transfer`-Bestätigung, ohne Legacy-Ausgaben,
Transfercontroller oder Veröffentlichungsgrenzen zu verändern.

**Architecture:** Der CLI-Einstieg prüft nach dem bestehenden Optionsparsing
den vorhandenen `CommonRuntimeCompatibilityV1`-Descriptor und akzeptiert nur
die Familie `transfer`. Die Prüfung wird vor jedem Datei-/Plan-/Storezugriff
ausgeführt; danach läuft die bestehende Route unverändert weiter. Es gibt
keinen neuen Controller und keinen Descriptor-Output.

**Tech Stack:** C++20 CLI, bestehende `bruce`-Optionsparser in
`apps/bruce_cli.cpp`, bestehende Common-Runtime-Descriptor-API, Python-
Integrationstests, CMake/CTest und der vorhandene CLI-Dokumentationsverifier.

**Spec:** `docs/superpowers/specs/2026-10-15-common-runtime-compatibility-v1-stage1-transfer-adapter-design.md`

## Global Constraints

- Der einzige gültige Familienwert ist exakt `transfer`.
- Die Familienprüfung muss vor jedem Datei-, Plan- oder Storezugriff liegen.
- Legacy-stdout, stderr, Exit-Codes, Transferplan-/Bundle-/Snapshotbytes,
  Controllersemantik und exklusive Publikationsgrenzen bleiben unverändert.
- Die Option wird nur auf `compatibility-plan`, `compatibility-apply`,
  `experience plan-add`, `experience apply-add`, `experience add`,
  `experience revoke` und `experience replace` akzeptiert.
- Kein Descriptor wird ausgegeben; es wird keine gemeinsame Runtime eingeführt.
- Generierte CLI-Artefakte werden nur über `docs/cli/verify_examples.py` in
  einer vollständigen temporären Kopie geprüft oder aktualisiert.
- Keine Abhängigkeiten, Planformate, Bundleversionen oder Experienceformate
  ändern.

## Review Focus

- Falsche Familienwerte dürfen bei fehlenden Quellen/Stores/Plänen keinen
  Datei- oder Storefehler maskieren. Test: `test_family_validation_precedes_io`
  für alle sieben Routen.
- Die sieben Routen haben unterschiedliche Positionsparameter und
  Ausgabepfade. Test: `test_explicit_transfer_is_byte_identical_on_all_routes`.
- Bereits aktivierte Transfersteuerung darf durch die Familienoption weder
  deaktiviert noch doppelt aktiviert werden. Test:
  `test_transfer_options_remain_combinable`.
- Parserfehler müssen je bestehender Route erhalten bleiben. Test:
  `test_duplicate_missing_and_foreign_options_keep_route_contract`.
- Ein gültiger Familienwert darf keinen Descriptor oder neuen Reportwrapper
  in die Legacy-Antwort einfügen. Test:
  `test_explicit_transfer_does_not_emit_descriptor`.

### Task 1: CLI-Adapter und Stage-1-Integrationstests

**Files:**
- Modify: `apps/bruce_cli.cpp` in den sieben Transfer-Dispatchzweigen und
  den öffentlichen Hilfezeilen.
- Create: `tests/integration/test_common_runtime_compatibility_v1_stage1_transfer.py`.
- Modify: `CMakeLists.txt` zur Registrierung von
  `common_runtime_compatibility_v1_stage1_transfer`.
- Reference: `include/bruce/application/common_runtime_compatibility_v1.hpp`
  und `apps/management_inspection_cli.hpp` als bestehende Descriptor-
  Validierungsmuster.

**Interfaces:**
- Consumes: `Options`, `get`, `options`,
  `control_family_compatibility_v1(std::string_view)` und
  `require_control_family_compatibility_v1(const CommonRuntimeCompatibilityV1&)`.
- Produces: eine interne CLI-Prüfung, die den gelieferten Wert validiert,
  nur `transfer` akzeptiert und bei falscher Familie
  `CommonRuntimeCompatibilityV1FamilyMismatch` wirft. Es entsteht keine neue
  öffentliche C++-API und kein neuer stdout-Vertrag.

- [ ] **Step 1: Write the failing integration tests**

  Implement the test fixture with temporary source, rule, plan, experience
  snapshot and missing paths. Add these exact test methods and assertions:

  - `test_explicit_transfer_is_byte_identical_on_all_routes`: execute each
    seven-route legacy invocation and its invocation with
    `--compatibility-family transfer`; assert equal return code, stdout,
    stderr and every newly published plan/bundle/snapshot byte sequence.
  - `test_help_lists_transfer_family_on_all_routes`: assert the help output
    contains the seven route forms with `--compatibility-family transfer`.
  - `test_family_validation_precedes_io`: for each route invoke a missing
    source/store/plan with `session`, `gate`, `management` and `unknown`;
    assert exit 2, empty stdout, the exact descriptor error and no output
    file.
  - `test_transfer_options_remain_combinable`: use a valid transfer family
    together with the existing checkpoint/cancellation options and assert the
    existing `controlled-transfer-v1` shape and `transfer-checkpoints-v1`
    usage model.
  - `test_duplicate_missing_and_foreign_options_keep_route_contract`: assert
    duplicate and missing family options retain parser/usage errors and that
    route-inapplicable budget options remain rejected.
  - `test_explicit_transfer_does_not_emit_descriptor`: assert successful
    stdout contains no `common-runtime-compatibility` field or descriptor
    wrapper and that input files remain byte-identical.

- [ ] **Step 2: Run the focused test before implementation**

  Run: `ctest --test-dir /tmp/bruce-v14-core-build -R '^common_runtime_compatibility_v1_stage1_transfer$' --output-on-failure`

  Expected: FAIL because the seven routes do not yet recognize the family
  option and the CTest target is not registered.

- [ ] **Step 3: Add a transfer-family validation helper in `apps/bruce_cli.cpp`**

  Reuse the existing descriptor API. The helper consumes the parsed
  `--compatibility-family` value, defaults to no validation when the option is
  absent, invokes `control_family_compatibility_v1` followed by
  `require_control_family_compatibility_v1`, and throws
  `CommonRuntimeCompatibilityV1FamilyMismatch` unless the resulting family is
  exactly `transfer`. Call it immediately after each route's existing option
  parser and before `transfer_checkpoint`, `LocalFiles::read`,
  `LocalFiles::active_rules`, `read_bounded`, plan construction, or any
  publication path.

- [ ] **Step 4: Add the option only to the seven route allow-lists**

  Add `--compatibility-family` to the two compatibility transfer allow-lists
  and the five experience transfer allow-lists. Preserve every existing
  positional index, required option, route-specific option and direct/native
  branch. Do not add it to `experience inspect`, `rules-status`, `gate-run`,
  `replay`, or unrelated commands.

- [ ] **Step 5: Update the seven help forms**

  Add the optional family suffix to the existing help lines while preserving
  the current wording and all other options. Do not advertise descriptor JSON
  or a new report contract.

- [ ] **Step 6: Register the integration CTest**

  Register the new Python test with `$<TARGET_FILE:bruce>` beside the existing
  Stage-1 Gate and Management tests. Keep native transfer workflow tests
  unchanged.

- [ ] **Step 7: Run the focused test after implementation**

  Run: `cmake --build /tmp/bruce-v14-core-build -j2 && ctest --test-dir /tmp/bruce-v14-core-build -R '^common_runtime_compatibility_v1_stage1_transfer$' --output-on-failure`

  Expected: the new adapter test passes and all seven routes preserve their
  legacy bytes, reports and errors.

### Task 2: Transfer-Dokumentation und CLI-Generierung

**Files:**
- Modify: `docs/cli/commands.md`.
- Modify: `docs/cli/compatibility-transfer.md`.
- Modify: `docs/cli/experience-transfer.md`.
- Modify: `docs/cli/transfer-control.md`.
- Modify: `docs/cli/bruce-cli-reference.md` and its JSON reference.
- Modify: `docs/contracts/06-control-and-publication.md`.
- Modify: `docs/implementation/184-control-contract-matrix-usage.md`.
- Generated via verifier only: `docs/cli/source-index.md`,
  `docs/cli/examples.json`, `docs/cli/validation.md`.

**Interfaces:**
- Consumes: the implemented seven-route CLI help and the Stage-1 transfer
  integration test.
- Produces: documentation that labels the adapter
  **IMPLEMENTIERT UND BELEGT** only after Task 1 and describes the exact
  `transfer` value, validation-before-I/O rule, unchanged output/bytes and
  seven-route scope.

- [ ] **Step 1: Document the option and route matrix**

  Add the option to all seven syntax examples, state that only `transfer` is
  valid, and explain that the option does not enable a controller or emit a
  descriptor. Include the route-specific read/publish boundaries from the
  spec and retain links to the existing transfer-control contract.

- [ ] **Step 2: Update the compatibility matrix and status references**

  Extend the Stage-1 text in the control contract and implementation usage
  document to mention the Gate, Management and Transfer confirmations. Keep
  the descriptor's family/unit/error/publication values unchanged.

- [ ] **Step 3: Regenerate the verifier-owned CLI artifacts**

  Create a complete temporary repository copy, build or point the verifier at
  the matching CLI binary using the established repository procedure, and run
  `docs/cli/verify_examples.py` there. Copy back only its exact generated
  outputs for `source-index.md`, `examples.json` and `validation.md` when the
  temporary run succeeds. Never edit those three files by hand.

- [ ] **Step 4: Synchronize the CLI reference and validate documents**

  Update `bruce-cli-reference.md` and its JSON counterpart using the existing
  repository convention. Parse the JSON, run `python3
  scripts/check_local_doc_links.py`, and verify that help examples and
  diagnostics mention the new option consistently.

### Task 3: Whole-scope verification and independent review

**Files:**
- Create: `runs/common-runtime-compatibility-v1-stage1-transfer-task1.md`.
- Review: all Task 1 and Task 2 files.

**Interfaces:**
- Consumes: the focused CTest, native transfer workflow tests, CLI verifier,
  local-link check and security scan.
- Produces: a reproducible report with command, exit code, test counts,
  generated-artifact result, documentation/link counts and explicit status
  labels.

- [ ] **Step 1: Run focused compatibility and native transfer tests**

  Run:

  ```sh
  cmake --build /tmp/bruce-v14-core-build -j2
  ctest --test-dir /tmp/bruce-v14-core-build -R \
    '^(common_runtime_compatibility_v1_contracts|common_runtime_matrix_binding|common_runtime_compatibility_v1_workflows|common_runtime_compatibility_v1_stage1_gate|common_runtime_compatibility_v1_stage1_management|common_runtime_compatibility_v1_stage1_transfer|compatibility_transfer_workflows|experience_transfer_workflows|control_contract_matrix|control_contract_matrix_unit)$' \
    --output-on-failure
  ```

  Expected: build exit 0 and every selected CTest passes. Report any
  environment-sensitive failure separately from product failures.

- [ ] **Step 2: Run repository documentation and safety checks**

  Run `python3 scripts/check_local_doc_links.py`, parse the reference JSON,
  run the CLI verifier in its complete temporary copy, and scan changed files
  for high-confidence secrets. Expected: zero broken local links, valid JSON,
  generated artifacts byte-matching verifier output and no exposed secret.

- [ ] **Step 3: Inspect final diff boundaries**

  Run `git diff --check` and inspect Git status. Confirm that only the
  intended CLI/tests/CMake/docs/report files changed; do not alter unrelated
  pre-existing worktree changes. Confirm no product code changed the transfer
  plan, bundle, snapshot, controller or publication contracts.

- [ ] **Step 4: Independent review gate**

  Have a fresh reviewer check the seven-route matrix, validation-before-I/O
  ordering, legacy byte equality, option scope, generated-artifact provenance
  and non-goals. Resolve concrete findings and rerun the relevant checks
  before declaring the adapter **IMPLEMENTIERT UND BELEGT**.

## Verification summary

The implementation is complete only when Task 1's focused test passes, the
native transfer workflow tests remain green, the CLI verifier and local-link
checks pass, the JSON reference parses, and the independent review finds no
unresolved scope or compatibility finding. A green adapter test does not
prove a common runtime, external execution, general proving or any of the
future capabilities excluded by the spec.
