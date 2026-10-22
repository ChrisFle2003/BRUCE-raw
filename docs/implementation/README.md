# Implementation records and acceptance evidence

This directory preserves roadmaps, contracts, usage records, acceptance
profiles, and numbered delivery packages. The current source tree, CMake
configuration, tests, CI, and reproducible evidence decide the current state;
the [Blueprint](../architecture/03-blueprint-consolidation.md) and
[control and publication contract](../contracts/06-control-and-publication.md)
are the current documentation entry points.

- [Test strategy](01-test-strategy.md)
- [Implementation phases](02-implementation-phases.md) — planning context.
- [Open decisions](03-open-decisions.md) — unresolved work, not implementation.
- [Consolidated roadmap](04-roadmap.md) — historical/planning context.
- [Acceptance evidence](acceptance/README.md)
- [Architecture and milestone history](75-architecture-history-core082.md)
- [Architecture and milestones](75-current-architecture-and-milestones.md)
- [Phase-DAG V1 sizes](phase-dag-v1-measurement.md) — focused serialization
  measurement; not a performance claim.
- [Phase-DAG V2 NanoCell sizes](phase-dag-v2-nanocell-measurement.md) — focused
  serialization measurement; not a performance result.

## MCP Inspector

**IMPLEMENTIERT UND BELEGT:** the optional static Mod enabled by
`BRUCE_BUILD_MOD_MCP=ON` requires `BRUCE_BUILD_MOD_NANOCELL=ON`.
`bruce mcp [--port 1..65535]` defaults to 8080 and binds only `127.0.0.1`,
with controlled failure for an unavailable requested port. Its only routes are
`GET /`, `GET /api/v1/health`, `POST /api/v1/inspect`, `POST /api/v3/inspect` and
`POST /api/dag-scheduler-v4/inspect`.

The read-only Phase-DAG V2 and Phase-DAG V3 inspector uses fixed NanoCell
catalogs: NanoCell/Evidence-Gate for V2 and NanoCell/Payload-Gate for V3.
`POST /api/v1/inspect` retains its 1 MiB (1048576 bytes) limit and response
model; `POST /api/v3/inspect` accepts V3 bundles up to 5 MiB (5242880 bytes).
These two routes replay existing bundles; no new DAG executions, no jobs or
imports. Wrong-version bundles are rejected. The browser requires explicit
selection of Phase-DAG V2, Phase-DAG V3 or DAG-Scheduler V4, with no automatic
detection from file bytes. Its JSON and DOM expose metadata only;
`request_bytes`, `context_bytes`, `output_bytes`, `evidence_bytes` and
`payload_bytes` are never exposed. There are no adapter plugins.
Replay/digest establishes bundle consistency, not author identity or a general
mathematical proof. The 5-MiB limit does not override tighter core/catalog limits.

A general scheduler and broader Phase-DAG orchestration remain
**DOKUMENTIERT / KONZEPT**.

The [C++ contracts](../../tests/mods/mcp/mcp_inspector_tests.cpp) and
[HTTP/CLI/DOM/documentation workflows](../../tests/mods/mcp/test_mcp_inspector.py)
cover the implemented [service](../../mods/mcp/src/mcp_inspector.cpp),
[transport](../../mods/mcp/src/mcp_inspector_server.cpp) and
[embedded UI](../../mods/mcp/src/mcp_inspector_assets.cpp).
[V2 final Release acceptance](../superpowers/specs/2026-10-12-phase-dag-v2-cockpit-inspector-design.md)
remains dated evidence for V2.

Files named `*-plan.md` are **DOCUMENTED / CONCEPT** until a specific source
and test acceptance record supports the described behavior. Files named
`*-usage.md` or `*-contract.md` record a stated scope and date; do not treat
their numbering as proof that every later branch or local build still matches.

