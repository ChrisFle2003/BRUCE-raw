# Control and publication — Core 0.102 current contract

**IMPLEMENTIERT UND BELEGT:** this contract consolidates the source-backed
control and publication behavior of Core 0.102. Stage 0 added no command,
controller, or option; Stage 1 adds explicit family confirmations for the six
generic session routes, `gate-run`, the two read-only management routes, and
all seven transfer routes described below. The shared
local-file adapter owns file operations; existing control boundaries remain
distinct. Stage 0 of Common Runtime Compatibility V1 provides a bounded,
canonical, read-only descriptor for those boundaries. See
the [control overview](../cli/control-overview.md) and
  [Blueprint](../architecture/03-blueprint-consolidation.md). Stage 1 now
  exposes explicit family confirmations on the six generic session routes,
  `gate-run`, the management routes, and the seven transfer routes; each
  validates the existing read-only descriptor internally and keeps legacy
  report bytes unchanged. No descriptor JSON is emitted by these CLI options.

## Shared meaning

1. A checkpoint budget counts admitted control points. It is not a universal
   count of mathematical operations, runtime, or process memory. Domain search
   budgets and byte limits retain their own units.
2. Every CLI invocation has its own account. A stored plan retains domain
   limits, not consumed transfer points or a balance transferable to later
   commands.
3. Exit code 0 means processing completed successfully. `INCOMPLETE`,
   ambiguity, and sample confirmation are not general mathematical certainty.
   Native budget exhaustion can return a replayable partial domain result.
4. A triggered control error terminates that invocation with exit code 2. It
   produces no new domain partial-report JSON on stdout. Error envelopes differ
   by entry point as specified below.
5. Exclusive exports do not overwrite targets. A controlled transfer reaches
   its export-preparation boundary before publication; a recognised callback
   interruption before publication attempts to remove the temporary file.
6. Digests bind content; they do not authenticate a human identity. A gate
   report or transfer plan does not activate a rule. The
   [human-rule-approval contract](05-human-rule-approval.md) remains governing.

## Actual control paths

| Path | Account and prerequisite | Controlled area | Deliberately outside the path |
| --- | --- | --- | --- |
| Eight `ApplicationTask` variants through `execute_cli` | `--verification replay` and optional `--max-session-checkpoints`; `session-checkpoints-v1`; the six generic routes also accept optional `--compatibility-family session` | Connected execution, packing, and replay points; the family confirmation is checked before route I/O; signal options depend on path | CLI file reading, final file export, atomic domain work, and a general task DAG |
| `gate-run` | optional `--compatibility-family gate`, `--max-session-checkpoints`, optional `--cancellation checkpoints` | Replay and gate inspection, including source audits; Stage 1 validates the selected `gate` family before file reading | File reading and export; a gate is not target authorization |
| Controlled transfer commands | optional `--compatibility-family transfer`, `--max-transfer-checkpoints`, `--transfer-cancellation none\|checkpoints` | Direct file imports, rule stores, trial work, connected domain checks, and export preparation; Stage 1 validates `transfer` before route-specific I/O | Individual system calls, text decoding, uninstrumented domain sections, and phase splitting |
| Management inspection through `experience inspect` and `rules-status` | Optional `--compatibility-family management`, `--max-management-checkpoints`; `management-checkpoints-v1` | Read-only snapshot or rule-history inspection | Mutating rule operations, transfer/session checkpoint accounts, and publication |
| Experience management without management or transfer control | Existing domain size and audit limits | Native checks for the named operation | A transfer/session checkpoint account |
| Rules, `replay`, `inspect-run`, `assess-run`, and simple address/calculation commands | Their individual local, format, and domain limits | Their own native contract | A new global CLI control contract |

Sources: [`apps/bruce_cli.cpp`](../../apps/bruce_cli.cpp),
[`execution_session.cpp`](../../src/application/execution_session.cpp),
[`session_budget.cpp`](../../src/application/session_budget.cpp),
[`transfer_control.hpp`](../../include/bruce/application/transfer_control.hpp),
[`management_inspection_control.hpp`](../../include/bruce/application/management_inspection_control.hpp),
and [`local_files.cpp`](../../src/io/local_files.cpp).

## Canonical Common Runtime Compatibility V1 descriptor

**IMPLEMENTED AND VERIFIED:** The read-only API in
[`common_runtime_compatibility_v1.hpp`](../../include/bruce/application/common_runtime_compatibility_v1.hpp)
describes one already selected control path. It does not select a path, grant
authority, authenticate a caller, activate a rule, or replace the original
input validation. Its canonical JSON has the fixed field order
`contract`, `family`, `budget_unit`, `checkpoint_order`,
`error_envelope`, `publication_boundary`, and `legacy_route`, with one LF and
no optional fields. The codec is bounded at 64 bytes per identifier and 768
bytes per complete JSON value.

