# Common Runtime Host V1 — `compare` Session Probe

**Status: IMPLEMENTIERT UND BELEGT.**

This document specifies the smallest Stage-2 vertical slice after Common
Runtime Compatibility V1 Stage 0 and its four Stage-1 family adapters. It is a
implemented local, in-process host adapter that delegates one existing
`compare` invocation to the unchanged Session execution path. It does not
claim that a common runtime already exists.

## Purpose

Stage 1 confirms that a caller selected an existing control family. Stage 2
adds a deliberately narrow host boundary: the host validates the canonical
Session descriptor and then delegates exactly once to the existing `compare`
execution path.

The host is an adapter around existing behavior. It does not create a second
execution engine, a second budget, a second error envelope, or a second
publication mechanism.

Repository evidence for the delegated path:

- `apps/bruce_cli.cpp`, `execute_cli(...)` — common ApplicationTask execution,
  verification, session budgets, rendering, and export;
- `apps/bruce_cli.cpp`, the `compare` branch — construction of
  `FractionComparisonTask` and delegation to `execute_cli`;
- `include/bruce/application/common_runtime_compatibility_v1.hpp` and
  `src/application/common_runtime_compatibility_v1.cpp` — canonical family
  descriptor and binding validation;
- `include/bruce/application/execution_session.hpp` — existing Session result,
  error, budget, and serialization contracts.

## Scope

The Stage-2 probe supports exactly one host target:

```text
host family: session
host entrypoint: compare
legacy route: execute-cli
```

The existing public invocation remains the entrypoint:

```text
bruce compare ...
bruce compare ... --compatibility-family session
```

There is no new top-level command and no new descriptor output. The explicit
`--compatibility-family session` form activates the host delegation for the
`compare` route. Without the option, the legacy route remains direct and
unchanged.

The host request is an internal, non-serialized value:

```cpp
struct CommonRuntimeHostV1Request {
    std::string entrypoint; // exactly `compare`
    CommonRuntimeCompatibilityV1 compatibility;
};
```

The host API is:

```cpp
using CommonRuntimeHostV1Delegate = std::function<void()>;

void validate_common_runtime_host_v1(
    const CommonRuntimeHostV1Request& request);

void delegate_common_runtime_host_v1(
    const CommonRuntimeHostV1Request& request,
    CommonRuntimeHostV1Delegate delegate);
```

`delegate_common_runtime_host_v1` validates before invoking the callback,
invokes a valid callback exactly once, and propagates the callback's existing
exceptions without translation. An empty callback is rejected as a host
contract error before invocation.

The implementation must require all of the following:

```text
request.entrypoint == "compare"
request.compatibility == control_family_compatibility_v1("session")
request.compatibility.legacy_route == "execute-cli"
```

The descriptor's canonical Session values are:

| Field | Required value |
| --- | --- |
| `family` | `session` |
| `budget_unit` | `session-checkpoints-v1` |
| `checkpoint_order` | `path-dependent` |
| `error_envelope` | `application-execution-failure-v1-v3` |
| `publication_boundary` | `final-export-outside` |
| `legacy_route` | `execute-cli` |

## Delegation and ordering

The `compare` CLI branch must keep the following order:

```text
parse options
↓
if --compatibility-family is present: build and validate the host request
↓
construct FractionComparisonTask
↓
delegate exactly once when the host option is present
↓
existing execute_cli / ApplicationTask execution
↓
existing render and optional exclusive export
```

The host performs no file reads, bundle reads, store access, budget visits,
signal installation, output rendering, or export. The existing `execute_cli`
path remains the owner of those operations.

The host must not call `execute_cli` more than once. It must not retry,
replay, wrap, inspect, or mutate the callback result.

## Compatibility invariants

The following behavior is mandatory:

1. A valid explicit Session host invocation produces byte-identical stdout,
   stderr, exit code, and optional output bundle bytes compared with the same
   legacy `compare` invocation without the option.
