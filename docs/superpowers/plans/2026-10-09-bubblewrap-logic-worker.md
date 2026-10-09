# Bubblewrap Logic-Worker Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Execute one checked rational-reduction reference worker only through Bubblewrap.

**Architecture:** The host owns route selection, sandbox launch, resource limits, strict protocol validation and replay evidence. The worker sees only one bounded request on stdin and returns one bounded result on stdout.

**Tech Stack:** C++20, POSIX process control, Bubblewrap, CMake/CTest.

**Spec:** `docs/superpowers/specs/2026-10-09-bubblewrap-logic-worker-design.md`

## Global Constraints

- Missing or failing Bubblewrap rejects; no direct worker fallback.
- No network, host user files or writable host paths in the sandbox.
- Request/result JSON is canonical, bounded and template-bound.
- Initial scope is only `math.rational.reduce.v1`.

## Review Focus

- Missing `bwrap` must reject before launching a worker.
- A timeout/malformed result/mismatched template must never produce a result.
- Worker cannot read a sentinel user file or open a network namespace.
- Same request produces identical result/replay digest.
- Data-mod packages remain unable to include worker code.

## Task 1: Sandboxed protocol runner

- [ ] Write failing C++ tests for unavailable Bubblewrap, malformed response, timeout and template mismatch.
- [ ] Add bounded request/result types and Bubblewrap-only runner with no-network/minimal-filesystem/process/time limits.
- [ ] Register focused target and commit `feat: add bubblewrap logic runner`.

## Task 2: Reference rational-reduction worker and replay

- [ ] Write failing tests for `2/4 → 1/2`, negative values, deterministic replay and isolation sentinel denial.
- [ ] Add reference worker and host result validator/replay binding; expose no generic code execution API.
- [ ] Run focused/full verification and commit `feat: run checked rational reduction worker`.