**IMPLEMENTIERT UND BELEGT:** DAG-Scheduler V4 is a separate read-only
structural inspector at exact `POST /api/dag-scheduler-v4/inspect`, with
explicit selection and a 5 MiB (5242880 bytes) limit on local `127.0.0.1`.
It checks the V4 header, complete bundle digest, decoding and canonical
repacking; no callbacks and no re-execution. `VALID` confirms structural
bundle consistency only, not successful replay, callback purity, author
identity or a mathematical proof. A structurally valid alteration with a
recomputed digest may pass; the inspector does not establish that the recorded
receipts or trace were executed. Phase-DAG V2/V3 keep their existing
replay-based routes and independent 1-MiB-V2/5-MiB-V3 limits.

V4 JSON and text-safe DOM show metadata only: `scheduler_version` is
`dag-scheduler-v2`; the overview contains DAG ID, start/receipt budgets and
node count. Nodes expose IDs, dependencies, `callback_id`, `request_size`
and `request_digest`; receipts add outcome/failure code and direct input
references (`producer_node_id`, `result_code`, `payload_size`, `payload_digest`),
plus output version, result code, size and digest or null. Trace entries
contain event and node ID. V4 has no `scheduler_explanation`; the V3 logical
explanation stays hidden and cleared when V4 is selected. No request,
context, output, evidence, input or payload bytes, hex/base64 encodings,
source paths or raw bundle text are returned. Bundles are local confidential
artifacts containing raw requests and payloads; metadata filtering provides
no encryption and no authentication or access control. Bytes remain only in
request memory with no persistence. Reset/version change clears selection
and views and invalidates pending replies; release does not promise physical
memory erasure.

There are no new DAG executions, no jobs or imports, no adapter plugins,
no replay buttons or routes, no persistent uploads, no payload download,
no dynamic plugins and no external access. Free language, a general prover,
GPU/network execution, external payload stores, Redis/Language management,
process starts, filesystem browsing and general runtime orchestration remain
**DOKUMENTIERT / KONZEPT**. The unchanged CLI example verifier covers MCP
argument parsing and V2/V3 examples; V4 structural HTTP/DOM/privacy evidence
comes from the dedicated MCP contracts, not generated examples.

## DAG-Scheduler V5 Run Inspector

**IMPLEMENTIERT UND BELEGT:** The local V5 worker boundary has a separate
read-only observer at `bruce dag-scheduler-v5 inspect BUNDLE`. It binds only to
`BUNDLE.run/`, validates the Bruce-owned JSONL run log and each referenced
artifact, and renders `dag-scheduler-v5-inspect-v1` metadata. It does not start
workers, invoke adapters, resume or repair runs, download payloads, or write
files. Complete runs return `VALID`/Exit 0; valid incomplete logs without
`RUN_FINISHED` return `INCOMPLETE`/Exit 1; corrupt or unsafe data returns the
compact `PROTOCOL_ERROR`/Exit 2.

The evidence is the
[Inspector implementation](../../src/worker/worker_run_inspector_v1.cpp),
[V5 CLI](../../apps/dag_scheduler_v5_cli.hpp),
[unit contract](../../tests/unit/worker_run_inspector_v1_tests.cpp),
[binary workflow](../../tests/integration/test_dag_scheduler_v5_inspector.py).
An Artifact Store, external payload references, dynamic plugins, GPU/network
execution and broad runtime orchestration remain **DOKUMENTIERT / KONZEPT**.

## V5 Execute Handoff Lineage V1

**IMPLEMENTIERT UND BELEGT:** `execute SPEC --output BUNDLE --handoff-v1`
adds `BUNDLE.handoff-v1.json` after the unchanged V5 Execute report. The
canonical sidecar records the Spec and report digests, ordered nodes and
dependencies, and one immutable direct-input binding set for each durably
started attempt. Root attempts have no inputs; failed retries are retained;
reserved attempts that never started are absent. `inspect BUNDLE
--handoff-v1` reads that sidecar, report, log, requests, accepted producer
artifacts and consumer inputs, then returns only metadata under
`dag-scheduler-v5-handoff-inspect-v1`. `VALID`/Exit 0 covers consistent
terminal READY, FAILED, BLOCKED and CANCELLED runs; a consistent unfinished
run is `INCOMPLETE`/Exit 1; unsafe or conflicting files are
`PROTOCOL_ERROR`/Exit 2. Sidecar adoption failure is
`HANDOFF_RECORD_INVALID`/Exit 2 with empty stdout and retained legacy files.

