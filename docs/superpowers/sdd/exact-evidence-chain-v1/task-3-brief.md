# Task 3 brief — Contract Documentation and Verification Record

Read this file first. It is the complete requirements source for Task 3 of `docs/superpowers/plans/2026-10-01-exact-evidence-chain.md`; the approved design is `docs/superpowers/specs/2026-10-01-exact-evidence-chain-design.md`.

## Scope

Modify only:

- `docs/cli/commands.md`
- `docs/cli/formats.md`
- `docs/cli/results-and-control.md`
- `docs/audit/2026-10-01-core-0102-ist-gegen-zielbild.md`

Do not modify C++ code, CMake, tests, fixtures, plans or specs. Do not spawn subagents.

## Required content

Document only the source-implemented, approved interface:

```text
bruce evidence create --train FILE --holdout FILE --output FILE
  [--domain bigint|rational] [--min N] [--max N] [--max-bits N]
  [--max-denominator N] [--max-trace-bytes N] [--max-candidates N]
  [--max-operations N] [--max-trace N] [--max-results N]
bruce evidence replay CHAIN
```

State exact fixed fields/header, raw embedded CSV bytes, local bounded replay, `bigint` default, and that only affine/enumerate Exact routes are accepted. Explain that `declared-disjoint-v1` means technical x-disjointness plus a local declaration, not externally established independence or data provenance. State explicitly: candidate, score, `VERIFIED_ON_SAMPLES`, digest and replay are not general mathematical proof; no learn/publish/rule-activation gate is opened.

No generic `replay`, `ApplicationTask`, cloud, publication, Strategy promotion, BSGS/Private-Key/Mining claim, or external checker is introduced.

## Verification-status rule

`pkg-config` and `gmpxx.h` are missing, so the added C++/CLI code has not been compiled or executed in this environment. Therefore do **not** describe the evidence chain as “implemented and verified”, “tested green”, or an accepted completed P0 package. In the audit, retain it as source-level implementation pending fresh build/test verification, and name the exact commands outstanding:

```bash
cmake -S . -B build
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

You may run documentation-only/read-only checks and existing executable validation, but do not install dependencies or claim the unavailable full verification passed.

## Report

Write a detailed report to `docs/superpowers/sdd/exact-evidence-chain-v1/task-3-report.md` via `apply_patch`: files changed, exact verification commands/results, statement-boundary check, and remaining blocker. Return only `DONE` or `DONE_WITH_CONCERNS`, report path, one-line check summary, and concerns.
