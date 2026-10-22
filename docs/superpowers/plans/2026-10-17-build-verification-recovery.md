# Build Verification Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the project-owned host-dependency wrapper reject whitespace-unsafe paths before pkg-config corruption, and establish a fresh, reproducible verification sequence from a native safe-path copy.

**Architecture:** `scripts/with-host-deps.sh` remains the sole child-process environment adapter. It will validate both documented unsafe path characters and whitespace before exporting pkg-config variables; it changes no dependency bundle. Verification builds from a complete temporary copy whose absolute path contains neither colon nor whitespace, then tests the current resulting executable and verifies generated CLI documents only in a second temporary copy.

**Tech Stack:** POSIX shell, CMake 3.20+, Ninja, CTest, C++20, Python 3.

**Spec:** `docs/superpowers/specs/2026-10-17-build-verification-and-cli-boundary-design.md`

## Global Constraints

- Preserve existing CLI command spellings, JSON contracts, file formats, static-mod switches, and error behavior.
- Do not delete, overwrite, or regenerate `build*/`, `build-host-deps/`, or `runs/` in the source tree.
- Generated CLI artifacts are verified or updated only by `docs/cli/verify_examples.py` in a temporary complete copy.
- The wrapper must remain a child-process-only environment adapter.
- A path with `:` or whitespace is unsafe for the current bundled pkg-config setup and must fail before CMake configuration.
- Do not claim a successful test suite establishes a general mathematical proof.

## Review Focus

- A native path containing a colon must still fail with exit code 2 and a useful diagnostic.
- A path containing whitespace must fail before `pkg-config` or CMake runs.
- A safe path must retain the existing exports and execute the supplied command unchanged.
- A generated-document verification must leave the source-tree generated files untouched.
- CTest must run against the newly built executable, not the stale local `build/bruce`.

---

### Task 1: Make unsafe dependency paths fail early

**Files:**
- Modify: `scripts/with-host-deps.sh:5-9`
- Modify: `AGENTS.md: repository wrapper guidance`
- Test: `tests/integration/test_with_host_deps_wrapper.py` (new)

**Interfaces:**
- Consumes: command arguments passed to `scripts/with-host-deps.sh`.
- Produces: exit 2 and an `Unsupported dependency path` diagnostic for colon or whitespace paths; otherwise `exec "$@"` behavior remains unchanged.

- [ ] **Step 1: Write a failing Python integration test for a whitespace-containing copy path**

Create a temporary project copy under a directory whose name contains one space. Invoke its `scripts/with-host-deps.sh` with `/usr/bin/true`; assert exit code 2, stderr contains `Unsupported dependency path`, and stdout is empty.

- [ ] **Step 2: Run the new test to verify the current wrapper fails the expectation**

Run: `python3 tests/integration/test_with_host_deps_wrapper.py`

Expected: FAIL because the current wrapper executes `/usr/bin/true` for a path containing whitespace.

- [ ] **Step 3: Extend the wrapper path guard**

In `scripts/with-host-deps.sh`, replace the colon-only case with one case that rejects both `*:*` and patterns containing POSIX whitespace. Keep exit code 2, avoid echoing exported environment values, and tell the user to use a native safe path without colon or whitespace.

- [ ] **Step 4: Extend the test for the existing colon case and safe path**

Add assertions that a colon-containing copy path exits 2 and that a safe temporary copy executes `/usr/bin/true` with exit 0.

- [ ] **Step 5: Run the wrapper test**

Run: `python3 tests/integration/test_with_host_deps_wrapper.py`

Expected: PASS.

- [ ] **Step 6: Update the project handbook wording**

In `AGENTS.md`, change the native-path requirement from “without `:`” to “without `:` or whitespace”; preserve the reason that the bundled dependency wrapper otherwise corrupts search paths.

- [ ] **Step 7: Commit the task when Git metadata is available**

Run `git status --short`, then commit only `scripts/with-host-deps.sh`, `AGENTS.md`, and `tests/integration/test_with_host_deps_wrapper.py`. If Git metadata is absent, record that the source changes remain uncommitted.

