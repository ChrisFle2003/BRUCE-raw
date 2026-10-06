# Task 1 report — Exact-Evidence Domain Codec

## Files changed

- `include/bruce/evidence/chain.hpp`: public chain run type and create/render/pack/replay API.
- `src/evidence/chain.cpp`: fixed V1 contract framing, raw CSV sample binding, native exact-bundle replay, canonical result rendering, full-body equality check, byte limits, and preserved native status.
- `tests/unit/evidence_chain_tests.cpp`: rational round-trip, fixed literals and order, `general_proof:false`, replay without original source variables, overlap and route rejection, rehashed policy/CSV/result mutations, trailing data, and incomplete-budget replay.
- `CMakeLists.txt`: adds the codec to `bruce_core` and registers `evidence_chain_contracts`.

No CLI, legacy bundle, fixtures, dispatcher, gate, Experience, or rule files were changed.

## RED evidence

Tests were written before the codec implementation. `cmake -S . -B build` exited 1: `Could NOT find PkgConfig (missing: PKG_CONFIG_EXECUTABLE)`. The direct test compile before the new public header existed, `c++ -std=c++20 -Iinclude -fsyntax-only tests/unit/evidence_chain_tests.cpp`, exited 1 with `fatal error: bruce/evidence/chain.hpp: No such file or directory`, the expected missing-feature RED. After adding only the public declaration, a direct link attempt against the pre-existing `build/libbruce_core.a` exited 1 earlier in compilation because `gmpxx.h` is absent. Thus an executable assertion-level RED run was not possible in this environment.

## Verification after implementation

| Command | Result |
|---|---|
| `cmake -S . -B build` | Failed during configure: `PKG_CONFIG_EXECUTABLE` missing. |
| `cmake --build build --target bruce_evidence_chain_tests` | Failed while regenerating CMake for the same missing `PkgConfig`; no new target was built. |
| `ctest --test-dir build -R '^evidence_chain_contracts$' --output-on-failure` | Exit 0, but `No tests were found!!!` because the stale build has no registration. This is **not** a test pass. |
| `ctest --test-dir build -R '^(exact_domain_contracts\|research_replay_control\|application_task_adapters\|evidence_chain_contracts)$' --output-on-failure` | Existing `exact_domain_contracts`, `application_task_adapters`, and `research_replay_control` passed (3/3); new chain test did not run. |

The pre-existing static library cannot serve as a source-build workaround: the system also lacks GMP development headers (`gmpxx.h`). No dependencies were installed or changed. New codec compilation and behavior remain unverified until the environment has its required existing build dependencies and CMake can regenerate.

## Concerns

- The source was inspected against the approved V1 framing and task brief, but no new-code compile or executable test could complete here. A fresh build and the requested focused/baseline CTest commands are still required in a dependency-complete environment.
