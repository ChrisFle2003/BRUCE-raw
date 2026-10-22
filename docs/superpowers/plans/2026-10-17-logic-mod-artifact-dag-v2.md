# Logic-Mod Artifact DAG V2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Execute a bounded JSON-defined local Logic-Mod DAG serially, with
host-published predecessor input artifacts and replayable evidence.

**Architecture:** A host-only DAG core parses canonical validated node data,
preflights every package before Store mutation, serially publishes internal
predecessor payload artifacts and invokes the unchanged Worker V1. A strict
CLI owns safe Spec/record paths, independent package lifecycles and public
metadata-only JSON.

**Tech Stack:** C++20, existing Logic-Mod Worker/Payload/Result Artifact V1,
Artifact Store V1/V1.2, CMake/CTest and Python real-binary workflows.

**Spec:** `docs/superpowers/specs/2026-10-17-logic-mod-artifact-dag-v2-design.md`

## Global Constraints

- At most 16 nodes; execution is serial stable topological order by node ID.
- Every edge uses only a host-published internal READY input artifact and the
  unchanged sealed Payload Capability V1 FD 6; no worker receives paths or a
  Store handle.
- Root packages require exactly `{artifact.output.v1, worker.execute}`;
  nonroots require exactly `{artifact.input.v1, artifact.output.v1, worker.execute}`.
- Validate all Spec/package/trust/ELF/budget/manifest requirements before the
  first worker or Store mutation; all output and input manifests differ.
- Record and JSON contain no paths or diagnostics. Record embeds existing Run
  V1 evidence but never separately copies payload bytes.
- Replay starts no worker; no parallelism, retry, dynamic nodes, remote Store,
  network, streams, free codecs or general scheduler.

## Review Focus

- A syntactically valid late-node package with invalid ELF must prevent every
  earlier node and Store mutation; Task 1 preflight test pins this.
- A node with two predecessors must receive deterministic ID-sorted payload
  content and no undeclared ancestor; Task 1 payload test pins this.
- A cycle, duplicate ID, dangling predecessor or manifest alias must fail
  before path/Store access; Task 2 real CLI test pins this.
- A failed node must start no dependent node and publish no DAG record; Task 2
  worker-start regression pins this.
- Replay must reject changed internal input/result artifacts with no worker;
  Task 1 and Task 2 replay regressions pin this.

### Task 1: DAG core, internal input artifacts and replay

**Files:**
- Create: `include/bruce/mod/logic_mod_artifact_dag_v2.hpp`
- Create: `src/mod/logic_mod_artifact_dag_v2.cpp`
- Create: `tests/unit/logic_mod_artifact_dag_v2_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Define `LogicModDagNodeV2` with ID, registry reference, package identity,
  text input, sorted predecessor IDs, optional `LogicModOutputV1 input_artifact`,
  mandatory `LogicModOutputV1 output_artifact`, and budgets.
- Define `LogicModArtifactDagRunV2` with acceptance/status/DAG ID, canonical
  Spec digest, stable node order, per-node `LogicModRunV1`, result identity and
  internal input identity.
- Produce `run_logic_mod_artifact_dag_v2(nodes, store, spec_digest)`, canonical
  record/read functions and `verify_logic_mod_artifact_dag_replay_v2`.

- [ ] Write failing unit tests for a three-node diamond, deterministic
  predecessor payload, late-node ELF preflight, cycle/dangling/manifest errors,
  failure stop and no-spawn artifact/record replay rejection.
- [ ] Run the target to capture RED for the missing API.
- [ ] Implement deterministic validation/topological ordering, existing Worker
  V1 invocation and Result Artifact V1 publication. Build each nonroot's
  bounded canonical DAG-input document, put it into the local Store, publish
  it to that node's explicit internal input manifest, and pass only that
  manifest object zero through `LogicModInputV1`.
- [ ] Implement bounded canonical DAG record/read/replay. Replay revalidates
  current bindings and all result/internal input identities and maps failures
  to `DAG_REPLAY_MISMATCH` without spawning.
- [ ] Run focused GREEN plus existing Logic-Mod compatibility targets; obtain a
  fresh Task 1 review.

### Task 2: Strict JSON Spec CLI and real workflows

**Files:**
- Create: `apps/logic_mod_artifact_dag_v2_cli.hpp`
- Modify: `apps/bruce_cli.cpp`
- Create: `tests/integration/test_logic_mod_artifact_dag_v2.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consume Task 1 core. Parse only `run SPEC_JSON --store LOCAL_STORE --record
  DAG_RECORD` and the matching `replay` form.
- Produce fixed `logic-mod-artifact-dag-v2` JSON containing status, DAG ID,
  Spec digest, ordered node IDs and artifact metadata only.

- [ ] Write a real-binary RED workflow that proves the command is absent.
- [ ] Run it and capture `UnknownCommand` before production changes.
- [ ] Implement a bounded strict JSON parser with duplicate-key rejection and
  canonical Spec digest; safe no-follow descriptor reads and private atomic
  no-replace Record publication consistent with Chain V1. Bootstrap an
  independent registry/trust/package lifecycle per node before invoking Task 1.
- [ ] Add workflows for diamond success, malformed/cyclic/dangling Spec,
  duplicate manifests, late preflight failure/no start, node failure/no
  successors/record, internal artifact tampering and zero-worker replay.
- [ ] Run focused GREEN and fresh Task 2 review.

### Task 3: Documentation, verification and whole-change review

**Files:**
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/superpowers/specs/2026-10-17-logic-mod-artifact-dag-v2-design.md`
- Create: `runs/logic-mod-artifact-dag-v2/reports/final-review.md`

- [ ] Document only verified final JSON syntax, internal input-artifact
  semantics, serial limit, record/replay guarantees and exclusions.
- [ ] Run full-copy CLI verifier, focused/full selections, link and whitespace
  checks. Report socket/Git restrictions separately; never hand-edit generated
  CLI artifacts.
- [ ] Obtain a fresh whole-change review, resolve findings and promote the
  Spec status only after evidence.

## Plan Self-Review

Task 1 owns all deterministic DAG data and no-spawn verification; Task 2 owns
all untrusted files, JSON and public CLI behavior; Task 3 makes only verified
claims. Every Spec requirement maps to one task, including the internal Store
artifact required by the unchanged Worker V1 contract. The plan deliberately
does not add scheduling or external execution.
