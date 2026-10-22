# Logic-Mod Artifact DAG V4 Budget Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add host-enforced global wave budgets to DAG V3.

**Architecture:** A V4 control extends the V3 wave loop with monotone time,
payload and start accounting before each wave; records/replay remain V3-compatible.

**Tech Stack:** C++20, DAG V3, CMake/CTest.

**Spec:** `docs/superpowers/specs/2026-10-17-logic-mod-artifact-dag-v4-budget-design.md`

## Global Constraints

- Limits are local and absent from spec/digest/record/replay.
- Check only before waves; drain started workers fully.
- Values are exact ranges from the Spec; no retries or mid-worker kill.

### Task 1: V4 budget core

**Files:** Create V4 header/source/unit tests; modify `CMakeLists.txt`.

- [ ] RED tests for each budget/cancel status, no future wave, artifact preservation and V3 record compatibility.
- [ ] Implement `LogicModArtifactDagControlV4` and V4 host wave accounting.
- [ ] GREEN focused tests and fresh review.

### Task 2: V4 CLI workflows

**Files:** Create V4 CLI header/integration test; modify CLI dispatch and CMake.

- [ ] RED real command test.
- [ ] Implement strict run-only options and early rejection.
- [ ] GREEN real workflows and fresh review.

### Task 3: Docs and final verification

**Files:** CLI docs, Blueprint, V4 Spec and final report.

- [ ] Document verified limits/statuses; run checks and whole-change review.