The sidecar is owner-only `0600` and private directories are `0700`.
Descriptor-relative, bounded `O_NOFOLLOW|O_CLOEXEC` operations, single-link
checks and identity rechecks protect the local read/write boundary. The
inspector does not write, repair, replay, resume, start a worker or open a
Store. Neither WorkerRequest/frame nor the non-opt-in report changes. The
separate `payload` route already has [Flow V2](#v5-artifact-payload-flow-v2).
`lineage_scope=LOCAL_CONSISTENCY` is a local consistency finding, not
historical authenticity or general proof.

Evidence: [codec](../../src/artifact/execute_handoff_lineage_v1.cpp),
[inspector](../../src/artifact/execute_handoff_inspector_v1.cpp),
[C++ contracts](../../tests/unit/execute_handoff_inspector_v1_tests.cpp),
[real workflows](../../tests/integration/test_dag_scheduler_v5_handoff.py),
and reproducible run evidence (archival local path, not versioned:
`runs/bruce-execute-handoff-lineage-v1-2026-10-15.md`).
Free language, general proving, GPU/network execution, free plugins,
external stores, recovery/resume and broad orchestration remain
**DOKUMENTIERT / KONZEPT**.

## V5 Worker Abort Control V1

**IMPLEMENTIERT UND BELEGT:** The local host-control route
`bruce dag-scheduler-v5 cancel BUNDLE` writes the fixed owner-only
`BUNDLE.run/control/cancel.request` marker and returns the metadata-only
statuses `REQUESTED`, `ALREADY_REQUESTED` or `ALREADY_FINISHED`. The V5 host
observes that marker before dispatch and during live attempts, cancels every
active local worker through the existing supervisor API, suppresses new
starts/retries, and finalizes active and unstarted nodes as `CANCELLED`.
Exactly one `CANCEL_REQUESTED` event is recorded per active attempt; cancelled
attempts do not publish artifacts. The existing 1000-ms supervisor grace,
escalation and process-group reaping remain unchanged.

The complete run is still inspectable as `VALID` with
`run_status=CANCELLED`; publishing a cancelled run remains rejected. The
public route does not expose PIDs, signals, private paths, payload bytes or
OS diagnostics. This V1 does not implement free language, general proving,
GPU/network execution, free plugins, external payload stores or broad
cross-runtime orchestration (**DOKUMENTIERT / KONZEPT**).

Evidence: [abort marker](../../include/bruce/worker/worker_abort_control_v1.hpp),
[abort implementation](../../src/worker/worker_abort_control_v1.cpp),
[CLI contract](../../tests/unit/dag_scheduler_v5_cli_tests.cpp),
[marker contracts](../../tests/unit/worker_abort_control_v1_tests.cpp),
[real workflows](../../tests/integration/test_dag_scheduler_v5_abort.py),
and Task 3 evidence (archival local path, not versioned:
`runs/worker-abort-control-v1-task3.md`).

## V5 Artifact Publish V1

**IMPLEMENTIERT UND BELEGT:** The explicit local bridge
`bruce dag-scheduler-v5 publish BUNDLE --store STORE` imports only a complete
V5 run that the read-only Inspector has validated. Before opening the store it
requires `RUN_FINISHED`, `VALID`/`READY`, and one verified
`ARTIFACT_ACCEPTED` result for every node in the bound V5 input set. It keeps
the canonical input/node order and resolves result files only below the
validated `BUNDLE.run/` root.

The metadata-only report uses contract `dag-scheduler-v5-publish-v1` and the
fixed outer order `contract`, `action`, `status`, `run_id`, `manifests`. Each
manifest entry has the fixed order `node_id`, `attempt_id`, `reference`,
`manifest_id`, `sha256`, `size_bytes`. Every accepted node becomes one
canonical `BRUCE_ARTIFACT_MANIFEST_V1` with `status=READY`, the adapter result
code, one digest/size-bound object and `origin=artifact/<node>`. The manifest
ID is deterministic (`v5-` plus SHA-256 over the run/node/attempt binding), so
repeating the same publish is idempotent; an existing conflicting or corrupt
object/manifest is rejected without overwrite.

The route returns the publish report with Exit 0 on success. Incomplete,
failed, blocked, cancelled, manipulated or unsafe runs, missing files,
symlinks, hardlinks, root escapes, store conflicts and malformed syntax return
empty stdout, the compact `PROTOCOL_ERROR` envelope and Exit 2. Execute
reports and run logs remain unchanged; the route starts no worker, calls no
adapter, performs no replay and does not download payloads. Store writes are
not transactional: an error after an earlier immutable object write may leave
an unreferenced object, and V1.1 has no rollback or GC.

Evidence: [publish implementation](../../apps/dag_scheduler_v5_cli.hpp),
[publish unit contract](../../tests/unit/dag_scheduler_v5_cli_tests.cpp),
[publish workflow](../../tests/integration/test_dag_scheduler_v5_publish.py),
[manifest codec](../../include/bruce/artifact/artifact_manifest_v1.hpp), and
[publish design](../../docs/superpowers/specs/2026-10-15-worker-artifact-publish-v1-design.md).

## V5 Artifact Payload Flow V1.3

**IMPLEMENTIERT UND BELEGT:** The explicit route
`bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE` adds the
host-controlled edge from a directly accepted V5 predecessor to the next
worker. It reuses the V3 Spec parser, the existing V5 WorkerRequest,
supervisor, JSONL run log and Artifact Store; `execute`, `inspect` and
`publish` keep their existing contracts.

The internal `ArtifactInputRefV13` has exactly `manifest_id`, `object_index`,
`sha256` and `size_bytes`. Bruce constructs it only from a direct DAG edge and
the predecessor's deterministic `READY` manifest. It checks manifest
canonicality, READY status, run/node/attempt provenance, object index, exact
digest and exact size, then performs the existing complete Store verification.
`origin` is metadata only and never becomes a read path.

Bruce is the sole Store reader and materializes each selected object with
bounded descriptor-relative streaming into a fresh owner-only private attempt
file. It verifies the destination's final size and digest, rolls back all
private inputs when a later direct edge fails, and gives the Worker only
relative private references plus expected digest/size and existing limits. A
worker cannot open the Store, receive Store paths, use the network, start a
process, load a plugin or publish a manifest.

V1.3 keeps the existing V5 defaults and maxima: wall `30000/600000 ms`, input
`65536/1048576 bytes`, output `65536/5242880 bytes`, operations
`1000000/10000000`, memory `268435456/2147483648 bytes`, retry `0/3` and
workers `1/4`. `max_wall_ms` covers reference resolution, materialization,
the Worker attempt, verification and publication. The unchanged WorkerRequest
contract permits 16 total inputs, so at most 15 direct payload objects are
accepted after reserving one slot for the request.

An accepted output is published immediately as an immutable object and
`READY` manifest before direct successors are unlocked. Reference, manifest,
integrity, Store, permission, materialization and input-limit failures produce
`FAILED` and transitively `BLOCKED` descendants; they do not trigger implicit
retry. Only the explicit `--retry N` option enables retries for the existing
retryable Worker failures, and each retry receives a fresh `attempt_id`.
The public `dag-scheduler-v5-payload-v13` report contains metadata, status,
digests, sizes and usage only; it exposes no payload bytes, private paths,
Manifest IDs, PIDs or OS diagnostics.

Evidence: [payload CLI](../../apps/dag_scheduler_v5_cli.hpp),
[payload API](../../include/bruce/artifact/artifact_payload_v1.hpp),
[payload implementation](../../src/artifact/artifact_payload_v1.cpp),
[Store copy primitive](../../include/bruce/artifact/artifact_store_v1.hpp),
[unit contracts](../../tests/unit/artifact_payload_v1_tests.cpp),
[CLI contracts](../../tests/unit/dag_scheduler_v5_payload_cli_tests.cpp),
[workflow](../../tests/integration/test_dag_scheduler_v5_payload.py).

Free or unchecked Manifest references outside the controlled V1.4 sidecar,
remote or network stores, GPU execution, free code plugins, free language,
general proving and broad cross-runtime orchestration remain
**DOKUMENTIERT / KONZEPT**.

## V5 Artifact Payload Flow V1.4: external local inputs

**IMPLEMENTIERT UND BELEGT:** V1.4 adds one explicit option to the existing
payload route:

```text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE \
  --external-inputs INPUTS
```

The option is additive. A V1.3 invocation without it keeps the existing
`dag-scheduler-v5-payload-v13` report and behavior. With it, Bruce reads an
owner-only canonical JSON sidecar with contract
`bruce-dag-scheduler-v5-external-inputs-v14`. Each binding names a DAG node and
ordered input references. Each reference contains exactly
`manifest_id`, `object_index`, `sha256`, `size_bytes`, `source_run_id`,
`source_node_id` and `source_attempt_id`.

The sidecar parser accepts only the exact field order and byte representation:
no whitespace, duplicate/unknown fields, alternate escapes, path-like IDs or
omitted fields. Bruce reads the sidecar with `O_NOFOLLOW` and owner-only,
regular-file, single-link checks. The sidecar digest is computed over the
exact bytes and appears as `external_inputs_digest` in the V1.4 report. Each
receipt additionally reports `external_input_count` and
`external_input_bytes`; no sidecar payload or private path is public.

External references are resolved through the same explicitly selected local
Artifact Store as in-run references. Bruce requires a canonical `READY`
manifest and checks source run/node/attempt provenance, object index, digest
and size before bounded materialization. Direct predecessor objects and
external objects share the unchanged 16-reference WorkerRequest boundary;
the request occupies one slot, so the combined maximum is 15 payload objects
per node. A later failure removes all private files created for that attempt.
The Worker receives only private relative references and expected metadata.

Sidecar syntax, file-safety, unknown-node, duplicate-object and arity errors
are rejected before run execution with exit 2. Missing/not-ready manifests,
object integrity, Store, input-limit and deadline failures after run start
produce a failed receipt and block dependent nodes, with exit 1. Retry remains
CLI-only through the explicit `--retry N` option. `execute`, `inspect`,
`publish`, Resolve and V2/V3/V4 contracts are unchanged.

Evidence: [external-input CLI](../../apps/dag_scheduler_v5_cli.hpp),
[external-input types](../../include/bruce/artifact/artifact_payload_external_v1.hpp),
[external-input parser](../../src/artifact/artifact_payload_external_v1.cpp),
[payload workflow](../../tests/integration/test_dag_scheduler_v5_payload.py),
and [V1.4 design](../../docs/superpowers/specs/2026-10-15-artifact-payload-v1-external-inputs-design.md).

## V5 Artifact Payload Stream V1.5

**IMPLEMENTIERT UND BELEGT:** only the explicit
`bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE --stream-v15`
path selects the V1.5 private stream preparation. For production, `--stream-v15` is payload-only,
valueless and accepted once, also with V1.4 external inputs. Execute, inspect and
publish reject it. The separate payload-inspect route uses the option only to
request verification of retained streams. Without the option, behavior stays unchanged; V1.3/V1.4
reports gain no stream fields and retain their existing contracts.

Bruce owns an isolated `payload-inputs/<attempt-id>/input-<index>/` directory
per attempt/input. Its `stream-payload/` contains owner-only `header.jsonl`,
ordered chunk files and `seal.json`. Directories have mode `0700`, files `0600`.
Chunks have a fixed 65,536-byte size except the last partial chunk; empty
payloads have no chunks. Canonical metadata, exact membership, SHA-256 digest,
exact size, chunk count, file type, ownership, permissions and single-link
identity are verified using descriptor-relative `O_NOFOLLOW` access. Byte
limits cap source reads before consumption, and monotonic deadlines cover
construction, verification and compatibility copying. Staging is verified and
fsynced before exclusive atomic adoption. Deadline checks cannot interrupt a
kernel filesystem operation already in progress.

The parent-approved integration boundary preserves the existing regular-file
supervisor contract: verified chunks are copied with a bounded 8 KiB buffer to
an owner-only regular compatibility file `input.bin`, then reopened and checked
for exact size and digest. **WorkerRequest binds that regular compatibility
file, not the stream directory.** The worker-v1 frame and supervisor remain
unchanged. This adds a bounded copy and does not provide a zero-copy worker
interface; existing child adapter/input limits remain in force. A temporary
source from the existing Store materializer is removed after successful stream
preparation. Retry attempts use separate directories.

All preparation and final prelaunch deadline failures remove the entire current
attempt input subtree, including already adopted inputs and partial copies,
clear pending references, launch no worker (`ATTEMPT_STARTED` is absent), and
publish nothing to the Store. Successful private stream/compatibility files
remain confidential local run artifacts; temporary sources and staging residue
are removed. Public stdout, stderr, bundle and run-log remain metadata-only:
no payload bytes/hex, private roots, stream paths/names or header/seal contents.
Verification authenticates owner-only storage at check time; it is not
kernel-enforced immutability against later writes by the same user.

Evidence: [stream core](../../src/artifact/artifact_payload_stream_v1.cpp),
[stream contracts](../../tests/unit/artifact_payload_stream_v1_tests.cpp),
[CLI contracts](../../tests/unit/dag_scheduler_v5_payload_cli_tests.cpp), and
[real payload workflows](../../tests/integration/test_dag_scheduler_v5_payload.py).
The workflows cover direct/external streams, mixed ordering, legacy report-byte
comparison (masking only nondeterministic wall time/RSS), chunk/seal/file
corruption, final deadline cleanup, Store stability and public-output leakage.

External/network stores, GPU execution, free plugins, free language, general
proving and large runtime orchestration remain **DOKUMENTIERT / KONZEPT**.

## Payload Run Inspector V1

**IMPLEMENTIERT UND BELEGT:** the separate read-only route
`bruce dag-scheduler-v5 payload-inspect BUNDLE [--stream-v15]` validates local
V1.3/V1.4 payload reports, their run logs, requests and referenced outputs.
The existing Execute inspector and V1.3/V1.4 writers/reports remain unchanged.
The new `dag-scheduler-v5-payload-inspect-v1` metadata report separates inspection
status from workflow outcome: `VALID`/exit 0 also permits `run_status=FAILED`;
`INCOMPLETE`/exit 1 means consistent complete lines without `RUN_FINISHED`.
Corruption, truncation, unsafe files, observed mutation or deadline expiry
produce only `{"error":"PROTOCOL_ERROR"}` on stderr and exit 2, no stdout.

`--stream-v15` requests verification; it does not prove the historical mode.
Without it, `stream_check=NOT_REQUESTED`; with it, `VERIFIED_PRESENT` counts only
retained verified streams, including zero when the whole tree is absent.
`input_completeness=NOT_PROVABLE` and `external_provenance=NOT_CHECKED` are always
reported. Neither a removed input tree nor later inserted consistent streams
can establish historical completeness, ordering or external provenance without
a persisted authenticated input index. The inspector never promises this.
It requires canonical header/seal/chunk content, matching regular `input.bin`,
known started attempts and contiguous input indices; names alone are no proof.
Legacy inputs, staging residue and unknown entries are errors, never repaired.

A dedicated payload state machine handles reserved preflight attempts and
READY results rejected during host publication, including SHA-256 verification
of unaccepted outputs. The CLI only parses, delegates and renders metadata.
No worker/adapter starts, Store access, replay, repair, publication, payload
materialization or file writes occur. Public output omits payload bytes/hex,
digests, private paths, stream/manifest details and OS diagnostics.
The fixed 30000 ms monotonic deadline guards opens/reads/hashes, with buffers
at most 65536 bytes, 64 JSON levels, Retry 0..3, Workers 1..4 and at most 15
inputs per attempt. Each input is at most 1048576 bytes; summed inputs stay
within the run input budget. Descriptor-relative `O_NOFOLLOW`, owner-only
stream directories/files (0700/0600), owner/single-link checks and snapshot
rechecks reject observed changes. Kernel calls already in progress cannot be
interrupted; atime can change on read, and this is not an immutable snapshot.

Evidence: [payload core](../../src/artifact/payload_run_inspector_v1.cpp),
[private stream inspector](../../src/artifact/payload_run_stream_inspector_v1.cpp),
[core contracts](../../tests/unit/payload_run_inspector_v1_tests.cpp),
[stream contracts](../../tests/unit/payload_run_stream_inspector_v1_tests.cpp),
[CLI contracts](../../tests/unit/dag_scheduler_v5_payload_inspector_cli_tests.cpp),
and [real workflows](../../tests/integration/test_dag_scheduler_v5_payload_inspector.py).
The workflow matrix covers V1.3/V1.4, solo/multi, streams, mixed inputs, FAILED,
Retry, BLOCKED, publication failure, removed Store/tree, corruption, repeatability
and before/after file snapshots. Recovery, resume, a historical input index and
large runtime orchestration remain **DOKUMENTIERT / KONZEPT**.

## V5 Artifact Payload Flow V2

**IMPLEMENTED AND EVIDENCED:** Flow V2 records the already implemented local
V1.3 producer-to-consumer payload edge as a private `BUNDLE.flow-v2.json`
sidecar. The opt-in commands are:

```text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE --flow-v2 [--stream-v15]
bruce dag-scheduler-v5 payload-inspect BUNDLE --flow-v2
```

The sidecar is a single canonical JSON line with a final LF and mode 0600.
Its fixed key order is `contract`, `run_id`, `spec_digest`, `report_digest`,
`stream_v15`, `nodes`, `attempts`. Nodes retain the validated Spec order and
dependencies; attempts retain `ATTEMPT_STARTED` order. Every started consumer
attempt records contiguous input bindings with producer node/attempt,
manifest ID, object index 0, SHA-256 and exact byte size. Root attempts use
`inputs:[]`; preflight IDs that never started are omitted. Payload bytes,
paths, PIDs, command lines, secrets and stream-internal names are excluded.

The sidecar is adopted only after the unchanged legacy report is complete.
`--external-inputs` is rejected with `FLOW_V2_EXTERNAL_INPUTS_UNSUPPORTED`
before any Spec, Store or runroot access. Encode, size, write or adoption
failures return `{"error":"FLOW_RECORD_INVALID"}` on stderr, exit 2, with
empty stdout. Without `--flow-v2`, V1.3/V1.4/V1.5 and legacy report bytes
remain unchanged.

The read-only Flow V2 inspector validates sidecar, report, log, requests,
outputs and the complete private legacy or stream input layout without Store,
worker, adapter, replay or repair. It returns
`dag-scheduler-v5-payload-flow-inspect-v2`: `VALID`/exit 0, `INCOMPLETE`/exit 1,
or the generic `PROTOCOL_ERROR`/exit 2. `provenance_scope=LOCAL_CONSISTENCY`
records local binding evidence only; it is not a historical authenticity claim.
The existing direct in-memory Scheduler V2 payload flow remains a separate
contract and receives no new API.

Evidence: [Flow model](../../include/bruce/artifact/payload_flow_v2.hpp),
[Flow inspector](../../src/artifact/payload_flow_inspector_v2.cpp),
[Payload workflows](../../tests/integration/test_dag_scheduler_v5_payload.py),
[Inspector workflows](../../tests/integration/test_dag_scheduler_v5_payload_flow_inspector.py),
and [CLI documentation contracts](../../tests/integration/test_cli_docs.py).
External payload stores, network/GPU execution, free plugins, free language,
general proving and large runtime orchestration remain **DOCUMENTED / CONCEPT**.

## Artifact Store V1

**IMPLEMENTIERT UND BELEGT:** Artifact Store V1 is a separate local,
content-addressed store. The exact public route is
`bruce artifact-store-v1 import FILE --store STORE`,
`bruce artifact-store-v1 inspect DIGEST --store STORE`, or
`bruce artifact-store-v1 verify DIGEST --store STORE`; V1.2 adds the separate
read-only route `bruce artifact-store-v1 resolve MANIFEST_ID --store STORE`.
The store hashes and
copies regular, owner-only, single-link source files through a bounded
streaming path and publishes
immutable digest-addressed objects atomically. The named object limit is
`bruce::max_file_bytes`, currently 32 MiB (33,554,432 bytes), and is enforced
before an object can exceed the bound.

The CLI emits only `contract`, `action`, `status`, `sha256`, and `size_bytes`
in its metadata-only JSON. It never emits payload bytes, source/store paths,
command lines or OS diagnostics. `inspect` and `verify` resolve the object by
digest and recheck its complete size and SHA-256. Duplicate publication is
idempotent for the same verified object and never overwrites an existing
inode. The core also provides the canonical metadata-only
`BRUCE_ARTIFACT_MANIFEST_V1` codec with fixed field order, 128-byte token
limits and at most 1024 object entries.

The local security boundary is descriptor-relative `O_NOFOLLOW` traversal,
owner-only store/object/manifest files, regular-file and single-link checks,
bounded reads, streaming SHA-256, `fsync`, and exclusive atomic publication.
The public CLI maps invalid syntax, missing/corrupt objects, wrong digest or
size, path escapes, symlinks, hardlinks, unsafe permissions and over-limit
objects to the compact `PROTOCOL_ERROR`/Exit 2 envelope. This is an
integrity-oriented local boundary, not encryption, authentication,
authorization or a remote trust system.

V5 `execute` does not automatically publish receipts; the explicit publish
bridge above is a separate command. Payload download, garbage collection,
network transport, external providers, remote stores, dynamic code plugins
and broad runtime orchestration remain **DOKUMENTIERT / KONZEPT**. The
Artifact Store does not change V2, V3, V4 or V5 output contracts.

### Artifact Store V1.2 Resolve

**IMPLEMENTIERT UND BELEGT:** Resolve liest ein bereits publiziertes,
kanonisches `BRUCE_ARTIFACT_MANIFEST_V1` und verifiziert jedes referenzierte
Objekt vollständig in Manifestreihenfolge. Erst wenn Manifest und alle Objekte
Digest- und Größenprüfung bestehen, liefert die Route den metadata-only Vertrag
`artifact-store-resolve-v1` mit `status:RESOLVED`. `manifest_status` bleibt der
gespeicherte Manifeststatus; ein leerer `origin` bleibt `""`, ein leeres
Manifest liefert `objects:[]`, und `media_type` wird nicht ausgegeben.

Der Erfolgsbericht hat die feste Reihenfolge
`contract`, `action`, `status`, `manifest_id`, `run_id`, `node_id`,
`attempt_id`, `manifest_status`, `result_code`, `objects`; je Objekt folgen
`sha256`, `size_bytes`, `origin`. Bei jedem Fehler bleiben stdout und der
Erfolgspfad leer; stderr erhält exakt
`{"contract":"artifact-store-resolve-v1","error":"PROTOCOL_ERROR"}` und
Exit 2. Die Route ist vollständig read-only: keine Payloadausgabe,
Materialisierung, Cache-/Indexschreibvorgänge, Pfadoffenlegung, Netzwerk-
oder Prozessstarts. V1.2 führt kein neues Diagnosebudget ein; die bestehende
bounded `max_file_bytes`-/32-MiB-Prüfung bleibt maßgeblich.

Belege: [Resolve-CLI](../../apps/artifact_store_v1_cli.hpp),
[Resolve-Core](../../src/artifact/artifact_store_v1.cpp),
[Resolve-Unit-Verträge](../../tests/unit/artifact_store_v1_cli_tests.cpp),
[Resolve-Workflow](../../tests/integration/test_artifact_store_resolve_v1.py)
und [Resolve-Spezifikation](../superpowers/specs/2026-10-15-artifact-store-resolve-v1-design.md).

Evidence: [store API](../../include/bruce/artifact/artifact_store_v1.hpp),
[manifest API](../../include/bruce/artifact/artifact_manifest_v1.hpp),
[CLI](../../apps/artifact_store_v1_cli.hpp),
[core contracts](../../tests/unit/artifact_store_v1_tests.cpp),
[CLI contracts](../../tests/unit/artifact_store_v1_cli_tests.cpp), and
[real filesystem workflows](../../tests/integration/test_artifact_store_v1.py).
