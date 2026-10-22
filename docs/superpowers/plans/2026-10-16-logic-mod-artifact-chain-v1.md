# Logic-Mod Artifact Chain V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Execute two explicitly supplied, authorized Logic-Mods serially with
one host-verified Artifact Store handoff and replayable chain evidence.

**Architecture:** A new host-only chain core coordinates two existing
`run_logic_mod_worker_v1` calls. It publishes A through the existing Result
Artifact V1 path, resolves it through existing Payload Capability V1 for B,
and serializes both unchanged run records in a separate chain record. The
dedicated CLI owns package/trust bootstrap and strict user argument parsing.

**Tech Stack:** C++20, existing Logic-Mod Plugin/Payload/Result Artifact V1,
Artifact Store V1/V1.2, CMake/CTest and real-binary Python workflows.

**Spec:** `docs/superpowers/specs/2026-10-16-logic-mod-artifact-chain-v1-design.md`

## Global Constraints

- Exactly two serial steps, one local Store, one A-to-B READY object at index
  zero; never direct plugin communication.
- A requires the exact set `{artifact.output.v1, worker.execute}`. B requires
  the exact set `{artifact.input.v1, artifact.output.v1, worker.execute}` so
  B, rather than A or the host, authorizes its own result publication.
- Existing worker frame, FD map, Seccomp, Plugin/Payload/Result contracts and
  bytes are unchanged.
- Paths, worker diagnostics and separately copied Artifact-payload bytes never
  enter records or JSON output. The private bounded Chain record embeds the
  existing Run V1 records unchanged, including their normalized input/result/
  response-frame evidence; public JSON exposes only metadata.
- Replay starts no worker and checks both embedded records plus fresh local
  A/B artifact identity.
- Generated CLI artifacts only through full-copy verifier; Git unavailable.

## Review Focus

- A failure must prevent B from starting; test its fixture side effect.
- A's output manifest must be the only B input reference; test substituted
  manifest/index/digest rejection.
- Any failure before A must avoid Store/worker mutation; test invalid
  capability/trust/manifest inputs.
- Replay must not spawn a worker and must reject missing/non-READY/changed
  A or B artifacts; test it with an exec-escape fixture.
- Existing single-plugin and Result Artifact records must remain byte-identical;
  pin them in the compatibility selection.

### Task 1: Chain core, canonical record and no-spawn replay

**Files:**
- Create: `include/bruce/mod/logic_mod_artifact_chain_v1.hpp`
- Create: `src/mod/logic_mod_artifact_chain_v1.cpp`
- Create: `tests/unit/logic_mod_artifact_chain_v1_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Define host-only `LogicModChainStepV1` containing a registry reference,
  mod/version/template, text input and `LogicModOutputV1`.
- Define `LogicModArtifactChainRunV1` with canonical chain ID, A/B
  `LogicModRunV1` records and A/B published artifact identities.
- Define `run_logic_mod_artifact_chain_v1(const LogicModChainStepV1& first,
  const LogicModChainStepV1& second, const std::filesystem::path& store)`;
  B's `LogicModInputV1` is constructed only from A's just-published manifest
  and object index zero.
- Define `logic_mod_artifact_chain_record_v1`, read counterpart and
  `verify_logic_mod_artifact_chain_replay_v1`.

- [ ] Write failing core tests for success, capability denial, A failure/no B
  start, B failure/no B manifest/record, substituted handoff and no-spawn
  replay.
- [ ] Run the new CTest target and capture RED for missing API/target.
- [ ] Implement the host-only core. Derive `chain_id` as SHA-256 of the
  canonical framed A/B binding identities, normalized inputs and requested
  output manifest IDs; do not use paths or random data.
- [ ] Serialize `BRUCE_LOGIC_MOD_ARTIFACT_CHAIN_V1` as a bounded framed record
  with chain ID, both run-record byte strings and both artifact identities.
  Strict parse/roundtrip must reject trailing/noncanonical fields.
- [ ] Replay parses but does not execute both steps: call existing run-record
  replay/Result Artifact verification with fresh local `MANIFEST_A:0` and
  `MANIFEST_B:0`; surface a controlled `CHAIN_REPLAY_MISMATCH`.
- [ ] Run focused GREEN plus existing Plugin/Payload/Result compatibility
  selection, then obtain a fresh Task 1 review.

### Task 2: Strict chain CLI and real workflows

**Files:**
- Create: `apps/logic_mod_artifact_chain_v1_cli.hpp`
- Modify: `apps/bruce_cli.cpp`
- Create: `tests/integration/test_logic_mod_artifact_chain_v1.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes Task 1 core and record codec.
- CLI accepts exactly the approved `run` or `replay` argument sequence.
  `run` publishes the chain record only after B and both result artifacts
  succeed; `replay` takes no output-manifest options and starts no worker.
- JSON has a fixed `logic-mod-artifact-chain-v1` contract and exposes only
  chain ID, accepted/status fields and A/B artifact digest/size/codec/manifest
  identifiers.

- [ ] Write real-binary RED workflows covering success, malformed/reordered/
  duplicate options, invalid trust/package, A failure/no B execution, B
  failure/no chain record, and replay record/artifact tampering.
- [ ] Run RED and prove the command is absent before implementation.
- [ ] Implement strict argument parser before any input file access; bootstrap
  separate registries/packages/truststores, run existing lifecycle steps and
  invoke Task 1 core. Use safe bounded record publication semantics consistent
  with the new inspector/path hardening; do not use generic unsafe reads.
- [ ] Add a `replay` submode using the same explicit package/trust/input/store
  inputs and a chain-record path; it must not instantiate workers.
- [ ] Run focused GREEN and related Artifact/Logic-Mod selection; fresh review
  checks CLI ordering, no leak, no B start after A failure and no-spawn replay.

### Task 3: Documentation, final verification and whole-change review

**Files:**
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/superpowers/specs/2026-10-16-logic-mod-artifact-chain-v1-design.md`
- Create: `runs/logic-mod-artifact-chain-v1/reports/final-review.md`

- [ ] Document only verified final syntax, two-step limit, host authority,
  chain record/replay and exclusions.
- [ ] Run full-copy CLI verifier, focused/full tests, links and whitespace;
  report socket/Git restrictions separately and never hand-edit generated docs.
- [ ] Obtain fresh whole-change review, resolve findings, then promote the
  spec status only with final evidence.

## Plan Self-Review

Task 1 owns deterministic coordination and replay evidence; Task 2 owns all
paths and user syntax; Task 3 documents only confirmed behavior. The plan
does not add parallelism, dynamic scheduling, a network layer or free-form
plugins, so it remains within the two-step V1 specification.
