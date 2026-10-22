# Artifact Store V3 HTTPS Fetch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fetch one digest- and size-bound object through an allowlisted,
TLS-verified HTTPS provider into the existing local Artifact Store V1.

**Architecture:** V3 owns a canonical HTTPS-provider parser and a bounded
transport port around the vendored cpp-httplib/OpenSSL client. It accepts only
the derived object GET, fully verifies bytes, then delegates atomic local
publication to the V1 boundary. A closed CLI consumes that core.

**Tech Stack:** C++20, existing OpenSSL, vendored cpp-httplib, Artifact Store
V1/V2 patterns, CMake/CTest and local TLS integration tests.

**Spec:** `docs/superpowers/specs/2026-10-16-artifact-store-v3-https-fetch-design.md`

## Global Constraints

- Public command is exactly `artifact-store-v3 fetch SHA256 SIZE --store
  LOCAL_STORE --provider PROVIDER_ID --providers PROVIDERS.json`.
- Provider file uses exact `bruce-artifact-store-providers-v3` canonical
  schema; HTTPS origins only, no URL freedom.
- TLS certificate and hostname verification are mandatory; only HTTP 200,
  zero redirects, one request and zero retry are permitted.
- Limits: 32 MiB object, 5 s connect timeout and 30 s whole request.
- No payload/path/origin/TLS/OS diagnostics in public output; errors are V3
  `PROTOCOL_ERROR`, Exit 2.
- V1/V1.2/V2/V5 contracts remain byte-identical; no worker networking,
  credentials, proxy, remote writes, cache or synchronization.
- Generated CLI documents are touched only through the verifier in a full
  temporary repository copy; Git metadata is unavailable.

## Review Focus

- A certificate or hostname failure must reject before publication; Task 1
  pins the transport error path and Task 2 pins real TLS workflows.
- Redirect/non-200 and malformed origin must never become alternate fetches;
  Task 1/2 pin these paths.
- Response bytes exceeding 32 MiB or stream truncation must leave no object;
  Task 1 pins the core snapshot.
- Deadline behavior must bound connect and full response lifetime; Task 1
  exercises the transport seam without external network.
- Existing V1/V1.2/V2 bytes and V1.4 local consumption after V3 fetch remain
  valid; Task 2 pins compatibility workflows.

### Task 1: Canonical HTTPS provider and bounded transport core

**Files:**
- Create: `include/bruce/artifact/artifact_store_v3.hpp`
- Create: `src/artifact/artifact_store_v3.cpp`
- Create: `tests/unit/artifact_store_v3_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ArtifactStoreV1`, existing V2 error/publication conventions,
  OpenSSL and cpp-httplib.
- Produces: canonical V3 provider parser plus a testable bounded transport
  abstraction and `fetch_to_store_v3(...)` returning `ArtifactIdV1`.

- [ ] Write failing unit tests for canonical V3 configuration/origin grammar,
  expected fixed GET path, HTTP 200-only behavior, no redirect, TLS/hostname
  error classification, deadline/stream limits, digest/size failures with
  unchanged store, and success/idempotence.
- [ ] Run the new target RED; expected missing target/API.
- [ ] Implement the minimal V3 parser, transport seam and bounded
  cpp-httplib/OpenSSL production adapter. Require TLS verification and map all
  internal errors to V3 protocol errors before V1 publication.
- [ ] Register CMake target and run focused core GREEN.

### Task 2: Closed CLI and local TLS workflows

**Files:**
- Create: `apps/artifact_store_v3_cli.hpp`
- Modify: `apps/bruce_cli.cpp`, `CMakeLists.txt`
- Create: `tests/unit/artifact_store_v3_cli_tests.cpp`
- Create: `tests/integration/test_artifact_store_v3.py`

**Interfaces:**
- Consumes: Task 1 V3 parser/fetch interface.
- Produces: exact V3 CLI report/error envelope and real-binary TLS workflows.

- [ ] Write failing parser tests and Python TLS-server workflows for trusted
  success, untrusted/wrong-host rejection, non-200, redirect, digest/size
  mismatch, argument closure, privacy, and V1/V1.2/V2 compatibility.
- [ ] Run V3 CLI workflows RED; expected unknown command/header.
- [ ] Implement only the closed route and fixed ordered JSON success/error
  rendering. Test server certificates remain test-only fixtures.
- [ ] Run focused CLI GREEN, including V1.4 local consumption of fetched bytes.

### Task 3: Documentation, verification and independent review

**Files:**
- Modify: `docs/cli/commands.md`, `docs/cli/formats.md`,
  `docs/architecture/03-blueprint-consolidation.md`, V3 spec status
- Create: `runs/artifact-store-v3-https-fetch/reports/final-review.md`

- [ ] Document exact V3 command, canonical HTTPS config, TLS/timeout limits,
  public privacy boundary and excluded network features after tests pass.
- [ ] Run native build, relevant CTests, links, whitespace and full-copy CLI
  verifier; record exact evidence and known sandbox/Git limits.
- [ ] Require fresh whole-change review covering TLS setup, allowlist closure,
  size/deadline enforcement, publication safety and compatibility.

## Plan Self-Review

Each spec requirement belongs to core, CLI/TLS workflow or final evidence;
later tasks consume interfaces produced by earlier tasks. The plan introduces
no authentication, proxy, free URL, remote-write or worker-network behavior.
