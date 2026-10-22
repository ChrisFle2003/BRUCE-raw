# Common Runtime Host V1 — `grid-batch` Session Probe

**Status: IMPLEMENTIERT UND BELEGT.**

This document specifies the next bounded Stage-2 increment after the
implemented `compare` and `grid` Session probes. It extends the existing
in-process host boundary to exactly one additional public Session route:
`grid-batch`. The implementation and focused verification are complete. The
contract remains limited to this bounded probe and does not claim a unified
runtime.

## Purpose

Stage 1 already confirms the selected Session family on the public
`grid-batch` route. Stage 2 should prove that the existing Common Runtime Host
V1 can delegate the batch route without creating a second grid-batch engine,
cache, query budget, Session budget, error envelope, or publication path.

The host remains a narrow adapter around the existing route. It validates the
canonical Session descriptor and delegates exactly once. `GridBatchTask`, path
splitting, cache construction, query-boundary handling, Session execution,
rendering, replay, and optional export remain owned by the existing CLI and
application paths.

Repository evidence for the current route:

- `apps/bruce_cli.cpp`, the `grid-batch` branch — parses the route options,
  builds `GridBatchTask`, splits and bounds `--paths`, installs signal control,
  and calls `execute_cli`;
- `apps/bruce_cli.cpp`, `execute_cli(...)` — owns common verification,
  replay, Session budgets, cancellation, rendering, and exclusive export;
- `include/bruce/application/grid_batch.hpp` and
  `src/application/grid_batch.cpp` — own the batch task, FIFO cache, query
  budget, partial-run state, bundle format, and grid-batch replay;
- `include/bruce/application/common_runtime_compatibility_v1.hpp` and
  `src/application/common_runtime_compatibility_v1.cpp` — define and validate
  the canonical Session descriptor;
- `include/bruce/application/common_runtime_host_v1.hpp` and
  `src/application/common_runtime_host_v1.cpp` — currently validate and
  delegate the `compare` and `grid` Stage-2 probes;
- `tests/integration/test_common_runtime_compatibility_v1_stage1_session.py`
  — establishes the existing public Session-family behavior for `grid-batch`;
- `tests/integration/test_common_runtime_compatibility_v1_stage2_grid.py`
  — establishes the existing Host V1 byte-preservation and boundary style.

## Decision: additive Host V1 API

Reuse the existing `CommonRuntimeHostV1Request` and
`CommonRuntimeHostV1Delegate` types. Extend the accepted host entrypoint set
from exactly `compare` and `grid` to exactly `compare`, `grid`, and
`grid-batch`.

Do not create `CommonRuntimeGridBatchHostV1Request`, a cache-specific host, or a
second delegate API. The request already carries the route identity and the
canonical family descriptor. The callback keeps all route-specific task,
cache, query, execution, and export behavior in the existing caller.

The API remains:

```cpp
struct CommonRuntimeHostV1Request {
    std::string entrypoint; // `compare`, `grid`, or `grid-batch`
    CommonRuntimeCompatibilityV1 compatibility;
};

using CommonRuntimeHostV1Delegate = std::function<void()>;

void validate_common_runtime_host_v1(
    const CommonRuntimeHostV1Request& request);

void delegate_common_runtime_host_v1(
    const CommonRuntimeHostV1Request& request,
    CommonRuntimeHostV1Delegate delegate);
```

The request remains internal and non-serialized. It contains no paths, cache
size, query budget, `GridBatchTask`, result, bundle, payload, or output field.

## Scope

The Stage-2 probe supports exactly:

```text
host family: session
host entrypoints: compare, grid, grid-batch
legacy route: execute-cli
```

The public invocation remains the existing command:

```text
bruce grid-batch --paths '2,6;4,3;2,6'
bruce grid-batch --paths '2,6;4,3;2,6' --compatibility-family session
```

Without `--compatibility-family`, the legacy `grid-batch` route remains direct
and unchanged. With the explicit `session` value, the CLI constructs
`CommonRuntimeHostV1Request{"grid-batch", descriptor}`, validates it through
the existing Host V1 API, and delegates exactly once to the existing
`execute_cli` callback.

The canonical Session descriptor is unchanged:

