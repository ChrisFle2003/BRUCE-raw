# Logic-Mod Artifact DAG V5 Remote Restore Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Restore an expected missing DAG predecessor artifact host-side before FD-6 handoff.

**Architecture:** V5 reuses V4 remote restore only after safe local absence detection; V4 DAG records/replay stay unchanged and offline.

**Spec:** `docs/superpowers/specs/2026-10-17-logic-mod-artifact-dag-v5-remote-restore-design.md`

### Task 1: V5 host restore core
- [ ] Create V5 core/header/tests/CMake; RED/GREEN local-hit, missing restore, corrupt/unreadable rejection, no-worker on failure and record compatibility.

### Task 2: V5 strict CLI workflows
- [ ] Create V5 CLI/header integration/CMake; run-only remote option, TLS workflows and replay rejection before access.

### Task 3: Documentation and final review
- [ ] Update CLI docs, formats, Blueprint, Spec and final report; verify docs/tests/review.
