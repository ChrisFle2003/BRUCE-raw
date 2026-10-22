# Cockpit-to-MCP Rename Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rename the active local Cockpit inspector module and its CLI command to MCP without changing its HTTP-inspector behavior.

**Architecture:** Move only the active inspector implementation into `mods/mcp` and expose it through `bruce mcp`; rename its CMake switch, manifest, C++ names, test target and CLI-facing error names in one coherent boundary change. Keep the separate Stdio adapter `tools/bruce_mcp.py` and unbuilt historical Cockpit fragments untouched, while current documentation explicitly distinguishes the two MCP-labelled components.

**Tech Stack:** C++20, CMake, Ninja, CTest, Python 3, cpp-httplib.

**Spec:** `docs/superpowers/specs/2026-10-18-cockpit-to-mcp-rename-design.md`

## Global Constraints

- Preserve all inspector HTTP routes, response bodies, loopback-only binding, port range and read-only behavior exactly.
- Replace `bruce cockpit` with `bruce mcp`; do not retain a CLI compatibility alias.
- Rename `BRUCE_BUILD_MOD_COCKPIT`, `bruce_mod_cockpit`, public Cockpit inspector C++ names and active source paths to their MCP equivalents.
- Keep `tools/bruce_mcp.py` functionally unchanged and describe it as the separate Stdio MCP adapter.
- Preserve historical documentation, `runs/`, third-party records, and these unbuilt fragments unchanged: `mods/cockpit/src/cockpit_assets.cpp`, `tests/mods/cockpit/cockpit_tests.cpp`, `tests/mods/cockpit/test_cockpit.py`, `tests/mods/cockpit/test_language_management.py`.
- Do not manually edit `docs/cli/source-index.md`, `docs/cli/examples.json` or `docs/cli/validation.md`; regenerate them only through `docs/cli/verify_examples.py` in a temporary copy.
- Retain `build-host-deps/`; it is the dependency runtime required by `scripts/with-host-deps.sh`, not a BRUCE build output.
- Delete only `build/` and the two analysis-created `/tmp/bruce-analysis-gm0oAd` and `/tmp/bruce-prebuilt-n5yzkM` directories as cleanup targets.
- The repository has no usable Git worktree metadata; do not attempt commits until that condition changes.

## Review Focus

- A standard build must accept `-DBRUCE_BUILD_MOD_MCP=OFF` and omit `mcp` from help while every other command remains available; Task 1 owns this build-matrix test.
- `bruce mcp --port 0` must retain rejection semantics under the renamed public error code `InvalidMcpPort`; Task 1 owns this assertion.
- `bruce cockpit` must become an unknown command rather than starting the inspector; Task 1 owns this assertion.
- Every existing inspector route must still provide the same method/status/body contract after source relocation; Task 1 owns the existing Python HTTP workflow suite.
- The Stdio adapter must remain discoverable without being confused with the browser inspector; Task 3 owns the documentation wording and the existing `bruce_mcp_workflows` test.

---

### Task 1: Establish the MCP inspector code and public CLI contract

**Files:**
- Create: `include/bruce/mods/mcp/mod.hpp`
- Create: `mods/mcp/CMakeLists.txt`
- Create: `mods/mcp/src/mod.cpp`
- Create: `mods/mcp/src/mcp_inspector.cpp`
- Create: `mods/mcp/src/mcp_inspector.hpp`
- Create: `mods/mcp/src/mcp_inspector_server.cpp`
- Create: `mods/mcp/src/mcp_inspector_server.hpp`
- Create: `mods/mcp/src/mcp_inspector_assets.cpp`
- Create: `mods/mcp/src/mcp_inspector_assets.hpp`
- Create: `tests/mods/mcp/mcp_inspector_tests.cpp`
- Create: `tests/mods/mcp/test_mcp_inspector.py`
- Delete: active files `include/bruce/mods/cockpit/mod.hpp`, `mods/cockpit/CMakeLists.txt`, `mods/cockpit/src/mod.cpp`, `mods/cockpit/src/cockpit_inspector.cpp`, `mods/cockpit/src/cockpit_inspector.hpp`, `mods/cockpit/src/cockpit_inspector_server.cpp`, `mods/cockpit/src/cockpit_inspector_server.hpp`, `mods/cockpit/src/cockpit_inspector_assets.cpp`, `mods/cockpit/src/cockpit_inspector_assets.hpp`, `tests/mods/cockpit/cockpit_inspector_tests.cpp`, `tests/mods/cockpit/test_cockpit_inspector.py`
- Modify: `CMakeLists.txt:10-15, 141-165, 175-177, 207-211, 538-542, 657-660, 963-968`
- Modify: `apps/bruce_cli.cpp:70-72, 600-602, 752-754`
- Modify: `src/mod/catalog.cpp:3-5, 31-33`
- Modify: `tests/unit/mod_catalog_tests.cpp:2-4, 40-43`
- Modify: `tests/integration/test_mod_build.py:20-22, 49-52`

