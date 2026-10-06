# Task 2 — Bounded CLI Create and Replay

## Changes

- `tests/integration/test_evidence_chain.py`: six real-CLI temporary-file tests covering rational create and evidence literals, nested `general_proof:false`, replay after source deletion, hold-out `REFUTED`, overlapping-x rejection without export, unsupported `--strategy` with existing-output preservation, six rehashed artifact mutations (policy, scope, independence, CSV, native bundle, result), strict replay arity, and `--max-operations 1` yielding/replaying `INCOMPLETE`.
- `apps/bruce_cli.cpp`: added the `evidence` command after `language` and before `research`, plus help syntax. `create` accepts only the specified option whitelist, defaults to bigint, fixes affine/enumerate routing, reads both CSVs at a 1 MiB limit, parses raw bytes with `parse_exact_samples`, creates and packs an exact evidence chain, and exports exclusively only after those steps succeed. `replay` requires one positional chain, performs a bounded read and chain replay, renders the chain, and does not write.
- `CMakeLists.txt`: registers `evidence_chain_workflows` with Python 3 and `$<TARGET_FILE:bruce>`.

## TDD record

The integration test was written before CLI or CMake implementation. Initial RED command:

```text
python3 tests/integration/test_evidence_chain.py build/bruce
```

Result: exit 1; six tests ran, five failed, one passed. All five failures were attributable to the missing command (`{"error":"UnknownCommand: evidence"}`); the one pass only checked rejection/no-output and is not considered a behavior pass for the new command. This is the expected missing-command RED state.

After implementation, the same direct workflow was attempted again (exit 1; six ran, five failed). The binary at `build/bruce` is still the old binary because the build cannot regenerate. The same `UnknownCommand: evidence` error persists. **No GREEN result is claimed.**

## Output and error contracts

The new tests specify these consumer-visible checks, but they remain unverified against a rebuilt CLI:

- Successful create must emit the chain JSON with exact evidence literals, sample counts, `VERIFIED_ON_SAMPLES`, and nested `general_proof:false`; the artifact must contain the chain and native bundle headers plus the fixed policy literals.
- Replay after deleting both CSV source files must reproduce the same JSON and reject an extra positional argument.
- A counterexample must emit `REFUTED` and replay identically; a one-operation budget must emit `INCOMPLETE` with `OperationBudgetExceeded` and replay identically.
- Overlapping x-values must produce an error with no new file. Unsupported `--strategy` must produce `UnknownOption` while leaving an existing output byte-for-byte intact.
- Rehashed tampering of policy, scope, independence, CSV, native bundle, and result must return an error.

The implementation is scoped to the evidence-chain API and does not call generic `replay` or `execute_cli`. The create option parser excludes strategy, planner, verification, session control, Experience, Grid, Rule, transfer, and publication options.

## Verification and exact limit

```text
cmake --build build --target bruce
  exit 1: CMake regeneration fails at find_package(PkgConfig):
  "Could NOT find PkgConfig (missing: PKG_CONFIG_EXECUTABLE)".

c++ -std=c++20 -fsyntax-only -Iinclude -Ibuild/generated apps/bruce_cli.cpp
  exit 1: include/bruce/math/numbers.hpp:3:10: fatal error: gmpxx.h: No such file or directory.

python3 -c 'import ast, pathlib; ast.parse(pathlib.Path("tests/integration/test_evidence_chain.py").read_text()); print("integration test syntax OK")'
  exit 0: integration test syntax OK.

python3 tests/integration/test_evidence_chain.py build/bruce
  exit 1: six ran, five failed; stale binary reports UnknownCommand: evidence.

ctest --test-dir build -R '^evidence_chain_(contracts|workflows)$' --output-on-failure
  exit 0 but "No tests were found!!!"; this is not a pass of either evidence-chain test.

ctest --test-dir build -R '^(exact_domain_workflows|power_research_workflows|cli_workflows|evidence_chain_contracts|evidence_chain_workflows)$' --output-on-failure
  exit 0: only three previously configured tests ran (`cli_workflows`,
  `exact_domain_workflows`, `power_research_workflows`), all passed. Neither
  evidence-chain test was discovered, so neither is counted as passed.
```

The missing `pkg-config` executable and `gmpxx.h` prevent rebuilding and therefore prevent GREEN verification or compilation of the changed CLI. No dependencies were installed and no workaround was used. A successful CMake reconfiguration/build and rerun of the direct and CTest workflows remain necessary in an environment with those existing project dependencies available.

## Review fix round 1

The integration test now targets the chain's **final** `RESULT\n` block separately: it splits the outer envelope, uses the last `RESULT\n` marker, changes only that block's `"general_proof":false` to `"general_proof":true ` (same byte length), and recomputes only the outer SHA-256 digest. All bytes before the final marker, including the native bundle and its digest, remain unchanged. Replay must reject it with the chain-level diagnostic `ReplayMismatch: evidence chain differs`. The previous first-occurrence mutation remains as separate native-result tampering coverage.

The overlap test now requires the validator's exact `DuplicateOrOverlappingSampleInput` diagnostic as well as absence of an exported file, rather than accepting any error.

Checks after this test change:

```text
python3 -c 'import ast, pathlib; ast.parse(pathlib.Path("tests/integration/test_evidence_chain.py").read_text()); print("integration test syntax OK")'
  exit 0: integration test syntax OK.

python3 tests/integration/test_evidence_chain.py build/bruce
  exit 1: six ran, six failed. The old build/bruce still returns
  {"error":"UnknownCommand: evidence"}; the stricter overlap test now also
  fails for this reason. Neither the outer-RESULT replay diagnostic nor the
  overlap diagnostic can be validated against this stale binary.
```

The CMake `pkg-config` and header `gmpxx.h` blockers above are unchanged. No dependencies were installed or bypassed.
