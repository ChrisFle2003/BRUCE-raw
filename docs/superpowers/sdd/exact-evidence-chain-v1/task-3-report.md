# Task 3 report — Contract Documentation and Verification Record

**Status:** Documentation updated; added C++/CLI evidence-chain code remains uncompiled and unexecuted in this environment. This is not a fresh-build acceptance or a completed, verified P0 package.

## Files changed

- `docs/cli/commands.md`: Added the exact `bruce evidence create|replay` CLI surface, `bigint` default, fixed Exact `affine`/`enumerate` route, local replay, CSV-byte binding, excluded options and claim/gate boundaries.
- `docs/cli/formats.md`: Added the `BRUCE_EVIDENCE_CHAIN_V1` ordered header/fixed fields, decimal byte-length blocks, raw embedded CSV/native bytes, local bounded replay and disjointness interpretation.
- `docs/cli/results-and-control.md`: Added the direct JSON envelope fields, native-status preservation and explicit proof/gate boundaries.
- `docs/audit/2026-10-01-core-0102-ist-gegen-zielbild.md`: Distinguished the pre-change build evidence from the new source-level Chain implementation; retained fresh build/test verification as outstanding and listed the exact command trio.
- This report records the scoped work and checks. No C++ code, CMake, tests, fixtures, plans or specs were edited for Task 3.

## Checks performed

| Command | Result and limit |
| --- | --- |
| `sed -n '1,240p' docs/superpowers/sdd/exact-evidence-chain-v1/task-3-brief.md` | Exit 0; Task 3 requirements read before editing. |
| `sed -n '180,202p' apps/bruce_cli.cpp` and `sed -n '425,477p' apps/bruce_cli.cpp` | Exit 0; source shows the documented create option whitelist, defaults and single-argument replay. Read-only source inspection, not execution. |
| `sed -n '1,260p' src/evidence/chain.cpp` | Exit 0; source shows the ordered fields, raw-byte blocks, fixed route and replay binding. Read-only source inspection, not compilation. |
| `rg -n -C 2 'Exact-Affine-Evidenzkette\|Exact-Affine-Evidenzergebnis\|Ausstehende Verifikation\|Exact-Affine-Evidence-Chain V1' docs/cli/commands.md docs/cli/formats.md docs/cli/results-and-control.md docs/audit/2026-10-01-core-0102-ist-gegen-zielbild.md` | Exit 0; all new sections and audit verification entry found. Here `\|` is Markdown-table escaping for the command's literal `|`. |
| `rg -n 'declared-disjoint-v1\|general_proof\|cmake -S \. -B build\|cmake --build build --parallel 2\|ctest --test-dir build --output-on-failure' docs/cli/commands.md docs/cli/formats.md docs/cli/results-and-control.md docs/audit/2026-10-01-core-0102-ist-gegen-zielbild.md` | Exit 0; matches were reviewed for explicit negation/qualification and the required outstanding commands. The historical 128/128 result is labeled pre-Chain. This is a statement-boundary check, not a build/test result. |
| `find /usr/include /usr/local/include -name gmpxx.h -print` | Exit 0, no output; header absent from those standard include locations. |
| `command -v pkg-config` | Exit 1, no output; executable unavailable on PATH. |
| `git status --short` | Exit 128, `fatal: not a git repository`; no Git diff/status claim is made. |

## Statement-boundary check

The documentation describes only the source-implemented, approved `evidence create|replay` interface. `declared-disjoint-v1` is explicitly limited to technical x-disjointness and a local declaration, not externally established independence or provenance. Candidate, score, `VERIFIED_ON_SAMPLES`, digest and replay are explicitly excluded as general mathematical proof. The sections exclude Learn/Publish/rule-activation gates, a generic replay or `ApplicationTask` route, external checker, cloud/publication, Strategy promotion and BSGS/Private-Key/Mining claims. Existing build/test evidence is not presented as verification of the added Chain.

## Remaining blocker and exact outstanding verification

The environment lacks `pkg-config` and the GMP development header `gmpxx.h`; the added C++/CLI code has not been compiled or executed here. No dependencies were installed. After the existing project dependencies are available, run and record the actual results of:

```bash
cmake -S . -B build
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

None of this command trio was run as Task 3 verification. The earlier audit's 128/128 tests apply only to the pre-Chain existing build; they cannot establish Chain runtime behavior.

## Review fix round 1 — declared scope versus executed coverage

The reviewer identified an overstatement in the original `docs/cli/formats.md` scope explanation. The fixed literal `countercheck_scope every-enumerated-candidate/all-training-and-holdout-samples` remains unchanged, but the format and result documentation now call it a **declared policy/scope**, not a claim that each enumerated candidate was fully checked against every sample. The audit carries the same qualification. Source inspection of `src/application/exact_research.cpp` shows first-contradiction short-circuit, an operation-budget stop within a candidate, and holdout entry only after training passes and result budget remains. The text now explicitly states that `INCOMPLETE` and contradiction do not imply complete coverage, and directs readers to status, stop reason, counters and trace for actual work.

Documentation/read-only checks for this fix:

| Command | Result and limit |
| --- | --- |
| `sed -n '95,142p' src/application/exact_research.cpp` | Exit 0; inspected the concrete control flow above. Source inspection only, not compilation or execution. |
| `rg -n '^countercheck_scope every-enumerated-candidate/all-training-and-holdout-samples$' docs/cli/formats.md` | Exit 0, line 47; required fixed literal preserved. |
| `rg -n 'countercheck_scope\|Kontradiktion\|INCOMPLETE\|Resultatbudget\|Vollabdeckung\|vollständige Sampleabdeckung' docs/cli/commands.md docs/cli/formats.md docs/cli/results-and-control.md docs/audit/2026-10-01-core-0102-ist-gegen-zielbild.md` | Exit 0; reviewed the qualified policy and incomplete-coverage wording in the format, result and audit pages. Markdown-table escaping turns the command's `|` into `\|` here. |
| `command -v pkg-config` | Exit 1, no output; build dependency blocker persists. |

No fresh configure, build or test was run in this fix round. The three outstanding commands listed above remain outstanding, and the new C++/CLI behavior remains unverified in this environment.
