# Contracts

These documents describe behavioral, data, control, publication, and replay
boundaries. The [control and publication contract](06-control-and-publication.md)
is the primary current reference for shared control behavior. A contract is
not, by itself, evidence that a capability is implemented; follow its source
and test links before making that claim.

- [Z compatibility](01-z-compatibility.md)
- [Tokens, language, and relations](02-token-language-relations.md)
- [Evidence and experience](03-evidence-and-experience.md)
- [Mathematics, evidence, and replay](04-math-evidence-replay.md)
- [Local human rule approval](05-human-rule-approval.md)
- [Rule-approval history](05-human-rule-approval-history.md) — historical
  context for the current rule-approval document.
- [Control and publication](06-control-and-publication.md) — current shared
  control reference.
- [Host Lifecycle Contract Matrix V1](07-host-lifecycle-contract-matrix-v1.md)
  — read-only crosswalk of the separate V5 batch, V6 DAG and Runtime
  Orchestrator V1 lifecycles; it does not create a common runtime.
- [Controller Observability Crosswalk V1](08-controller-observability-crosswalk-v1.md)
  — bounded read-only projection of native controller observations; it does
  not create shared cancellation, publication, or recovery semantics.
- [Admission Diagnostics V1](09-admission-diagnostics-v1.md) — bounded
  Runtime Orchestrator V1 admission projection; it exposes no plans, paths or
  V6 completion details.
- [Local Mod Lifecycle V2](10-local-mod-lifecycle-v2.md) — common local-user
  discovery, build, verification, durable Data/Logic installation, explicit
  activation, side-by-side update/rollback, routing ownership, and isolated
  Logic execution. It does not define a daemon or multi-user service.
- [Local Mod Trust V2](../cli/README.md#local-mod-trust-v2) —
  **IMPLEMENTIERT UND BELEGT:** explicit V1 import, immutable generations,
  prepare/verify/activate/rollback/status, safe rejection or explicit atomic
  emergency deactivation, and the running-worker boundary. Source and test
  links are included; an external truststore cannot replace active V2 trust.
- [Offline Core release signing V1](../implementation/150-release-signing-contract.md)
  — **IMPLEMENTIERT UND BELEGT:** audited TGZ handoff, protected key-file
  input, complete release directory and independent pinned Public-Key
  verification. A production key ceremony remains **OFFENE ENTSCHEIDUNG**.
