# Common Runtime Host V1 — `grid` Session Probe

**Status: IMPLEMENTIERT UND BELEGT.**

This document specifies the next bounded Stage-2 increment after the
implemented `compare` Session probe. It extends the existing in-process host
boundary to exactly one more public Session route: `grid`. The implementation
and focused verification are complete; this document remains limited to the
specified grid probe and does not claim a unified runtime.

## Purpose

The existing Host V1 already validates a canonical Session descriptor and
delegates one `compare` callback exactly once. The next increment should prove
that the same host boundary can carry a second existing ApplicationTask route
without creating a route-specific runtime, a new budget, or a new result
format.

The `grid` route is a suitable second probe because it already uses the
generic `execute_cli` path and produces the existing `ExecutionSession`,
rendering, replay, and optional export behavior. The host must remain an
adapter around that behavior.

Repository evidence for the existing route:

- `apps/bruce_cli.cpp`, the `grid` branch — parses the existing grid options,
  creates `GridTask`, and calls `execute_cli`;
- `apps/bruce_cli.cpp`, `execute_cli(...)` — owns verification, replay,
  Session budgets, cancellation, rendering, and exclusive export;
- `include/bruce/application/grid.hpp` and `src/application/grid.cpp` — own
  grid evaluation, rendering, packing, and replay;
- `include/bruce/application/execution_session.hpp` and
  `src/application/execution_session.cpp` — own the Session result, error,
  budget, and serialization contracts;
- `include/bruce/application/common_runtime_host_v1.hpp` and
  `src/application/common_runtime_host_v1.cpp` — currently validate and
  delegate the single `compare` host entrypoint;
- `tests/integration/test_common_runtime_compatibility_v1_stage2_compare.py`
  — establishes the existing byte-preservation and host-boundary test style.

## Decision: additive Host V1 API

Use the existing `CommonRuntimeHostV1Request` and
`CommonRuntimeHostV1Delegate` API. Extend the accepted entrypoint set from
exactly `compare` to exactly `compare` and `grid`.

Do not create `CommonRuntimeGridHostV1Request`, a second route-specific host,
or a grid-specific delegate API. The current request already carries the
route identity and canonical family descriptor, while the callback keeps the
route's task and execution semantics in the caller. A second host type would
duplicate validation and create two Stage-2 boundaries before there is a
shared runtime contract.

This is an additive API change with these limits:

```cpp
struct CommonRuntimeHostV1Request {
    std::string entrypoint; // `compare` or `grid`
    CommonRuntimeCompatibilityV1 compatibility;
};

using CommonRuntimeHostV1Delegate = std::function<void()>;

void validate_common_runtime_host_v1(
    const CommonRuntimeHostV1Request& request);

void delegate_common_runtime_host_v1(
    const CommonRuntimeHostV1Request& request,
    CommonRuntimeHostV1Delegate delegate);
```

The request remains internal and non-serialized. It contains no `GridTask`,
path string, budget, callback result, bundle, payload, or output field. The
caller captures those values in the callback exactly as the existing `grid`
branch does.

## Scope

The Stage-2 probe supports exactly these host targets:

```text
host families: session
host entrypoints: compare, grid
legacy route: execute-cli
```

The public invocation remains the existing command:

```text
bruce grid --path '2,6/4,3'
bruce grid --path '2,6/4,3' --compatibility-family session
```

Without `--compatibility-family`, the legacy `grid` route remains direct and
unchanged. With the explicit `session` value, the CLI constructs a request
with `entrypoint == "grid"`, validates it through the existing Host V1 API,
and delegates exactly once to the existing `execute_cli` callback.

The canonical Session descriptor remains:

| Field | Required value |
| --- | --- |
| `family` | `session` |
| `budget_unit` | `session-checkpoints-v1` |
| `checkpoint_order` | `path-dependent` |
| `error_envelope` | `application-execution-failure-v1-v3` |
| `publication_boundary` | `final-export-outside` |
| `legacy_route` | `execute-cli` |

## Delegation and ordering

The `grid` CLI branch must preserve this ordering for the explicit host form:

```text
parse options
↓
resolve the supplied compatibility family
↓
validate CommonRuntimeHostV1Request{entrypoint: "grid", session descriptor}
↓
construct GridTask and perform existing route validation
↓
delegate exactly once to execute_cli
↓
existing GridTask/ApplicationTask execution
↓
existing render and optional exclusive export
```

The host validation must occur immediately after the existing `options(...)`
call and before route-specific processing. In particular, a wrong or unknown
family must fail before the grid path is parsed into a task and before
`execute_cli` is entered. The host itself performs no path evaluation, signal
installation, budget visit, rendering, bundle packing, file write, or export.

The callback may construct the existing `GridTask` and invoke
`execute_cli(args, std::move(task))`; this keeps task construction and all
route semantics outside the host. The host must not call the callback more
than once and must not catch, wrap, retry, replay, inspect, or mutate its
result.

## Compatibility invariants

The following behavior is mandatory:

1. A valid explicit Session host invocation produces byte-identical stdout,
   stderr, exit code, and optional output bundle bytes compared with the same
   legacy `grid` invocation without the option.
