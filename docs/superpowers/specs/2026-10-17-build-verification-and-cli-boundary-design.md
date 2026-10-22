# Build verification and CLI-boundary consolidation

**Status:** PROPOSAL — implementation begins only after this specification is
reviewed and its implementation plan is approved.

## Purpose

Restore a reproducible current-source verification path, make the distinction
between local generated build artifacts and source evidence explicit, reduce
the size of the top-level CLI dispatch boundary without changing any public
command contract, and document the security boundary for local inspection and
remote artifact-store features.

## Current evidence and problem statement

The current CMake project declares Core 0.102.0 and registers CTest coverage.
The local `build/bruce` binary predates `CMakeLists.txt` and
`apps/bruce_cli.cpp`, so it cannot establish the behavior of the current
source. A fresh configuration through `scripts/with-host-deps.sh` currently
fails because the imported GMP target contains an invalid include directory.
The failure must be diagnosed from the generated pkg-config/CMake values; no
dependency archive, generated build directory, or user-local package is to be
deleted or overwritten as part of this work.

## Scope

1. Diagnose and correct the project-owned host-dependency wrapper or CMake
   integration so a fresh out-of-tree build works from a native path.
2. Add a documented, non-mutating verification sequence: configure, build,
   CTest, generated CLI-documentation comparison in a temporary copy, and
   local Markdown-link checking.
3. Clarify that `build*/`, `build-host-deps/`, and `runs/` are local evidence
   or dependency work areas, not current-source implementation evidence.
4. Extract the top-level command-selection branches in `apps/bruce_cli.cpp`
   into internal command-family adapters while preserving command spellings,
   stdout/stderr JSON schemas, exit codes, compile-time feature gates, and
   command order where it affects parsing.
5. Add a current security-boundary document covering Cockpit and remote
   artifact stores: local binding, metadata-only handling, no arbitrary shell
   execution, explicit file/URL inputs, bounded reads, and the condition that
   external exposure requires a separate authentication/authorization design.

## Non-goals

- No new MCP server, API server, authentication system, database, Docker
  deployment, dynamic executable plugin loader, or remote worker protocol.
- No behavior change to existing BRUCE CLI commands or persisted formats.
- No deletion, regeneration, or checked-in update of `build*/`, `runs/`, or
  generated CLI artifacts by hand.
- No assertion that a successful test suite proves a general mathematical
  result.

## Design

### Verification boundary

The dependency wrapper remains a child-process-only environment adapter. The
implementation will first record the exact pkg-config variables and generated
GMP include paths that cause the failure. The smallest project-owned correction
will then be selected: repair only an erroneous wrapper environment setting or
add CMake-side validation with a clear error, never rewrite the bundled
dependency tree. A pristine `/tmp` build directory is configured and built.

The verification sequence uses the newly built `bruce`, not the existing
`build/bruce`. Generated CLI documents are verified only through
`docs/cli/verify_examples.py` in a temporary complete copy, then compared
against tracked artifacts. Markdown links are checked by the repository's
existing local checker or the CI-equivalent offline tool when available.

### CLI boundary

Internal adapter functions will group existing command families, for example
reference demos/schedulers, artifact stores, runtime/mod workflows, and core
application commands. `main()` remains the only process entry point and keeps
the outer JSON exception handling. Every branch retains its exact existing
command and argument validation behavior. Tests must demonstrate unchanged
success and error outputs for representative commands in each extracted family.

### Security boundary

A current document will state that Cockpit is a local, read-only metadata
inspector bound to loopback; that local artifacts and bundles may contain
confidential bytes; that remote artifact-store features accept only explicit,
bounded inputs; and that no feature may translate model/user text into an
arbitrary subprocess invocation. It will identify external network exposure
as an open architectural decision requiring a separate authentication,
authorization, rate-limit, audit, and threat-model design.

## Compatibility and failure behavior

The changes preserve all public CLI command spellings, option names, JSON
contracts, file formats, version numbers, static-mod configuration switches,
and nonzero failure behavior. A bad host-dependency setup must fail early with
an actionable diagnostic rather than configuring a build that later compiles
against unintended host headers.

## Tests and acceptance evidence

- Fresh configure/build/CTest with the documented native-path wrapper.
- Existing integration tests for representative command families.
- CLI document verification in a temporary copy; generated files remain
  unchanged unless their generator produces a deliberate, reviewed update.
- Local Markdown-link verification.
- `git diff --check` and status when Git metadata is available. The current
  supplied tree has no usable `.git` directory, so that limitation must be
  reported rather than hidden.

## Rollback

All source changes are independently revertible. No generated build directory
or dependency tree is modified. If the wrapper diagnosis shows that the defect
is solely in the externally supplied dependency bundle, retain the diagnostic,
leave source behavior unchanged, and document the environment blocker.
