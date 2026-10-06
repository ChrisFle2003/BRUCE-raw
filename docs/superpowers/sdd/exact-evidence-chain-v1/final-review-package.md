# Final review package — Exact Evidence Chain V1

## Description

This change adds a bounded, local `BRUCE_EVIDENCE_CHAIN_V1` source implementation and `bruce evidence create|replay` interface around the existing Exact-Affine native bundle. It embeds raw training/hold-out CSV bytes, enforces a fixed declared policy/scope, binds embedded samples to the native task and replays the entire envelope. It intentionally does not alter native Exact bundles, generic application replay, gates, rules, Experience, cloud/publication behavior, or proof status.

## Review inputs

- Spec: `docs/superpowers/specs/2026-10-01-exact-evidence-chain-design.md`
- Plan: `docs/superpowers/plans/2026-10-01-exact-evidence-chain.md`
- Ledger: `docs/superpowers/sdd/exact-evidence-chain-v1/progress.md`
- Reports: `task-1-report.md`, `task-2-report.md`, `task-3-report.md` in this directory.

## Changed implementation files

- `include/bruce/evidence/chain.hpp`
- `src/evidence/chain.cpp`
- `apps/bruce_cli.cpp`
- `tests/unit/evidence_chain_tests.cpp`
- `tests/integration/test_evidence_chain.py`
- `CMakeLists.txt`
- `docs/cli/commands.md`
- `docs/cli/formats.md`
- `docs/cli/results-and-control.md`
- `docs/audit/2026-10-01-core-0102-ist-gegen-zielbild.md`

## Verification fact pattern

- Source-level task and scoped re-reviews approved all three tasks after two documented fixes.
- The prior existing build passed 128 CTest tests before this feature, but it cannot verify later Chain sources.
- This environment lacks `pkg-config` and `gmpxx.h`; CMake cannot regenerate and the new C++/CLI code was not compiled or executed. New CTest entries are absent from the stale build and must not be counted as passed.
- A system-package install request was rejected because the approved plan required documenting the dependency gap rather than changing the environment.

## Required final-review focus

1. Exact format, field order, raw bytes, size limits and outer/inner digest binding.
2. Sample/route binding and complete rejection of non-Affine/non-Exact inputs.
3. CLI whitelist, output ordering, replay arity and isolation from generic gates/dispatchers.
4. Tests for outer-result binding, tampering and short-circuit/budget semantics.
5. Documentation truthfulness: no proof, external-independence, runtime-test, or P0-acceptance overclaim.

There is no Git commit range because this workspace is not a Git repository. Inspect complete current files against the spec/plan rather than inferring a diff. Do not modify files.