| Field | Required value |
| --- | --- |
| `family` | `session` |
| `budget_unit` | `session-checkpoints-v1` |
| `checkpoint_order` | `path-dependent` |
| `error_envelope` | `application-execution-failure-v1-v3` |
| `publication_boundary` | `final-export-outside` |
| `legacy_route` | `execute-cli` |

The descriptor confirms an existing control family. It grants no authority,
does not select a cache policy, and does not activate replay.

## Validation ordering and delegation boundary

The explicit `grid-batch` branch must preserve this ordering:

```text
parse CLI options
↓
resolve the supplied compatibility family
↓
validate CommonRuntimeHostV1Request{entrypoint: "grid-batch", Session descriptor}
↓
construct GridBatchTask and parse route-owned values
↓
validate mode, modulus, path byte/query limits, cache and query settings
↓
delegate exactly once to execute_cli
↓
existing GridBatch/ApplicationTask execution
↓
existing render and optional exclusive export
```

Host validation occurs immediately after the existing `options(...)` call. It
must run before all route-specific processing, including:

- conversion of `--cache-bytes` and `--max-query-steps`;
- mode and modulus validation;
- reading the required `--paths` value into the task;
- semicolon splitting and empty-path handling;
- aggregate input-byte and query-count checks;
- `GridCache` construction or cache-limit checks;
- signal registration;
- entry into `execute_cli`.

The CLI parser still owns option-shape errors. Therefore unknown options,
duplicate options, and a missing value are reported by `options(...)` before a
host request can be built. Once options have been parsed, a wrong or unknown
family must fail before `--paths`, cache, or query processing.

The host performs no path splitting, canonicalization, cache lookup or
insertion, query-budget consumption, signal installation, Session checkpoint,
rendering, bundle packing, file read, file write, export, replay, retry, or
exception translation. It must not call the callback more than once.

The route callback may contain the existing `GridBatchTask` construction and
`execute_cli(args, std::move(task), {[]{ return interrupted != 0; }})` call. The
legacy no-option branch should continue to execute the same route statements
directly, so the host remains opt-in and the legacy path remains auditable.

## Route-owned grid-batch semantics

After a valid host request, the existing route remains responsible for all of
the following behavior:

- `--paths` is required and contains 1..1024 semicolon-separated paths;
- total raw path input is bounded by `max_grid_batch_input_bytes` (262144);
- each path is evaluated using the selected GridSpec mode and limits;
- `--cache-bytes` remains 0..1048576 with default 65536;
- cache accounting remains canonical key plus V1 result bundle bytes, with the
  existing FIFO policy and maximum of 64 entries;
- `--max-query-steps` remains optional and bounded to 0..1024;
- zero query steps still produce the existing `INCOMPLETE` result and
  `QueryBudgetExceeded` stop reason;
- cache hits still count as processed queries;
- cancellation and signal behavior remain owned by the route and
  `execute_cli`;
- complete and partial grid-batch bundle formats remain unchanged;
- `grid-resume` continues to own continuation of partial bundles;
- replay, serialization cancellation, Session checkpoint budgets, rendering,
  and exclusive export remain unchanged.

The host must not convert any of these route-owned values into a host budget or
combine them with Gate, Transfer, or Management budgets.

## Compatibility invariants

The implementation must satisfy all of these invariants:

1. Valid explicit Session `grid-batch` success produces byte-identical stdout,
   stderr, and exit code compared with the same legacy invocation.
2. Valid explicit invocations produce byte-identical complete bundle bytes and
   byte-identical partial bundle bytes when written to separate temporary
   output paths.
3. Existing grid-batch task, cache, query-budget, replay, cancellation,
   serialization, and export errors retain their exact rendered bytes and
   exit codes.
4. The existing Session budget remains `session-checkpoints-v1`; the host
   creates no additional budget counter or checkpoint.
5. A wrong or mutated descriptor fails before the delegate and before any
   grid-batch route side effect.
6. Existing `compare` and `grid` host behavior remains unchanged.
7. The descriptor is descriptive only. It is not an authorization token, task
   specification, cache admission decision, approval, or result wrapper.
