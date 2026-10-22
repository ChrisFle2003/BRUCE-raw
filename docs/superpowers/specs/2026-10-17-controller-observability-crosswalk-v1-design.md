# Controller Observability Crosswalk V1

**DOCUMENTED / CONCEPT:** This design record states the intentionally narrow
projection boundary. Current implementation evidence is maintained separately
in [the contract](../../contracts/08-controller-observability-crosswalk-v1.md).

The crosswalk receives already-materialized native V5 inspection, V6 canonical
result plus optional validated diagnostic sidecar, or Runtime Orchestrator
status. It returns source-tagged phase, terminal kind, cancellation origin and
independent known/unknown facts for success, publication and durable evidence.
It never promotes a status string or sidecar to authority.

It has no filesystem read, worker invocation, shared cancel API, new record,
queue recovery or common runtime state machine.