2. Existing `ExecutionSessionError` values and rendered error bytes pass
   through unchanged.
3. Existing `Error` values and rendered error bytes pass through unchanged.
4. The host owns no budget. Session checkpoints remain
   `session-checkpoints-v1`; no grid-specific or host-specific counter is
   created or combined with another family.
5. The host does not change grid modes (`exact`, `modular`, `metric`), grid
   limits, domain validation, cancellation, verification, serialization,
   `INCOMPLETE` behavior, replay semantics, or export behavior.
6. A wrong or mutated descriptor fails before the callback and before any
   route side effect.
7. The descriptor is descriptive only. It is not an authorization token, an
   approval, a task specification, or a replacement for grid input validation.
8. Existing `compare` host behavior remains unchanged while accepting the new
   `grid` entrypoint.

## Error contract

The host adds no new grid-specific error envelope. It retains these Host V1
errors:

| Condition | Error |
| --- | --- |
| Unknown family value | existing `UnknownCommonRuntimeCompatibilityV1Family` |
| Entrypoint other than `compare` or `grid` | `CommonRuntimeHostV1EntrypointMismatch` |
| Known family with a mutated descriptor | existing `CommonRuntimeCompatibilityV1FamilyMismatch` |
| Empty delegate | `EmptyCommonRuntimeHostV1Delegate` |

The CLI continues to use the existing top-level JSON exception rendering. A
callback exception is never converted into a host error. Route parser errors,
grid validation errors, replay errors, and export errors retain their current
messages and precedence after a valid host request.

## Test contract

### Unit contract

Extend `tests/unit/common_runtime_host_v1_tests.cpp` to prove:

- a canonical `grid` request delegates exactly once;
- `compare` still delegates exactly once;
- an unsupported entrypoint does not invoke the delegate;
- Gate, Transfer, and Management descriptors do not invoke the delegate for
  either accepted entrypoint;
- every mutated Session descriptor field is rejected before delegation;
- an empty delegate is rejected without side effects;
- a delegate exception retains its original type and message;
- the host request still exposes no budget, output, bundle, or descriptor
  rendering state.

### Integration contract

Add `tests/integration/test_common_runtime_compatibility_v1_stage2_grid.py`
and register CTest `common_runtime_compatibility_v1_stage2_grid`. It must
prove:

- legacy and explicit-host `grid` success stdout, stderr, and exit code are
  byte-identical;
- legacy and explicit-host optional `grid` bundle bytes are byte-identical;
- an existing grid execution or input error remains byte-identical;
- explicit Session replay and Session budget options remain valid;
- exact, modular, and metric grid modes remain route-owned and unchanged;
- wrong known families and unknown families fail with the existing errors;
- duplicate and missing family values retain the existing parser errors;
- invalid host validation happens before grid route side effects;
- the existing `compare` Stage-2 probe remains green;
- `grid-batch`, `grid-resume`, `research`, `compatibility`, and `replay` do
  not silently acquire the `grid` Host V1 behavior as a side effect.

The integration test must use temporary output paths and remove them through
the test framework's temporary-directory lifecycle. It must not depend on a
pre-existing bundle for the success case unless that fixture is already a
repository contract.

## Non-goals

This increment does not introduce:

- a new top-level command or a `runtime` command;
- a route-specific host type or a second host implementation;
- a shared scheduler, worker, process, network, GPU, plugin, external-store,
  or Cockpit path;
- a unified budget, status, cancellation, error, replay, or publication
  model;
- payload transport, artifact materialization, or an external payload store;
- a new grid bundle version, Session version, JSON field, report field, or
  descriptor output mode;
- host delegation for `grid-batch`, `grid-resume`, `research`,
  `compatibility`, Gate, Transfer, Management, task-spec, language, or
  dialogue routes;
- changes to V2, V3, V4, V5, Phase-DAG, Artifact Store, or worker contracts;
- free-form language or general proving.

## Stage-3 boundary

The broader Common Runtime remains **DOKUMENTIERT / KONZEPT** and requires a
separate V2 contract. This grid probe must not be used as evidence that the
repository has a unified runtime. Before expanding the host further, a later
contract must decide at minimum:

- whether control counters remain family-local or gain a common unit;
- canonical status and terminal-state semantics;
- cancellation and budget precedence;
- legacy-error preservation and any new error envelope;
- publication and rollback boundaries;
- replay and lineage binding;
- local versus Cockpit exposure;
- host authority and process/worker ownership.

## Acceptance criteria

The grid probe is accepted only when:

1. `common_runtime_host_v1_contracts` passes with both `compare` and `grid`;
2. `common_runtime_compatibility_v1_stage2_grid` passes;
3. explicit and legacy grid outputs and bundles compare byte-for-byte;
4. host mismatch tests prove no callback invocation and no route side effect;
5. the existing compare Stage-2 test and all Stage-0/Stage-1 tests remain
   green;
6. no generated report, Session bundle, or public descriptor field changes;
7. documentation labels this design as a concept until implementation and
   focused verification pass;
8. an independent review confirms the additive API decision and rejects
   Stage-3 scope creep.
