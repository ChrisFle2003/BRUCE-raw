# Release Hardening and Trust Rotation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to implement this plan one task at
> a time. Use a fresh implementer context and a fresh reviewer context for
> every task; never run two implementation agents concurrently in the shared
> tree.

**Goal:** Close the single-worker cancel-before-acceptance race, recover a
truthful version-control boundary, add fresh Release/Debug/scoped sanitizer
evidence, make the existing built-in Mod matrix an explicit mandatory CI
stage, add offline Ed25519 signing for Core release archives, and add local
transactional Mod trust rotation with rollback.

**Architecture:** Put the final cancel decision inside the Common Runtime Host
acceptance boundary. Preserve the current package and V1 truststore bytes;
wrap truststores in immutable, journaled generations rather than changing Mod
signatures. Keep CI responsible only for reproducible unsigned TGZ validation,
and use a separate non-installed developer executable for local release
signing and verification. Treat Git provenance, sanitized targets, static
worker fixtures, and network-port ownership as explicit gates rather than
papering over environmental ambiguity.

**Tech Stack:** C++20, CMake/Ninja/CTest, OpenSSL EVP Ed25519 and SHA-256,
existing descriptor-based private-file and journal patterns, Python 3
integration tests, GitHub Actions, CPack/TGZ, Markdown/CLI documentation
verification.

**Spec:**
`docs/superpowers/specs/2026-10-19-release-hardening-and-trust-rotation-design.md`

**Current status:** **DOKUMENTIERT / KONZEPT**. Checkboxes below describe work
to perform; none is evidence until the named test has been run against the
resulting source.

## Global execution rules

- Before every task, inspect the real repository status and relevant diffs.
  The current sandbox exposes an empty read-only `.git` mask, so no task may
  claim a commit, branch, clean status, or recovered history until a read-only
  host-side inspection proves it.
- Before product edits, create a source manifest and recoverable archive under
  a unique `/tmp/bruce-hardening-*` root. Exclude `.git`, secret files such as
  `.env`, build directories, `runs`, `.worktrees`, `.agents`, `.codex`, and
  Python caches; record the exclusion list next to the manifest.
- Prefer an existing attached managed worktree after inspecting task
  artifacts. Create a new worktree only when an intact matching repository is
  proven. If no safe Git worktree is possible, stop before product edits and
  request the required host-side provenance decision; do not substitute
  `git init` or copied metadata.
- Preserve unrelated user work. Never reset, clean, overwrite, delete, or
  bundle unrelated changes.
- Run `scripts/with-host-deps.sh` only from a complete native source copy or
  worktree whose path contains neither whitespace nor `:`.
- Use test-driven development for every behavior change: add one focused
  failing test, run it and record the expected failure, implement the minimum
  behavior, rerun the focus set, then run neighboring regressions.
- Generated CLI artifacts are changed only by
  `docs/cli/verify_examples.py` in a complete temporary copy, followed by a
  byte comparison. Never edit them manually.
- Productive release seeds never enter the repository, command line,
  environment, stdin, CI, logs, or reports. Tests use only conspicuously named
  fixture seeds.
- Every public machine-readable success is semantically parsed in tests;
  exit code 0 or arbitrary text is not sufficient evidence.
- When Git is healthy, each task ends in the named narrow commit after review.
  While Git is unavailable, mark that commit step **BLOCKED** rather than
  fabricating version history.
- Maintain a subagent ledger containing task, implementer, red test, green
  tests, reviewer, findings, and commit or block reason. One implementation
  agent works at a time; review may start only after that agent finishes.

## Target file map

| File | Responsibility |
| --- | --- |
| `src/application/common_runtime_host_v2.cpp` | Last single-worker abort probe and terminal acceptance decision |
| `include/bruce/application/common_runtime_host_v2_acceptance.hpp` | Production acceptance-decision seam used by host and deterministic contract tests |
| `tests/unit/common_runtime_host_v2_tests.cpp` | Deterministic single-worker cancel/invalid/late-marker contracts |
| `tests/integration/test_common_runtime_host_v2_stage3.py` | Real-process regression and bounded parallel stress |
| `tests/integration/test_builtin_mod_matrix.py` | Build-type-aware six-valid/two-invalid feature matrix |
| `cmake/BruceSanitizers.cmake` | Target-scoped ASan/UBSan helper and exclusions |
| `CMakeLists.txt` | New test/tool targets, labels, matrix resource rules, sanitizer wiring |
| `.github/workflows/ci.yml` | Explicit matrix, package, Debug, sanitizer, and static-fixture stages |
| `include/bruce/release/release_signature_v1.hpp` | Canonical release signature data and API |
| `src/release/release_signature_v1.cpp` | Digest, key parsing, signing, verification, and canonical JSON |
| `apps/bruce_release_tool.cpp` | Non-installed local `sign` and `verify` developer CLI |
| `include/bruce/mod/runtime_trust_store_v2.hpp` | Immutable trust generations and prospective validation model |
| `src/mod/runtime_trust_store_v2.cpp` | Generation persistence, journal recovery, activation, rollback |
| `include/bruce/mod/installed_mod_host_v2.hpp` | Cross-registry atomic trust/activation orchestration |
| `src/mod/installed_mod_host_v2.cpp` | Installed package verification and emergency deactivation transaction |
| `apps/runtime_mod_cli.hpp` | `mods trust` grammar, JSON output, and stable error mapping |
| `scripts/verify_install.py` | Existing TGZ verifier used by CI without signing authority |
| `docs/implementation/150-release-signing-contract.md` | Implemented release-signing contract after code exists |

