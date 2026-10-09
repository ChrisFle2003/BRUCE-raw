# English Documentation Overhaul Implementation Plan

> **For agentic workers:** Execute this documentation-only plan in the current checkout. Do not modify source, tests, CMake, CI, generated CLI artifacts, or existing untracked work.

**Goal:** Provide an English, source-grounded documentation entry point and repository map for the current BRUCE_raw core.

**Architecture:** Documentation will direct readers from the root README to current source-backed references, then to contracts and CLI details. Current claims are kept separate from historical, audit, roadmap, ADR, and concept material by using the repository's required status labels. A new structure guide will map source directories and the two distinct mod mechanisms without duplicating generated CLI reference material.

**Tech Stack:** Markdown, CMake/C++ source and CTest registration as evidence, local Markdown-link validation.

**Spec:** User-approved documentation requirements in the 2026-10-09 task conversation.

## Global Constraints

- Use only **IMPLEMENTED AND VERIFIED**, **IMPLEMENTED, BUT NOT CURRENTLY VERIFIED**, **DOCUMENTED / CONCEPT**, **PROPOSAL**, **OPEN DECISION**, and **NOT DERIVABLE FROM THE REPOSITORY** as status labels.
- Treat source, tests, CI, reproducible runs, and generated CLI-validation evidence as the only basis for implementation claims.
- Do not hand-edit `docs/cli/source-index.md`, `docs/cli/examples.json`, or `docs/cli/validation.md`.
- Preserve historical and conceptual files; update only their entry-point labelling where needed.
- Preserve pre-existing untracked files and do not create a commit.

## Review Focus

- CMake has no `BRUCE_BUILD_MOD_COCKPIT`; current documentation must not claim a Cockpit build target or executable.
- Static Language/NanoCell mods and signed runtime data-mod packages must not be conflated with dynamic executable plugins.
- Successful CLI processing must not be described as a complete search or general mathematical proof.
- Every added local Markdown link must resolve from its containing file.
- Generated CLI artifacts remain byte-for-byte untouched.

### Task 1: Replace the root reader entry point

**Files:**
- Modify: `README.md`

- [ ] Write an English README that states scope, evidence status, current build identity, boundaries, setup, CLI starting points, mod distinctions, and next reading.
- [ ] Derive build options and executable names from `CMakeLists.txt`, and CLI claims from `apps/` and `docs/cli/`.
- [ ] Check its local Markdown links.

### Task 2: Normalize documentation navigation

**Files:**
- Modify: `docs/README.md`
- Modify: `docs/{architecture,contracts,implementation,concepts,history,adr,audit,superpowers}/README.md`

- [ ] Replace reader-facing German navigation with concise English taxonomy and status boundaries.
- [ ] Make the Blueprint and control/publication contract the current references, and retain numbered implementation records as historical evidence rather than current claims.
- [ ] Check links and preserve existing archival files.

### Task 3: Establish current architecture references

**Files:**
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/contracts/06-control-and-publication.md`
- Create: `docs/architecture/repository-structure.md`
- Modify: `docs/cli/README.md`

- [ ] Translate the current Blueprint and control/publication contract, correcting only statements contradicted by CMake/source evidence.
- [ ] Add an evidence-scoped directory, dependency, conceptual-flow, authority, and mod-boundary guide.
- [ ] Correct CLI overview links and static-mod description without changing generated artifacts or inventing CLI forms.
- [ ] Check all links in changed files.

### Task 4: Verify the documentation-only change

**Files:**
- Verify: changed Markdown files

- [ ] Run a local Markdown-link check across edited files.
- [ ] Run `git diff --check` and inspect `git status --short`.
- [ ] Run safe repository documentation checks when their dependencies are already available; otherwise report the exact limitation.
