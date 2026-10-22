# Phase-DAG V2 Cockpit Inspector Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an optional local `bruce cockpit` static mod that validates and renders saved NanoCell-backed Phase-DAG-V2 bundles without executing new work or retaining submitted data.

**Architecture:** A narrow cockpit library turns only a successfully replayed `PhaseDagRunV2` into a JSON presentation model. A loopback-only HTTP adapter serves an embedded interface and routes bounded binary uploads to that library. The existing historical Cockpit asset and orphaned tests remain unbuilt reference material; the new mod uses isolated `cockpit_inspector_*` files.

**Tech Stack:** C++20, CMake 3.20+, CTest, Python 3 integration tests, OpenSSL Crypto (existing), GMP/GMPXX (existing), Threads, vendored `cpp-httplib` v0.58.0.

**Spec:** `docs/superpowers/specs/2026-10-12-phase-dag-v2-cockpit-inspector-design.md`

## Global Constraints

- Keep C++20 and the existing `-Wall -Wextra -Werror` target policy.
- Build the Cockpit only as an optional static mod via `BRUCE_BUILD_MOD_COCKPIT`; require `BRUCE_BUILD_MOD_NANOCELL=ON` whenever it is enabled.
- Bind only the numeric IPv4 loopback address `127.0.0.1`; never offer a host option or nonlocal fallback.
- Accept only `GET /`, `GET /api/v1/health`, and `POST /api/v1/inspect`; do not add process, job, storage, import, Redis, file-browsing, or DAG-execution routes.
- Cap one inspect request at `1 MiB` before application-level processing; do not write uploaded bytes to disk.
- Derive `VALID` data only after `replay_phase_dag_run_v2(bytes, phase_dag_nanocell_catalog_v2())` succeeds. Map all expected bundle/replay failures to `REJECTED`; reserve `ERROR` for service failures.
- Render every bundle-derived string as text, never as HTML, JavaScript, or a URL.
- Vendor `cpp-httplib` v0.58.0 from the official upstream tag, preserve its MIT license, record source URL, release commit, and computed SHA-256, and make CMake/network-free after vendoring.
- Preserve `mods/cockpit/src/cockpit_assets.cpp`, `tests/mods/cockpit/cockpit_tests.cpp`, and `tests/mods/cockpit/test_cockpit.py` as unbuilt historical fragments; do not rename, overwrite, or register them.
- Run `docs/cli/verify_examples.py` only in a complete temporary native copy. The supplied checkout lacks usable Git metadata, so record validation output instead of planning commits or Git-status claims.

## Review Focus

1. A request larger than 1 MiB must receive a controlled HTTP rejection before V2 replay; Task 3 adds this black-box test.
2. A syntactically plausible but modified V2 bundle must produce `REJECTED`, no persisted file, and no `VALID` fields; Task 2 covers the service result and Task 3 covers the route.
3. Binding a port already held on `127.0.0.1` must fail rather than binding another address or port; Task 3 covers the process behavior.
4. A build with NanoCell disabled and Cockpit enabled must fail CMake configuration with a clear dependency error; Task 1 covers the configuration matrix.
5. A bundle string containing HTML/script-like text must remain literal UI text; Task 3 asserts the asset uses `textContent` and response JSON remains escaped.

---

## File Structure

| Path | Responsibility |
| --- | --- |
| `third_party/cpp-httplib/httplib.h` | Unmodified official `cpp-httplib` v0.58.0 single-header dependency. |
| `third_party/cpp-httplib/LICENSE` | Upstream MIT license accompanying the vendored header. |
| `include/bruce/mods/cockpit/mod.hpp` | Public manifest and `cockpit_cli` declaration only. |
| `mods/cockpit/CMakeLists.txt` | `bruce_mod_cockpit` source list, private vendor include path, links and warnings. |
| `mods/cockpit/src/mod.cpp` | Cockpit manifest, strict `--port` parsing and server handoff. |
| `mods/cockpit/src/cockpit_inspector.hpp/.cpp` | Private, deterministic V2 replay-to-presentation service. |
| `mods/cockpit/src/cockpit_inspector_assets.hpp/.cpp` | Embedded static HTML/CSS/JavaScript for the read-only inspector. |
| `mods/cockpit/src/cockpit_inspector_server.hpp/.cpp` | Loopback-only `cpp-httplib` routes and HTTP status mapping. |
| `tests/mods/cockpit/cockpit_inspector_tests.cpp` | Direct C++ contract tests plus a deterministic V2 fixture writer. |
| `tests/mods/cockpit/test_cockpit_inspector.py` | Real-binary HTTP and CLI integration tests. |
| `CMakeLists.txt` | Feature switch, dependency guard, targets, static catalog links, CTest registration. |
| `src/mod/catalog.cpp`, `apps/bruce_cli.cpp` | Static manifest catalog and guarded command dispatch/help. |
| `DEPENDENCIES.md`, `THIRD_PARTY_NOTICES.md` | Exact vendored dependency provenance, hash and license. |
| `README.md`, `docs/architecture/03-blueprint-consolidation.md`, `docs/cli/README.md`, `docs/cli/commands.md` | Current feature status, user command and security limit documentation after validation. |

