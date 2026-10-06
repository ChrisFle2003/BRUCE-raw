# Task 1 brief — Exact-Evidence Domain Codec

Read this file first. It is the complete requirements source for Task 1 of `docs/superpowers/plans/2026-10-01-exact-evidence-chain.md`; the approved design is `docs/superpowers/specs/2026-10-01-exact-evidence-chain-design.md`.

## Scope

Create only:

- `include/bruce/evidence/chain.hpp`
- `src/evidence/chain.cpp`
- `tests/unit/evidence_chain_tests.cpp`

Modify only the `bruce_core` source list and unit-test registration in `CMakeLists.txt`.

Do not change the CLI, documentation, `BRUCE_EXACT_BUNDLE_V1`, historical fixtures, general application dispatcher, gates, Experience, or rules. Do not spawn subagents.

## Required public interface

```cpp
namespace bruce {
struct ExactEvidenceChainRun {
    ExactResearchRun research;
    std::string training_csv, holdout_csv;
};
ExactEvidenceChainRun create_exact_evidence_chain(
    ExactResearchTask task, std::string training_csv, std::string holdout_csv,
    const std::function<bool(std::uint64_t)>& cancelled = {});
std::string render_exact_evidence_chain(const ExactEvidenceChainRun&);
std::string pack_exact_evidence_chain(
    const ExactEvidenceChainRun&, const ReplayCheckpoint& checkpoint = {});
ExactEvidenceChainRun replay_exact_evidence_chain(
    std::string_view bundle, std::uint64_t operation_limit = 10000000,
    const ReplayCheckpoint& checkpoint = {});
}
```

## Exact constraints

- Header: `BRUCE_EVIDENCE_CHAIN_V1`.
- Fixed fields, strict order, exact values:
  - `contract affine-evidence-chain-v1`
  - `independence declared-disjoint-v1`
  - `countercheck_policy affine-inverse-v1`
  - `countercheck_scope every-enumerated-candidate/all-training-and-holdout-samples`
- Embed length-prefixed original `training_csv`, `holdout_csv`, one native `BRUCE_EXACT_BUNDLE_V1`, then one canonical `RESULT` JSON.
- Only `strategy="affine"`, `planner="enumerate"`, `domain="bigint"|"rational"`; enforce on create and replay.
- Parse retained raw CSV bytes with `parse_exact_samples`; bind their ordered canonical samples exactly to the native task. Validate sample non-emptiness and disjointness through existing Exact validation.
- Native replay is `replay_exact_research`; reconstruct the full Chain body and require exact equality to reject trailing or rehashed semantic mutations.
- Native research status must be retained. Do not create any general proof or gate claim; rendered nested result retains `general_proof:false`.
- Inputs remain 1 MiB each; reject a completed envelope over `max_file_bytes`.
- Reuse established strict length reader/framing behavior from `src/application/grid_research.cpp`; do not modify `src/application/exact_replay.cpp`.

## TDD and tests

Write unit tests before the implementation and run them once to demonstrate the expected missing-feature failure. Then implement minimal code and run:

```bash
cmake -S . -B build
cmake --build build --target bruce_evidence_chain_tests
ctest --test-dir build -R '^evidence_chain_contracts$' --output-on-failure
ctest --test-dir build -R '^(exact_domain_contracts|research_replay_control|application_task_adapters|evidence_chain_contracts)$' --output-on-failure
```

The unit suite must cover: rational successful round-trip; fixed literals; `general_proof:false`; replay after original strings are discarded; overlap rejection; rehashed policy mutation; mismatched embedded CSV; altered native result; and incomplete budget status surviving replay.

The baseline cannot presently regenerate CMake because `pkg-config` is missing. Do not install dependencies or claim a fresh build; run the maximal executable checks available and report the limitation precisely.

## Report

Write a detailed report to `docs/superpowers/sdd/exact-evidence-chain-v1/task-1-report.md` via `apply_patch`: files changed, RED command/result, GREEN commands/results, the exact environment limitation, and any concerns. Return only `DONE` or `DONE_WITH_CONCERNS`, the report path, and a one-line test summary.
