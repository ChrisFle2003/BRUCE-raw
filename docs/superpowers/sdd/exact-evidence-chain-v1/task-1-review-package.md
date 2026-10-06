# Task 1 review package — source-based fallback

The workspace is not a Git repository, so no commit-range diff can be produced. Review the current complete versions of these changed files against the Task 1 brief and approved spec:

- `include/bruce/evidence/chain.hpp`
- `src/evidence/chain.cpp`
- `tests/unit/evidence_chain_tests.cpp`
- `CMakeLists.txt` (only the `src/evidence/chain.cpp` registration and `bruce_evidence_chain_tests` block)

Implementer report: `docs/superpowers/sdd/exact-evidence-chain-v1/task-1-report.md`.

Verification evidence is limited: the new code was not compiled because `pkg-config` and GMP development headers are unavailable. The reviewer must distinguish source-level concerns from runtime behavior that cannot be verified in this environment.

Binding constraints: header and literals must match the Task 1 brief exactly; only affine/enumerate/bigint-or-rational routes; original raw CSV bytes and ordered sample binding; unchanged `BRUCE_EXACT_BUNDLE_V1`; strict framing/digest/length/replay equality; `general_proof:false`; no gates; no unrelated files.