### Task 1: Create the optional static Cockpit boundary and vendored HTTP dependency

**Files:**
- Create: `third_party/cpp-httplib/httplib.h`, `third_party/cpp-httplib/LICENSE`, `include/bruce/mods/cockpit/mod.hpp`, `mods/cockpit/CMakeLists.txt`, `mods/cockpit/src/mod.cpp`
- Modify: `CMakeLists.txt:8-105,123-140,141-230`, `src/mod/catalog.cpp:1-48`, `apps/bruce_cli.cpp:37-46,480-490,610-620`, `DEPENDENCIES.md`, `THIRD_PARTY_NOTICES.md`
- Test: `tests/mods/cockpit/cockpit_inspector_tests.cpp`, `tests/mods/cockpit/test_cockpit_inspector.py`

**Interfaces:**
- Produces `const ModManifestV1& cockpit_mod_manifest_v1()` with ID `cockpit`, version `cockpit-mod-v1`, API `bruce-mod-api-v1`, and exactly `ModCapabilityV1::presentation`.
- Produces `int cockpit_cli(int argc, char** argv)`, accepting only `bruce cockpit` and `bruce cockpit --port N`, with `N` in `1..65535`.
- Produces CMake option `BRUCE_BUILD_MOD_COCKPIT` defaulting to `ON`, the static `bruce_mod_cockpit` target, and CTest registration conditional on both Cockpit and NanoCell.

- [ ] **Step 1: Write failing activation and command-dispatch tests in the new test files**

In `cockpit_inspector_tests.cpp`, assert exact manifest identity and one `presentation` capability. In `test_cockpit_inspector.py`, configure one temporary build with Cockpit disabled and assert `bruce cockpit` reports `UnknownCommand: cockpit`; configure another with Cockpit enabled/NanoCell disabled and assert configuration fails with `CockpitRequiresNanoCell`.

- [ ] **Step 2: Run the focused configuration tests and verify the expected failures**

Run from a complete temporary native copy:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-cockpit-off -DBUILD_TESTING=ON -DBRUCE_BUILD_MOD_COCKPIT=OFF
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-cockpit-invalid -DBRUCE_BUILD_MOD_COCKPIT=ON -DBRUCE_BUILD_MOD_NANOCELL=OFF
```

Expected: the off build has no Cockpit command; the invalid combination does not yet fail with the required explicit CMake diagnostic.

- [ ] **Step 3: Vendor and document `cpp-httplib` v0.58.0**

Retrieve only `httplib.h` and `LICENSE` from the official upstream `v0.58.0` tag into a temporary directory, compare each source file with the repository copy, retain headers unchanged, and calculate SHA-256 locally. Update `DEPENDENCIES.md` and `THIRD_PARTY_NOTICES.md` from their stale, nonexistent v0.59.0 claim to the actual tag, release commit, official source URL, license and calculated hashes. Add the two files to the CMake source-file index inputs without adding any fetch or package action to CMake.

- [ ] **Step 4: Implement the static mod boundary**

Add `BRUCE_BUILD_MOD_COCKPIT`; if it is true while `BRUCE_BUILD_MOD_NANOCELL` is false, stop configuration with `CockpitRequiresNanoCell`. Add `mods/cockpit` only when Cockpit is true. `bruce_mod_cockpit` links `bruce_mod_runtime`, `bruce_phase_dag_nanocell`, `Threads::Threads`, and includes `third_party/cpp-httplib` privately. Add guarded compile definitions, static catalog inclusion in canonical order `cockpit`, `language`, `nanocell`, and direct `bruce_mod_cockpit` linkage for CLI dispatch. The temporary `cockpit_cli` implementation accepts only help/validated ports and delegates to the Task-3 server factory.

- [ ] **Step 5: Run boundary tests and existing mod regressions**

Run from a complete temporary native copy:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-cockpit-boundary -DBUILD_TESTING=ON
scripts/with-host-deps.sh cmake --build /tmp/bruce-cockpit-boundary
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-cockpit-boundary -R '^(cockpit_inspector_contracts|nanocell_contracts|nanocell_mod_contracts|phase_dag_nanocell_contracts)$' --output-on-failure
```

Expected: Cockpit manifest/CLI tests pass and every existing NanoCell contract remains green.

### Task 2: Implement deterministic Phase-DAG-V2 inspection and presentation encoding