---

## Wave 0 — Preserve the source and establish provenance

### Task 1: Create a recoverable baseline and resolve the Git gate

**Files:**
- Create outside repository: `/tmp/bruce-hardening-*/source-manifest.sha256`
- Create outside repository: `/tmp/bruce-hardening-*/source-baseline.tar`
- Create outside repository: `/tmp/bruce-hardening-*/exclusions.txt`
- Create outside repository: `/tmp/bruce-hardening-*/git-provenance.md`
- Do not modify: `.git`

- [ ] Run `git status --short` and capture the failure or real status without
  suppressing it.
- [ ] List currently attached worktrees/artifacts. Reuse a suitable attached
  worktree if it points to this exact source lineage.
- [ ] Create the exclusion manifest first. Enumerate every archived source
  path without printing file contents, hash each regular file, then create the
  archive from exactly that list.
- [ ] Verify the archive by extracting it into a second fresh temporary
  directory and comparing its hashes to `source-manifest.sha256`.
- [ ] Inspect the host repository read-only: worktree root, HEAD, branch,
  remotes with credentials redacted, `git fsck`, status, and relation to the
  visible tree.
- [ ] Compare any candidate sibling history by commit ancestry and complete
  source-path digest, not by directory name.
- [ ] If provenance is unique, create or attach an isolated managed worktree
  and record its exact base commit. If it is not unique, stop implementation
  here and hand over archive, manifest, and ambiguity report.
- [ ] Reviewer confirms no secret path/content is present in the report or
  archive list and no Git metadata was written or copied.

**Expected gate:** The current masked view continues to fail safely. A green
result requires either a proven matching host worktree or an explicit stop;
“new repository initialized” is a test failure.

**Commit:** none. This is a preservation/provenance gate.

---

## Wave 1 — Close the Common Runtime Host race

### Task 2: Pin the final cancel-before-acceptance contract

**Files:**
- Modify: `tests/unit/common_runtime_host_v2_tests.cpp`
- Modify: `CMakeLists.txt` only if a focused test selector/target is needed

- [ ] Add a deterministic test fixture that returns canonical `READY`, creates
  a valid abort marker immediately before the host acceptance decision, and
  observes one `CANCEL_REQUESTED`, no `ARTIFACT_ACCEPTED`, terminal
  `CANCELLED`, exit code 1, and no reference/digest/size.
- [ ] Add the same boundary test with an invalid marker; require
  `PROTOCOL_ERROR`, no acceptance, and no leaked marker contents.
- [ ] Add a control case without a marker; require exactly one acceptance and
  exit code 0.
- [ ] Add a late-marker case after durable acceptance; require no retroactive
  status change and no appended cancel event.
- [ ] Add a boundary case where publication occurs after the successful final
  probe but before the acceptance event append; require it to be treated as a
  late marker according to the explicit linearization contract.
- [ ] Run the focused unit target and observe RED because `State::complete`
  currently trusts the earlier pre-poll abort probe.

**RED command:** build `bruce_common_runtime_host_v2_tests`, then run its CTest
contract with `--output-on-failure` from the safe worktree build.

### Task 3: Move the abort probe into the acceptance boundary

**Files:**
- Create: `include/bruce/application/common_runtime_host_v2_acceptance.hpp`
- Modify: `src/application/common_runtime_host_v2.cpp`
- Reference without broad rewrite:
  `src/application/common_runtime_host_v2_batch.cpp`
- Modify: `tests/unit/common_runtime_host_v2_tests.cpp`

- [ ] Extract or add a private State operation that probes abort immediately
  before a canonical READY result can append `ARTIFACT_ACCEPTED`.