The current source binding is:

| Family | Budget unit | Checkpoint order | Error envelope | Publication boundary | Legacy route |
| --- | --- | --- | --- | --- | --- |
| Session | `session-checkpoints-v1` | `path-dependent` | `application-execution-failure-v1-v3` | `final-export-outside` | `execute-cli` |
| Gate | `session-checkpoints-v1` | `signal-before-consumption` | `simple-error-json` | `no-export` | `gate-run` |
| Transfer | `transfer-checkpoints-v1` | `signal-budget-consume` | `simple-error-json` | `export-preparation` | `transfer-cli` |
| Management | `management-checkpoints-v1` | `signal-budget-consume` | `simple-error-json` | `read-only` | `experience-inspect+rules-status` |

The session envelope value is deliberately a family summary: the active
session path still emits its existing version-specific `-v1`, `-v2`, or `-v3`
envelope. The descriptor does not collapse those legacy variants.

The binding is checked by
[`common_runtime_matrix_binding_tests.cpp`](../../tests/unit/common_runtime_matrix_binding_tests.cpp)
and by the real workflow test
[`test_common_runtime_compatibility_v1.py`](../../tests/integration/test_common_runtime_compatibility_v1.py).
The existing `control_contract_matrix` output remains unchanged.

The Stage-1 CLI adapter is deliberately narrower than the descriptor API.
The six generic session routes (`compare`, `grid`, `grid-batch`, `grid-resume`,
`research` including CSV/Exact/Grid variants, and `compatibility` including
candidate and `--from-run --at` sources) accept only
`--compatibility-family session`. `bruce gate-run RUN.bruce
--compatibility-family gate` accepts only `gate`,
the two read-only management routes accept only `management`, and exactly
these seven transfer routes accept only `transfer`: `compatibility-plan`,
`compatibility-apply`, `experience plan-add`, `experience apply-add`,
`experience add`, `experience revoke`, and `experience replace`. Each route
validates its descriptor before opening its input, plan, store, or output and
emits exactly the same stdout/stderr bytes and exit code as the legacy
invocation without the option. Wrong families, unknown values, duplicate
options, and missing values are controlled parser or compatibility errors. The
descriptor is not included in any CLI response; the transfer confirmation
does not activate or alter `TransferControl`.

**IMPLEMENTIERT UND BELEGT:** Stage 2 adds four bounded in-process host probes
for the explicit `compare --compatibility-family session`,
`grid --compatibility-family session`, `grid-batch --compatibility-family session`,
and `grid-resume --compatibility-family session` forms. The internal
[`CommonRuntimeHostV1Request`](../../include/bruce/application/common_runtime_host_v1.hpp)
accepts exactly these four Stage-2 entrypoints with the canonical Session
descriptor, validates it before route work, and delegates the existing
`execute_cli` path exactly once. It owns no budget, path/cache/query access,
file access, output rendering, bundle field, export, retry, or exception
translation. The legacy and explicit compare/grid/grid-batch/grid-resume forms are
byte-identical for success, complete and partial bundles, cache/query errors,
and execution errors; non-target routes do not acquire this host behavior.
Evidence is recorded in the archival local, non-versioned paths
`runs/common-runtime-host-v1-compare.md`,
`runs/common-runtime-host-v1-grid.md`,
`runs/common-runtime-host-v1-grid-batch.md`, and
`runs/common-runtime-host-v1-grid-resume.md`,
the
compare [design spec](../superpowers/specs/2026-10-15-common-runtime-host-v1-compare-design.md),
compare [implementation plan](../superpowers/plans/2026-10-15-common-runtime-host-v1-compare.md),
grid [design spec](../superpowers/specs/2026-10-15-common-runtime-host-v1-grid-design.md),
grid [implementation plan](../superpowers/plans/2026-10-15-common-runtime-host-v1-grid.md),
grid-batch [design spec](../superpowers/specs/2026-10-15-common-runtime-host-v1-grid-batch-design.md),
grid-batch [implementation plan](../superpowers/plans/2026-10-15-common-runtime-host-v1-grid-batch.md),
[`common_runtime_host_v1_tests.cpp`](../../tests/unit/common_runtime_host_v1_tests.cpp),
[`test_common_runtime_compatibility_v1_stage2_compare.py`](../../tests/integration/test_common_runtime_compatibility_v1_stage2_compare.py),
[`test_common_runtime_compatibility_v1_stage2_grid.py`](../../tests/integration/test_common_runtime_compatibility_v1_stage2_grid.py),
[`test_common_runtime_compatibility_v1_stage2_grid_batch.py`](../../tests/integration/test_common_runtime_compatibility_v1_stage2_grid_batch.py),
and [`test_common_runtime_compatibility_v1_stage2_grid_resume.py`](../../tests/integration/test_common_runtime_compatibility_v1_stage2_grid_resume.py).
The complete and partial grid-batch bundle bytes remain unchanged, including
FIFO cache, query-boundary, replay, and serialization-checkpoint behavior.
For grid-resume, host validation immediately follows option parsing, before
`read_bounded` opens the parent. The single delegate retains parent reading,
additional-query parsing, signal installation, task construction, and execution
in their existing order. Parent completeness/replay checks, checkpoint budgets,
cancellation, complete/partial resume bundle bytes, and exclusive publication
remain route-owned. See the resume
[design spec](../superpowers/specs/2026-10-15-common-runtime-host-v1-grid-resume-design.md)
and [implementation plan](../superpowers/plans/2026-10-15-common-runtime-host-v1-grid-resume.md).
These are four bounded probes, not a unified runtime; Stage 3 remains an
open decision requiring a separate contract.

