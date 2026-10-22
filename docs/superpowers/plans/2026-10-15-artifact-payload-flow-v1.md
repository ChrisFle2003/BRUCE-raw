# BRUCE Artifact Payload Flow V1.3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to implement this plan
> task-by-task. This plan is a future implementation plan; Phase 0 itself
> changes no production code.

**Goal:** Add an explicit, host-controlled V1.3 payload path that resolves
direct DAG predecessor artifacts, materializes bounded private inputs, and
keeps existing V1/V2/V3/V4/V5 contracts byte-compatible.

**Architecture:** Bruce remains the sole Artifact Store reader and DAG
authority. A new versioned `ArtifactInputRefV13` identifies a manifest object
by opaque manifest ID, object index, digest, and exact size; Bruce verifies and
streams it into a private Attempt input file before invoking the existing
worker boundary. The new behavior uses an explicit V1.3 route and does not
reinterpret existing `execute`, V2, V3, V4, or V1.2 Resolve behavior.

**Tech Stack:** C++20, existing Artifact Store V1/V1.2 primitives, existing
V5 worker supervisor and JSONL run log, CMake/CTest, Python integration tests,
and current SHA-256/safe-file helpers.

**Spec:** `docs/superpowers/specs/2026-10-15-artifact-payload-flow-v1-design.md`

**Phase-0 evidence:**
`runs/bruce-artifact-payload-v1/contract-inventory.md` and
`runs/bruce-artifact-payload-v1/design-review.md` are the source analyses.
They are evidence for the baseline only; the V1.3 working decisions below are
now accepted. The plan is **IMPLEMENTIERUNGSBEREIT**; production code is still
unchanged.

## Global Constraints

- Preserve existing Phase-DAG V2/V3, DAG-Scheduler V2/V3/V4, Artifact Store V1/V1.2, and V5 execute/inspect/publish contracts byte-for-byte unless an explicit V1.3 route is selected.
- Bruce is the only component allowed to open the Artifact Store and materialize payload bytes.
- `origin` is metadata only; no path, URL, provider name, or user-selected destination may become a read path.
- Workers receive only Bruce-owned relative private input references and may not open the Store, use the network, start processes, load plugins, or publish manifests.
- Existing defaults/maxima remain authoritative until a separate decision changes them: wall `30_000/600_000 ms`, input `65_536/1_048_576 bytes`, output `65_536/5_242_880 bytes`, operations `1_000_000/10_000_000`, memory `268_435_456/2_147_483_648 bytes`, workers `1/4`.
- A node may reference at most 15 direct payload objects in V1.3 because the
  unchanged WorkerRequest contract reserves one of its 16 input slots for the
  node request itself.
- The public route is exactly `bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE`.
- Only canonical manifests with `status=READY` may be consumed.
- `ArtifactInputRefV13` contains exactly `manifest_id`, `object_index`, `sha256`, and `size_bytes`; provenance is checked through the direct DAG edge and manifest fields.
- Bruce uses host-mediated bounded file materialization; workers never open the Store.
- Existing `max_wall_ms` is the end-to-end deadline; V1.3 adds no separate Resolve budget.
- An accepted READY node is published immediately, and Bruce adopts Store/DAG state deterministically before unlocking successors.
- Retry is permitted only through the explicit CLI option.
- No payload bytes, private paths, OS diagnostics, PIDs, secrets, or command lines enter public reports.
- No network, external provider, GC, retention, cache, repair, automatic migration, GPU adapter, free plugin, free language, general prover, or cross-runtime orchestration is part of V1.3.
- Generated CLI artifacts are checked only through `docs/cli/verify_examples.py` in a temporary copy.
- Use C++20 and the existing CMake/CTest flow; do not install packages or use network access.

## Review Focus

- **Reference/path confusion:** a path-like manifest ID, `origin`, or private Attempt reference must be rejected before Store access and never reach a worker. Test in Task 1 and Task 3.
- **Late integrity failure:** a later object in a multi-input set must fail the whole node without a partial READY result or leaked bytes. Test in Task 2 and Task 3.
- **Budget accounting:** object count, aggregate input bytes, wall deadline, and child memory must be enforced before worker start. Test in Task 2 and Task 4.
- **DAG state adoption:** failed input makes the node `FAILED` and transitively dependent nodes `BLOCKED`; only an accepted attempt unlocks successors. Test in Task 4.
- **Compatibility and determinism:** existing V1/V2/V3/V4/V5 routes remain unchanged and independent READY nodes produce deterministic reports and Store adoption. Test in Task 5.

