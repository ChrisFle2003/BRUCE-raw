# Architecture

The [Blueprint consolidation](03-blueprint-consolidation.md) is the current
architecture entry point. It states the evidence-backed core, its boundaries,
and what does not follow from a successful CLI run.

- [Repository structure](repository-structure.md) — tracked areas, source
  domains, build flow, runtime flow, and mod boundaries.
- [Local security boundaries](04-local-security-boundaries.md) — current
  MCP, artifact, and external-exposure limits.
- [Identity and invariants](01-identity-and-invariants.md) — foundational
  vocabulary and intended invariants; read its own status and date.
- [System architecture](02-system-architecture.md) — architectural target
  context, not by itself implementation evidence.
- [Historical blueprint through Core 0.93](03-blueprint-history-through-093.md)
  — historical context only.

**DOCUMENTED / CONCEPT:** architecture, ADR, and planning documents can define
or propose a boundary, but only source, tests, CI, reproducible runs, and
generated CLI-validation evidence establish an implementation claim.