## Kompatibilitätsmatrix der Steuerungswege

These paths are intentionally not normalized into a universal runtime.
[ADR-015](../adr/015-control-contract-compatibility.md) records that boundary;
the `control_contract_matrix` tests use real temporary CLI inputs to check it,
while the Common Runtime V1 binding tests check the canonical descriptors.

| Family | Version and unit | Signal / budget ordering | Public error envelope | Publication and test evidence |
| --- | --- | --- | --- | --- |
| Session | `session-checkpoints-v1`, plus a separate serialization-byte limit | Path dependent; at replay or serialization points, consumption can precede signal checking, so exhaustion can win | `application-execution-failure-v1`, `-v2`, or `-v3` according to active control | Preparation reads and final export are outside; `control_contract_matrix`, `session_work_budget_workflows`, and `execution_session_states` |
| Gate | `session-checkpoints-v1` through `--max-session-checkpoints` | Signal before consumption | Simple `{"error":"…"}` | File reading is outside; no export; `control_contract_matrix`, `result_assessment_workflows`, and Common Runtime workflows |
| Transfer | `transfer-checkpoints-v1` | Signal, budget check, then increment | Simple `{"error":"…"}` | Export control ends before publication; recognised interruption best-effort removes temporary files; transfer and matrix tests |
| Management | `management-checkpoints-v1` through `--max-management-checkpoints` | Signal before budget consumption in the management controller | Simple `{"error":"…"}` | Read-only inspection; no publication; `experience` and `rule_store` workflow tests plus Common Runtime workflows |

## Cancellation, budgets, and errors remain distinct

| Case | Current behavior |
| --- | --- |
| Transfer checkpoint | Signal predicate, then budget check, then counter increment. Errors are JSON on stderr. |
| Session replay or serialization checkpoint with signal checking | Counter consumption precedes signal checking. Concurrent exhaustion can report the budget first. Other execution paths have their own cancellation semantics. |
| `gate-run` checkpoint | Signal before consumption; `GateInspectionCancelled` or a session-budget error appears in the simple error JSON. |
| Management inspection checkpoint | Signal before budget consumption; a management cancellation or budget error appears in the simple error JSON. The inspected snapshot or rule store is not published. |
| Native Z-rule check | Native check/trace limits apply before its rule callback. Exhaustion is a normal `INCOMPLETE` result; a later permitted callback can still terminate a controlled transfer. |
| Session error | An execution error envelope gives phase, cause, and applicable usage; no domain `result` is emitted. A later CLI export failure is outside the session and uses the simple error JSON. |
| Transfer or other CLI error | `{"error":"…"}`; no structured partial consumption is guaranteed. |

## Evidence and remaining limits

The [Core 0.101 control acceptance](../implementation/182-control-consolidation-usage.md)
connects an independent research finding to separate compatibility and
experience targets. It verifies their source/target binding, replay, selected
budget boundaries, and publication behavior for that profile. It does not
establish guaranteed signal latency, universal time/RAM limits, automatic rule
promotion, or overall Blueprint acceptance.

**OPEN DECISION:** a future common runtime must explicitly migrate error
priority, error envelope, and file boundaries before it can replace these
distinct paths. It must not silently reinterpret existing checkpoint counts.
