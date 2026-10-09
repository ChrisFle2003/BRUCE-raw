# Control and publication — Core 0.102 current contract

**IMPLEMENTED AND VERIFIED:** this contract consolidates the source-backed
control and publication behavior of Core 0.102. It adds no command, controller,
or option. The shared local-file adapter owns file operations; existing control
boundaries remain distinct. See the [control overview](../cli/control-overview.md)
and [Blueprint](../architecture/03-blueprint-consolidation.md).

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
| Eight `ApplicationTask` variants through `execute_cli` | `--verification replay` and optional `--max-session-checkpoints`; `session-checkpoints-v1` | Connected execution, packing, and replay points; signal options depend on path | CLI file reading, final file export, atomic domain work, and a general task DAG |
| `gate-run` | `--max-session-checkpoints`, optional `--cancellation checkpoints` | Replay and gate inspection, including source audits | File reading and export; a gate is not target authorization |
| Controlled transfer commands | `--max-transfer-checkpoints`, `--transfer-cancellation none\|checkpoints` | Direct file imports, rule stores, trial work, connected domain checks, and export preparation | Individual system calls, text decoding, uninstrumented domain sections, and phase splitting |
| Experience management without transfer control | Existing domain size and audit limits | Native checks for the named operation | A transfer/session checkpoint account |
| Rules, `replay`, `inspect-run`, `assess-run`, and simple address/calculation commands | Their individual local, format, and domain limits | Their own native contract | A new global CLI control contract |

Sources: [`apps/bruce_cli.cpp`](../../apps/bruce_cli.cpp),
[`execution_session.cpp`](../../src/application/execution_session.cpp),
[`session_budget.cpp`](../../src/application/session_budget.cpp),
[`transfer_control.hpp`](../../include/bruce/application/transfer_control.hpp),
and [`local_files.cpp`](../../src/io/local_files.cpp).

## Compatibility matrix

These paths are intentionally not normalized into a universal runtime.
[ADR-015](../adr/015-control-contract-compatibility.md) records that boundary;
the `control_contract_matrix` tests use real temporary CLI inputs to check it.

| Family | Version and unit | Signal / budget ordering | Public error envelope | Publication and test evidence |
| --- | --- | --- | --- | --- |
| Session | `session-checkpoints-v1`, plus a separate serialization-byte limit | Path dependent; at replay or serialization points, consumption can precede signal checking, so exhaustion can win | `application-execution-failure-v1`, `-v2`, or `-v3` according to active control | Preparation reads and final export are outside; `control_contract_matrix`, `session_work_budget_workflows`, and `execution_session_states` |
| Gate | Local gate-inspection account using `--max-session-checkpoints` | Signal before consumption | Simple `{"error":"…"}` | File reading is outside; no export; `control_contract_matrix` and `result_assessment_workflows` |
| Transfer | `transfer-checkpoints-v1` | Signal, budget check, then increment | Simple `{"error":"…"}` | Export control ends before publication; recognised interruption best-effort removes temporary files; transfer and matrix tests |

## Cancellation, budgets, and errors remain distinct

| Case | Current behavior |
| --- | --- |
| Transfer checkpoint | Signal predicate, then budget check, then counter increment. Errors are JSON on stderr. |
| Session replay or serialization checkpoint with signal checking | Counter consumption precedes signal checking. Concurrent exhaustion can report the budget first. Other execution paths have their own cancellation semantics. |
| `gate-run` checkpoint | Signal before consumption; `GateInspectionCancelled` or a session-budget error appears in the simple error JSON. |
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