**Files:**
- Create: `mods/cockpit/src/cockpit_inspector.hpp`, `mods/cockpit/src/cockpit_inspector.cpp`
- Modify: `mods/cockpit/CMakeLists.txt`, `tests/mods/cockpit/cockpit_inspector_tests.cpp`
- Test: `tests/mods/cockpit/cockpit_inspector_tests.cpp`

**Interfaces:**
- Produces `inline constexpr std::size_t cockpit_max_inspect_bytes_v1 = 1024U * 1024U`.
- Produces `enum class CockpitInspectionStatusV1 { valid, rejected };`.
- Produces `struct CockpitInspectionResultV1 { CockpitInspectionStatusV1 status; std::string code; std::string json; };`.
- Produces `CockpitInspectionResultV1 inspect_phase_dag_v2_bundle(std::string_view bytes);` and `std::string cockpit_inspection_status_name(CockpitInspectionStatusV1 status);`.

- [ ] **Step 1: Write failing direct inspection tests**

Extend `cockpit_inspector_tests.cpp` with a helper that builds the existing deterministic one-node NanoCell V2 request, executes it with `phase_dag_nanocell_catalog_v2()`, and packs it. Assert `inspect_phase_dag_v2_bundle` returns `valid`, code `VALID`, and JSON fields for `dag_version`, `dag_id`, node ID, adapter ID/version, request/output/evidence digests, receipt outcome and trace. Assert a single-byte mutation, an oversize byte string, and a non-V2 header return `rejected`, never reproduce input bytes in JSON, and expose no filesystem paths.

- [ ] **Step 2: Run the contract executable and verify it fails**

Run:

```sh
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-cockpit-boundary -R '^cockpit_inspector_contracts$' --output-on-failure
```

Expected: the target is absent or fails because the V2 inspector interface does not exist.

- [ ] **Step 3: Implement replay-gated presentation encoding**

Implement the byte limit before replay. Catch the repository's expected `Error` bundle/replay failures and return `{rejected, "REJECTED", ...}` without forwarding raw exception text or submitted bytes. For success, call exactly `replay_phase_dag_run_v2(bytes, phase_dag_nanocell_catalog_v2())`, then build canonical JSON only from the returned run. Render enum values through exhaustive switches. Use the existing JSON string escaping helper for every dynamically sourced value. Add a test-only fixture writer in this executable that emits the same canonical valid bundle to a path supplied by `--write-valid-bundle`; it is only for Task-3 black-box tests.

- [ ] **Step 4: Run focused service and V2 regression tests**

Run:

```sh
scripts/with-host-deps.sh cmake --build /tmp/bruce-cockpit-boundary
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-cockpit-boundary -R '^(cockpit_inspector_contracts|phase_dag_v2_contracts|phase_dag_nanocell_contracts)$' --output-on-failure
```

Expected: the new direct inspection tests and all existing V2 replay contracts pass.

### Task 3: Add the loopback HTTP adapter and text-safe read-only interface

**Files:**
- Create: `mods/cockpit/src/cockpit_inspector_assets.hpp`, `mods/cockpit/src/cockpit_inspector_assets.cpp`, `mods/cockpit/src/cockpit_inspector_server.hpp`, `mods/cockpit/src/cockpit_inspector_server.cpp`
- Modify: `mods/cockpit/src/mod.cpp`, `mods/cockpit/CMakeLists.txt`, `tests/mods/cockpit/cockpit_inspector_tests.cpp`, `tests/mods/cockpit/test_cockpit_inspector.py`
- Test: `tests/mods/cockpit/test_cockpit_inspector.py`

**Interfaces:**
- Produces `const std::string& cockpit_inspector_html_v1();`.
- Produces `int run_cockpit_inspector_server(std::uint16_t port);`.
- The server accepts only `GET /`, `GET /api/v1/health`, and `POST /api/v1/inspect`; it binds exactly `127.0.0.1`.

- [ ] **Step 1: Write failing HTTP and UI tests**

In `test_cockpit_inspector.py`, start the actual `bruce cockpit --port N` process, poll only `http://127.0.0.1:N/api/v1/health`, and use the Task-2 fixture writer for a valid request. Assert: root returns the embedded UI; health returns JSON; valid raw bytes return HTTP 200 with `VALID`; a one-byte mutation returns HTTP 422 with `REJECTED`; an over-1-MiB body receives a controlled non-2xx response; `POST /api/v1/jobs`, `GET /api/v1/imports`, and arbitrary paths return 404. Hold a local listening socket before launch and assert the Cockpit process exits nonzero without a successful health response. In the C++ asset test, assert the UI contains only the three allowed routes, uses `textContent` for returned fields, and has no `innerHTML`, job, import, process or file-path API markers.

- [ ] **Step 2: Run the integration test and verify it fails**

Run:

```sh
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-cockpit-boundary -R '^cockpit_inspector_workflows$' --output-on-failure
```