- [ ] Make the last decision directly unit-testable by factoring a production
  acceptance-decision helper which performs the probe itself. The RED test
  may include the planned production header before it exists; the helper is
  part of the real host path, not a test-only callback, sleep, or environment
  switch.
- [ ] On `REQUESTED`, append `CANCEL_REQUESTED` at most once, request worker
  cancellation idempotently, retain usage, clear all artifact fields, and
  finish as `CANCELLED`.
- [ ] On `INVALID`, retain usage, clear all artifact fields, and finish as
  `PROTOCOL_ERROR`; do not append a valid-cancel claim for malformed input.
- [ ] Leave the earlier poll probe in place as an eager cancellation
  optimization, but make the new acceptance probe authoritative.
- [ ] Define the successful final probe as the linearization point. A marker
  published after that probe, including before the following journal append,
  is late and must not retroactively cancel the acceptance.
- [ ] Preserve event ordering for all existing success, worker-failure,
  timeout, and already-cancelled cases.
- [ ] Run the new focused tests GREEN, then run
  `common_runtime_host_v2_contracts`, stage3, stage3b, stage3c, stage3d,
  scheduler abort, and worker log/inspector neighbors.
- [ ] Reviewer compares the single-worker decision with
  `record_attempt_acceptance_v2` and checks exactly-once logging and terminal
  field stripping.

**Commit:** `fix: close single-worker cancel acceptance race`

### Task 4: Replace flaky synchronization and prove the race stays closed

**Files:**
- Modify: `tests/integration/test_common_runtime_host_v2_stage3.py`
- Create if needed:
  `tests/integration/common_runtime_host_v2_acceptance_fixture.cpp`
- Modify: `CMakeLists.txt`

- [ ] Replace the SIGSTOP/zombie timing assumption as the correctness oracle
  with an observable deterministic fixture boundary. Keep a real-process
  stress scenario as supplementary coverage.
- [ ] The fixture must use existing public worker/host protocol behavior; do
  not add production sleeps, environment backdoors, or test-only branches to
  the host.
- [ ] Run the focused integration case before its fixture/final host wiring
  and observe RED for the expected boundary.
- [ ] Run the corrected test 20 sequential times and then at least 800 cases
  with the previously reproducing eight-way parallel pressure.
- [ ] Record iteration count, concurrency, elapsed time, failures, and the
  exact binary digest outside the repository.
- [ ] Run all Common Runtime Host V2 integration groups once more normally.
- [ ] Reviewer confirms the test can fail against the pre-fix host and does
  not pass merely by missing the acceptance window.

**Commit:** `test: make cancel acceptance regression deterministic`

---

## Wave 2 — Build profiles and explicit CI quality gates

### Task 5: Make the existing built-in Mod matrix profile-aware and bounded

**Files:**
- Modify: `tests/integration/test_builtin_mod_matrix.py`
- Modify: `CMakeLists.txt`
- Modify: `.github/workflows/ci.yml`
- Modify: `tests/integration/test_builtin_mod_help.py` only if profile output
  is asserted there

- [ ] Add tests for a required `--build-type` value limited to `Release` or
  `Debug`, an optional bounded `--parallel` value, and an optional compiler
  launcher forwarded as a CMake cache argument rather than shell text.
- [ ] Run RED against the current matrix, which hardcodes Release and leaves
  child Ninja parallelism implicit.
- [ ] Configure each nested case with the requested build type; pass
  `--parallel N` to child builds and preserve the six valid plus two invalid
  configurations.
- [ ] Forward `CMAKE_CXX_COMPILER_LAUNCHER` as a tokenized CMake definition
  only when configured. Do not evaluate a shell command.
- [ ] Give `builtin_mod_build_matrix` explicit CTest labels and serialize it
  against the MCP inspector workflow using `RESOURCE_LOCK` or `RUN_SERIAL`.
- [ ] In Release CI, run the matrix in a named step and run the remaining
  CTest suite with exactly `^builtin_mod_build_matrix$` excluded.
- [ ] Add a temporary-copy mutation test that intentionally breaks one
  expected feature state and proves the named matrix step fails.
- [ ] Reviewer checks there is no duplicate matrix execution, zero-test false
  positive, lost invalid-configuration case, or hidden `continue-on-error`.

**Commit:** `ci: expose and bound built-in mod matrix`

### Task 6: Add target-scoped ASan/UBSan support

**Files:**
- Create: `cmake/BruceSanitizers.cmake`
- Modify: `CMakeLists.txt`
- Modify: `mods/language/CMakeLists.txt`
- Modify: `mods/nanocell/CMakeLists.txt`
- Modify: `mods/mcp/CMakeLists.txt`
- Create: `tests/integration/test_sanitizer_configuration.py`

