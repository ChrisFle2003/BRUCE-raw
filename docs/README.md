# BRUCE_raw documentation

This is the authoritative English map for the current BRUCE_raw repository.
It separates current source-backed references from historical records, plans,
ADRs, audits, and concepts. Read a document's status before treating it as a
claim about the current executable.

## How to interpret the documentation

| Area | Role | How to use it |
| --- | --- | --- |
| [architecture/](architecture/README.md) | Current system structure, invariants, and boundaries | Begin with the [Blueprint](architecture/03-blueprint-consolidation.md). Historical architecture files are context only. |
| [contracts/](contracts/README.md) | Behavioral, data, control, publication, and replay boundaries | The [control and publication contract](contracts/06-control-and-publication.md) is the primary current control reference. Contracts still require source and tests for implementation claims. |
| [cli/](cli/README.md) | Authoritative user-facing command, format, tutorial, and validation reference | Use this for actual invocation and file formats; generated artifacts are maintained only by its verifier. |
| [implementation/](implementation/README.md) | Numbered implementation packages, usage records, and acceptance evidence | Retained for traceability. Their date and stated evidence scope matter; they are not a replacement for current source and tests. |
| [audit/](audit/README.md) | Dated repository assessments | Historical snapshots, not a statement about the present working tree. |
| [history/](history/README.md) | Prior root documentation and previous states | Read after current references when reconstructing a decision or change. |
| [concepts/](concepts/README.md) | Non-implemented ideas and design context | **DOCUMENTED / CONCEPT**; never treat as runtime evidence. |
| [adr/](adr/README.md) | Architectural decisions and proposed designs | A decision record explains intent; it is not proof that code exists. |
| [superpowers/](superpowers/README.md) | Planning and specification work records | **DOCUMENTED / CONCEPT**, not runtime or test evidence. |

## Status legend

- **IMPLEMENTED AND VERIFIED** — current repository evidence supports the claim.
- **IMPLEMENTED, BUT NOT CURRENTLY VERIFIED** — source exists without the
  current verification evidence cited for that claim.
- **DOCUMENTED / CONCEPT** — a design or contextual document, not an
  implementation claim.
- **PROPOSAL** — suggested future work.
- **OPEN DECISION** — an unresolved choice.
- **NOT DERIVABLE FROM THE REPOSITORY** — insufficient tracked evidence.

## Recommended reading paths

### CLI users

1. [CLI manual](cli/README.md)
2. [Commands](cli/commands.md), [formats](cli/formats.md), and
   [results and control](cli/results-and-control.md)
3. [Executable tutorial](cli/tutorial.sh)
4. [Control overview](cli/control-overview.md) and
   [local-file rules](cli/local-files.md)

### Contributors

1. [Root README](../README.md) and [project handbook](../AGENTS.md)
2. [Repository structure](architecture/repository-structure.md)
3. [Current Blueprint](architecture/03-blueprint-consolidation.md)
4. [Test strategy](implementation/01-test-strategy.md) and
   [contributor guide](../CONTRIBUTING.md)
5. For mod work, the [Local Mod Lifecycle V2 contract](contracts/10-local-mod-lifecycle-v2.md)
6. The specific CLI, contract, source, and test files affected by the work

### Architecture reviewers

1. [Current Blueprint](architecture/03-blueprint-consolidation.md)
2. [Control and publication contract](contracts/06-control-and-publication.md)
3. [Repository structure](architecture/repository-structure.md)
4. The linked headers, sources, CMake targets, tests, and acceptance records

### Historical investigations

1. [History entry point](history/README.md) and [HISTORY.md](../HISTORY.md)
2. [Historical architecture](architecture/03-blueprint-history-through-093.md)
3. The relevant numbered record in [implementation/](implementation/README.md)
4. [Audits](audit/README.md), [ADRs](adr/README.md), and
   [planning records](superpowers/README.md), always retaining their stated date
   and evidence boundary

## Why numbered implementation records remain

The numbered files preserve decisions, acceptance commands, fixtures, and the
sequence in which capabilities were introduced. They are valuable evidence for
a stated historical revision, but they do not automatically describe today’s
build. When they conflict with source, CMake, tests, or the current CLI
reference, those current implementation sources take precedence.