2. Existing `ExecutionSessionError` values and their rendered error bytes pass
   through unchanged.
3. Existing `Error` values and their rendered error bytes pass through
   unchanged.
4. The host owns no budget. Session checkpoints remain
   `session-checkpoints-v1`; no transfer or management counter is created or
   combined with them.
5. The host does not change `INCOMPLETE`, domain status, verification,
   serialization, cancellation, or export behavior.
6. A wrong or mutated descriptor fails before delegation and before any
   callback side effect.
7. The descriptor is descriptive only. It is not an authorization token, a
   human approval, a rule activation, or a replacement for input validation.

## Error contract

The host adds only these host-level failures:

| Condition | Error |
| --- | --- |
| Unknown family value | existing `UnknownCommonRuntimeCompatibilityV1Family` error |
| Entrypoint other than `compare` | `CommonRuntimeHostV1EntrypointMismatch` |
| Known family with a mutated descriptor | existing `CommonRuntimeCompatibilityV1FamilyMismatch` error |
| Empty delegate | `EmptyCommonRuntimeHostV1Delegate` |

The CLI adapter continues to use the repository's existing top-level JSON
exception rendering. A callback exception is never converted into one of the
host errors.

## Test contract

The unit contract test must prove:

- valid canonical Session request delegates exactly once;
- a wrong entrypoint does not invoke the delegate;
- a Gate, Transfer, or Management descriptor does not invoke the delegate;
- a mutated Session descriptor does not invoke the delegate;
- an empty delegate is rejected without side effects;
- an exception thrown by the delegate preserves its type and message;
- the host does not expose a budget, output, bundle, or descriptor-rendering
  field.

The integration test must prove:

- legacy and explicit-host `compare` success outputs are byte-identical;
- legacy and explicit-host optional bundle bytes are byte-identical;
- legacy and explicit-host error outputs and exit codes are byte-identical;
- Session budgets and replay options still behave identically;
- wrong, unknown, duplicate, and missing family options retain their existing
  parser/compatibility errors;
- host validation occurs before delegation and before any route side effect;
- non-target routes do not acquire the Stage-2 host behavior.

## Non-goals

This probe does not introduce:

- a shared scheduler or common runtime loop;
- a unified budget, status model, cancellation model, or error envelope;
- a new descriptor output mode;
- a new top-level `runtime` command;
- Gate, Transfer, or Management host delegation;
- Worker, process, network, GPU, plugin, external-store, or Cockpit
  orchestration;
- dynamic ApplicationTask registration;
- payload transport or artifact materialization;
- a new bundle version or report field;
- changes to V2, V3, V4, V5, Phase-DAG, Artifact Store, or worker contracts;
- free-form language or general proving.

## Stage-3 boundary

Stage 3 remains **OFFENE ENTSCHEIDUNG / DOKUMENTIERTES KONZEPT**. It requires a
separate Common Runtime V2 contract before any unification of checkpoints,
statuses, cancellation ordering, error envelopes, publication boundaries, or
worker ownership is attempted.

The V2 contract must at minimum decide:

- whether control counters remain family-local or gain a new common unit;
- the canonical status and terminal-state model;
- cancellation and budget precedence;
- legacy-error preservation and any new error envelope;
- publication and rollback boundaries;
- replay and lineage binding;
- local versus Cockpit exposure;
- host authority and process/worker ownership.

## Acceptance criteria

The probe is accepted only when:

1. the unit and integration tests pass;
2. explicit and legacy `compare` outputs and bundles compare byte-for-byte;
3. host mismatch tests prove no callback invocation;
4. all four Stage-1 adapter tests, Stage-0 matrix tests, and existing
   execution-session tests remain green;
5. the host adds no generated report field or public descriptor output;
6. documentation labels the probe as **IMPLEMENTIERT UND BELEGT** only after
   the implementation and focused verification pass;
7. an independent review confirms the Stage-2 scope and no Stage-3 creep.