## Files and Boundaries

| File | Future responsibility |
|---|---|
| `include/bruce/artifact/artifact_payload_v1.hpp` | Define `ArtifactInputRefV13`, bounded-read/materialization result types, and explicit error categories. |
| `src/artifact/artifact_payload_v1.cpp` | Implement host-only descriptor-relative bounded streaming and final digest/size verification. |
| `include/bruce/artifact/artifact_store_v1.hpp` / `src/artifact/artifact_store_v1.cpp` | Add only the smallest read primitive needed by the new payload layer; keep V1.2 Resolve metadata-only. |
| `tests/unit/artifact_payload_v1_tests.cpp` | Test reference validation, Store safety, ordering, limits, and no-write/no-leak behavior. |
| `apps/dag_scheduler_v5_cli.hpp` | Add the explicitly selected V1.3 route and connect host resolution to private WorkerRequest inputs; leave existing branches unchanged. |
| `tests/unit/dag_scheduler_v5_payload_cli_tests.cpp` | Test parser, exact success/error envelope, defaults/maxima, retry option, and route isolation. |
| `tests/integration/test_dag_scheduler_v5_payload.py` | Exercise real DAG Specs, one/multiple inputs, failures, retries, parallel nodes, and public output hygiene. |
| `include/bruce/worker/worker_boundary_v1.hpp` / `src/worker/*` | Modify only if an explicitly versioned V1.3 host-to-worker field is required; do not extend the existing V1 frame silently. |
| `CMakeLists.txt` | Register new sources and focused tests without changing existing target behavior. |
| `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/implementation/README.md`, `docs/architecture/03-blueprint-consolidation.md` | Document the implemented V1.3 route and retain explicit concept/non-goal labels. |
| `tests/integration/test_cli_docs.py` | Assert the exact new contract and unchanged compatibility claims. |
| `runs/bruce-artifact-payload-v1-2026-10-15.md` | Record evidence labels, decisions, verification counts, limitations, and remaining concepts. |

## Task 0: Record Accepted Phase-0 Decisions (complete)

**Files:**
- Modify: `docs/superpowers/specs/2026-10-15-artifact-payload-flow-v1-design.md`
- Modify: `docs/superpowers/plans/2026-10-15-artifact-payload-flow-v1.md`

**Interfaces:**
- Consumes: the Phase-0 Contract Inventory and Design Review.
- Produces: the accepted route name, exact reference/provenance fields,
  status rule, object-count rule, budget semantics, error envelope, and
  publication timing used by the implementation tasks.

- [x] **Step 1: Decide the public boundary.** The V1.3 subcommand is
  `dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE`; existing
  `execute` remains unchanged.
- [x] **Step 2: Decide reference and provenance.** The reference contains
  exactly `manifest_id`, `object_index`, `sha256`, and `size_bytes`. Provenance
  is checked through the direct edge and manifest fields, not extra ref fields.
- [x] **Step 3: Decide status, cardinality, budgets, and errors.** Inputs must
  be `READY`; at most 15 direct objects are accepted because the unchanged
  worker contract reserves one of its 16 total input slots for the request;
  existing defaults and
  maxima remain; `max_wall_ms` is end-to-end; Retry is CLI-only; errors are
  stable metadata-only classes.
- [x] **Step 4: Decide publication and adoption.** Each accepted READY node is
  published immediately; Bruce adopts Store/DAG state deterministically before
  enabling successors.
- [x] **Step 5: Review compatibility.** No decision adds fields to V2/V3/V4/
  V5 or turns V1.2 Resolve into a byte reader; host-mediated bounded file
  materialization is the V1.3 handoff.

## Task 1: Versioned Reference and Core Validation

**Files:**
- Create: `include/bruce/artifact/artifact_payload_v1.hpp`
- Create: `src/artifact/artifact_payload_v1.cpp`
- Create: `tests/unit/artifact_payload_v1_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ArtifactStoreV1::read_manifest`, `resolve_manifest`, existing
  `ArtifactIdV1`, safe-file validation, and chosen Phase-0 decisions.
- Produces: `ArtifactInputRefV13`, deterministic reference validation,
  bounded object-selection API, and typed errors with no public rendering.