### Task 2: Add a reproducible current-source verification runner

**Files:**
- Create: `scripts/verify_current_source.sh`
- Modify: `README.md: Build, test, and install`
- Test: `tests/integration/test_verify_current_source_script.py` (new)

**Interfaces:**
- Consumes: an optional safe build directory; otherwise creates a unique `/tmp/bruce-current-verify-*` directory.
- Produces: a configured Release build, passing CTest invocation, generated-CLI-document comparison from a temporary copy, and local Markdown-link check; returns the first failing command status.

- [ ] **Step 1: Write a failing test for runner argument validation**

Test that `scripts/verify_current_source.sh` rejects a supplied build directory containing colon or whitespace before configuring, with exit code 2 and no source-tree writes.

- [ ] **Step 2: Run the test to verify it fails before the runner exists**

Run: `python3 tests/integration/test_verify_current_source_script.py`

Expected: FAIL because the script does not exist.

- [ ] **Step 3: Implement `scripts/verify_current_source.sh`**

Use `set -eu`. Validate the source root and requested build path. When no build path is supplied, create a unique `/tmp` build directory with a safe fixed prefix. Configure through `scripts/with-host-deps.sh`, build, run CTest, copy the complete source into a separate safe temporary directory while excluding local build/agent areas, invoke `docs/cli/verify_examples.py` there against the newly built binary, compare the three generated artifacts, and invoke `scripts/check_local_doc_links.py` for the repository documentation scope.

- [ ] **Step 4: Add a fast script-shape test**

The Python test must verify unsafe path rejection and that the script refers to the newly built binary rather than `build/bruce`; it must not run the full suite.

- [ ] **Step 5: Run fast runner tests**

Run: `python3 tests/integration/test_verify_current_source_script.py`

Expected: PASS.

- [ ] **Step 6: Run the complete current-source verifier from a safe temporary copy**

Copy the repository to a `/tmp` directory without colon or whitespace, then run its `scripts/verify_current_source.sh`. Record configure/build/CTest/document/link results exactly; do not use the source-tree `build/` directory.

- [ ] **Step 7: Document the verification command**

Add the runner as the preferred current-source verification command in `README.md`, including its safe-path requirement and the fact that it does not modify checked-in generated CLI artifacts.

- [ ] **Step 8: Commit the task when Git metadata is available**

Commit only the runner, its tests, and README update; otherwise report the absent Git metadata.

### Task 3: Clarify local artifact evidence boundaries

**Files:**
- Modify: `docs/architecture/repository-structure.md: Generated and local-only paths`
- Modify: `README.md: Build and evidence guidance`
- Test: `scripts/check_local_doc_links.py` existing checker

**Interfaces:**
- Consumes: existing repository conventions for `build*/`, `build-host-deps/`, and `runs/`.
- Produces: documentation that distinguishes current source, generated build products, dependency roots, and local historical runs.

- [ ] **Step 1: Add the documented evidence distinction**

State that a local executable is evidence only for the source revision and configuration that produced it; when its timestamps or build metadata do not match current sources, it must not be used as current-source verification. Add `build-host-deps/` as a local dependency root rather than a source-of-truth implementation directory.

- [ ] **Step 2: Run the local documentation-link checker**

Run: `python3 scripts/check_local_doc_links.py`

Expected: PASS with no broken local Markdown links.

- [ ] **Step 3: Commit the task when Git metadata is available**

Commit only the documentation changes; otherwise report the absent Git metadata.

## Self-review

- Spec coverage: Tasks 1–3 cover host-wrapper diagnosis, clean verification, generated-document handling, and local-artifact documentation. CLI extraction and security policy are intentionally separate follow-up plans because they are independently reviewable subsystems.
- Type consistency: Shell and Python test interfaces use explicit executable paths and exit codes; no new C++ public interface is introduced.
- Review focus coverage: Task 1 covers unsafe and safe paths; Task 2 pins the newly built-binary and generated-document boundaries; Task 3 covers evidence documentation.
- Proportion: The plan supplies exact files, behaviors, tests, and commands without prescribing implementation bodies beyond the safety-critical shell flow.