**Interfaces:**
- Consumes: the existing `cockpit_*` active implementation and its C++/HTTP contract.
- Produces: `bruce::mcp_mod_manifest_v1()`, `int bruce::mcp_cli(int argc, char** argv)`, `bruce_mod_mcp`, `BRUCE_BUILD_MOD_MCP`, and the `bruce mcp [--port 1..65535]` command.

- [ ] **Step 1: Change the active test expectations to the new public names before changing the implementation.**

  Move the registered inspector C++ and Python tests to `tests/mods/mcp/`; replace active inspector type/function/file references with `Mcp*`/`mcp_*`, start the binary with `mcp`, expect `InvalidMcpArguments` and `InvalidMcpPort`, and add an assertion that `cockpit` returns `UnknownCommand: cockpit`.

- [ ] **Step 2: Run the renamed Python inspector workflow to verify it fails against the still-Cockpit binary.**

  Run: `python3 tests/mods/mcp/test_mcp_inspector.py build/bruce`

  Expected: FAIL because the current binary does not recognize `mcp`.

- [ ] **Step 3: Move the active implementation and rename its internal boundary.**

  Create `mods/mcp` and `include/bruce/mods/mcp` from the active inspector files only. Rename `cockpit_*` declarations, definitions, namespaces, manifest identity/version (`mcp`, `mcp-mod-v1`), UI titles, CLI error codes, include paths and server entry point to MCP names. Preserve all route strings, content types, status selection, size limits and loopback address byte-for-byte unless a displayed name contains `Cockpit`.

- [ ] **Step 4: Rewire CMake, the CLI dispatcher and built-in catalog.**

  Replace the module option with `BRUCE_BUILD_MOD_MCP`; retain the NanoCell prerequisite but emit `McpRequiresNanoCell`. Register `mods/mcp`, link `bruce_mod_mcp`, build `bruce_mcp_inspector_tests`, register `mcp_inspector_contracts` and `mcp_inspector_workflows`, and pass `BRUCE_BUILD_MOD_MCP` to catalog tests. In `apps/bruce_cli.cpp`, show and dispatch only `mcp`. In `src/mod/catalog.cpp`, include and register `mcp_mod_manifest_v1()` so the sorted catalog contains `mcp`.

- [ ] **Step 5: Configure an isolated verification build, then run focused contracts and workflows.**

  Run: `scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-mcp-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release && scripts/with-host-deps.sh cmake --build /tmp/bruce-mcp-build --target bruce bruce_mcp_inspector_tests && scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-mcp-build --output-on-failure -R 'mcp_inspector_(contracts|workflows)|mod_catalog_contracts|mod_build_language_off'`

  Expected: PASS; the workflow verifies renamed CLI behavior and unchanged HTTP routes, while the build-matrix test verifies `BRUCE_BUILD_MOD_MCP=OFF`.

### Task 2: Regenerate the current CLI evidence and verify the source boundary

**Files:**
- Modify: `docs/cli/verify_examples.py:60-61`
- Generated through the prescribed verifier only: `docs/cli/source-index.md`, `docs/cli/examples.json`, `docs/cli/validation.md`
- Modify: `tests/integration/test_cli_docs.py` only if its expectations name the old command or error codes

**Interfaces:**
- Consumes: the `bruce mcp` CLI and `InvalidMcpPort`/`InvalidMcpArguments` error contract from Task 1.
- Produces: CLI examples and generated reference material that name `mcp` while retaining marked historical `cockpit` source records.

- [ ] **Step 1: Change generator-owned measured examples to the MCP command and new error codes.**

  In `docs/cli/verify_examples.py`, rename labels `cockpit-invalid-port` and `cockpit-extra-arguments` to MCP labels; invoke `mcp` and assert `InvalidMcpPort` and `InvalidMcpArguments`.

- [ ] **Step 2: Run the generator in a full temporary repository copy.**

  Copy the repository to a native no-space temporary path, configure and build
  it there with `scripts/with-host-deps.sh`, then run
  `python3 docs/cli/verify_examples.py /tmp/bruce-mcp-cli-docs/build/bruce`
  from that copy. This follows the project rule that generated CLI files are
  not manually edited.

  Expected: it rewrites only `docs/cli/source-index.md`, `docs/cli/examples.json` and `docs/cli/validation.md` in the temporary copy and completes without an example mismatch.

