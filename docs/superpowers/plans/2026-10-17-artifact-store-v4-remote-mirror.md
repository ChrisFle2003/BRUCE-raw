# Artifact Store V4 Remote Mirror Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Mirror and restore verified READY artifacts through a host-only HTTPS remote.

**Architecture:** A bounded Store V4 transport validates every manifest/object
identity locally before upload and after download. A separate CLI owns strict
arguments and metadata-only output; Worker contracts remain unchanged.

**Tech Stack:** C++20, Artifact Store V1/V3 HTTPS primitives, OpenSSL, CMake/CTest.

**Spec:** `docs/superpowers/specs/2026-10-17-artifact-store-v4-remote-mirror-design.md`

## Global Constraints

- HTTPS is host-only; no URLs/credentials in records, JSON or workers.
- Mirror never mutates local data; restore never overwrites existing local data.
- Verify IDs, SHA-256, sizes, READY manifest and limits before acceptance.
- Replay/DAG automation and remote deletion are excluded.

### Task 1: Bounded remote mirror core

**Files:** Create V4 header/source/unit tests; modify CMake.

- [ ] RED tests for ready upload, missing restore, digest/size/manifest/TLS/timeout rejection and no local overwrite.
- [ ] Implement host-only mirror/restore using existing HTTPS helper and Store primitives.
- [ ] GREEN focused tests and fresh review.

### Task 2: Strict CLI and real HTTPS workflows

**Files:** Create CLI header/integration test; modify dispatcher/CMake.

- [ ] RED unknown command test.
- [ ] Implement strict mirror/restore parsing and metadata-only JSON.
- [ ] GREEN local HTTPS fixture workflows and fresh review.

### Task 3: Docs and final verification

**Files:** CLI docs, formats, Blueprint, Spec and final report.

- [ ] Document verified behavior only; verifier, tests, links and whole review.

## Plan Self-Review

Tasks isolate transport, public input handling and documentation while keeping
workers and DAG replay outside the network boundary.
