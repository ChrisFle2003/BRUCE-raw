# Local Data-Mod Builder Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build local canonical signed data-mod packages and a first installable user example without exposing private key material.

**Architecture:** A small local builder consumes explicit JSON inputs and `.env` key material, reuses the runtime’s canonical schema, creates an Ed25519-signed V1 container and writes an explicit truststore. The existing runtime remains the verifier/installer.

**Tech Stack:** C++20, OpenSSL EVP Ed25519, CMake/CTest, existing runtime package APIs.

**Spec:** `docs/superpowers/specs/2026-10-09-local-data-mod-builder-design.md`

## Global Constraints

- `.env` is ignored, local-only and never logged, rendered, committed or accepted as a CLI argument.
- No output package or truststore may overwrite an existing file.
- Builder emits only V1 data members; no executable, worker, network, GPU or model capability.
- Runtime verification must accept the generated package before the example is installed.

## Review Focus

- A malformed/missing key fails before an output is created.
- Same fixed test key and inputs produce byte-identical packages.
- Existing package/truststore output fails without replacement.
- Diagnostics never contain private key bytes.
- Generated example verifies, installs inactive, activates and routes successfully.

## Task 1: Canonical local builder and key boundary

**Files:** Create builder header/source and focused C++ tests; modify CMake.

- [ ] Write failing tests for key validation, deterministic container/signature, no-overwrite and secret-free errors.
- [ ] Implement a local builder API that reads an ignored `.env`, validates canonical tree/golden JSON, emits a V1 package and explicit public truststore with atomic no-replace writes.
- [ ] Run focused contracts and commit `feat: build signed local data mods`.

## Task 2: First user example and end-to-end workflow

**Files:** Create example input/README and integration test; modify CLI documentation only through the verifier.

- [ ] Write failing integration test: build example with test-only environment, verify, install inactive, activate and route its golden case.
- [ ] Add the controlled mathematical example and its safe authoring instructions; do not add real `.env` material.
- [ ] Run builder/runtime/full verification and documentation verifier; commit `feat: add local data mod example`.
