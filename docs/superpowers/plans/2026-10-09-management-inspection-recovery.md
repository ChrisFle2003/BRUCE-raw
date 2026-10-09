# Management Inspection Recovery Plan

**Goal:** Restore the complete, previously implemented read-only management
inspection contract that commit `129ad50` referenced incompletely, so the
baseline C++ build is valid before runtime tree-mod work begins.

**Evidence:** The complete historical contract exists in local commit
`c53c787`: `apps/management_inspection_cli.hpp`,
`include/bruce/application/management_inspection_control.hpp`, tests, and
integration wiring. The current branch has the include and call site but lacks
the adapter at `apps/bruce_cli.cpp:48` and `:1122`.

**Scope:** Restore only the management-inspection controller, CLI adapter,
`rules-status` routing support, its focused tests and CMake registration.
Do not alter runtime-tree code, package code, or unrelated documentation.

## Task 1: Recover and verify the missing management-inspection contract

**Files:**
- Create: `apps/management_inspection_cli.hpp`
- Create: `include/bruce/application/management_inspection_control.hpp`
- Create: `tests/unit/management_inspection_control_tests.cpp`
- Modify: `apps/rule_store.hpp`
- Modify: `CMakeLists.txt`
- Modify only focused existing integration tests when needed to prove both
  `experience inspect` and `rules-status` retain legacy output and activate
  the opt-in management wrapper.

- [ ] Start from a failing build or focused test proving the absent header.
- [ ] Recover the historical header-only controller and CLI adapter from
  `c53c787`; preserve the public strings, precedence, and byte-compatible
  no-option behavior. Do not invent a redesigned controller.
- [ ] Wire `rules-status` through the same adapter so its advertised options
  are actually accepted.
- [ ] Add or recover focused unit/integration regression coverage for exact
  budgets, cancellation precedence, legacy output, error JSON, and read-only
  inputs.
- [ ] Register the unit target using local CMake conventions; run focused
  build/CTest checks plus relevant integration tests when the available host
  dependencies permit them.
- [ ] Commit only these recovery files with:
  `fix: recover management inspection contract`.

