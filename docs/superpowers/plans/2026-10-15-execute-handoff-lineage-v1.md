# Execute Handoff Lineage V1 Implementation Plan

> **Execution mode:** Subagent-driven development. Each task is implemented
> independently, reviewed, and verified before the next task starts.

## Goal

Add an additive `--handoff-v1` audit sidecar and read-only inspector to the
existing V5 `execute` route. The feature records and verifies actual
producer-to-consumer input bindings while preserving the existing worker
frame, execute report bytes, V2/V3/V4 routes, payload flow and artifact-store
contracts.

## Binding contract

- Execute opt-in: `bruce dag-scheduler-v5 execute SPEC --output BUNDLE --handoff-v1`.
- Inspect opt-in: `bruce dag-scheduler-v5 inspect BUNDLE --handoff-v1`.
- Sidecar: `BUNDLE.handoff-v1.json`, one canonical JSON line plus LF.
- Contract string: `bruce-execute-handoff-v1`.
- Inspect contract string: `dag-scheduler-v5-handoff-inspect-v1`.
- Sidecar keys, in order: `contract`, `run_id`, `spec_digest`,
  `report_digest`, `nodes`, `attempts`.
- Node keys, in order: `node_id`, `callback_id`, `request_digest`,
  `dependencies`.
- Attempt keys, in order: `consumer_node_id`, `consumer_attempt_id`, `inputs`.
- Input keys, in order: `input_index`, `producer_node_id`,
  `producer_attempt_id`, `reference`, `sha256`, `size_bytes`.
- Request input is excluded from `inputs`; only direct dependencies are bound.
- Root attempts have `inputs:[]`; every actually started attempt, including a
  failed retry, has one immutable entry. A preflight attempt never started is
  excluded. No new entry is created after host cancellation is observed.
- Sidecar is owner-only `0600`, private directories are `0700`, all reads and
  writes use bounded descriptor-relative `O_NOFOLLOW|O_CLOEXEC` handling,
  single-hardlink checks, and identity checks before/after reads.
- Sidecar adoption is atomic and exclusive after the legacy report is ready.
- Sidecar/report failures emit empty stdout, stderr
  `{"error":"HANDOFF_RECORD_INVALID"}\n`, exit 2. Existing legacy files
  remain for diagnosis.
- Existing execute behavior without `--handoff-v1` is byte-identical.
- No WorkerRequest/frame changes, no new signals/PIDs, no external store,
  recovery, network, GPU, plugins, free language or general proving.

## File map

Create:

- `include/bruce/artifact/execute_handoff_lineage_v1.hpp` — bounded value
  model, limits, validation and canonical codec declarations.
- `src/artifact/execute_handoff_lineage_v1.cpp` — canonical JSON encoder and
  strict decoder/validator.
- `include/bruce/artifact/execute_handoff_inspector_v1.hpp` — read-only
  inspection result and entry point.
- `src/artifact/execute_handoff_inspector_v1.cpp` — sidecar/report/log/file
  binding checks.
- `tests/unit/execute_handoff_lineage_v1_tests.cpp` — codec, limits,
  ordering and malformed-input contracts.
- `tests/unit/execute_handoff_inspector_v1_tests.cpp` — valid, incomplete,
  mutation and state-binding contracts.
- `tests/integration/test_dag_scheduler_v5_handoff.py` — real-binary execute,
  retries, fan-out/fan-in, cancellation and inspect workflows.

Modify:

- `CMakeLists.txt` — core sources, unit targets and integration test.
- `apps/dag_scheduler_v5_cli.hpp` — parse the two opt-in forms, collect
  immutable execute lineage, adopt the sidecar, and render the new inspector
  report without changing the legacy path.
- `tests/unit/dag_scheduler_v5_cli_tests.cpp` — exact parser, usage, output
  and legacy-byte regression contracts.
- `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/cli/README.md` — public
  option, sidecar and inspector contracts.
- `docs/architecture/03-blueprint-consolidation.md` and
  `docs/implementation/README.md` — evidence and boundaries.
- Generated `docs/cli/source-index.md`, `docs/cli/examples.json` and
  `docs/cli/validation.md` only through `docs/cli/verify_examples.py` in a
  complete temporary repository copy.
- `runs/bruce-execute-handoff-lineage-v1-2026-10-15.md` — reproducible
  evidence and environment limits.

## Task 1: Add the bounded lineage codec

1. Write failing unit tests for exact key order, canonical LF termination,
   root attempts, fan-in input order, retries, identifiers, digest/size
   limits, duplicate/unknown keys, truncation, trailing bytes, deep JSON and
   allocation limits.
