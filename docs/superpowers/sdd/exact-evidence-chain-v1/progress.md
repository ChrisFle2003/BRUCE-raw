# SDD ledger — plan: docs/superpowers/plans/2026-10-01-exact-evidence-chain.md

## Execution context

- 2026-10-01: The selected execution method is subagent-driven.
- 2026-10-01: `/home/chris/Desktop/Bruce_raw` is not a Git repository; no branch, worktree, commit range, or skill-owned Git scratch workspace is available. This ledger is stored beside the plan artifacts instead. Review packages will be path/file based, and changed files will be inspected directly.

## Preflight interface scan

| Tasks/interfaces | Producer → consumer | Finding / ruling |
|---|---|---|
| Task 1 → Task 2 (`ExactEvidenceChainRun`, create/pack/replay/render) | New evidence API → CLI subcommands | Compatible: Task 1 owns all public APIs; Task 2 only calls them. |
| Task 1 → Task 3 (contract literals/JSON) | Codec output → CLI documentation | Compatible: documentation must use implemented literals, not invent alternatives. |
| Task 2 → Task 3 (CLI surface) | `evidence create|replay` → commands/formats docs | Compatible: Task 3 only documents the tested CLI surface. |
| Task 1 internal constraints | Raw bytes, Exact V1, fixed literals, no gates | Compatible: no `ApplicationTask` or native Exact codec change is permitted. |
| Task 2 own constraints | `bigint` default, `bigint|rational` only, exclusive export | Compatible with the approved spec after its explicit default-domain clarification. |
| Task 3 verification | Full configure/build vs. unavailable `pkg-config` | Ruling: attempt fresh configure/build; if unavailable, report the blocker and run the largest executable existing-build suite. Cost if wrong: no fresh-build claim can be made. |

## Rulings

- Ruling: Use a plan-local ledger rather than the skill script’s Git-owned workspace because this checkout has no `.git` directory — it preserves recovery evidence without inventing Git state — cost if wrong: artifacts must be reviewed manually rather than by commit range.
- Ruling: Preserve existing `DuplicateOrOverlappingSampleInput` rather than the design draft’s earlier `HoldoutOverlapOrDuplicate` wording, because the existing Exact validator already owns this semantic boundary — cost if wrong: a chain-specific diagnosis may be less descriptive but avoids incompatible duplicate taxonomy.
- Ruling: Replace the inaccurate `every-enumerated-candidate/all-training-and-holdout-samples` Chain scope literal with an explicit short-circuit scope that matches the unchanged Exact engine, rather than modifying the historical native engine or claiming exhaustive coverage — the evidence artifact remains precise about actual behavior — cost if wrong: downstream users must update to the new V1 draft contract before a stable release, but no false all-sample assurance is shipped.
- Ruling: Preserve historical SDD briefs, reports and ledger entries containing the superseded scope literal; update all active source, tests, spec/plan and product documentation instead — this keeps an auditable decision history — cost if wrong: historical text can confuse readers unless they follow the explicit supersession note in the final-fix report.

## Task progress

- Task 1: implementation received; source-level review pending. New code could not compile because this environment lacks `pkg-config` and `gmpxx.h`; `evidence_chain_contracts` is not registered in the stale build. Existing adjacent build tests passed 3/3 but do not verify the new code.
- Task 1: minor (deferred): mutation tests rehash only the outer digest after a native-result change and trailing-data append; a future dependency-complete test run should add rehashed inner-native and trailing-body cases.
- Task 1: complete (no Git commit range available, source review clean; executable verification blocked by missing `pkg-config` and `gmpxx.h`).
- Task 2: implementation received; source-level review pending. Its integration test recorded a missing-command RED state; the stale binary remains unable to execute the new command because the project cannot rebuild without `pkg-config` and `gmpxx.h`.
- Task 2: fix round 1/5 (2 addressed, 0 open — outer Chain RESULT mutation and overlap diagnostic; no Git commit range available).
- Task 2: complete (no Git commit range available, source review and scoped re-review clean; executable verification blocked by missing `pkg-config` and `gmpxx.h`).
- Task 3: implementation received; documentation source review pending. It retains the new Chain as source-level implementation pending a fresh configure/build/test, rather than claiming the pre-Chain 128-test build verified it.
- Task 3: fix round 1/5 (1 addressed, 0 open — declared Countercheck scope vs. executed coverage; no Git commit range available).
- Task 3: complete (no Git commit range available, source review and scoped re-review clean; executable verification blocked by missing `pkg-config` and `gmpxx.h`).
- Final review: NOT READY. Important findings: inaccurate scope literal; missing strict-parser/coverage test cases; blocked fresh build; diagnostic catalog omission. A single final fix wave is required.
- Final fix wave: source, tests, spec/plan/docs and diagnostic catalog updated. Fresh Chain compilation/runtime tests remain blocked by missing `pkg-config` and `gmpxx.h`; stale existing-build regression suite passed 128/128 and explicitly excludes Chain targets.
- Final scoped re-review: all source-level corrections addressed; no new Critical/Important finding. Source-review READY; runtime verification and acceptance BLOCKED pending usable local `pkg-config` and GMP development headers.