- [ ] **Step 1: Write failing tests for `ArtifactInputRefV13`.** Cover valid
  opaque IDs, zero-based indices, lowercase 64-hex digests, exact sizes,
  path-like IDs, negative/overflow indices, invalid digests, and size overflows.
- [ ] **Step 2: Run the focused target and observe the expected missing-type
  failure.** Existing Artifact Store tests must remain buildable for comparison.
- [ ] **Step 3: Implement the reference value type and validator.** Keep the
  type independent of filesystem paths and retain explicit error categories.
- [ ] **Step 4: Write failing tests for manifest/object binding.** Assert
  selected object index, digest/size equality, chosen READY/provenance rule,
  manifest order, zero-object rejection for an out-of-range index, and no
  Store writes.
- [ ] **Step 5: Implement read-only selection.** Resolve the canonical
  manifest, verify the selected object with existing Store rules, and return a
  bounded logical object description without returning payload bytes.
- [ ] **Step 6: Run the unit target plus `artifact_store_v1_contracts` and
  `artifact_store_v1_cli_contracts`.** Expected: new validation passes and
  all existing Store contracts remain green.

## Task 2: Bounded Host Materialization

**Files:**
- Modify: `include/bruce/artifact/artifact_store_v1.hpp`
- Modify: `src/artifact/artifact_store_v1.cpp`
- Modify: `include/bruce/artifact/artifact_payload_v1.hpp`
- Modify: `src/artifact/artifact_payload_v1.cpp`
- Modify: `tests/unit/artifact_payload_v1_tests.cpp`

**Interfaces:**
- Consumes: Task 1 reference validation and exact Phase-0 budget values.
- Produces: a host-only bounded stream/copy operation that writes only into a
  caller-created private Attempt input and returns digest/size/usage metadata.

- [ ] **Step 1: Write failing tests for bounded reads.** Cover one object,
  multiple objects, exact boundary size, aggregate input limit, object-count
  limit, missing object, late corruption, symlink, hardlink, insecure mode,
  non-regular file, and destination overwrite refusal.
- [ ] **Step 2: Run the focused unit target and confirm the reader is absent.**
- [ ] **Step 3: Implement descriptor-relative streaming.** Use `O_NOFOLLOW`,
  regular-file/owner-only/single-hardlink checks, fixed-size chunks, a deadline,
  and a newly created private destination. Never interpret `origin` as a path.
- [ ] **Step 4: Verify the destination after streaming.** Recompute digest and
  exact size, fsync/close according to existing private-run conventions, and
  reject any mismatch before returning success.
- [ ] **Step 5: Add no-leak and no-write assertions.** Public-facing result data
  must contain no bytes or paths; failures must leave no accepted input file or
  partial metadata state.
- [ ] **Step 6: Run core Artifact Store, Worker Boundary, Inspector, and
  focused payload tests.** Record exact pass counts before route work starts.

## Task 3: Explicit V1.3 CLI Route and Worker Request Binding