2. Register the new unit target and confirm the focused test fails before the
   implementation.
3. Implement the value model and strict codec following the existing
   `payload_flow_v2` bounded JSON style. Enforce maximum nodes, attempts,
   dependencies, inputs and `bruce::max_file_bytes` before allocation.
4. Run `execute_handoff_lineage_v1_contracts` and the existing payload-flow
   codec tests.

Expected result: a standalone, deterministic codec with no CLI or filesystem
side effects.

## Task 2: Capture execute lineage and adopt the sidecar

1. Add failing CLI unit tests for `--handoff-v1` placement, duplicate use,
   rejection on other routes, exact usage, sidecar path and unchanged output
   without the option.
2. Add failing integration workflows for a root, linear chain, fan-out,
   fan-in, retry and cancellation-before-next-start.
3. Extend the execute state with an in-memory lineage snapshot. Freeze one
   attempt entry only after its `ATTEMPT_STARTED` event is durably appended;
   keep the dependency order from the parsed spec and the accepted producer
   attempt/digest/size/reference.
4. Render the legacy report exactly as before. When opted in, hash those exact
   report bytes, encode the sidecar, write it through a temporary sibling,
   `fsync`, close, validate owner/mode/hardlink identity and atomically rename
   with no overwrite.
5. Ensure encode/adoption failure suppresses stdout and returns the fixed
   `HANDOFF_RECORD_INVALID` error while leaving legacy artifacts available.
6. Run the new integration workflow plus existing V5 execute, abort, payload,
   publish and inspector workflows.

Expected result: opt-in lineage evidence is produced without a worker protocol
change; the non-opt-in path remains byte-identical.

## Task 3: Add the read-only handoff inspector

1. Write failing unit tests for READY, FAILED, BLOCKED, CANCELLED and
   INCOMPLETE runs; fan-in/fan-out; retry histories; missing/extra bindings;
   report/log mismatches; digest/size/reference changes; symlink, hardlink,
   permissions, truncation and sidecar mutation.
2. Add the exact parser and renderer tests for
   `inspect BUNDLE --handoff-v1`, including exit 0/1/2 behavior and the
   metadata-only report.
3. Implement descriptor-relative bounded reads of the sidecar, BUNDLE,
   `control/run-log.jsonl`, request files and referenced private artifacts.
   Cross-check `report_digest`, `spec_digest`, `run_id`, nodes, dependencies,
   started attempts, statuses, accepted producers and file identities.
4. Treat a structurally valid run without `RUN_FINISHED` as `INCOMPLETE`;
   treat consistent terminal FAILED/BLOCKED/CANCELLED runs as VALID with the
   observed run status. Never write, repair, replay or start a worker.
5. Run the inspector unit target and the real-binary handoff workflow.

Expected result: the sidecar becomes an independently verifiable local
consistency record, not an authenticity or proof claim.

## Task 4: Documentation and generated CLI validation

1. Document the opt-in execute/inspect forms, exact sidecar schema, status and
   failure behavior, security boundaries and the fact that payload flow V2
   already covers the separate `payload` route.
2. Mark the feature **IMPLEMENTIERT UND BELEGT** only after Tasks 1–3 pass.
3. Mark free language, general proving, GPU/network execution, free plugins,
   external stores, recovery/resume and broad orchestration as
   **DOKUMENTIERT / KONZEPT**.
4. Run `docs/cli/verify_examples.py` only in a complete temporary copy and
   update generated artifacts from that copy, never by hand.
5. Record exact commands, exit codes, pass counts, documentation-link results,
   secret-scan result and the Cockpit socket limitation separately.

## Verification commands

```sh
cmake --build /tmp/bruce-v14-core-build -j2
ctest --test-dir /tmp/bruce-v14-core-build -R '^(execute_handoff_lineage_v1_contracts|execute_handoff_inspector_v1_contracts|dag_scheduler_v5_cli_contracts|dag_scheduler_v5_handoff_workflows|dag_scheduler_v5_abort_workflows|dag_scheduler_v5_payload_workflows|dag_scheduler_v5_payload_flow_inspector_workflows|dag_scheduler_v5_inspector_workflows)$' --output-on-failure
python3 tests/integration/test_cli_docs.py /tmp/bruce-cockpit-build/bruce
python3 scripts/check_local_doc_links.py
```

The full suite may include the known Cockpit socket PermissionError in the
sandbox. That environment failure must be reported separately from feature
tests; no full-suite success claim is allowed unless every test passes.
