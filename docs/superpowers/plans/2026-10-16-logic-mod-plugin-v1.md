# Logic-Mod Plugin V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Execute one explicitly activated, signed Logic-Mod worker package
under BRUCE host validation and resource control without loading foreign native
code into the host.

**Architecture:** Extend the existing signed Runtime Data Mod package boundary
with a strictly bound worker artifact and activation registry. A private framed
child-process protocol transports one canonical request and candidate response;
the host validates every identity and result before recording replay evidence.

**Tech Stack:** C++20, existing runtime package/truststore code, POSIX worker
supervision, SHA-256, CMake/CTest and Python real-binary workflows.

**Spec:** `docs/superpowers/specs/2026-10-16-logic-mod-plugin-v1-design.md`

## Global Constraints

- Never load a Logic-Mod shared library into the Host process.
- Exactly one regular, owner-only, nonsymlink worker artifact is bound by
  package digest and worker SHA-256.
- Only explicit ACTIVE mod/template pairs run; the Host owns policy, budgets,
  validation, claim scope, ledger and replay.
- Worker has no shell command, network authority, truststore/registry write
  access, arbitrary artifact URL or claim-scope authority.
- Existing Runtime Data Mod, V1/V2/V3 Artifact Store, Worker and bundle
  contracts remain byte-identical.
- Generated CLI artifacts only via verifier in full temporary copy; Git
  metadata is unavailable.

## Review Focus

- Tampering worker bytes after package verification must block start; Task 1
  binds and tests worker digest.
- Inactive/disabled/revoked or template/capability-mismatched mods must never
  spawn; Task 1/2 pin state gates.
- A malformed/oversized/identity-mismatched frame must not accept output;
  Task 2 pins frame and host validation.
- Crash, timeout and budget breach must reap child and yield no result;
  Task 2 pins each lifecycle path.
- Existing Data-Mod and Artifact/Worker outputs stay unchanged; Task 2 pins
  representative compatibility.

### Task 1: Logic package binding and activation registry

**Files:**
- Create: `include/bruce/mod/logic_mod_plugin_v1.hpp`
- Create: `src/mod/logic_mod_plugin_v1.cpp`
- Create: `tests/unit/logic_mod_plugin_v1_tests.cpp`
- Modify: `include/bruce/mod/runtime_package.hpp`, `src/mod/runtime_package.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: existing runtime package manifest, truststore and golden-case
  verification.
- Produces: verified `LogicModPackageV1`, `LogicModRegistryV1` and immutable
  `LogicModBindingV1` for Task 2.

- [ ] Write failing unit tests for signed package/worker digest binding,
  regular-owner-only worker enforcement, Golden Case gate, explicit state
  transitions and template/capability eligibility.
- [ ] Run RED for the new target; expected missing symbols/target.
- [ ] Implement minimal package extension and activation registry with
  canonical identity serialization. Verify worker artifact before activation
  and freshly before every binding request.
- [ ] Build and run focused Task 1 GREEN.

### Task 2: Isolated worker runner, CLI and replay evidence

**Files:**
- Create: `include/bruce/mod/logic_mod_worker_v1.hpp`
- Create: `src/mod/logic_mod_worker_v1.cpp`
- Create: `apps/logic_mod_plugin_v1_cli.hpp`
- Modify: `apps/bruce_cli.cpp`, `CMakeLists.txt`
- Create: `tests/unit/logic_mod_worker_v1_tests.cpp`
- Create: `tests/integration/test_logic_mod_plugin_v1.py`

**Interfaces:**
- Consumes: Task 1 `LogicModBindingV1`.
- Produces: private canonical request/result frames, supervised execution and
  a closed CLI workflow with replayable host evidence.

- [ ] Write failing tests for valid demo plugin, malformed/oversized/mismatched
  result, crash, timeout, output breach, no claim-scope escalation, child reap
  and existing-contract compatibility.
- [ ] Run RED against absent CLI/runner.
- [ ] Implement one-child execution with private framed IPC, fixed inherited
  descriptors/environment, bounded request/result, host-side digest/schema/
  identity validation and controlled replay record. Add only exact CLI routes
  needed to install, activate, run and inspect the V1 demo flow.
- [ ] Run focused GREEN plus representative existing Runtime Data Mod and
  Artifact/Worker compatibility tests.

### Task 3: Documentation, final verification and independent review

**Files:**
- Modify: `docs/cli/commands.md`, `docs/cli/formats.md`,
  `docs/architecture/03-blueprint-consolidation.md`, plugin spec status
- Create: `runs/logic-mod-plugin-v1/reports/final-review.md`

- [ ] Document exact package/state/worker contract, authority boundary,
  budgets, replay identity and excluded capabilities only after success.
- [ ] Run native build, relevant/full CTests, links, whitespace and full-copy
  CLI verifier; record commands, results, hashes and environment limits.
- [ ] Obtain fresh whole-change review over package verification, process
  isolation, frames, policy authority and legacy compatibility.

## Plan Self-Review

Task 1 defines identities and states before Task 2 executes them; Task 2 owns
all child-process and public route tests; Task 3 owns evidence. The plan adds
no dynamic host library loading, network, shell or unbounded plugin behavior.
