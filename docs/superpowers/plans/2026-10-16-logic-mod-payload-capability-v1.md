# Logic-Mod Payload Capability V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give an active eligible Logic-Mod one verified local Artifact Store
object through sealed read-only FD 6, bound to request and replay evidence.

**Architecture:** A host-only input resolver validates V1.2 manifest/object
identity and copies at most 1 MiB into a sealed Memfd. The existing isolated
worker receives only the fixed descriptor and canonical identity fields; host
replay resolves and verifies the same reference anew.

**Tech Stack:** C++20, Artifact Store V1/V1.2, existing Logic-Mod worker
Seccomp/Memfd runtime, CMake/CTest and real-binary workflows.

**Spec:** `docs/superpowers/specs/2026-10-16-logic-mod-payload-capability-v1-design.md`

## Global Constraints

- Exactly one `READY` local object, maximum 1 MiB, only with
  `artifact.input.v1` capability.
- No paths, URLs, store access, output write or extra input descriptor for
  worker; input uses sealed read-only FD 6.
- Canonical request/record bind manifest ID, digest, size and
  `artifact-bytes-v1`; replay revalidates locally.
- Existing inputfree plugin behavior and Artifact Store/V1.2/V1.4 contracts
  remain byte-identical.
- Generated CLI docs only through full-copy verifier; Git unavailable.

## Review Focus

- Capability/reference/store errors must happen before spawn; Task 1 tests pin
  this.
- FD 6 must be readable but not writable and no other filesystem access opens;
  Task 2 tests pin this.
- Exactly 1 MiB succeeds and 1 MiB+1 fails with no result; Task 1/2 pin this.
- Replay must reject changed/missing/non-READY object; Task 2 pins this.
- Inputfree V1 bytes must stay unchanged; Task 2 pins compatibility.

### Task 1: Host input resolver and bound request identity

**Files:**
- Create: `include/bruce/mod/logic_mod_payload_v1.hpp`
- Create: `src/mod/logic_mod_payload_v1.cpp`
- Create: `tests/unit/logic_mod_payload_v1_tests.cpp`
- Modify: `CMakeLists.txt`, Logic-Mod worker interfaces as necessary

- [ ] Write failing tests for capability gate, manifest/object validation,
  1 MiB bounds, sealed byte copy and canonical identity.
- [ ] Run RED for missing target/API.
- [ ] Implement host-only resolver over local V1.2 verification; make a sealed
  in-memory payload object and return immutable metadata/FD ownership.
- [ ] Run focused Task 1 GREEN.

### Task 2: FD-6 runner, session CLI and replay

**Files:**
- Modify: Logic-Mod worker source/header, `apps/logic_mod_plugin_v1_cli.hpp`,
  `apps/bruce_cli.cpp`, `CMakeLists.txt`
- Create: `tests/unit/logic_mod_payload_worker_v1_tests.cpp`
- Create: `tests/integration/test_logic_mod_payload_v1.py`

- [ ] Write failing tests for exact CLI pairing, FD 6 read-only behavior,
  no further filesystem access, success/max/oversize, bad input/no start,
  record/replay binding and inputfree byte compatibility.
- [ ] Run RED against current inputfree CLI/runner.
- [ ] Add only optional paired input-artifact/store route, request/record
  fields, FD-6 Seccomp allowance and fresh replay resolution.
- [ ] Run focused GREEN plus Artifact/Plugin compatibility selection.

### Task 3: Documentation, final verification and review

**Files:**
- Modify: CLI/formats/blueprint/spec status docs
- Create: `runs/logic-mod-payload-capability-v1/reports/final-review.md`

- [ ] Document one-object/1-MiB/capability/FD-6/replay boundary and non-goals.
- [ ] Run build, relevant/full tests, links, whitespace and full-copy verifier;
  record evidence and limits.
- [ ] Obtain fresh whole-change review.

## Plan Self-Review

Task 1 produces resolver identity before Task 2 consumes it; Task 2 owns
process/CLI/replay behavior; Task 3 owns evidence. No multiple input, path,
remote, stream or worker-store feature is added.
