# Final fix brief — Exact Evidence Chain V1

This is the single final fix wave for the approved Evidence Chain work. Read the spec, plan, ledger and final-review package in this directory first. Do not spawn subagents. Use `apply_patch` for every file edit. Append a detailed report to `final-fix-report.md`.

## Binding ruling

The existing Exact engine correctly short-circuits on first contradiction and budgets. It must not be changed. Therefore the evidence contract must **not** claim every candidate/all samples were checked. Replace every occurrence of the inaccurate scope literal with one exact, truthful V1 literal:

```text
countercheck_scope short-circuit-training-then-holdout-v1
```

Keep `countercheck_policy affine-inverse-v1`. Update the design, plan, source, unit/integration tests, CLI docs, audit and any helper strings together. The prose must state: each candidate is checked training-first; a contradiction stops that candidate; inverse countercheck runs only after a matching forward evaluation; holdout begins only after training passes and when result/budget flow reaches it; `INCOMPLETE` is not coverage.

## Required source/test fixes

1. Preserve strict `BRUCE_EVIDENCE_CHAIN_V1` framing and exact fixed field order with the new scope literal.
2. Extend unit tests for rehashed malformed length, missing field, duplicate field, out-of-order field, oversized declared block/envelope, valid non-Affine native envelope, and rehashed trailing payload. Tests must target parser/replay behavior rather than only outer-digest mismatch.
3. Improve native tampering coverage: retain inner-digest rejection but add a test whose inner native payload and inner digest are both recomputed, with outer body/digest recomputed, so `replay_exact_research` reaches semantic mismatch/replay behavior.
4. Update integration tests to use the new literal and add at least the outer rehashed trailing-payload rejection plus one strict-field framing rejection that reaches the chain parser.
5. Add every new Chain/CLI diagnostic literal to `docs/cli/source-index.md` in its existing source-index style. Do not claim automatic generator verification.
6. Preserve all no-proof/no-gate/no-external-independence boundaries and the honest build blocker.

## Scope

You may modify only the Evidence Chain source/header/tests/CMake, related design/plan/audit/CLI docs/source-index, and SDD final-fix report. Do not alter the native Exact engine, historical bundle codec/fixtures, generic dispatcher, gates, rules, Experience, grid, language or NanoCell.

## Verification

Run syntax/documentation checks and attempt the specified CMake/build/test commands. `pkg-config` and `gmpxx.h` remain missing: do not install packages or claim compilation/tests pass. Clearly distinguish source review from unexecuted tests.

## Report

State every file changed; describe new test cases and their intended parser/replay path; list exact commands/results; repeat that fresh build/runtime verification remains blocked.
