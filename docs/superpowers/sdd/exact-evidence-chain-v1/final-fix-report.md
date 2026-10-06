# Final fix report — Exact Evidence Chain V1

## 2026-10-01 — single final fix wave

**Status:** Requested source/test/documentation corrections applied; fresh C++ build and Chain runtime verification remain blocked. This is not a passing Chain test result or P0 acceptance.

Read the final-fix brief, approved design, implementation plan, ledger and final-review package before editing. Applied the binding scope ruling without altering the Exact engine or native codec. Used `apply_patch` for all authored file edits; installed no dependencies, spawned no agents, and changed no unrelated systems.

### Files changed in this wave

1. `src/evidence/chain.cpp` — replaced the scope in the ordered body, canonical JSON and exact replay comparison; header, policy, field order, parser and native execution behavior remain unchanged.
2. `tests/unit/evidence_chain_tests.cpp` — changed literal assertions; added precise diagnostic assertions and length-aware native substitution; added framing, size, valid non-Affine native and semantic native-replay cases described below.
3. `tests/integration/test_evidence_chain.py` — changed scope assertions/mutations; added an outer rehash helper and exact-diagnostic trailing-payload/duplicate-field cases.
4. `docs/superpowers/specs/2026-10-01-exact-evidence-chain-design.md` — replaced both format/JSON scope values, removed the false all-candidate/all-sample success criterion, documented actual short-circuit semantics and expanded acceptance cases.
5. `docs/superpowers/plans/2026-10-01-exact-evidence-chain.md` — replaced the fixed scope, added immutable-engine execution semantics and final parser/native-replay test requirements.
6. `docs/cli/commands.md` — added the fixed scope/policy and precise execution limits.
7. `docs/cli/formats.md` — updated exact framing and short-circuit scope interpretation.
8. `docs/cli/results-and-control.md` — updated JSON scope and status/coverage interpretation.
9. `docs/cli/source-index.md` — manually added the Evidence CLI option branch and all 12 new diagnostic literals/source references; explicitly disclaimed automatic generator verification.
10. `docs/audit/2026-10-01-core-0102-ist-gegen-zielbild.md` — named the truthful scope and actual execution limits while preserving the fresh-build blocker and non-acceptance statement.
11. `docs/superpowers/sdd/exact-evidence-chain-v1/final-fix-report.md` — this report.

No change was needed to `include/bruce/evidence/chain.hpp`, `apps/bruce_cli.cpp` or `CMakeLists.txt`. No native Exact engine, historical codec/fixture, dispatcher, gate, rule, Experience, grid, language or NanoCell implementation was edited. Configure/test commands may refresh their normal build logs/cache/test artifacts; the tutorial created its normal temporary artifacts under `/tmp/bruce-cli-tutorial-j7rssO`.

### Current contract and preserved boundaries

```text
BRUCE_EVIDENCE_CHAIN_V1
countercheck_policy affine-inverse-v1
countercheck_scope short-circuit-training-then-holdout-v1
```

The full framing still requires contract, independence, countercheck policy, countercheck scope, training block, holdout block, native block and canonical RESULT in their original exact order. Raw CSV bytes are retained and bound to native samples.

Each reached candidate is checked training-first. A contradiction stops that candidate. Inverse countercheck runs only after a matching forward evaluation. Holdout begins only after training passes and when result/budget flow reaches it. Budgets can stop within a candidate. `INCOMPLETE` is not coverage, and the scope does not claim all candidates or all samples were checked.

`declared-disjoint-v1` remains a local declaration plus technical x-disjointness, not external independence or provenance. `general_proof` remains false; no new proof claim, activation/publish/learn gate, external checker or generic application route was introduced.

### Tests added/strengthened and intended rejection paths

All paths below are source-reviewed expectations, **not executed Chain test results**. The environment cannot compile the new code.

