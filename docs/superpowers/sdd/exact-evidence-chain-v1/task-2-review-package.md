# Task 2 review package — source-based fallback

The workspace is not a Git repository, so no commit-range diff can be produced. Review the current complete versions of these changed files against the Task 2 brief and approved spec:

- `apps/bruce_cli.cpp` (help addition and `command=="evidence"` branch only)
- `tests/integration/test_evidence_chain.py`
- `CMakeLists.txt` (only the `evidence_chain_workflows` registration)

Implementer report: `docs/superpowers/sdd/exact-evidence-chain-v1/task-2-report.md`.

Verification evidence is limited: the rebuilt CLI cannot be produced because `pkg-config` and GMP development headers are unavailable. Review source scope, option whitelist, raw-byte binding, exclusive export ordering, replay arity, test adequacy and absence of general dispatcher/gate integration. Do not treat unavailable executable verification as a test pass.

Binding constraints: only `evidence create|replay`; `bigint` default and `bigint|rational` only; fixed affine/enumerate route; raw CSV bytes read at 1 MiB then parsed; Task 1 API only; no generic replay/execute_cli; all disallowed options must remain unaccepted; no non-proof or activation claim.
