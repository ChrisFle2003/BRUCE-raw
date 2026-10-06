# Task 2 fix-round 1 re-review package

Review only the Task-2 test changes addressing these two findings:

1. The outer Chain `RESULT` must be mutated separately from the embedded native bundle, with only the outer digest recomputed, so replay reaches the Chain result-binding check.
2. The overlap workflow must assert `DuplicateOrOverlappingSampleInput` and no export.

Read the original Task 2 brief, the Task 2 report (especially “Review fix round 1”), and inspect `tests/integration/test_evidence_chain.py:79-132`. Do not review unrelated code and do not modify files.

The rebuilt binary remains unavailable because `pkg-config` and `gmpxx.h` are missing. Verdict each finding as ADDRESSED or NOT ADDRESSED based on the test source; flag only new breakage in this scoped range.