| Test input | Digest handling | Intended path / exact diagnostic |
| --- | --- | --- |
| Policy, scope or independence value changed | Outer digest recomputed | Fixed-value validation: `UnknownEvidenceChainPolicy` |
| Training length `not-a-number` | Outer recomputed | Reader integer conversion: `InvalidInteger: not-a-number` |
| Training length one byte too short | Outer recomputed | Block boundary becomes invalid before holdout field: `InvalidEvidenceChainField` |
| Missing independence field | Outer recomputed | Ordered Reader field validation: `InvalidEvidenceChainField` |
| Duplicate independence field | Outer recomputed | Ordered Reader expects countercheck policy: `InvalidEvidenceChainField` |
| Independence and policy fields swapped | Outer recomputed | Ordered Reader expects independence: `InvalidEvidenceChainField` |
| Training declared length 1048577 | Outer recomputed | 1 MiB block bound: `InvalidEvidenceChainBlockSize` |
| Native declared length `max_file_bytes + 1` | Outer recomputed | Native block bound: `InvalidEvidenceChainBlockSize` |
| Envelope expanded to `max_file_bytes + 1` | Outer recomputed | Early whole-envelope bound: `BundleByteLimit` (intentionally before digest/parser) |
| Native RESULT proof flag changed, inner digest left stale | Outer recomputed | Existing inner integrity rejection retained: `InvalidEvidence: digest mismatch` |
| Native RESULT proof flag changed, inner body digest recomputed, native byte count and outer digest recomputed | Both valid | Native `replay_exact_research` reconstructs its payload: `ReplayMismatch: exact input, result or trace differs` |
| Real power-affine run packed as a valid native power bundle | Native bundle independently replayed and compared first; outer recomputed after length-aware embedding | Chain's Exact-V1 native boundary: `EvidenceChainRequiresExactNativeBundle` |
| Outer RESULT changed without touching native payload | Outer recomputed | Canonical Chain reconstruction: `ReplayMismatch: evidence chain differs` |
| Trailing outer payload | Outer recomputed | Canonical Chain reconstruction: `ReplayMismatch: evidence chain differs` |

The unit test also retains the un-rehashed trailing-byte rejection, normal rational round-trip, source-string disposal, no-proof assertion, CSV binding/overlap/route checks and unchanged `INCOMPLETE` replay. Field-order assertions now include scope → training → holdout → native.

The integration test adds `test_rehashed_trailing_payload_and_duplicate_field_reach_chain_parser`: its two subcases expect exactly `ReplayMismatch: evidence chain differs` and `InvalidEvidenceChainField`. It recomputes the outer digest, so a generic digest mismatch cannot satisfy either assertion. Existing rational, refuted, overlap, exclusive-output, mutation and incomplete workflows remain.

### Commands and observed results

Commands were run from `/home/chris/Desktop/Bruce_raw` unless shown otherwise.

| Command | Observed result and limit |
| --- | --- |
| `git status --short` | Exit 128: not a Git repository. No diff, commit or branch verification claimed. |
| `cmake -S . -B build` | Exit 1: `Could NOT find PkgConfig (missing: PKG_CONFIG_EXECUTABLE)` at `CMakeLists.txt:7`; configuration incomplete. |
| `cmake --build build --parallel 2` | Exit 1: regeneration fails on the same missing PkgConfig; Ninja cannot rebuild `build.ninja`. |
| `cmake --build build --target bruce_evidence_chain_tests bruce` | Exit 1: same regeneration/dependency blocker. Neither new target was compiled. |
| `c++ -std=c++20 -Iinclude -fsyntax-only tests/unit/evidence_chain_tests.cpp src/evidence/chain.cpp` | Exit 1: both translation units stop at `include/bruce/math/numbers.hpp:3`, `fatal error: gmpxx.h: No such file or directory`. No C++ syntax-pass claim. |
| `ctest --test-dir build -R '^evidence_chain_(contracts\|workflows)$' --output-on-failure --no-tests=error` | Exit 8: no tests found in the stale registration. The escaped pipe here is Markdown-table escaping. Neither Chain test executed. |
| `python3 tests/integration/test_evidence_chain.py build/bruce` | Exit 1, 7 failures; all reach the stale binary's `UnknownCommand: evidence`, not current parser/replay code. This is not a RED/GREEN demonstration of the new implementation. |
| `bash -n docs/cli/tutorial.sh` | Exit 0; shell syntax only. |
| `python3 -c 'import ast; from pathlib import Path; ast.parse(Path("tests/integration/test_evidence_chain.py").read_text()); ast.parse(Path("docs/cli/verify_examples.py").read_text()); print("Python AST and tutorial shell syntax: OK")'` | Exit 0; Python syntax only, no Chain execution. This was run after the shell syntax check using `&&`. |
| `bash docs/cli/tutorial.sh ./build/bruce` | Exit 0; existing tutorial succeeds using the stale binary, artifacts at `/tmp/bruce-cli-tutorial-j7rssO`. No new Chain coverage. |
| `command -v pkg-config` | Exit 1, no executable on PATH. |
| `rg -n 'every-enumerated-candidate/all-training-and-holdout-samples' src include tests docs/cli docs/audit docs/superpowers/specs docs/superpowers/plans` | Exit 1, no matches in active source/tests/docs. |
| `rg '(src/application/exact_research.cpp\|src/application/exact_replay.cpp\|tests/fixtures/)' build/source-files.sha256 \| sha256sum -c --quiet` | Exit 0; selected recorded Exact engine/codec/fixture hashes match. Pipes are Markdown-table escaped here. This does not hash-bind the added Chain to the old build. |