8. The host does not alter bundle versions, JSON fields, Session envelope
   variants, publication boundaries, or legacy route output.

## Error contract

The host adds no grid-batch-specific result envelope. Existing errors remain:

| Condition | Error |
| --- | --- |
| Unknown family value | `UnknownCommonRuntimeCompatibilityV1Family` |
| Entrypoint other than `compare`, `grid`, or `grid-batch` | `CommonRuntimeHostV1EntrypointMismatch` |
| Known family with a mutated descriptor | `CommonRuntimeCompatibilityV1FamilyMismatch` |
| Empty delegate | `EmptyCommonRuntimeHostV1Delegate` |

The CLI keeps its existing top-level JSON exception rendering. A callback
exception is never caught or translated by the host. Route parser errors,
`GridQueryLimit`, `GridBatchInputByteLimit`, `GridCacheByteLimit`,
`InvalidGridQueryBudget`, `InvalidGridCancellation`, GridSpec errors,
ExecutionSession errors, replay errors, and export errors retain their current
precedence after a valid host request.

## Test contract

### Unit contract

Extend `tests/unit/common_runtime_host_v1_tests.cpp` to prove:

- a canonical `grid-batch` request delegates exactly once;
- existing `compare` and `grid` requests still delegate exactly once;
- an unsupported entrypoint does not invoke the delegate;
- Gate, Transfer, and Management descriptors are rejected for every accepted
  entrypoint without invoking the delegate;
- every mutated Session descriptor field is rejected before delegation;
- an empty delegate is rejected without side effects;
- a delegate exception retains its original type and message;
- the host request still exposes no cache, query-budget, Session-budget,
  output, bundle, or descriptor-rendering state.

### Integration contract

Add `tests/integration/test_common_runtime_compatibility_v1_stage2_grid_batch.py`
and register CTest `common_runtime_compatibility_v1_stage2_grid_batch`. It
must prove:

- legacy and explicit-host success stdout, stderr, and exit code are
  byte-identical;
- legacy and explicit-host complete output bundle bytes are byte-identical;
- legacy and explicit-host partial output bundle bytes are byte-identical;
- an existing grid-batch route error, such as `--max-query-steps 1025` or a
  malformed path, remains byte-identical;
- cache options, query-boundary options, all GridSpec modes, replay, Session
  checkpoint budgets, serialization cancellation, and output remain valid;
- wrong known families and unknown families produce existing compatibility
  errors;
- duplicate and missing family values retain existing CLI parser errors;
- a family mismatch with a missing or malformed `--paths` value fails at the
  host boundary before route processing;
- `compare`, `grid`, `grid-resume`, `research`, `compatibility`, and `replay`
  retain their existing host/non-target boundaries;
- the existing Stage-2 compare and grid probes remain green.

The integration test must use temporary output paths and the test framework's
temporary-directory lifecycle. It must not rely on a pre-existing bundle for a
success case.

## Documentation and evidence contract

After implementation and verification, update the existing control/publication
documentation to link this spec, plan, source, unit test, integration test, and
evidence report. Generated CLI artifacts should only be regenerated if the
verifier detects a required change, and then only in a complete temporary copy
through `docs/cli/verify_examples.py`.

The evidence report must label the result **IMPLEMENTIERT UND BELEGT** only
when the host unit test, focused integration test, and selected regressions
pass. Until then, this design remains **DOKUMENTIERT / KONZEPT**.

## Non-goals

This increment does not introduce:

- a new top-level command or `runtime` command;
- a route-specific grid-batch host type or a second host implementation;
- changes to `GridBatchTask`, the FIFO cache, query budgets, or bundle formats;
- a unified budget, scheduler, worker, process, network, GPU, plugin,
  external-store, or Cockpit runtime;
- payload transport, artifact materialization, or external payload storage;
- host delegation for `grid-resume`, `research`, `compatibility`, Gate,
  Transfer, Management, task-spec, language, dialogue, or replay;
- changes to V2, V3, V4, V5, Phase-DAG, Artifact Store, or worker contracts;
- free-form language or general proving.

Those broader capabilities remain **DOKUMENTIERT / KONZEPT** or **OFFENE
ENTSCHEIDUNG** unless separately implemented and verified.