**Interfaces:**
- Add `BRUCE_ENABLE_ASAN_UBSAN` default `OFF`.
- Add a helper such as `bruce_enable_sanitizers(TARGET)` that adds both
  compile and link options only to explicitly selected dynamic targets.
- Add a CTest label for the compatible runtime set and a separate label for
  static worker-fixture coverage.

- [ ] Write a configuration test that inspects generated compile/link commands
  and requires sanitizer flags on Core, built-ins, CLIs, normal unit tests,
  and compatible integration fixtures.
- [ ] Require absence of sanitizer flags on these static fixtures:
  `bruce_logic_mod_worker_fixture`, `bruce_logic_mod_exec_escape_fixture`,
  `bruce_logic_mod_chain_worker_fixture`,
  `bruce_logic_mod_dag_worker_fixture`,
  `bruce_logic_mod_dag_v3_worker_fixture`,
  `bruce_logic_mod_dag_v5_worker_fixture`, and
  `bruce_logic_mod_payload_worker_fixture`.
- [ ] Enumerate every registered CTest and require exactly one classification:
  sanitizer-compatible, static-fixture-dependent, or a documented explicit
  exclusion with a reason. Fail for unclassified or multiply classified
  tests; a hand-selected passing subset is not complete coverage evidence.
- [ ] Run RED because no scoped sanitizer option exists.
- [ ] Implement the helper and apply it target-by-target; do not use global
  `CMAKE_CXX_FLAGS`, remove `-static -s`, or relax sandbox limits.
- [ ] Build the scoped profile and run its labeled tests with
  `ASAN_OPTIONS=halt_on_error=1` and
  `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
- [ ] Build and run the static fixture label in a separate unsanitized Debug
  tree.
- [ ] Use a disposable out-of-tree sanitizer probe to prove a diagnostic
  makes the lane nonzero; do not commit an intentional defect.
- [ ] Reviewer inspects representative link commands and verifies coverage
  labels do not misrepresent static fixtures as sanitized.

**Commit:** `build: add scoped address and undefined sanitizers`

### Task 7: Add Debug, package, sanitizer, and static-fixture CI stages

**Files:**
- Modify: `.github/workflows/ci.yml`
- Modify: `scripts/verify_install.py` only if deterministic report fields need
  a backward-compatible addition
- Create: `tests/integration/test_ci_contract.py`
- Modify: `CMakeLists.txt`

- [ ] Add a workflow-contract test that parses the YAML text and asserts:
  Release matrix is explicit, the remaining Release suite excludes only that
  matrix, package verification includes `--package`, Debug uses Debug, the
  sanitizer option is ON only in its scoped job, and no quality step is
  permissive.
- [ ] Run RED against the current single Release job.
- [ ] Preserve the existing Release build, docs byte comparison, offline link
  check, pinned actions, ccache stats, and secret scan.
- [ ] Add `CMAKE_BUILD_PARALLEL_LEVEL` and job timeouts appropriate to nested
  matrix work.
- [ ] Make the Release package step call `verify_install.py --package` with an
  explicit new archive output in the runner temporary directory. Do not sign
  or publish it.
- [ ] Add a Debug configure/build/full-CTest job whose matrix receives
  `--build-type Debug`.
- [ ] Add a scoped ASan/UBSan job and a separate unsanitized static-fixture
  job or step.
- [ ] Ensure MCP loopback tests retain coverage and contention is prevented;
  do not convert socket denial into a skip.
- [ ] Run the workflow-contract test and locally reproduce every shell command
  possible in fresh no-space copies.
- [ ] Reviewer checks action pins, permissions, artifacts, secret exposure,
  duplicate work, and the exact package path passed to the verifier.

**Commit:** `ci: verify debug sanitizers and release package`

---

## Wave 3 — Offline Core release signing

### Task 8: Define canonical release-signature primitives

**Files:**
- Create: `include/bruce/release/release_signature_v1.hpp`
- Create: `src/release/release_signature_v1.cpp`
- Create: `tests/unit/release_signature_v1_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `ReleaseSignatureEnvelopeV1` with contract, version, archive basename,
  archive SHA-256, key ID, and Ed25519 signature.
- Strict parse/render functions for the signature envelope and
  `release-key-v1.json`.
- Domain-separated preimage builder and sign/verify functions using OpenSSL
  EVP raw Ed25519 keys.
- Key ID equals full lowercase SHA-256 of the raw 32-byte public key.