The direct workflow failures were: `test_holdout_counterexample_is_refuted_and_replays`, `test_operation_budget_incomplete_and_replay`, `test_overlap_rejects_without_export`, `test_rational_literals_proof_and_source_independent_replay`, `test_rehashed_policy_scope_independence_csv_native_and_result_mutations_reject`, `test_rehashed_trailing_payload_and_duplicate_field_reach_chain_parser`, and `test_unsupported_option_preserves_existing_output`. All are blocked by the missing command in the stale binary; no assertion about fresh Chain runtime behavior follows.

### Read-only documentation check

The following final command exited 0 and printed: `Manual read-only documentation checks: 17 Chain/CLI diagnostics catalogued; 12 new source references and CLI relative links valid. No generator run.`

```bash
python3 - <<'PY'
from pathlib import Path
import re
root = Path.cwd()
index = (root / 'docs/cli/source-index.md').read_text()
chain = (root / 'src/evidence/chain.cpp').read_text()
cli = (root / 'apps/bruce_cli.cpp').read_text()
cli = cli.split('} else if(command=="evidence") {', 1)[1].split('} else if(command=="research") {', 1)[0]
messages = set(re.findall(r'Error\("([^"\n]+)"', chain + cli))
for message in messages:
    assert f'`{message.replace("|", "&#124;")}`' in index, message
for path in (root / 'docs/cli').glob('*.md'):
    for target in re.findall(r'\]\(([^)]+)\)', path.read_text()):
        if target.startswith(('http:', 'https:', '#')):
            continue
        assert (path.parent / target.split('#')[0]).exists(), (path, target)
new_messages = {m for m in messages if 'Evidence' in m or 'evidence' in m} - {'InvalidEvidence: digest mismatch'}
for row in index.splitlines():
    match = re.match(r'\| `([^`]+)` \| \[([^\]]+):(\d+)\]', row)
    if not match:
        continue
    diagnostic, source, line = match.groups()
    diagnostic = diagnostic.replace('&#124;', '|')
    if diagnostic in new_messages:
        assert diagnostic in (root / source).read_text().splitlines()[int(line)-1], row
print(f'Manual read-only documentation checks: {len(messages)} Chain/CLI diagnostics catalogued; {len(new_messages)} new source references and CLI relative links valid. No generator run.')
PY
```

An initial broad source-reference check hit the existing stale `EquivalenceRequiresExpressionStrategy` CLI line reference; narrowed the check to the new Chain/CLI diagnostics in scope. That check then identified inaccurate newly entered Chain line references, which were corrected before the final successful command. Unrelated historical index line labels were not regenerated.

`python3 docs/cli/verify_examples.py ./build/bruce` was deliberately not run: inspection shows it regenerates `source-index.md`, `examples.json` and `validation.md` and would overwrite scoped manual changes / unrelated generated artifacts while using a stale binary. No automatic generator verification is claimed. The read-only link/diagnostic check and existing tutorial above are the documentation checks performed.

### Historical SDD records and remaining blocker

Per the coordinator's explicit final-wave ruling, the following historical audit records were preserved unchanged despite containing the superseded scope text:

- `docs/superpowers/sdd/exact-evidence-chain-v1/progress.md:23` — records the ruling to replace it.
- `docs/superpowers/sdd/exact-evidence-chain-v1/task-1-brief.md:44` — historical task input, not the current contract.
- `docs/superpowers/sdd/exact-evidence-chain-v1/task-3-report.md:44` and `:51` — historical first-fix explanation/check, superseded by this final wave.

The final-fix brief plus corrected active design/plan/source/docs define the current contract. The historical literals are not accepted contract alternatives.

**Fresh build/runtime verification remains blocked by missing `pkg-config` and `gmpxx.h`.** No packages were installed or environment workaround substituted. The new C++ unit cases and current CLI integration behavior remain unexecuted. A dependency-complete fresh configure/build and actual registration/execution of both Chain targets are required before acceptance.

### Completed existing-build regression run

`ctest --test-dir build --output-on-failure` exited 0: **128/128 passed**, zero failures, **211.68 seconds**. This includes adjacent Exact-domain, power, application-adapter and replay-control tests, but neither `evidence_chain_contracts` nor `evidence_chain_workflows` is registered in that stale build. These successful existing-binary regressions do **not** validate the changed Chain source/tests/CLI or satisfy fresh-build acceptance.

After the final unit-test edits, repeated `c++ -std=c++20 -Iinclude -fsyntax-only tests/unit/evidence_chain_tests.cpp src/evidence/chain.cpp`; exit 1 still reports missing `gmpxx.h` for both translation units. `rg -n 'evidence_chain' CMakeLists.txt` confirms source registrations at lines 218–223, not generated/build registration. The final-fix requirements are implemented at source level; fresh Chain compilation and execution remain the outstanding verification concern.
