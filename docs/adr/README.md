# Architecture Decision Records

Architecture Decision Records explain accepted decisions, alternatives, and
intended migration or replay effects. They are documentation, not independent
proof that a described feature is present in the current build. Confirm every
implementation claim through the linked source, CMake targets, tests, and the
[current Blueprint](../architecture/03-blueprint-consolidation.md).

Open choices are collected in [implementation open decisions](../implementation/03-open-decisions.md).

- [ADR-010: Shared research and gate contract](010-research-contract-and-gates.md)
- [ADR-011: Typed shared task entry](011-typed-application-entry.md)
- [ADR-012: Checked execution phases](012-checked-execution-phases.md)
- [ADR-013: Human rule authority](013-human-rule-authority.md)
- [ADR-014: Shared local-file adapter](014-local-file-adapter.md)
- [ADR-015: Control-contract compatibility](015-control-contract-compatibility.md)

The similarly numbered history variants preserve context. Read the current
file’s status, scope, and linked implementation evidence before treating an
ADR as more than **DOCUMENTED / CONCEPT**.