- [ ] Test exact canonical JSON, LF termination, sorted/defined field order,
  exact 64/128 lowercase hex sizes, safe basename, bounded version, unknown or
  duplicate fields, oversized input, and round-trip identity.
- [ ] Test a fixed non-production Ed25519 vector, wrong key, wrong version,
  wrong archive name, wrong digest, and changed signature.
- [ ] Run RED because the API and target do not exist.
- [ ] Implement bounded parsers using existing JSON and crypto conventions;
  compare digests/signatures without leaking input values in errors.
- [ ] Run the focused unit target twice and compare canonical bytes.
- [ ] Reviewer independently recomputes the key ID and verifies every envelope
  field participates in the signed preimage.

**Commit:** `feat: add canonical release signature v1`

### Task 9: Add the non-installed offline release tool

**Files:**
- Create: `apps/bruce_release_tool.cpp`
- Create: `tests/integration/test_release_tool.py`
- Modify: `CMakeLists.txt`

**Interfaces:**

```text
bruce-release-tool sign \
  --archive ARCHIVE --audit-report INSTALL-AUDIT.json \
  --private-key KEY --trusted-public-key PINNED-KEY.json \
  --output-dir NEW-RELEASE-DIR

bruce-release-tool verify \
  --release-dir RELEASE-DIR \
  --trusted-public-key PINNED-KEY.json
```

- [ ] Add CLI tests for help, missing/unknown/duplicate options, wrong types,
  missing files, unsafe basename, output collision, corrupted archive,
  checksum, signature, public key, audit report, and semantically mismatched
  version.
- [ ] Add private-key security tests: exact two-line format, regular file,
  owner UID, mode `0600`, link count one, no symlink/FIFO, bounded size, and
  no seed/path in stdout or stderr.
- [ ] Run RED because the executable does not exist.
- [ ] Parse the successful `verify_install.py --package` report and derive
  version, archive basename, and archive digest from it; accept no free-form
  `--version`. Recompute name and digest against the exact archive bytes.
- [ ] Derive the public key from the private seed and require an exact match to
  the independently supplied pinned key before signing.
- [ ] Implement `sign` by creating a private sibling staging directory,
  copying the audited archive, writing checksum/signature/pinned-key copy and
  a canonical release-set manifest, fsyncing files/directories, then atomically
  renaming the complete directory to the new `--output-dir`. Never replace an
  existing directory; a set without the manifest is invalid.
- [ ] Implement `verify` as read-only. It recalculates archive SHA-256, checks
  the checksum filename/digest, parses all contracts, requires the bundled key
  to equal the independently supplied pinned key, and verifies the
  domain-separated signature.
- [ ] Return canonical JSON reports on stdout for success and bounded JSON
  errors on stderr for failure.
- [ ] Do not include `bruce-release-tool` in `install(TARGETS ...)` or the TGZ.
- [ ] Run the integration suite under a scrubbed environment and scan captured
  output for fixture seed fragments and absolute temporary paths.
- [ ] Reviewer checks TOCTOU resistance, exclusive creation, cleanup scope,
  crash-partial publication, secret zeroization, and independent trust-anchor
  enforcement. Producing the first real pinned key remains a separate key
  ceremony, not a test-fixture substitution.

**Commit:** `feat: add offline core release signing tool`

### Task 10: Verify the exact tested TGZ through sign and tamper workflows

**Files:**
- Modify: `tests/integration/test_release_tool.py`
- Modify: `scripts/verify_install.py` only if it cannot hand the exact tested
  archive to the next local step without regeneration
- Modify: `CMakeLists.txt`

- [ ] Add an integration workflow that builds/receives one CPack TGZ, invokes
  `verify_install.py --package --archive-out --report`, hashes those exact
  bytes, signs them using that same audit report, and verifies in a fresh
  directory against a separately located pinned fixture key.
- [ ] Prove verification fails independently for one-byte archive mutation,
  renamed archive, changed checksum, changed audit version, attacker-generated
  replacement key/set, wrong pinned public key, and changed signature.
- [ ] Prove verification of the original remains successful after each
  isolated negative case.
- [ ] Confirm a second signing attempt cannot overwrite the release directory
  and a crash before final directory rename never yields a verifiable set.
- [ ] Reviewer checks the archive is never regenerated between install audit
  and signing and CI still has no signing step.

**Commit:** `test: bind release signature to audited archive`

---

## Wave 4 — Transactional Mod trust generations

### Task 11: Define trust-generation contracts and pure prospective checks

