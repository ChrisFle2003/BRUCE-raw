# Implementation records and acceptance evidence

This directory preserves roadmaps, contracts, usage records, acceptance
profiles, and numbered delivery packages. The current source tree, CMake
configuration, tests, CI, and reproducible evidence decide the current state;
the [Blueprint](../architecture/03-blueprint-consolidation.md) and
[control and publication contract](../contracts/06-control-and-publication.md)
are the current documentation entry points.

- [Test strategy](01-test-strategy.md)
- [Implementation phases](02-implementation-phases.md) — planning context.
- [Open decisions](03-open-decisions.md) — unresolved work, not implementation.
- [Consolidated roadmap](04-roadmap.md) — historical/planning context.
- [Acceptance evidence](acceptance/README.md)
- [Architecture and milestone history](75-architecture-history-core082.md)
- [Architecture and milestones](75-current-architecture-and-milestones.md)

Files named `*-plan.md` are **DOCUMENTED / CONCEPT** until a specific source
and test acceptance record supports the described behavior. Files named
`*-usage.md` or `*-contract.md` record a stated scope and date; do not treat
their numbering as proof that every later branch or local build still matches.