**Files:**
- Modify: `apps/dag_scheduler_v5_cli.hpp`
- Create: `tests/unit/dag_scheduler_v5_payload_cli_tests.cpp`
- Create: `tests/integration/test_dag_scheduler_v5_payload.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Tasks 1–2, existing V5 Spec parser, `WorkerRequest`, supervisor,
  run log, and selected CLI decisions.
- Produces: a new explicit versioned route, exact success/error renderer,
  private input binding, and CLI-only retry selection.

- [ ] **Step 1: Write failing parser tests.** Accept only the selected route;
  reject missing Store/Spec/output, path-like refs, unsupported options,
  invalid defaults/maxima, and attempts to enable retry except through the
  explicit CLI option.
- [ ] **Step 2: Run the unit target to confirm the route is absent.**
- [ ] **Step 3: Add a separate parser branch.** Do not alter `execute`,
  `inspect`, or `publish` parsing, usage, fields, or defaults.
- [ ] **Step 4: Define the metadata-only success and error envelopes.** Fix
  field order, node statuses, object counts, usage, route contract, exit code,
  and the rule that no partial READY report is emitted.
- [ ] **Step 5: Build WorkerRequests from private materialized inputs.** The
  worker receives only safe relative refs, expected digest/size, and existing
  limits. It never receives Store IDs as paths or opens the Store.
- [ ] **Step 6: Implement CLI-only retry.** Each retry gets a fresh Attempt ID;
  integrity, permission, and reference failures are never retried implicitly.
- [ ] **Step 7: Register and run real-binary tests.** Assert success, exact
  stderr/stdout, no payload/path/OS-error leakage, idempotent behavior, and
  unchanged existing V5 workflows.

## Task 4: DAG State Machine and Publication Ordering

**Files:**
- Modify: `apps/dag_scheduler_v5_cli.hpp` or a new focused V5 orchestration
  source if the existing file cannot preserve boundaries
- Modify: `include/bruce/worker/worker_run_log_v1.hpp` only if a new versioned
  metadata field is required
- Create/modify: focused unit and integration tests

**Interfaces:**
- Consumes: explicit V1.3 route, bounded materializer, existing Kahn ordering,
  worker supervisor, and Artifact Store Publish primitives.
- Produces: direct-edge payload flow, deterministic state adoption, and
  transitive `BLOCKED` propagation.

- [ ] **Step 1: Write failing DAG tests.** Cover one edge, multiple direct
  edges in canonical order, no-input node, missing predecessor, cycle,
  corrupted predecessor, and transitive blocking.
- [ ] **Step 2: Run the tests and confirm the V1.3 state path is absent.**
- [ ] **Step 3: Add only direct-edge reference construction.** Accept a
  predecessor only after its output has been verified and published; never
  infer an indirect payload.
- [ ] **Step 4: Add deterministic state transitions.** A failed input makes
  the node `FAILED`; all transitively dependent nodes become `BLOCKED`; only
  an accepted READY Attempt unlocks successors.
- [ ] **Step 5: Add publication ordering.** Publish each accepted output and
  canonical manifest before enabling direct successors. Resolve conflicts
  deterministically and never expose private Attempt paths.
- [ ] **Step 6: Run multi-node integration tests with `--workers 1` and the
  accepted parallel worker count.** Compare metadata reports and Store state.

## Task 5: Compatibility, Documentation, and Final Verification

**Files:**
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/implementation/README.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `tests/integration/test_cli_docs.py`
- Create: `runs/bruce-artifact-payload-v1-2026-10-15.md`

**Interfaces:**
- Consumes: implemented route and all accepted decisions from Tasks 0–4.
- Produces: evidence-labelled documentation and a reproducible final report.

- [ ] **Step 1: Document the exact route and formats.** Include reference
  fields, ordering, budgets, error envelopes, retry semantics, and host/
  worker/store boundaries.
- [ ] **Step 2: Document compatibility and non-goals.** Keep V2/V3/V4/V5
  existing routes byte-compatible; mark network, external stores, GPU,
  plugins, free language, proving, and large runtime orchestration as
  **DOKUMENTIERT / KONZEPT**.
- [ ] **Step 3: Add documentation tests.** Verify route, contract, limits,
  and compatibility wording. Validate generated CLI artifacts only through a
  temporary copy of `docs/cli/verify_examples.py`.
- [ ] **Step 4: Run the prescribed build.**
  `scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release`
  then build and run focused CTest targets.
- [ ] **Step 5: Run compatibility and repository checks.** Run existing V1,
  Worker, Inspector, V5 execute/inspect/publish, docs-link, full CTest,
  `git diff --check` where usable, Git-status inspection, and the defensive
  secret scan. Record sandbox limitations exactly.
- [ ] **Step 6: Write the final report and request whole-change review.** Do
  not label V1.3 implemented until the focused tests, compatibility tests,
  documentation checks, and whole-change review pass.

## Verification Commands

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-artifact-payload-v1-build \
  -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-artifact-payload-v1-build
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-artifact-payload-v1-build -R \
  'artifact_payload_v1|artifact_store_v1|worker_boundary_v1|worker_run_inspector_v1|dag_scheduler_v5' \
  --output-on-failure
python3 tests/integration/test_cli_docs.py \
  /tmp/bruce-artifact-payload-v1-build/bruce
python3 scripts/check_local_doc_links.py
```

## Plan status

**IMPLEMENTIERUNGSBEREIT.** Task 0 is complete and its decisions are recorded
in the Phase-0 report. Tasks 1–5 are the staged implementation sequence;
production behavior remains unimplemented until those tasks, tests, and
reviews pass.