**Files:**
- Create: `include/bruce/mod/runtime_trust_store_v2.hpp`
- Create: `src/mod/runtime_trust_store_v2.cpp`
- Create: `tests/unit/runtime_trust_store_v2_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `RuntimeTrustGenerationV2` with 20-digit generation, exact V1 trust bytes,
  trust SHA-256, and canonical metadata.
- `RuntimeTrustStateV2` with optional prepared, active, and previous
  generation/digest pairs.
- Pure functions to parse/render metadata/state and calculate which active
  installed Mods lose trust under a candidate generation.

- [ ] Test exact V1 truststore preservation, generation 1 import model,
  canonical generation width/order, digest binding, duplicate/missing/unknown
  JSON fields, oversized input, invalid key sets, and state-reference
  consistency.
- [ ] Define timestamps as canonical UTC RFC-3339 supplied through an explicit
  Clock dependency. Unit tests inject a fixed clock; optional comments are
  explicit inputs and are excluded from time-independent identity comparisons.
- [ ] Build synthetic active Data/Logic records signed by retained, removed,
  and newly added keys; require deterministic affected-Mod ordering.
- [ ] Run RED because the V2 contract does not exist.
- [ ] Implement only pure parsing/rendering/diff logic first; do not touch a
  Mod root in these functions.
- [ ] Run the focused tests twice and byte-compare outputs.
- [ ] Reviewer confirms no package signature format, `signing_key_id`, or V1
  truststore serialization changed.

**Commit:** `feat: define mod trust generations v2`

### Task 12: Persist prepare, verify, activate, and rollback with recovery

**Files:**
- Modify: `include/bruce/mod/runtime_trust_store_v2.hpp`
- Modify: `src/mod/runtime_trust_store_v2.cpp`
- Modify: `tests/unit/runtime_trust_store_v2_tests.cpp`
- Modify as narrowly needed: `src/mod/runtime_registry.cpp`
- Modify as narrowly needed: `src/mod/logic_mod_registry_v1.cpp`

**On-disk contract:**

```text
trust/generations/<20-digit-generation>/truststore.pub
trust/generations/<20-digit-generation>/metadata.json
trust/state.json
trust/journal.v1
```

- [ ] Add RED tests for `open_existing`, explicit legacy import, prepare,
  idempotent verify, activate, rollback, immutable generation collisions,
  missing generations, journal/hash corruption, symlink/FIFO/hardlink/path
  attacks, ownership/mode errors, and concurrent writers.
- [ ] Add fault injection tests at all five spec boundaries: after generation
  write, after journal prepare, after snapshot, after commit, and during
  emergency activation-state changes.
- [ ] Reuse the existing `registry.lock` and private-root discipline; do not
  create a second lock domain.
- [ ] Make `trust/journal.v1` the root coordinator authority. Each Prepare
  record binds old/new trust digests and exact old/new Data and Logic
  snapshots. Its fsync-confirmed Commit record is the sole linearization
  point; projection into both registry journals/snapshots, `state.json`, and
  root `truststore.pub` occurs idempotently afterward under the same lock.
- [ ] Recovery ignores an uncommitted Prepare and completes every projection
  for a committed transaction before serving any Mod operation. Thus callers
  observe either the complete old state or complete new state.
- [ ] `import-v1` reads only the root's already bound `truststore.pub` and
  creates generation 1; it refuses any pre-existing V2 state and never accepts
  a replacement truststore argument.
- [ ] During explicit import, first persist a coordinator Prepare containing
  the exact original trust bytes. Then atomically replace root
  `truststore.pub` with one exact V1-invalid migration sentinel and fsync the
  root. This is the rootwide old-binary fence and makes the import irreversible
  from that point.
- [ ] Only after the rootwide fence, append durable V2 migration barriers to
  both existing registry journals and commit the coordinator transaction.
  New parsers recognize sentinel/barriers and complete recovery; old Data and
  Logic readers fail closed. After complete projection, restore the active
  canonical V1 bytes to root `truststore.pub`; journal barriers continue to
  reject old binary parsers.
- [ ] Test real old-reader fixtures after the fence, after the Data barrier,
  after the Logic barrier, after coordinator commit, and after final
  projection. Neither old registry type may open or mutate the root at any
  post-fence crash boundary.
- [ ] `prepare` reads an external safe V1 truststore, creates the next
  immutable generation, and does not alter active trust.
- [ ] `verify` opens read-only and validates journal, state, generation
  digests, and every installed package's structural integrity. Report active
  and prepared trust compatibility separately: inactive historical packages
  signed by removed keys are findings, not structural failures; only active
  incompatibility blocks a default transition.
- [ ] Keep the root-level `truststore.pub` as the compatibility projection of
  the active generation. A trust-journal transition binds its old and new
  digests and recovery restores the projection consistently; existing Data
  and Logic registries must never observe a different trust value from the
  active generation passed to them.
- [ ] Run all V2 tests plus Data and Logic registry recovery/concurrency suites.
- [ ] Reviewer traces every fsync/rename/journal ordering boundary and checks
  no partial state can authorize an unintended key.

**Commit:** `feat: persist transactional mod trust generations`

### Task 13: Atomically protect active Mods during trust changes

**Files:**
- Modify: `include/bruce/mod/installed_mod_host_v2.hpp`
- Modify: `src/mod/installed_mod_host_v2.cpp`
- Modify: `tests/unit/installed_mod_host_v2_tests.cpp`
- Modify: `tests/unit/runtime_trust_store_v2_tests.cpp`

**Interfaces:**
- Host operations for import, prepare, verify, activate, and rollback.
- Activation result lists deactivated `(mod_id, version, signing_key_id)` in
  canonical order without local paths.
- Default transition rejects when any active Mod loses trust.
- Emergency transition requires `deactivate_untrusted=true` and commits all
  affected deactivations with the trust switch under one lock/journaled unit.

- [ ] Add RED tests spanning active Data and Logic Mods, multiple versions,
  retained/removed keys, inactive untrusted packages, and new-key packages.
- [ ] Default activation and rollback must leave every byte/state unchanged
  when an active Mod would lose trust.
- [ ] Emergency activation/rollback must deactivate exactly the affected
  active versions, preserve installed package bytes and older versions, and
  switch trust atomically.
- [ ] Add recovery tests for interruption before and after each combined
  commit boundary.
- [ ] Verify fresh execution/binding uses active trust; do not terminate or
  rewrite already running processes.
- [ ] Require every install, update, activation, binding, route, and execution
  operation to resolve active trust under `registry.lock`; cached `trust_` or
  a caller-supplied stale Legacy truststore cannot override V2 state.
- [ ] Run installed-host, Data registry, Logic registry, route, and Mod CLI
  regressions.
- [ ] Reviewer confirms there is one lock order, no cross-registry deadlock,
  and prospective checks use exact verified package bytes rather than cached
  filenames.

**Commit:** `feat: rotate mod trust with safe deactivation`

### Task 14: Expose the `mods trust` CLI lifecycle

**Files:**
- Modify: `apps/runtime_mod_cli.hpp`
- Modify: `apps/bruce_cli.cpp`
- Modify: `tests/unit/runtime_mod_cli_tests.cpp`
- Modify: `tests/integration/test_runtime_mods.py`
- Modify: `CMakeLists.txt`

**Interfaces:**

```text
bruce mods trust status --mod-root ROOT
bruce mods trust import-v1 --mod-root ROOT
bruce mods trust prepare NEXT_TRUST.pub --mod-root ROOT
bruce mods trust verify --mod-root ROOT
bruce mods trust activate --mod-root ROOT [--deactivate-untrusted]
bruce mods trust rollback --mod-root ROOT [--deactivate-untrusted]
```

- [ ] Add parser tests for help, every valid command, smallest valid root,
  missing/duplicate/unexpected arguments, missing files, wrong types,
  traversal/symlink/FIFO inputs, contradictory emergency flags, and read-only
  status behavior.
- [ ] Add real-binary workflows for legacy import, prepare without activation,
  verify, safe activation, blocked removal, emergency deactivation, rollback,
  restart recovery, and canonical JSON fields.
- [ ] Extend existing `mods install`, `mods update`, `mods activate`, route,
  and Logic-run tests on a V2 root: stale external trust is rejected, packages
  signed only by removed keys cannot install/activate, and packages signed by
  the newly active key can install/update. A Legacy `--trust-store` argument
  may be checked for equality but can never replace active V2 trust.
- [ ] Run RED because `mods trust` is absent.
- [ ] Wire the CLI directly to installed-host operations; do not reimplement
  persistence or package verification in the parser.
- [ ] Emit only canonical result JSON on stdout. Map stable public error codes
  to stderr/exit 2 and redact internal paths, journal content, and key bytes.
- [ ] Ensure `status` does not create a root, lock, generation, or snapshot.
- [ ] Run runtime Mod unit/integration suites and check filesystem snapshots on
  every failed command.
- [ ] Reviewer compares help, parser, implementation, errors, and contract
  fields one-to-one.

**Commit:** `feat: add local mod trust rotation commands`

---

## Wave 5 — Documentation and final evidence

### Task 15: Update source documentation and generated CLI artifacts

**Files:**
- Modify: `docs/cli/README.md`
- Modify: `docs/contracts/README.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/implementation/150-release-signing-contract.md`
- Modify as needed: `README.md`, `DEPENDENCIES.md`, `SECURITY.md`
- Generate only through verifier:
  `docs/cli/source-index.md`, `docs/cli/examples.json`,
  `docs/cli/validation.md`

- [ ] Document the implemented/non-implemented boundary, local-user model,
  offline release sequence, key-file protections, trust generations,
  explicit V1 import, safe default, emergency flag, rollback, and running-
  process boundary.
- [ ] Remove the stale Apache-2.0 sentence from the release-signing page and
  describe only the actual repository license without adding legal analysis.
- [ ] Add all new CLI examples and error cases to the generator's source
  examples; do not edit generated output directly.
- [ ] In a complete temporary copy, run `docs/cli/verify_examples.py` against
  the final binary, then byte-compare all three generated artifacts.
- [ ] Run the local Markdown link checker and the same offline link scope as
  CI.
- [ ] Search docs for old claims that CI signs releases, trust changes are
  impossible, the matrix is absent, or all worker fixtures are sanitized.
- [ ] Reviewer matches every security statement to code/tests and labels any
  remaining future work **DOKUMENTIERT / KONZEPT** or **OFFENE ENTSCHEIDUNG**.

**Commit:** `docs: describe release signing and trust rotation`

### Task 16: Run the fresh acceptance matrix and independent final review

**Files:**
- Create outside repository: `/tmp/bruce-hardening-*/acceptance/`
- Create outside repository: command log, redacted environment summary,
  compiler/dependency identities, CTest logs, package/signature reports,
  stress report, docs comparison, final manifest comparison
- Modify repository files only to fix findings through a new RED/GREEN task;
  never patch opportunistically during acceptance

- [ ] Create a fresh complete native source copy under a no-space/no-colon
  path and copy the required host-dependency runtime.
- [ ] Configure/build Release with tests, run the explicit Mod matrix, then run
  the remaining complete CTest suite. Record counts, skips, failures, and
  reruns separately.
- [ ] Configure/build Debug with tests and run its complete CTest suite,
  including the Debug-aware matrix.
- [ ] Configure/build scoped ASan/UBSan and run its declared compatible suite
  with fail-fast sanitizer options.
- [ ] Configure/build the separate unsanitized static-fixture profile and run
  all labeled static/sandbox tests.
- [ ] Repeat the cancel-boundary stress workload at the approved concurrency
  and iteration count.
- [ ] Run CPack through
  `verify_install.py --package --archive-out --report`, sign the
  exact audited TGZ using the emitted audit report and a temporary fixture
  key, verify it against a separately pinned fixture public key, and run every
  tamper case.
- [ ] Run complete Trust V2 unit/integration/recovery/crash/concurrency suites
  twice and byte-compare deterministic reports.
- [ ] Run the CLI documentation generator in a temporary full copy, bytewise
  comparisons, local Markdown links, `git diff --check`, and final status.
- [ ] If Git remains unavailable, compare against the initial source manifest
  and enumerate all intended changes without claiming Git cleanliness.
- [ ] Run an independent final code/contract/security review. Any actionable
  finding returns to a focused RED test and reviewed fix before this task can
  complete.
- [ ] Classify every failure as product, test, documentation, environment/
  sandbox, missing coverage, unimplemented concept, or open decision.

**Final success condition:** Only the tested portions may be labeled
**IMPLEMENTIERT UND BELEGT**. Any skipped profile, unresolved Git provenance,
sanitizer exclusion, socket restriction, or failed stress iteration remains an
explicit limitation; never summarize the whole system as fully functional
unless every relevant gate above is green.

**Commit:** `test: complete release hardening acceptance` only for intentional
test/report sources that belong in the repository. Runtime logs and temporary
evidence stay outside it.

## Planned commit sequence when Git is proven healthy

1. `fix: close single-worker cancel acceptance race`
2. `test: make cancel acceptance regression deterministic`
3. `ci: expose and bound built-in mod matrix`
4. `build: add scoped address and undefined sanitizers`
5. `ci: verify debug sanitizers and release package`
6. `feat: add canonical release signature v1`
7. `feat: add offline core release signing tool`
8. `test: bind release signature to audited archive`
9. `feat: define mod trust generations v2`
10. `feat: persist transactional mod trust generations`
11. `feat: rotate mod trust with safe deactivation`
12. `feat: add local mod trust rotation commands`
13. `docs: describe release signing and trust rotation`

Commits are not an excuse to cross the Git gate: if provenance is unresolved,
the implementation stops after Task 1 and this sequence remains
**DOKUMENTIERT / KONZEPT**.