Expected: failure because no local server, routes, or UI exist.

- [ ] **Step 3: Implement the bounded local server and embedded UI**

Use `cpp-httplib` only as the transport. Configure its payload limit to `cockpit_max_inspect_bytes_v1`, register exactly the three routes, and bind using `bind_to_port("127.0.0.1", port)` before serving. Return 413 for oversized payloads, 422 for inspection rejection, 500 only for unexpected service failures, and JSON with `Content-Type: application/json` for API routes. Do not enable TLS, CORS, directory serving, static-file lookup, redirects, wildcards, regex routes, or a host option. The embedded UI uploads only the selected file bytes to `/api/v1/inspect`, handles result/reset state locally, and renders every response field through `textContent`.

- [ ] **Step 4: Run local server and inspection tests**

Run:

```sh
scripts/with-host-deps.sh cmake --build /tmp/bruce-cockpit-boundary
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-cockpit-boundary -R '^(cockpit_inspector_contracts|cockpit_inspector_workflows|phase_dag_v2_contracts|phase_dag_nanocell_contracts)$' --output-on-failure
```

Expected: all selected Cockpit and V2 tests pass; no unsupported historical Cockpit routes respond.

### Task 4: Document the implemented limit and complete repository verification

**Files:**
- Modify: `README.md`, `docs/architecture/03-blueprint-consolidation.md`, `docs/cli/README.md`, `docs/cli/commands.md`, `docs/implementation/README.md`, `docs/superpowers/specs/2026-10-12-phase-dag-v2-cockpit-inspector-design.md`
- Test: generated CLI artifacts through `docs/cli/verify_examples.py`; complete CTest suite

**Interfaces:**
- Documents `bruce cockpit [--port N]`, fixed loopback binding, 1-MiB in-memory inspection limit, V2/NanoCell-only replay boundary, and explicit non-goals.
- Records the Cockpit as **IMPLEMENTIERT UND BELEGT** only after the Task-4 validation results exist; otherwise retains **VORSCHLAG**.

- [ ] **Step 1: Write failing documentation assertions**

Add precise checks to `test_cockpit_inspector.py` for `bruce --help` usage, fixed loopback wording, V2/NanoCell inspection-only scope, and absence of job/import/execution claims. Add any generated CLI example expected by `verify_examples.py` through its source inputs rather than editing generated artifacts.

- [ ] **Step 2: Run the documentation checks and verify they fail**

Run from a complete temporary native source copy:

```sh
python3 docs/cli/verify_examples.py --check
```

Expected: the new user-visible command and its limits are not yet consistently documented.

- [ ] **Step 3: Update only validated documentation**

Replace the current broad statement that no buildable Cockpit can be derived with the narrower, accurate claim: an optional static read-only Phase-DAG-V2 NanoCell inspector exists when `BRUCE_BUILD_MOD_COCKPIT=ON`; the historical job/import/language Cockpit remains absent. State the loopback, 1-MiB, no-persistence and no-execution boundaries. Update the design status and implementation index only after the tests have produced the stated evidence. Regenerate `docs/cli/source-index.md`, `docs/cli/examples.json`, and `docs/cli/validation.md` only through `verify_examples.py` in the temporary copy.

- [ ] **Step 4: Run the prescribed complete validation flow**

Run from a complete temporary native source copy whose path has no colon:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-cockpit-final -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-cockpit-final
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-cockpit-final --output-on-failure
python3 docs/cli/verify_examples.py --check
python3 scripts/check_local_doc_links.py
```

Expected: configuration, build, every available CTest, generated CLI validation, and local Markdown links complete with exit code 0. Record counts and environment limitations exactly; do not infer broader correctness from a green suite.

## Self-Review

- **Spec coverage:** Task 1 creates the optional static boundary and dependency guard. Task 2 provides replay-gated V2 inspection and response separation. Task 3 provides only the approved loopback routes and text-safe UI. Task 4 documents the true scope and performs every prescribed validation. No spec requirement is unassigned.
- **Step scan:** Each task begins with an observable failure, introduces a named interface, and ends with a bounded verification command. The plan intentionally has no commit steps because this checkout has no usable Git metadata.
- **Type consistency:** Task 2 owns `inspect_phase_dag_v2_bundle` and the 1-MiB constant; Task 3 consumes both. Task 1 owns `cockpit_cli`; Task 3 owns the delegated `run_cockpit_inspector_server`. Manifest names align with the existing Language/NanoCell conventions.
- **Review focus:** Oversize data is Task 3, mutation rejection is Tasks 2 and 3, port conflict is Task 3, CMake dependency combinations are Task 1, and text-safe rendering is Task 3.
- **Proportion:** Four independent, reviewable deliverables follow the existing static-mod, CTest, and documentation patterns without reproducing source implementations.
