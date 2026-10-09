# Language API Recovery Plan

**Goal:** Restore the complete Language V1/V2/V3 module matching the current
CLI, which was partially imported and currently prevents the default build.

**Evidence:** `apps/bruce_cli.cpp` is byte-identical to local baseline commit
`c53c787`, which contains the matching additive nine-file Language subset.
The active V1 module came from a different import and lacks those types.

## Task 1: Recover and verify the matching Language module

**Files:**
- Restore only these files from `c53c787`:
  `include/bruce/mods/language/language.hpp`,
  `include/bruce/mods/language/language_run.hpp`,
  `mods/language/src/language.cpp`, `mods/language/src/language_run.cpp`,
  `mods/language/src/language_core_binding.cpp`, `mods/language/CMakeLists.txt`,
  `tests/mods/language/language_tests.cpp`,
  `tests/mods/language/language_run_tests.cpp`, and
  `tests/mods/language/test_language.py`.
- Do not import dispatch, Cockpit, NanoCell, Phase-DAG, GPU, or unrelated docs.
- Start by proving the current default compile failure. Restore tests and source
  as one contract, retaining V1 while adding the historical V2/V3 APIs.
- Verify fresh default configuration/build and focused Language CTest targets;
  run the full available suite if possible. Separately verify Language OFF.
- Commit only the recovered Language subset with:
  `fix: recover language v2 and v3 api contract`.