- [ ] **Step 3: Copy only the generator-produced CLI artifacts back and run their integration test.**

  Run: `python3 tests/integration/test_cli_docs.py /tmp/bruce-mcp-build/bruce`

  Expected: PASS with measured MCP examples. Any retained `cockpit` records must point only to preserved historical fragments and be documented in Task 3.

### Task 3: Update current documentation without rewriting historical evidence

**Files:**
- Modify: `README.md:43-47, 140-167, 292-303`
- Modify: `docs/README.md` where it indexes the active inspector
- Modify: `docs/architecture/01-identity-and-invariants.md`
- Modify: `docs/architecture/02-system-architecture.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/architecture/04-local-security-boundaries.md:41-54`
- Modify: `docs/architecture/README.md`
- Modify: `docs/architecture/repository-structure.md:105-130`
- Modify: `docs/cli/README.md:5-11, 101-110, 159-165, 341 onward`
- Modify: `docs/cli/commands.md` and `docs/cli/formats.md` in their active inspector sections
- Modify: `docs/implementation/README.md` only if it names the active inspector

**Interfaces:**
- Consumes: Task 1's `mcp` command and Task 2's regenerated CLI evidence.
- Produces: current user-facing documentation that calls the browser interface the MCP Inspector and distinguishes it from `tools/bruce_mcp.py`.

- [ ] **Step 1: Update current product documentation and build instructions.**

  Replace active Inspector headings, command examples, CMake option names, module names, error names and CMake prerequisite text with MCP names. State that the MCP Inspector remains a loopback HTTP browser interface and that `tools/bruce_mcp.py` is a separate Stdio MCP adapter.

- [ ] **Step 2: Preserve and label intentional historical exceptions.**

  In `docs/architecture/repository-structure.md`, update the active paths to `mcp_inspector_*` and `tests/mods/mcp/test_mcp_inspector.py`, while retaining the four unbuilt legacy Cockpit fragments as historical. Do not edit dated audit reports, old specifications, old plans, `runs/`, third-party notices or patch artifacts.

- [ ] **Step 3: Check documentation consistency.**

  Run: `python3 scripts/check_local_doc_links.py && grep -RIn --exclude-dir=.git --exclude-dir=runs --exclude-dir=build --exclude-dir=build-host-deps -E 'BRUCE_BUILD_MOD_COCKPIT|bruce cockpit|CockpitRequiresNanoCell' README.md docs apps include mods src tests CMakeLists.txt`

  Expected: Markdown links pass; remaining output is limited to the four preserved historical fragments and explicitly historical documentation/reference records.

- [ ] **Step 4: Confirm the independent Stdio adapter remains valid.**

  Run: `scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-mcp-build --output-on-failure -R '^bruce_mcp_workflows$'`

  Expected: PASS, proving that the pre-existing `tools/bruce_mcp.py` boundary was not broken by the terminology change.

### Task 4: Remove stale build output and perform the clean acceptance build

**Files:**
- Delete: `build/`
- Delete: `/tmp/bruce-analysis-gm0oAd`
- Delete: `/tmp/bruce-prebuilt-n5yzkM`
- Preserve: `build-host-deps/`

**Interfaces:**
- Consumes: the renamed source tree and current documentation from Tasks 1–3.
- Produces: a fresh `build/` configured with the default MCP module and verified test results.

- [ ] **Step 1: Validate the exact cleanup targets, then remove only stale output.**

  Confirm `build/` is a generated CMake directory and the two named `/tmp` paths are analysis copies. Delete those three paths; do not touch `build-host-deps/`, `runs/`, sources or documents.

- [ ] **Step 2: Configure a fresh production-equivalent build.**

  Run: `scripts/with-host-deps.sh cmake -G Ninja -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release`

  Expected: PASS and report `BRUCE_BUILD_MOD_MCP:BOOL=ON`; no `BRUCE_BUILD_MOD_COCKPIT` option remains.

- [ ] **Step 3: Build and run the full test suite.**

  Run: `scripts/with-host-deps.sh cmake --build build && scripts/with-host-deps.sh ctest --test-dir build --output-on-failure`

  Expected: PASS, including `mcp_inspector_contracts`, `mcp_inspector_workflows`, `bruce_mcp_workflows`, and the normal non-MCP CLI suite.

- [ ] **Step 4: Perform final non-destructive acceptance checks.**

  Run: `build/bruce --help`, `build/bruce mcp --port 0`, `build/bruce cockpit`, `git diff --check` and `git status --short` when usable.

  Expected: help lists `bruce mcp [--port 1..65535]`; the invalid port returns `InvalidMcpPort`; old `cockpit` returns `UnknownCommand: cockpit`; Git checks are recorded as unavailable if the workspace remains without usable Git metadata.
