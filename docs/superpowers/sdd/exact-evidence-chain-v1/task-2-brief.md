# Task 2 brief — Bounded CLI Create and Replay

Read this file first. It is the complete requirements source for Task 2 of `docs/superpowers/plans/2026-10-01-exact-evidence-chain.md`; the approved design is `docs/superpowers/specs/2026-10-01-exact-evidence-chain-design.md`.

## Scope

Modify only:

- `apps/bruce_cli.cpp`
- `CMakeLists.txt` (only the new integration-test registration)

Create only:

- `tests/integration/test_evidence_chain.py`

Consume the Task 1 public API in `include/bruce/evidence/chain.hpp`. Do not alter Task 1 sources/tests, native bundles, generic application replay, docs, gates, Experience, rules, Grid, language or NanoCell. Do not spawn subagents.

## Required CLI

```text
bruce evidence create --train FILE --holdout FILE --output FILE
    [--domain bigint|rational] [--min N] [--max N] [--max-bits N]
    [--max-denominator N] [--max-trace-bytes N] [--max-candidates N]
    [--max-operations N] [--max-trace N] [--max-results N]
bruce evidence replay CHAIN
```

Add the command after `language` and before `research`. Update the CLI help with both syntax lines.

For `create`, use only the listed whitelist, default domain `bigint`, and force `task.strategy="affine"`, `task.planner="enumerate"`. Read each CSV with `read_bounded(path, 1024*1024)`, parse raw bytes with `parse_exact_samples` into the chosen `NumberDomain`, call `create_exact_evidence_chain`, pack, then exclusively export only after success. Print `render_exact_evidence_chain`.

For `replay`, require exactly one positional artifact, read it with `read_bounded`, call `replay_exact_evidence_chain`, print `render_exact_evidence_chain`, and never write anything.

Do not permit `--strategy`, `--planner`, `--verification`, generic session control, Experience, Grid, Rule, transfer, or publication options. Do not call generic `replay` or `execute_cli`.

## TDD and tests

Write `tests/integration/test_evidence_chain.py` first. It must exercise real CLI behavior with temporary files and cover:

1. a positive rational affine run, exact evidence literals, and nested `general_proof:false`;
2. deleting both original CSV files, then successful `evidence replay`;
3. a hold-out counterexample yielding `REFUTED` and identical replay;
4. x-overlap rejection with no exported file;
5. rejection of unsupported `--strategy` and preservation of an existing output file after failure;
6. rehashed mutations of policy, scope, independence, CSV, native bundle and result rejected;
7. `--max-operations 1` yielding and replaying `INCOMPLETE`.

Register `evidence_chain_workflows` using Python 3 and `$<TARGET_FILE:bruce>`. Before implementation run the direct workflow and record the expected missing-command RED failure. After implementation attempt:

```bash
cmake --build build --target bruce
python3 tests/integration/test_evidence_chain.py build/bruce
ctest --test-dir build -R '^evidence_chain_(contracts|workflows)$' --output-on-failure
ctest --test-dir build -R '^(exact_domain_workflows|power_research_workflows|cli_workflows|evidence_chain_contracts|evidence_chain_workflows)$' --output-on-failure
```

`pkg-config` and `gmpxx.h` are missing. Do not install dependencies or bypass that restriction; report all resulting verification limitations without calling skipped tests passing.

## Report

Write a detailed report to `docs/superpowers/sdd/exact-evidence-chain-v1/task-2-report.md` via `apply_patch`: files changed, RED and GREEN command/result, output/error contract checks, and the exact verification limit. Return only `DONE` or `DONE_WITH_CONCERNS`, report path, one-line test summary, and concerns.
