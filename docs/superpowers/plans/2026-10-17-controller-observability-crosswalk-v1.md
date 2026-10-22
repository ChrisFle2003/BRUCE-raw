# Controller Observability Crosswalk V1 plan

**DOCUMENTED / CONCEPT:** The completed implementation is evidenced by the
[current contract](../../contracts/08-controller-observability-crosswalk-v1.md)
and its linked source and CTest target.

1. Map each native observation into a bounded source-tagged value object.
2. Keep success, publication and durable evidence independently unknown unless
   supplied by the canonical native input.
3. Treat an already validated V6 sidecar as diagnostic presence only.
4. Cover native-family mapping, absence/incompleteness, non-authority and
   redaction with targeted unit tests.
