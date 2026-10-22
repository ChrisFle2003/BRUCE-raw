# Quellenbasierter Options- und Diagnoseindex

[Handbuch](README.md)

Stand Core 0.102.0. Automatisch durch `verify_examples.py` aus dem lokalen Quellbaum ermittelt.

## Optionen nach Parserzweig

Gemeinsame Optionen sind absichtlich pro Befehl aufgeführt. Die Liste beschreibt Annahme durch den Parser, nicht die Zulässigkeit jeder Wertkombination; diese steht in der Befehlsreferenz.

### `compare`

`--execution-cancellation`, `--serialization-cancellation`, `--verification`, `--verification-cancellation`, `--max-replay-bytes`, `--max-serialized-bytes`, `--max-session-checkpoints`, `--compatibility-family`, `--left-numerator`, `--left-denominator`, `--right-numerator`, `--right-denominator`, `--domain-policy`, `--max-bits`, `--max-operations`, `--output`

### `coordinate`

`--x`, `--y`, `--z`

### `grid`

`--execution-cancellation`, `--serialization-cancellation`, `--verification`, `--verification-cancellation`, `--max-replay-bytes`, `--max-serialized-bytes`, `--max-session-checkpoints`, `--compatibility-family`, `--path`, `--rule`, `--stage`, `--mode`, `--modulus`, `--max-bits`, `--zero-zero`, `--output`

### `grid-stage-verify`

`--from-sweep`, `--output`

### `grid-stage-sweep`

`--serialization-cancellation`, `--verification`, `--verification-cancellation`, `--max-replay-bytes`, `--max-serialized-bytes`, `--max-session-checkpoints`, `--path`, `--from-stage`, `--to-stage`, `--mode`, `--modulus`, `--max-bits`, `--cache-bytes`, `--debug`, `--output`, `--debug`

### `grid-batch`

`--execution-cancellation`, `--serialization-cancellation`, `--verification`, `--verification-cancellation`, `--max-replay-bytes`, `--max-serialized-bytes`, `--max-session-checkpoints`, `--compatibility-family`, `--paths`, `--rule`, `--stage`, `--mode`, `--modulus`, `--max-bits`, `--zero-zero`, `--cache-bytes`, `--max-query-steps`, `--output`

### `grid-resume`

`--execution-cancellation`, `--serialization-cancellation`, `--verification`, `--verification-cancellation`, `--max-replay-bytes`, `--max-serialized-bytes`, `--max-session-checkpoints`, `--compatibility-family`, `--from-run`, `--additional-queries`, `--output`

### `calculate`

`--domain`, `--op`, `--left`, `--right`, `--max-bits`, `--modulus`

### `dialogue`

`--text`, `--output`

### `dialogue`

`--alternative`, `--output`

### `dialogue`

`--output`

### `evidence`

`--train`, `--holdout`, `--output`, `--domain`, `--min`, `--max`, `--max-bits`, `--max-denominator`, `--max-trace-bytes`, `--max-candidates`, `--max-operations`, `--max-trace`, `--max-results`

### `research`

`--execution-cancellation`, `--serialization-cancellation`, `--verification`, `--verification-cancellation`, `--max-replay-bytes`, `--max-serialized-bytes`, `--max-session-checkpoints`, `--compatibility-family`, `--train`, `--holdout`, `--min`, `--max`, `--max-candidates`, `--max-operations`, `--max-trace`, `--max-results`, `--output`, `--domain`, `--max-bits`, `--max-denominator`, `--max-trace-bytes`, `--strategy`, `--max-power`, `--planner`, `--max-degree`, `--experience`, `--max-nodes`, `--equivalence`, `--domain-cache`, `--expression-planner`, `--operators`, `--grid-train`, `--grid-holdout`, `--grid-axis`, `--max-total-operations`

### `compatibility`

`--execution-cancellation`, `--serialization-cancellation`, `--verification`, `--verification-cancellation`, `--max-replay-bytes`, `--max-serialized-bytes`, `--max-session-checkpoints`, `--compatibility-family`, `--rules`, `--rule-store`, `--candidates`, `--from-run`, `--at`, `--max-candidates`, `--max-checks`, `--max-trace`, `--output`

### `gate-run`

`--compatibility-family`, `--max-session-checkpoints`, `--max-replay-bytes`, `--cancellation`

### `compatibility-plan`

`--from-run`, `--rules`, `--rule-store`, `--at`, `--max-source-bytes`, `--max-candidates`, `--max-checks`, `--max-trace`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`, `--compatibility-family`.

### `compatibility-apply`

Plan als Positionsargument, danach `--from-run`, `--rules`, `--rule-store`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`, `--compatibility-family`.

### `experience`

`add`: `--store`, `--from-run`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`, `--compatibility-family`.

`revoke`: `--store`, `--id`, `--reason`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`, `--compatibility-family`.

`replace`: `--store`, `--id`, `--reason`, `--from-run`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`, `--compatibility-family`.

`inspect`: `--compatibility-family`, `--max-management-checkpoints`, `--management-cancellation`.

`plan-add`: `--from-run`, `--store`, `--max-source-bytes`, `--max-store-bytes`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`, `--compatibility-family`.

`apply-add`: Plan als Positionsargument, danach `--from-run`, `--store`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`, `--compatibility-family`.

### `language`

`interpret`: `--text`, `--train`, `--holdout`, `--grammar`, `--train-alias`, `--holdout-alias`, `--max-bits`, `--max-denominator`, `--max-candidates`, `--max-operations`, `--max-trace`, `--max-results`, `--output`.

`select`: Entwurf als Positionsargument; `--alternative`, `--output`.

`execute`: Entwurf als Positionsargument; `--output`.

`replay`: Lauf als Positionsargument; keine Optionen.

### `task-spec`

`create`: `--train`, `--holdout`, `--route`, `--token-table-version`, `--output`, `--min`, `--max`, `--max-bits`, `--max-denominator`, `--max-power`, `--max-degree`, `--max-trace-bytes`, `--max-candidates`, `--max-operations`, `--max-trace`, `--max-results`.

`inspect`: Spec als einziges Positionsargument; keine Optionen.

`execute`: Spec als Positionsargument; `--output`.

`replay`: Lauf als einziges Positionsargument; keine Optionen.

### `nanocell`

`run`: `--active-index`, `--context-file`, `--context-id`, `--core`, `--max-operations`, `--max-trace-entries`, `--output`, `--technique`.

`replay`: Lauf als Positionsargument; keine Optionen.

### `phase-dag-v2`

Quelle: [apps/phase_dag_v2_cli.hpp](../../apps/phase_dag_v2_cli.hpp). Optional mit NanoCell ON; fester Adapterkatalog `nanocell-phase-v1`/`v1` und `evidence-gate-v1`/`v1`, seriell und begrenzt.

`execute`: Spec als Positionsargument, danach verpflichtend `--output` mit neuem Bundleziel; exklusiver Export.

`inspect` und `replay`: genau ein Bundle als Positionsargument; keine Optionen. Beide bestätigen gespeicherte Belege durch Replay.

### `phase-dag-v3`

Quelle: [apps/phase_dag_v3_cli.hpp](../../apps/phase_dag_v3_cli.hpp). Optional mit NanoCell ON; getrennte V3-Marker, fester Katalog `nanocell-phase-v3`/`v1` und `payload-gate-v1`/`v1`, direkte Payloads bis 65536 Bytes. MCP prüft bestehende V2-/V3-Bundles read-only über getrennte Inspect-Routen.

`execute`: V3-Spec als Positionsargument, danach verpflichtend `--output` mit neuem Bundleziel, anschließend optional `--max-parallel-nodes N` (Standard 1, Bereich 1..4); exklusiver atomarer Export.

`inspect` und `replay`: genau ein V3-Bundle als Positionsargument; keine Optionen. Beide bestätigen tatsächliches Replay ohne Originaldateien und ohne Payload-Rohbytes im Report.

### `mcp`

Keine Zusatzargumente oder genau `--port` mit dezimalem Wert 1..65535; Standard 8080. Optionaler statischer Mod, nur read-only Loopback-Inspector; `POST /api/v1/inspect` für V2 bis 1 MiB und `POST /api/v3/inspect` für V3 bis 5 MiB. Explizite Auswahl, nur Metadaten einschließlich Payloadgrößen und Digests, keine Rohpayloads, Jobs oder Importe.

### `mods` und `route`

`mods inspect`: Data-/Logic-Paket als Positionsargument; keine Optionen.

`mods verify`: Paket als Positionsargument; `--trust-store`.

`mods install`: Paket als Positionsargument; `--mod-root`, `--trust-store`.

`mods update`: Paket als Positionsargument; `--mod-root`, `--trust-store`.

`mods list`: optional `--mod-root`; ohne Root nur Built-ins.

`mods status`: Mod-ID und optional Version als Positionsargumente; optional `--mod-root`.

`mods run`: Mod-ID als Positionsargument; `--template`, `--input`, `--mod-root`.

`mods activate`: Mod-ID und Version als Positionsargumente; `--mod-root`.

`mods disable`: Mod-ID als Positionsargument; `--mod-root`.

`mods trust status|import-v1|verify`: `--mod-root`; status ist read-only.

`mods trust prepare`: kanonischer V1-Truststore als Positionsargument; `--mod-root`; keine Aktivierung.

`mods trust activate|rollback`: `--mod-root`, optional einmaliges Flag `--deactivate-untrusted`; standardmäßig Ablehnung bei Vertrauensverlust aktiver Mods.

`route`: `--text`, `--mod-root`, optional `--output` und Flag `--mod-details`.

`route replay`: Report als Positionsargument; `--mod-root`, optional Flag `--mod-details`.

## Diagnose-Literale

Aus `Error("…")`, Stopaufrufen und Zuweisungen von `stop_reason` in `src/`, `include/` und der CLI. Dies sind vollständige gefundene Textliterale bzw. Präfixe; dynamisch zusammengesetzte Suffixe und Betriebssystemmeldungen sind nicht enumerierbar. Nicht jeder interne Fehler ist bei jedem CLI-Aufruf erreichbar. Quellverweise zeigen die Entstehungsstelle.

| Literal / Präfix | Quelle |
| --- | --- |
| `AdaptiveOrderRequiresRationalPortfolio` | [src/application/research_contract.cpp:38](../../src/application/research_contract.cpp) |
| `AdaptiveRequiresPowerStrategy` | [src/application/research_contract.cpp:43](../../src/application/research_contract.cpp) |
| `ApplicationDispatchPayloadTooLarge` | [mods/cockpit/src/cockpit_assets.cpp:221](../../mods/cockpit/src/cockpit_assets.cpp) |
| `ApplicationReplayMismatch` | [src/application/execution_session.cpp:83](../../src/application/execution_session.cpp) |
| `ArtifactCopyDeadlineExceeded` | [src/artifact/artifact_payload_v1.cpp:141](../../src/artifact/artifact_payload_v1.cpp) |
| `ArtifactCopyDestinationInvalid` | [src/artifact/artifact_store_v1.cpp:488](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactCopyDestinationWriteFailed` | [src/artifact/artifact_store_v1.cpp:533](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactCopyLimitExceeded` | [src/artifact/artifact_store_v1.cpp:498](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactDigestFailure` | [src/artifact/artifact_payload_external_v1.cpp:45](../../src/artifact/artifact_payload_external_v1.cpp) |
| `ArtifactExpectedDigestInvalid` | [src/artifact/artifact_store_v1.cpp:425](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactExpectedDigestMismatch` | [src/artifact/artifact_store_v1.cpp:268](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactExpectedSizeInvalid` | [src/artifact/artifact_store_v1.cpp:427](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactExpectedSizeMismatch` | [src/artifact/artifact_store_v1.cpp:270](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactIdInvalid` | [src/artifact/artifact_store_v1.cpp:141](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactManifestConflict` | [src/artifact/artifact_store_v1.cpp:571](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactManifestContractMismatch` | [src/artifact/artifact_manifest_v1.cpp:120](../../src/artifact/artifact_manifest_v1.cpp) |
| `ArtifactManifestCorrupt` | [src/artifact/artifact_store_v1.cpp:627](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactManifestIdInvalid` | [apps/artifact_store_v1_cli.hpp:62](../../apps/artifact_store_v1_cli.hpp) |
| `ArtifactManifestInvalid` | [src/artifact/artifact_manifest_v1.cpp:9](../../src/artifact/artifact_manifest_v1.cpp) |
| `ArtifactManifestInvalidNumber` | [src/artifact/artifact_manifest_v1.cpp:67](../../src/artifact/artifact_manifest_v1.cpp) |
| `ArtifactManifestMissing` | [src/artifact/artifact_store_v1.cpp:628](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactManifestNonCanonical` | [src/artifact/artifact_manifest_v1.cpp:150](../../src/artifact/artifact_manifest_v1.cpp) |
| `ArtifactManifestObjectLimit` | [src/artifact/artifact_manifest_v1.cpp:134](../../src/artifact/artifact_manifest_v1.cpp) |
| `ArtifactManifestPublishFailed` | [src/artifact/artifact_store_v1.cpp:600](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactManifestSyncFailed` | [src/artifact/artifact_store_v1.cpp:596](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactManifestTooLarge` | [src/artifact/artifact_manifest_v1.cpp:112](../../src/artifact/artifact_manifest_v1.cpp) |
| `ArtifactManifestTrailingBytes` | [src/artifact/artifact_manifest_v1.cpp:73](../../src/artifact/artifact_manifest_v1.cpp) |
| `ArtifactManifestTruncated` | [src/artifact/artifact_manifest_v1.cpp:122](../../src/artifact/artifact_manifest_v1.cpp) |
| `ArtifactManifestUnknownField` | [src/artifact/artifact_manifest_v1.cpp:87](../../src/artifact/artifact_manifest_v1.cpp) |
| `ArtifactObjectConflict` | [src/artifact/artifact_store_v1.cpp:281](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactObjectCorrupt` | [src/artifact/artifact_store_v1.cpp:202](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactObjectMissing` | [src/artifact/artifact_store_v1.cpp:301](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactObjectTooLarge` | [src/artifact/artifact_store_v1.cpp:173](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactPayloadDestinationIntegrityMismatch` | [src/artifact/artifact_payload_v1.cpp:131](../../src/artifact/artifact_payload_v1.cpp) |
| `ArtifactPayloadDigestMismatch` | [apps/dag_scheduler_v5_cli.hpp:1956](../../apps/dag_scheduler_v5_cli.hpp) |
| `ArtifactPayloadInputLimitExceeded` | [apps/dag_scheduler_v5_cli.hpp:1930](../../apps/dag_scheduler_v5_cli.hpp) |
| `ArtifactPayloadManifestInvalid` | [apps/dag_scheduler_v5_cli.hpp:1938](../../apps/dag_scheduler_v5_cli.hpp) |
| `ArtifactPayloadManifestNotReady` | [apps/dag_scheduler_v5_cli.hpp:1954](../../apps/dag_scheduler_v5_cli.hpp) |
| `ArtifactPayloadReferenceInvalid` | [apps/dag_scheduler_v5_cli.hpp:1960](../../apps/dag_scheduler_v5_cli.hpp) |
| `ArtifactPayloadStoreViolation` | [apps/dag_scheduler_v5_cli.hpp:1738](../../apps/dag_scheduler_v5_cli.hpp) |
| `ArtifactPayloadTimeout` | [apps/dag_scheduler_v5_cli.hpp:1748](../../apps/dag_scheduler_v5_cli.hpp) |
| `ArtifactPublishFailed` | [src/artifact/artifact_store_v1.cpp:275](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactPublishSyncFailed` | [src/artifact/artifact_store_v1.cpp:271](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactPublishWriteFailed` | [src/artifact/artifact_store_v1.cpp:239](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactReadFailed` | [src/artifact/artifact_store_v1.cpp:180](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactSourceHardlink` | [src/artifact/artifact_store_v1.cpp:171](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactSourceMissing` | [src/artifact/artifact_store_v1.cpp:70](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactSourceNotRegular` | [src/artifact/artifact_store_v1.cpp:169](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactSourcePermissions` | [src/artifact/artifact_store_v1.cpp:170](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactSourceReadFailed` | [src/artifact/artifact_store_v1.cpp:436](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactSourceSymlink` | [src/artifact/artifact_store_v1.cpp:156](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactStoreActionInvalid` | [apps/artifact_store_v1_cli.hpp:87](../../apps/artifact_store_v1_cli.hpp) |
| `ArtifactStoreCreateFailed` | [src/artifact/artifact_store_v1.cpp:101](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactStoreDigestInvalid` | [apps/artifact_store_v1_cli.hpp:66](../../apps/artifact_store_v1_cli.hpp) |
| `ArtifactStoreOpenFailed` | [src/artifact/artifact_store_v1.cpp:94](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactStoreSymlink` | [src/artifact/artifact_store_v1.cpp:316](../../src/artifact/artifact_store_v1.cpp) |
| `ArtifactStoreUsage` | [apps/artifact_store_v1_cli.hpp:47](../../apps/artifact_store_v1_cli.hpp) |
| `ArtifactTempCreateFailed` | [src/artifact/artifact_store_v1.cpp:230](../../src/artifact/artifact_store_v1.cpp) |
| `AtRequiresResearchRun` | [apps/bruce_cli.cpp:1316](../../apps/bruce_cli.cpp) |
| `BuiltinModImmutable` | [apps/runtime_mod_cli.hpp:384](../../apps/runtime_mod_cli.hpp) |
| `BundleByteLimit` | [src/application/compatibility.cpp:224](../../src/application/compatibility.cpp) |
| `CANCEL_TARGET_UNAVAILABLE` | [apps/dag_scheduler_v5_cli.hpp:951](../../apps/dag_scheduler_v5_cli.hpp) |
| `CHAIN_REPLAY_MISMATCH` | [src/mod/logic_mod_artifact_chain_v1.cpp:202](../../src/mod/logic_mod_artifact_chain_v1.cpp) |
| `Cancelled` | [src/application/exact_research.cpp:51](../../src/application/exact_research.cpp) |
| `CandidateBudgetExceeded` | [src/application/exact_research.cpp:95](../../src/application/exact_research.cpp) |
| `CandidateByteLimit` | [src/compatibility/snapshot.cpp:95](../../src/compatibility/snapshot.cpp) |
| `CandidateInputLimit` | [src/compatibility/snapshot.cpp:116](../../src/compatibility/snapshot.cpp) |
| `CannotCreateExport: ` | [src/io/local_files.cpp:68](../../src/io/local_files.cpp) |
| `CannotReadFile: ` | [src/io/local_files.cpp:49](../../src/io/local_files.cpp) |
| `CheckpointRequiresAdaptivePlanner` | [src/application/research_contract.cpp:70](../../src/application/research_contract.cpp) |
| `CockpitImportTooLarge` | [mods/cockpit/src/cockpit_assets.cpp:317](../../mods/cockpit/src/cockpit_assets.cpp) |
| `CommonRuntimeCompatibilityV1FamilyMismatch` | [apps/bruce_cli.cpp:142](../../apps/bruce_cli.cpp) |
| `CommonRuntimeCompatibilityV1TooLarge` | [src/application/common_runtime_compatibility_v1.cpp:81](../../src/application/common_runtime_compatibility_v1.cpp) |
| `CommonRuntimeHostV1EntrypointMismatch` | [src/application/common_runtime_host_v1.cpp:9](../../src/application/common_runtime_host_v1.cpp) |
| `ComparisonReplayCheckpointsRequireCallback` | [src/application/execution_session.cpp:39](../../src/application/execution_session.cpp) |
| `ComparisonReplayCheckpointsRequireComparisonTask` | [src/application/execution_session.cpp:40](../../src/application/execution_session.cpp) |
| `CompatibilityProjectionRequiresInt64: fractional value` | [src/application/compatibility.cpp:108](../../src/application/compatibility.cpp) |
| `CompatibilityProjectionRequiresInt64: out of range` | [src/application/compatibility.cpp:111](../../src/application/compatibility.cpp) |
| `CompatibilityReplayCheckpointsRequireCallback` | [src/application/execution_session.cpp:35](../../src/application/execution_session.cpp) |
| `CompatibilityReplayCheckpointsRequireCompatibilityTask` | [src/application/execution_session.cpp:36](../../src/application/execution_session.cpp) |
| `CompatibilityTransferByteLimit` | [src/application/compatibility_transfer.cpp:15](../../src/application/compatibility_transfer.cpp) |
| `CompatibilityTransferDigestMismatch` | [src/application/compatibility_transfer.cpp:27](../../src/application/compatibility_transfer.cpp) |
| `CompatibilityTransferInvalidBudget` | [apps/bruce_cli.cpp:1287](../../apps/bruce_cli.cpp) |
| `CompatibilityTransferPlanByteLimit` | [src/application/compatibility_transfer.cpp:25](../../src/application/compatibility_transfer.cpp) |
| `CompatibilityTransferRulesChanged` | [src/application/compatibility_transfer.cpp:39](../../src/application/compatibility_transfer.cpp) |
| `CompatibilityTransferSourceChanged` | [src/application/compatibility_transfer.cpp:37](../../src/application/compatibility_transfer.cpp) |
| `Completed` | [include/bruce/application/exact_research.hpp:47](../../include/bruce/application/exact_research.hpp) |
| `ConflictingReplayCheckpoints` | [src/application/execution_session.cpp:33](../../src/application/execution_session.cpp) |
| `ConflictingSerializationCheckpoints` | [src/application/execution_session.cpp:52](../../src/application/execution_session.cpp) |
| `ConflictingTaskSpecRoles` | [src/token/task_spec.cpp:168](../../src/token/task_spec.cpp) |
| `ControllerObservabilityInvalidEvidence` | [src/observability/controller_observability_cli_v1.cpp:14](../../src/observability/controller_observability_cli_v1.cpp) |
| `CoordinateOverflow` | [src/coordinate/address.cpp:13](../../src/coordinate/address.cpp) |
| `CountercheckMismatch` | [src/application/exact_research.cpp:120](../../src/application/exact_research.cpp) |
| `DAG_GLOBAL_BUDGET_EXHAUSTED` | [src/mod/logic_mod_artifact_dag_v4.cpp:252](../../src/mod/logic_mod_artifact_dag_v4.cpp) |
| `DAG_HOST_CANCELLED` | [src/mod/logic_mod_artifact_dag_v4.cpp:249](../../src/mod/logic_mod_artifact_dag_v4.cpp) |
| `DAG_REPLAY_MISMATCH` | [src/mod/logic_mod_artifact_dag_v2.cpp:306](../../src/mod/logic_mod_artifact_dag_v2.cpp) |
| `DUPLICATE_ATTEMPT_ID` | [src/application/common_runtime_host_v2_batch.cpp:289](../../src/application/common_runtime_host_v2_batch.cpp) |
| `DagReceiptBudgetExceeded` | [src/phase_dag/phase_dag_v3.cpp:192](../../src/phase_dag/phase_dag_v3.cpp) |
| `DagSchedulerCycle: ` | [src/dag_scheduler/dag_scheduler_v1.cpp:86](../../src/dag_scheduler/dag_scheduler_v1.cpp) |
| `DagSchedulerV2Cycle: ` | [src/dag_scheduler/dag_scheduler_v2.cpp:93](../../src/dag_scheduler/dag_scheduler_v2.cpp) |
| `DagSchedulerV2ReceiptBudgetExceeded: ` | [src/dag_scheduler/dag_scheduler_v2.cpp:118](../../src/dag_scheduler/dag_scheduler_v2.cpp) |
| `DagSchedulerV3PayloadLimitExceeded` | [apps/dag_scheduler_v3_cli.hpp:33](../../apps/dag_scheduler_v3_cli.hpp) |
| `DagSchedulerV4ReplayMismatch` | [src/dag_scheduler/dag_scheduler_v4.cpp:242](../../src/dag_scheduler/dag_scheduler_v4.cpp) |
| `DagSchedulerV5DependencyNotAccepted` | [apps/dag_scheduler_v5_cli.hpp:1646](../../apps/dag_scheduler_v5_cli.hpp) |
| `DagSchedulerV5OutputExists` | [apps/dag_scheduler_v5_cli.hpp:2784](../../apps/dag_scheduler_v5_cli.hpp) |
| `DagSchedulerV5RunRootUnavailable` | [apps/dag_scheduler_v5_cli.hpp:2324](../../apps/dag_scheduler_v5_cli.hpp) |
| `DepthBudgetExceeded` | [src/coordinate/address.cpp:46](../../src/coordinate/address.cpp) |
| `DigestFailure` | [src/evidence/bundle.cpp:14](../../src/evidence/bundle.cpp) |
| `DivisionByZero` | [src/math/numbers.cpp:45](../../src/math/numbers.cpp) |
| `DivmodRequiresBigint` | [src/application/exact_research.cpp:228](../../src/application/exact_research.cpp) |
| `DomainCacheRequiresModularExpression` | [apps/bruce_cli.cpp:1212](../../apps/bruce_cli.cpp) |
| `DomainWitnessCountercheckFailed` | [src/math/expression_normal_form.cpp:279](../../src/math/expression_normal_form.cpp) |
| `DuplicateCandidateId` | [src/compatibility/selection.cpp:11](../../src/compatibility/selection.cpp) |
| `DuplicateDagSchedulerNode: ` | [src/dag_scheduler/dag_scheduler_v1.cpp:52](../../src/dag_scheduler/dag_scheduler_v1.cpp) |
| `DuplicateDagSchedulerV2Node: ` | [src/dag_scheduler/dag_scheduler_v2.cpp:59](../../src/dag_scheduler/dag_scheduler_v2.cpp) |
| `DuplicateDagSchedulerV3SpecKey` | [apps/dag_scheduler_v3_cli.hpp:117](../../apps/dag_scheduler_v3_cli.hpp) |
| `DuplicateExperienceTrainingInput` | [src/experience/store.cpp:64](../../src/experience/store.cpp) |
| `DuplicateExpressionConstant` | [src/math/expression.cpp:62](../../src/math/expression.cpp) |
| `DuplicateInterpolationInput` | [src/math/polynomial.cpp:19](../../src/math/polynomial.cpp) |
| `DuplicateLanguageAlias` | [mods/language/src/language.cpp:65](../../mods/language/src/language.cpp) |
| `DuplicateLogicModVersion` | [src/mod/logic_mod_plugin_v1.cpp:98](../../src/mod/logic_mod_plugin_v1.cpp) |
| `DuplicateOption: ` | [apps/bruce_cli.cpp:95](../../apps/bruce_cli.cpp) |
| `DuplicateOrOverlappingGridInput` | [src/application/grid_research.cpp:61](../../src/application/grid_research.cpp) |
| `DuplicateOrOverlappingSampleInput` | [src/application/research_contract.cpp:87](../../src/application/research_contract.cpp) |
| `DuplicateOrUnsortedTaskSpecAlias` | [src/token/task_spec.cpp:159](../../src/token/task_spec.cpp) |
| `DuplicatePayloadGateInput` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:168](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `DuplicatePhaseDagSpecKey: ` | [apps/phase_dag_v2_cli.hpp:130](../../apps/phase_dag_v2_cli.hpp) |
| `DuplicateRuntimeModJsonKey` | [src/mod/runtime_package.cpp:105](../../src/mod/runtime_package.cpp) |
| `DuplicateRuntimeOrchestratorRunId` | [src/runtime/runtime_orchestrator_v1.cpp:95](../../src/runtime/runtime_orchestrator_v1.cpp) |
| `DuplicateTaskSpecFileAlias` | [apps/bruce_cli.cpp:965](../../apps/bruce_cli.cpp) |
| `DuplicateTaskSpecRoleAsset` | [src/token/task_spec.cpp:117](../../src/token/task_spec.cpp) |
| `DuplicateTrainingInput` | [src/application/research.cpp:19](../../src/application/research.cpp) |
| `DuplicateTreeAddress` | [src/tree/registry.cpp:54](../../src/tree/registry.cpp) |
| `DuplicateTreeSiblingKey` | [src/tree/registry.cpp:75](../../src/tree/registry.cpp) |
| `DuplicateWorkerResult` | [src/worker/worker_supervisor_v1.cpp:688](../../src/worker/worker_supervisor_v1.cpp) |
| `EXTERNAL_INPUT_BINDING_INVALID` | [apps/dag_scheduler_v5_cli.hpp:3206](../../apps/dag_scheduler_v5_cli.hpp) |
| `EXTERNAL_INPUT_LIMIT_EXCEEDED` | [apps/dag_scheduler_v5_cli.hpp:1971](../../apps/dag_scheduler_v5_cli.hpp) |
| `EXTERNAL_INPUT_TIMEOUT` | [apps/dag_scheduler_v5_cli.hpp:1891](../../apps/dag_scheduler_v5_cli.hpp) |
| `EmptyCommonRuntimeHostV1Delegate` | [src/application/common_runtime_host_v1.cpp:25](../../src/application/common_runtime_host_v1.cpp) |
| `EmptyGridHoldoutSource` | [apps/bruce_cli.cpp:1252](../../apps/bruce_cli.cpp) |
| `EmptySamples` | [src/application/exact_research.cpp:214](../../src/application/exact_research.cpp) |
| `EmptyTokenTable` | [src/token/task_spec.cpp:144](../../src/token/task_spec.cpp) |
| `EmptyTreeFragment` | [src/tree/registry.cpp:44](../../src/tree/registry.cpp) |
| `EquivalenceRequiresExpressionStrategy` | [apps/bruce_cli.cpp:1217](../../apps/bruce_cli.cpp) |
| `EvidenceChainNativeResultMismatch` | [src/evidence/chain.cpp:123](../../src/evidence/chain.cpp) |
| `EvidenceChainRequiresExactNativeBundle` | [src/evidence/chain.cpp:120](../../src/evidence/chain.cpp) |
| `EvidenceChainSampleBindingMismatch` | [src/evidence/chain.cpp:35](../../src/evidence/chain.cpp) |
| `ExactOptionsRequireBigintOrRationalDomain` | [apps/bruce_cli.cpp:1262](../../apps/bruce_cli.cpp) |
| `ExecuteHandoffLineageV1Invalid` | [src/artifact/execute_handoff_lineage_v1.cpp:10](../../src/artifact/execute_handoff_lineage_v1.cpp) |
| `ExecutionCancellationRequiresReplay` | [apps/bruce_cli.cpp:427](../../apps/bruce_cli.cpp) |
| `ExecutionCompatibilitySourceCancelled` | [src/application/application_task.cpp:52](../../src/application/application_task.cpp) |
| `ExecutionCompatibilitySourceRequiresCompatibilityTask` | [src/application/application_task.cpp:61](../../src/application/application_task.cpp) |
| `ExecutionCompatibilitySourceRequiresOrigin` | [src/application/compatibility.cpp:141](../../src/application/compatibility.cpp) |
| `ExecutionExperienceCancelled` | [src/application/application_task.cpp:48](../../src/application/application_task.cpp) |
| `ExecutionExperienceCheckpointsRequireLearnedResearch` | [src/application/application_task.cpp:72](../../src/application/application_task.cpp) |
| `ExecutionGridSourceCancelled` | [src/application/application_task.cpp:50](../../src/application/application_task.cpp) |
| `ExecutionGridSourceCheckpointsRequireGridResearch` | [src/application/application_task.cpp:64](../../src/application/application_task.cpp) |
| `ExecutionResumeImportCancelled` | [src/application/application_task.cpp:54](../../src/application/application_task.cpp) |
| `ExecutionResumeImportRequiresResumeTask` | [src/application/application_task.cpp:58](../../src/application/application_task.cpp) |
| `ExecutionSessionFailed at ` | [src/application/execution_session.cpp:19](../../src/application/execution_session.cpp) |
| `ExecutionSessionNotReady` | [src/application/execution_session.cpp:92](../../src/application/execution_session.cpp) |
| `ExpectedLogicModSession` | [apps/logic_mod_plugin_v1_cli.hpp:52](../../apps/logic_mod_plugin_v1_cli.hpp) |
| `ExperienceByteLimit` | [src/application/exact_replay.cpp:334](../../src/application/exact_replay.cpp) |
| `ExperienceEventLimit` | [src/experience/store.cpp:34](../../src/experience/store.cpp) |
| `ExperienceRecordLimit` | [src/experience/store.cpp:61](../../src/experience/store.cpp) |
| `ExperienceRequiresCompletedHoldoutRun` | [src/application/experience_source.cpp:57](../../src/application/experience_source.cpp) |
| `ExperienceRequiresIndependentPortfolioSource` | [src/application/experience_source.cpp:22](../../src/application/experience_source.cpp) |
| `ExperienceRequiresRationalPortfolio` | [apps/bruce_cli.cpp:1260](../../apps/bruce_cli.cpp) |
| `ExperienceRequiresVerifiedHypothesis` | [src/application/experience_source.cpp:67](../../src/application/experience_source.cpp) |
| `ExperienceSourceOperationLimit` | [src/application/experience_source.cpp:25](../../src/application/experience_source.cpp) |
| `ExperienceTransferBaseChanged` | [src/application/experience_transfer.cpp:38](../../src/application/experience_transfer.cpp) |
| `ExperienceTransferByteLimit` | [src/application/experience_transfer.cpp:11](../../src/application/experience_transfer.cpp) |
| `ExperienceTransferDigestMismatch` | [src/application/experience_transfer.cpp:28](../../src/application/experience_transfer.cpp) |
| `ExperienceTransferInvalidBudget` | [apps/bruce_cli.cpp:1338](../../apps/bruce_cli.cpp) |
| `ExperienceTransferPlanByteLimit` | [src/application/experience_transfer.cpp:26](../../src/application/experience_transfer.cpp) |
| `ExperienceTransferSourceChanged` | [src/application/experience_transfer.cpp:37](../../src/application/experience_transfer.cpp) |
| `ExportCloseFailure` | [src/io/local_files.cpp:92](../../src/io/local_files.cpp) |
| `ExportCommitDurabilityUnknown` | [src/io/local_files.cpp:106](../../src/io/local_files.cpp) |
| `ExportExistsOrUnavailable: ` | [src/io/local_files.cpp:99](../../src/io/local_files.cpp) |
| `ExportSyncFailure` | [src/io/local_files.cpp:89](../../src/io/local_files.cpp) |
| `ExportTemporaryCleanupFailed` | [src/io/local_files.cpp:104](../../src/io/local_files.cpp) |
| `ExportWriteFailure` | [src/io/local_files.cpp:85](../../src/io/local_files.cpp) |
| `ExpressionCacheRequiresIndex` | [src/math/expression.cpp:55](../../src/math/expression.cpp) |
| `ExpressionConstantSlotLimit` | [src/application/research_contract.cpp:91](../../src/application/research_contract.cpp) |
| `ExpressionCountercheckMismatch` | [src/application/expression_research.cpp:82](../../src/application/expression_research.cpp) |
| `ExpressionDivisionByZero` | [include/bruce/math/expression.hpp:14](../../include/bruce/math/expression.hpp) |
| `ExpressionEquivalenceConflict` | [src/application/expression_research.cpp:184](../../src/application/expression_research.cpp) |
| `ExpressionNormalizationDegreeLimit` | [include/bruce/math/expression_normal_form.hpp:4](../../include/bruce/math/expression_normal_form.hpp) |
| `ExpressionNormalizationMismatch` | [src/application/expression_research.cpp:112](../../src/application/expression_research.cpp) |
| `ExpressionPlannerRequiresFiveFamilyPortfolio` | [apps/bruce_cli.cpp:1216](../../apps/bruce_cli.cpp) |
| `ExpressionRankOutOfRange` | [src/math/expression.cpp:117](../../src/math/expression.cpp) |
| `ExpressionRequiresGrammarPlanner` | [src/application/research_contract.cpp:39](../../src/application/research_contract.cpp) |
| `FLOW_RECORD_INVALID` | [apps/dag_scheduler_v5_cli.hpp:3191](../../apps/dag_scheduler_v5_cli.hpp) |
| `FLOW_V2_EXTERNAL_INPUTS_UNSUPPORTED` | [apps/dag_scheduler_v5_cli.hpp:3226](../../apps/dag_scheduler_v5_cli.hpp) |
| `FileReadError` | [src/io/local_files.cpp:59](../../src/io/local_files.cpp) |
| `FixtureJoinRequestInvalid` | [src/phase_dag/phase_dag.cpp:76](../../src/phase_dag/phase_dag.cpp) |
| `FixtureSourceFailure` | [src/phase_dag/phase_dag.cpp:60](../../src/phase_dag/phase_dag.cpp) |
| `FixtureSourceRequestInvalid` | [src/phase_dag/phase_dag.cpp:61](../../src/phase_dag/phase_dag.cpp) |
| `Forged NanoCell technique` | [mods/nanocell/src/nanocell.cpp:164](../../mods/nanocell/src/nanocell.cpp) |
| `FractionRequiresRationalDomain` | [src/math/numbers.cpp:25](../../src/math/numbers.cpp) |
| `GateInspectionCancelled` | [apps/bruce_cli.cpp:1436](../../apps/bruce_cli.cpp) |
| `GridAxisRequiresGridTraining` | [apps/bruce_cli.cpp:1208](../../apps/bruce_cli.cpp) |
| `GridBatchInputByteLimit` | [apps/bruce_cli.cpp:902](../../apps/bruce_cli.cpp) |
| `GridBudgetRequiresUnbudgetedOrigin` | [src/application/grid_research.cpp:189](../../src/application/grid_research.cpp) |
| `GridCacheByteLimit` | [src/application/grid_batch.cpp:48](../../src/application/grid_batch.cpp) |
| `GridExperienceBudgetMismatch` | [src/application/grid_research.cpp:148](../../src/application/grid_research.cpp) |
| `GridHoldoutRequiresGridTraining` | [apps/bruce_cli.cpp:1209](../../apps/bruce_cli.cpp) |
| `GridInputByteLimit` | [src/application/grid.cpp:47](../../src/application/grid.cpp) |
| `GridProjectionInputOutOfRange` | [src/application/compatibility.cpp:84](../../src/application/compatibility.cpp) |
| `GridQueryLimit` | [apps/bruce_cli.cpp:907](../../apps/bruce_cli.cpp) |
| `GridReplayCheckpointsRequireBatchOrResume` | [src/application/execution_session.cpp:50](../../src/application/execution_session.cpp) |
| `GridReplayCheckpointsRequireCallback` | [src/application/execution_session.cpp:48](../../src/application/execution_session.cpp) |
| `GridResearchRequiresExactDomain` | [apps/bruce_cli.cpp:1222](../../apps/bruce_cli.cpp) |
| `GridResearchRequiresExactValues` | [src/application/grid_research.cpp:40](../../src/application/grid_research.cpp) |
| `GridResearchRequiresFixedSlice` | [src/application/grid_research.cpp:60](../../src/application/grid_research.cpp) |
| `GridResearchSampleBindingMismatch` | [src/application/grid_research.cpp:217](../../src/application/grid_research.cpp) |
| `GridResearchSourceByteLimit` | [src/application/grid_research.cpp:19](../../src/application/grid_research.cpp) |
| `GridResearchSpecMismatch` | [src/application/grid_research.cpp:46](../../src/application/grid_research.cpp) |
| `GridResearchStoppedSample` | [src/application/grid_research.cpp:50](../../src/application/grid_research.cpp) |
| `GridResumeGenerationLimit` | [src/application/grid_resume.cpp:46](../../src/application/grid_resume.cpp) |
| `GridResumeRequiresIncompleteSource` | [src/application/grid_resume.cpp:59](../../src/application/grid_resume.cpp) |
| `GridWorkBudgetMismatch` | [src/application/grid_research.cpp:150](../../src/application/grid_research.cpp) |
| `HANDOFF_RECORD_INVALID` | [apps/dag_scheduler_v5_cli.hpp:2163](../../apps/dag_scheduler_v5_cli.hpp) |
| `HoldoutOverlapOrDuplicate` | [src/application/research.cpp:21](../../src/application/research.cpp) |
| `INVALID_REJECTION_REASON` | [src/application/common_runtime_host_v2_batch.cpp:148](../../src/application/common_runtime_host_v2_batch.cpp) |
| `INVALID_REQUEST` | [src/application/common_runtime_host_v2_batch.cpp:286](../../src/application/common_runtime_host_v2_batch.cpp) |
| `IncompleteGridSource` | [src/application/grid_research.cpp:45](../../src/application/grid_research.cpp) |
| `IncompleteResearchCannotBeRanked` | [src/application/compatibility.cpp:89](../../src/application/compatibility.cpp) |
| `Inconsistent NanoCell run` | [mods/nanocell/src/nanocell.cpp:407](../../mods/nanocell/src/nanocell.cpp) |
| `InputByteLimit` | [src/application/exact_research.cpp:200](../../src/application/exact_research.cpp) |
| `InstalledLogicModRequired` | [src/mod/installed_mod_host_v2.cpp:428](../../src/mod/installed_mod_host_v2.cpp) |
| `InstalledModGoldenCaseFailed` | [src/mod/installed_mod_host_v2.cpp:150](../../src/mod/installed_mod_host_v2.cpp) |
| `InstalledModKindConflict` | [src/mod/installed_mod_host_v2.cpp:286](../../src/mod/installed_mod_host_v2.cpp) |
| `InstalledModNotActive` | [src/mod/installed_mod_host_v2.cpp:381](../../src/mod/installed_mod_host_v2.cpp) |
| `InstalledModNotInstalled` | [src/mod/installed_mod_host_v2.cpp:351](../../src/mod/installed_mod_host_v2.cpp) |
| `InstalledModVersionConflict` | [src/mod/installed_mod_host_v2.cpp:290](../../src/mod/installed_mod_host_v2.cpp) |
| `InsufficientGridImportBudget` | [src/application/grid_research.cpp:23](../../src/application/grid_research.cpp) |
| `InsufficientTrainingSamples` | [src/application/polynomial_research.cpp:35](../../src/application/polynomial_research.cpp) |
| `IntegerBitLimit` | [include/bruce/math/numbers.hpp:7](../../include/bruce/math/numbers.hpp) |
| `IntegerOverflow` | [src/math/exact.cpp:33](../../src/math/exact.cpp) |
| `Invalid NanoCell active index` | [apps/nanocell_cli.hpp:138](../../apps/nanocell_cli.hpp) |
| `Invalid NanoCell answer version` | [mods/nanocell/src/nanocell.cpp:346](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell bundle framing or size` | [mods/nanocell/src/nanocell.cpp:419](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell context digest` | [mods/nanocell/src/nanocell.cpp:146](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell context id` | [mods/nanocell/src/nanocell.cpp:144](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell core layout version` | [mods/nanocell/src/nanocell.cpp:142](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell core length` | [mods/nanocell/src/nanocell.cpp:298](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell digest encoding` | [mods/nanocell/src/nanocell.cpp:259](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell digest length` | [mods/nanocell/src/nanocell.cpp:257](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell digest separator` | [mods/nanocell/src/nanocell.cpp:421](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell number` | [mods/nanocell/src/nanocell.cpp:233](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell operation budget` | [mods/nanocell/src/nanocell.cpp:147](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell program bytes` | [mods/nanocell/src/nanocell.cpp:308](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell program length` | [mods/nanocell/src/nanocell.cpp:153](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell request version` | [mods/nanocell/src/nanocell.cpp:141](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell technique id` | [mods/nanocell/src/nanocell.cpp:151](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell technique version` | [mods/nanocell/src/nanocell.cpp:152](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell trace budget` | [mods/nanocell/src/nanocell.cpp:148](../../mods/nanocell/src/nanocell.cpp) |
| `Invalid NanoCell trace count` | [mods/nanocell/src/nanocell.cpp:379](../../mods/nanocell/src/nanocell.cpp) |
| `InvalidBitLimit` | [src/application/grid_spec.cpp:11](../../src/application/grid_spec.cpp) |
| `InvalidBitLimit: expected 1..16384` | [src/math/numbers.cpp:9](../../src/math/numbers.cpp) |
| `InvalidBitLimit: expected 1..64` | [src/math/exact.cpp:5](../../src/math/exact.cpp) |
| `InvalidBudget: candidates 1..1000000, operations 1..10000000, trace 2..100002, results 1..4096` | [src/application/research.cpp:16](../../src/application/research.cpp) |
| `InvalidBundleBlockSize` | [src/application/compatibility.cpp:66](../../src/application/compatibility.cpp) |
| `InvalidBundleField: ` | [src/application/fraction_compare.cpp:70](../../src/application/fraction_compare.cpp) |
| `InvalidBundleField: debug` | [src/application/grid_stage_sweep.cpp:114](../../src/application/grid_stage_sweep.cpp) |
| `InvalidBundleField: stage` | [src/application/grid.cpp:114](../../src/application/grid.cpp) |
| `InvalidBundleField: stage_source` | [src/application/grid.cpp:113](../../src/application/grid.cpp) |
| `InvalidCandidateCount: expected 1..4096` | [src/compatibility/snapshot.cpp:22](../../src/compatibility/snapshot.cpp) |
| `InvalidCandidateHeader` | [src/compatibility/snapshot.cpp:100](../../src/compatibility/snapshot.cpp) |
| `InvalidCandidateRow` | [src/compatibility/snapshot.cpp:112](../../src/compatibility/snapshot.cpp) |
| `InvalidCommonRuntimeCompatibilityV1` | [src/application/common_runtime_compatibility_v1.cpp:38](../../src/application/common_runtime_compatibility_v1.cpp) |
| `InvalidCommonRuntimeCompatibilityV1Json` | [src/application/common_runtime_compatibility_v1.cpp:43](../../src/application/common_runtime_compatibility_v1.cpp) |
| `InvalidCommonRuntimeHostV2Batch` | [src/application/common_runtime_host_v2_batch.cpp:283](../../src/application/common_runtime_host_v2_batch.cpp) |
| `InvalidCommonRuntimeHostV2Run` | [src/application/common_runtime_host_v2.cpp:269](../../src/application/common_runtime_host_v2.cpp) |
| `InvalidComparisonCancellation` | [src/application/fraction_compare.cpp:78](../../src/application/fraction_compare.cpp) |
| `InvalidComparisonOperationBudget: expected 1..1000000` | [src/application/fraction_compare.cpp:77](../../src/application/fraction_compare.cpp) |
| `InvalidCompatibilityBudget` | [src/compatibility/snapshot.cpp:133](../../src/compatibility/snapshot.cpp) |
| `InvalidCompatibilityBundleField` | [src/application/compatibility.cpp:60](../../src/application/compatibility.cpp) |
| `InvalidCompatibilityTransfer` | [src/application/compatibility_transfer.cpp:29](../../src/application/compatibility_transfer.cpp) |
| `InvalidDagSchedulerCallbackRegistry` | [src/dag_scheduler/dag_scheduler_v1.cpp:35](../../src/dag_scheduler/dag_scheduler_v1.cpp) |
| `InvalidDagSchedulerCallbackRegistry: ` | [src/dag_scheduler/dag_scheduler_v1.cpp:40](../../src/dag_scheduler/dag_scheduler_v1.cpp) |
| `InvalidDagSchedulerCallbackResult: ` | [src/dag_scheduler/dag_scheduler_v1.cpp:167](../../src/dag_scheduler/dag_scheduler_v1.cpp) |
| `InvalidDagSchedulerDemoOutcome` | [apps/dag_scheduler_v1_cli.hpp:90](../../apps/dag_scheduler_v1_cli.hpp) |
| `InvalidDagSchedulerDemoTraceEvent` | [apps/dag_scheduler_v1_cli.hpp:102](../../apps/dag_scheduler_v1_cli.hpp) |
| `InvalidDagSchedulerDependency: ` | [src/dag_scheduler/dag_scheduler_v1.cpp:61](../../src/dag_scheduler/dag_scheduler_v1.cpp) |
| `InvalidDagSchedulerIdentifier` | [src/dag_scheduler/dag_scheduler_v1.cpp:20](../../src/dag_scheduler/dag_scheduler_v1.cpp) |
| `InvalidDagSchedulerReceiptOutcome` | [mods/mcp/src/mcp_inspector.cpp:235](../../mods/mcp/src/mcp_inspector.cpp) |
| `InvalidDagSchedulerSpec` | [src/dag_scheduler/dag_scheduler_v1.cpp:43](../../src/dag_scheduler/dag_scheduler_v1.cpp) |
| `InvalidDagSchedulerStartBudget` | [src/dag_scheduler/dag_scheduler_v1.cpp:45](../../src/dag_scheduler/dag_scheduler_v1.cpp) |
| `InvalidDagSchedulerTraceEvent` | [mods/mcp/src/mcp_inspector.cpp:248](../../mods/mcp/src/mcp_inspector.cpp) |
| `InvalidDagSchedulerV2CallbackRegistry` | [src/dag_scheduler/dag_scheduler_v2.cpp:38](../../src/dag_scheduler/dag_scheduler_v2.cpp) |
| `InvalidDagSchedulerV2CallbackRegistry: ` | [src/dag_scheduler/dag_scheduler_v2.cpp:43](../../src/dag_scheduler/dag_scheduler_v2.cpp) |
| `InvalidDagSchedulerV2CallbackResult: ` | [src/dag_scheduler/dag_scheduler_v2.cpp:182](../../src/dag_scheduler/dag_scheduler_v2.cpp) |
| `InvalidDagSchedulerV2DemoIndependent` | [apps/dag_scheduler_v2_cli.hpp:47](../../apps/dag_scheduler_v2_cli.hpp) |
| `InvalidDagSchedulerV2DemoInput` | [apps/dag_scheduler_v2_cli.hpp:28](../../apps/dag_scheduler_v2_cli.hpp) |
| `InvalidDagSchedulerV2DemoJoin` | [apps/dag_scheduler_v2_cli.hpp:53](../../apps/dag_scheduler_v2_cli.hpp) |
| `InvalidDagSchedulerV2DemoOutcome` | [apps/dag_scheduler_v2_cli.hpp:81](../../apps/dag_scheduler_v2_cli.hpp) |
| `InvalidDagSchedulerV2DemoSource` | [apps/dag_scheduler_v2_cli.hpp:34](../../apps/dag_scheduler_v2_cli.hpp) |
| `InvalidDagSchedulerV2DemoTraceEvent` | [apps/dag_scheduler_v2_cli.hpp:93](../../apps/dag_scheduler_v2_cli.hpp) |
| `InvalidDagSchedulerV2DemoTransform` | [apps/dag_scheduler_v2_cli.hpp:40](../../apps/dag_scheduler_v2_cli.hpp) |
| `InvalidDagSchedulerV2Dependency: ` | [src/dag_scheduler/dag_scheduler_v2.cpp:68](../../src/dag_scheduler/dag_scheduler_v2.cpp) |
| `InvalidDagSchedulerV2Spec` | [src/dag_scheduler/dag_scheduler_v2.cpp:21](../../src/dag_scheduler/dag_scheduler_v2.cpp) |
| `InvalidDagSchedulerV2Spec: ` | [src/dag_scheduler/dag_scheduler_v2.cpp:47](../../src/dag_scheduler/dag_scheduler_v2.cpp) |
| `InvalidDagSchedulerV3Outcome` | [apps/dag_scheduler_v3_cli.hpp:289](../../apps/dag_scheduler_v3_cli.hpp) |
| `InvalidDagSchedulerV3SpecCallback` | [apps/dag_scheduler_v3_cli.hpp:97](../../apps/dag_scheduler_v3_cli.hpp) |
| `InvalidDagSchedulerV3SpecCallbackArity` | [apps/dag_scheduler_v3_cli.hpp:45](../../apps/dag_scheduler_v3_cli.hpp) |
| `InvalidDagSchedulerV3SpecDecimal` | [apps/dag_scheduler_v3_cli.hpp:129](../../apps/dag_scheduler_v3_cli.hpp) |
| `InvalidDagSchedulerV3SpecHeader` | [apps/dag_scheduler_v3_cli.hpp:175](../../apps/dag_scheduler_v3_cli.hpp) |
| `InvalidDagSchedulerV3SpecHex` | [apps/dag_scheduler_v3_cli.hpp:154](../../apps/dag_scheduler_v3_cli.hpp) |
| `InvalidDagSchedulerV3SpecLine` | [apps/dag_scheduler_v3_cli.hpp:112](../../apps/dag_scheduler_v3_cli.hpp) |
| `InvalidDagSchedulerV3SpecOrder` | [apps/dag_scheduler_v3_cli.hpp:119](../../apps/dag_scheduler_v3_cli.hpp) |
| `InvalidDagSchedulerV3SpecSeparator` | [apps/dag_scheduler_v3_cli.hpp:185](../../apps/dag_scheduler_v3_cli.hpp) |
| `InvalidDagSchedulerV3SpecText` | [apps/dag_scheduler_v3_cli.hpp:170](../../apps/dag_scheduler_v3_cli.hpp) |
| `InvalidDagSchedulerV3TraceEvent` | [apps/dag_scheduler_v3_cli.hpp:301](../../apps/dag_scheduler_v3_cli.hpp) |
| `InvalidDagSchedulerV4Bundle` | [src/dag_scheduler/dag_scheduler_v4.cpp:17](../../src/dag_scheduler/dag_scheduler_v4.cpp) |
| `InvalidDagSchedulerV5Option` | [apps/dag_scheduler_v5_cli.hpp:218](../../apps/dag_scheduler_v5_cli.hpp) |
| `InvalidDenominatorLimit` | [src/application/research_contract.cpp:47](../../src/application/research_contract.cpp) |
| `InvalidDialogueStatus` | [apps/bruce_cli.cpp:342](../../apps/bruce_cli.cpp) |
| `InvalidDigestHeader` | [src/application/compatibility.cpp:235](../../src/application/compatibility.cpp) |
| `InvalidDomainId` | [src/compatibility/snapshot.cpp:28](../../src/compatibility/snapshot.cpp) |
| `InvalidDomainPolicy` | [src/application/fraction_compare.cpp:75](../../src/application/fraction_compare.cpp) |
| `InvalidDurableLogicModState` | [apps/runtime_mod_cli.hpp:86](../../apps/runtime_mod_cli.hpp) |
| `InvalidEvaluationMode` | [src/application/grid_spec.cpp:9](../../src/application/grid_spec.cpp) |
| `InvalidEvidence: digest mismatch` | [src/application/compatibility.cpp:237](../../src/application/compatibility.cpp) |
| `InvalidEvidenceChainBlockSize` | [src/evidence/chain.cpp:87](../../src/evidence/chain.cpp) |
| `InvalidEvidenceChainField` | [src/evidence/chain.cpp:80](../../src/evidence/chain.cpp) |
| `InvalidEvidenceGateInput` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:142](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `InvalidEvidenceGateMetadata` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:250](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `InvalidEvidenceGateRequestVersion` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:218](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `InvalidEvidenceReference` | [src/compatibility/snapshot.cpp:72](../../src/compatibility/snapshot.cpp) |
| `InvalidExactBudget` | [src/application/research_contract.cpp:51](../../src/application/research_contract.cpp) |
| `InvalidExactBundleField` | [src/application/exact_replay.cpp:147](../../src/application/exact_replay.cpp) |
| `InvalidExactInteger` | [src/math/numbers.cpp:14](../../src/math/numbers.cpp) |
| `InvalidExactSample` | [src/application/exact_replay.cpp:159](../../src/application/exact_replay.cpp) |
| `InvalidExactSearchRange: width must be 1..4096` | [src/application/research_contract.cpp:75](../../src/application/research_contract.cpp) |
| `InvalidExecutionTransition` | [src/application/execution_session.cpp:15](../../src/application/execution_session.cpp) |
| `InvalidExperienceChain` | [src/experience/store.cpp:42](../../src/experience/store.cpp) |
| `InvalidExperienceConfiguration` | [src/application/research_contract.cpp:59](../../src/application/research_contract.cpp) |
| `InvalidExperienceCorrectionReason` | [src/experience/store.cpp:50](../../src/experience/store.cpp) |
| `InvalidExperienceDigest` | [src/experience/store.cpp:113](../../src/experience/store.cpp) |
| `InvalidExperienceDigestHeader` | [src/experience/store.cpp:111](../../src/experience/store.cpp) |
| `InvalidExperienceField` | [src/experience/store.cpp:17](../../src/experience/store.cpp) |
| `InvalidExperienceRankingShape` | [src/application/experience_routing.cpp:51](../../src/application/experience_routing.cpp) |
| `InvalidExperienceReplacementChain` | [src/experience/store.cpp:55](../../src/experience/store.cpp) |
| `InvalidExperienceTransfer` | [src/application/experience_transfer.cpp:30](../../src/application/experience_transfer.cpp) |
| `InvalidExpressionChild` | [src/math/expression.cpp:18](../../src/math/expression.cpp) |
| `InvalidExpressionConfiguration` | [src/application/research_contract.cpp:69](../../src/application/research_contract.cpp) |
| `InvalidExpressionExponent` | [src/math/expression.cpp:22](../../src/math/expression.cpp) |
| `InvalidExpressionGrammar` | [src/math/expression.cpp:56](../../src/math/expression.cpp) |
| `InvalidExpressionOperator` | [src/math/expression.cpp:17](../../src/math/expression.cpp) |
| `InvalidExpressionOperators` | [src/math/expression.cpp:43](../../src/math/expression.cpp) |
| `InvalidExpressionPortfolioConfiguration` | [src/application/research_contract.cpp:68](../../src/application/research_contract.cpp) |
| `InvalidExpressionTree` | [src/math/expression.cpp:12](../../src/math/expression.cpp) |
| `InvalidFiveFamilyVersionRequest` | [src/application/portfolio_version.cpp:21](../../src/application/portfolio_version.cpp) |
| `InvalidGridAdditionalQueries` | [src/application/grid_resume.cpp:47](../../src/application/grid_resume.cpp) |
| `InvalidGridCancellation` | [src/application/grid_batch.cpp:99](../../src/application/grid_batch.cpp) |
| `InvalidGridParentSize` | [src/application/grid_resume.cpp:91](../../src/application/grid_resume.cpp) |
| `InvalidGridPath: expected x,y/x,y` | [src/coordinate/address.cpp:41](../../src/coordinate/address.cpp) |
| `InvalidGridQueryBudget` | [src/application/grid_batch.cpp:98](../../src/application/grid_batch.cpp) |
| `InvalidGridResearchBlockSize` | [src/application/grid_research.cpp:120](../../src/application/grid_research.cpp) |
| `InvalidGridResearchField` | [src/application/grid_research.cpp:114](../../src/application/grid_research.cpp) |
| `InvalidGridResumeField` | [src/application/grid_resume.cpp:18](../../src/application/grid_resume.cpp) |
| `InvalidGridRule` | [src/application/grid_spec.cpp:6](../../src/application/grid_spec.cpp) |
| `InvalidGridStage` | [apps/bruce_cli.cpp:118](../../apps/bruce_cli.cpp) |
| `InvalidGridTotalBudget` | [src/application/grid_research.cpp:20](../../src/application/grid_research.cpp) |
| `InvalidInstalledModState` | [src/mod/installed_mod_host_v2.cpp:60](../../src/mod/installed_mod_host_v2.cpp) |
| `InvalidInteger: ` | [include/bruce/core/types.hpp:19](../../include/bruce/core/types.hpp) |
| `InvalidInterpolationInput` | [src/math/polynomial.cpp:14](../../src/math/polynomial.cpp) |
| `InvalidLanguageAlias` | [mods/language/src/language.cpp:61](../../mods/language/src/language.cpp) |
| `InvalidLanguageAlternativeCount` | [mods/language/src/language.cpp:241](../../mods/language/src/language.cpp) |
| `InvalidLanguageCorePhases` | [mods/language/src/language_run.cpp:114](../../mods/language/src/language_run.cpp) |
| `InvalidLanguageDraftField` | [mods/language/src/language.cpp:80](../../mods/language/src/language.cpp) |
| `InvalidLanguageDraftHeader` | [mods/language/src/language.cpp:214](../../mods/language/src/language.cpp) |
| `InvalidLanguageInputCount` | [mods/language/src/language.cpp:59](../../mods/language/src/language.cpp) |
| `InvalidLanguageModCodec` | [apps/bruce_cli.cpp:267](../../apps/bruce_cli.cpp) |
| `InvalidLanguageRunField` | [mods/language/src/language_run.cpp:126](../../mods/language/src/language_run.cpp) |
| `InvalidLanguageRunHeader` | [mods/language/src/language_run.cpp:190](../../mods/language/src/language_run.cpp) |
| `InvalidLanguageRunPhases` | [mods/language/src/language_run.cpp:87](../../mods/language/src/language_run.cpp) |
| `InvalidLanguageSelection` | [mods/language/src/language.cpp:134](../../mods/language/src/language.cpp) |
| `InvalidLanguageStatus` | [apps/bruce_cli.cpp:160](../../apps/bruce_cli.cpp) |
| `InvalidLanguageUtf8` | [mods/language/src/language.cpp:40](../../mods/language/src/language.cpp) |
| `InvalidLocalAddress` | [src/coordinate/address.cpp:32](../../src/coordinate/address.cpp) |
| `InvalidLocalCoordinate` | [src/coordinate/address.cpp:28](../../src/coordinate/address.cpp) |
| `InvalidLogicModAction` | [apps/logic_mod_plugin_v1_cli.hpp:82](../../apps/logic_mod_plugin_v1_cli.hpp) |
| `InvalidLogicModArtifactDagV6Plan` | [src/mod/logic_mod_artifact_dag_v6_plan.cpp:10](../../src/mod/logic_mod_artifact_dag_v6_plan.cpp) |
| `InvalidLogicModChainArguments` | [apps/logic_mod_artifact_chain_v1_cli.hpp:20](../../apps/logic_mod_artifact_chain_v1_cli.hpp) |
| `InvalidLogicModContainer` | [src/mod/runtime_package.cpp:345](../../src/mod/runtime_package.cpp) |
| `InvalidLogicModDagArguments` | [apps/logic_mod_artifact_dag_v2_cli.hpp:205](../../apps/logic_mod_artifact_dag_v2_cli.hpp) |
| `InvalidLogicModDirectory` | [src/mod/logic_mod_plugin_v1.cpp:27](../../src/mod/logic_mod_plugin_v1.cpp) |
| `InvalidLogicModInputReference` | [apps/logic_mod_plugin_v1_cli.hpp:68](../../apps/logic_mod_plugin_v1_cli.hpp) |
| `InvalidLogicModJournal` | [src/mod/logic_mod_registry_v1.cpp:212](../../src/mod/logic_mod_registry_v1.cpp) |
| `InvalidLogicModJournalGeneration` | [src/mod/logic_mod_registry_v1.cpp:236](../../src/mod/logic_mod_registry_v1.cpp) |
| `InvalidLogicModMember` | [src/mod/runtime_package.cpp:347](../../src/mod/runtime_package.cpp) |
| `InvalidLogicModOption` | [apps/logic_mod_plugin_v1_cli.hpp:56](../../apps/logic_mod_plugin_v1_cli.hpp) |
| `InvalidLogicModPreparedExecution` | [src/mod/logic_mod_worker_v1.cpp:192](../../src/mod/logic_mod_worker_v1.cpp) |
| `InvalidLogicModPreparedWorker` | [src/mod/logic_mod_worker_attempt_v6.cpp:263](../../src/mod/logic_mod_worker_attempt_v6.cpp) |
| `InvalidLogicModRegistry` | [src/mod/logic_mod_registry_v1.cpp:117](../../src/mod/logic_mod_registry_v1.cpp) |
| `InvalidLogicModResultInspectArguments` | [apps/logic_mod_result_inspector_v1_cli.hpp:18](../../apps/logic_mod_result_inspector_v1_cli.hpp) |
| `InvalidLogicModTemplates` | [src/mod/runtime_package.cpp:359](../../src/mod/runtime_package.cpp) |
| `InvalidLogicModTransition` | [src/mod/logic_mod_plugin_v1.cpp:74](../../src/mod/logic_mod_plugin_v1.cpp) |
| `InvalidLogicModWorker` | [src/mod/runtime_package.cpp:370](../../src/mod/runtime_package.cpp) |
| `InvalidLogicModWorkerAttempt` | [src/mod/logic_mod_worker_attempt_v6.cpp:384](../../src/mod/logic_mod_worker_attempt_v6.cpp) |
| `InvalidLogicSession` | [apps/logic_mod_plugin_v1_cli.hpp:91](../../apps/logic_mod_plugin_v1_cli.hpp) |
| `InvalidLogicState` | [apps/logic_mod_plugin_v1_cli.hpp:26](../../apps/logic_mod_plugin_v1_cli.hpp) |
| `InvalidMcpArguments` | [mods/mcp/src/mod.cpp:24](../../mods/mcp/src/mod.cpp) |
| `InvalidMcpInspectionStatus` | [mods/mcp/src/mcp_inspector.cpp:315](../../mods/mcp/src/mcp_inspector.cpp) |
| `InvalidMcpPort` | [mods/mcp/src/mod.cpp:31](../../mods/mcp/src/mod.cpp) |
| `InvalidModCapabilities` | [src/mod/mod_api.cpp:99](../../src/mod/mod_api.cpp) |
| `InvalidModCapability` | [src/mod/mod_api.cpp:32](../../src/mod/mod_api.cpp) |
| `InvalidModManifest` | [src/mod/mod_api.cpp:96](../../src/mod/mod_api.cpp) |
| `InvalidModPackageEnvelope` | [src/mod/package_inspection_v2.cpp:139](../../src/mod/package_inspection_v2.cpp) |
| `InvalidModPackageKind` | [src/mod/package_inspection_v2.cpp:89](../../src/mod/package_inspection_v2.cpp) |
| `InvalidModRunCodec` | [src/mod/mod_api.cpp:79](../../src/mod/mod_api.cpp) |
| `InvalidModRunField` | [src/mod/mod_api.cpp:59](../../src/mod/mod_api.cpp) |
| `InvalidModRunHeader` | [src/mod/mod_api.cpp:140](../../src/mod/mod_api.cpp) |
| `InvalidModularPredicate` | [src/compatibility/snapshot.cpp:75](../../src/compatibility/snapshot.cpp) |
| `InvalidModulus` | [src/application/grid_spec.cpp:13](../../src/application/grid_spec.cpp) |
| `InvalidNanoCellCoreHex` | [apps/nanocell_cli.hpp:47](../../apps/nanocell_cli.hpp) |
| `InvalidNanoCellModCodec` | [mods/nanocell/src/mod.cpp:16](../../mods/nanocell/src/mod.cpp) |
| `InvalidNanoCellPhaseContextId` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:270](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `InvalidNanoCellPhaseCoreLayout` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:280](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `InvalidNanoCellPhaseCoreSize` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:261](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `InvalidNanoCellPhaseFinalState` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:205](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `InvalidNanoCellPhaseLength` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:41](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `InvalidNanoCellPhaseMetadata` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:325](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `InvalidNanoCellPhaseNumber` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:60](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `InvalidNanoCellPhasePayload` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:75](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `InvalidNanoCellPhaseRequest` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:278](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `InvalidNanoCellPhaseRequestVersion` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:257](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `InvalidNanoCellPhaseStatus` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:91](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `InvalidNanoCellStatus` | [apps/nanocell_cli.hpp:72](../../apps/nanocell_cli.hpp) |
| `InvalidNanoCellTraceKind` | [apps/nanocell_cli.hpp:81](../../apps/nanocell_cli.hpp) |
| `InvalidNormalizationConfiguration` | [src/math/domain_cache.cpp:5](../../src/math/domain_cache.cpp) |
| `InvalidNormalizedRouteInput` | [src/tree/registry.cpp:83](../../src/tree/registry.cpp) |
| `InvalidOrDuplicateRuleId` | [src/compatibility/snapshot.cpp:67](../../src/compatibility/snapshot.cpp) |
| `InvalidPayloadGateDecision` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:68](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `InvalidPayloadGateInput` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:154](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `InvalidPayloadGateInputCount` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:146](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `InvalidPayloadGateMetadataVersion` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:226](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `InvalidPayloadGatePayload` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:247](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `InvalidPayloadGateRequestVersion` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:220](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `InvalidPhaseAdapterOutput` | [src/phase_dag/phase_dag.cpp:56](../../src/phase_dag/phase_dag.cpp) |
| `InvalidPhaseDagBundleCount` | [src/phase_dag/phase_dag.cpp:298](../../src/phase_dag/phase_dag.cpp) |
| `InvalidPhaseDagBundleDigest` | [src/phase_dag/phase_dag.cpp:536](../../src/phase_dag/phase_dag.cpp) |
| `InvalidPhaseDagBundleHeader` | [src/phase_dag/phase_dag.cpp:530](../../src/phase_dag/phase_dag.cpp) |
| `InvalidPhaseDagBundleInteger` | [src/phase_dag/phase_dag.cpp:250](../../src/phase_dag/phase_dag.cpp) |
| `InvalidPhaseDagBundleLength` | [src/phase_dag/phase_dag.cpp:235](../../src/phase_dag/phase_dag.cpp) |
| `InvalidPhaseDagBundleOutputMarker` | [src/phase_dag/phase_dag.cpp:351](../../src/phase_dag/phase_dag.cpp) |
| `InvalidPhaseDagCatalog` | [src/phase_dag/phase_dag_v2.cpp:49](../../src/phase_dag/phase_dag_v2.cpp) |
| `InvalidPhaseDagDependency` | [src/phase_dag/phase_dag.cpp:404](../../src/phase_dag/phase_dag.cpp) |
| `InvalidPhaseDagInput` | [src/phase_dag/phase_dag_v3.cpp:214](../../src/phase_dag/phase_dag_v3.cpp) |
| `InvalidPhaseDagLimit` | [src/phase_dag/phase_dag.cpp:383](../../src/phase_dag/phase_dag.cpp) |
| `InvalidPhaseDagNode` | [src/phase_dag/phase_dag.cpp:396](../../src/phase_dag/phase_dag.cpp) |
| `InvalidPhaseDagParallelLimit` | [apps/phase_dag_v3_cli.hpp:37](../../apps/phase_dag_v3_cli.hpp) |
| `InvalidPhaseDagReceiptOutcome` | [apps/phase_dag_v2_cli.hpp:157](../../apps/phase_dag_v2_cli.hpp) |
| `InvalidPhaseDagSpec` | [src/phase_dag/phase_dag.cpp:379](../../src/phase_dag/phase_dag.cpp) |
| `InvalidPhaseDagSpecActiveIndex` | [apps/phase_dag_v2_cli.hpp:96](../../apps/phase_dag_v2_cli.hpp) |
| `InvalidPhaseDagSpecAssignment` | [apps/phase_dag_v2_cli.hpp:128](../../apps/phase_dag_v2_cli.hpp) |
| `InvalidPhaseDagSpecContextFile` | [apps/phase_dag_v2_cli.hpp:101](../../apps/phase_dag_v2_cli.hpp) |
| `InvalidPhaseDagSpecCore` | [apps/phase_dag_v2_cli.hpp:86](../../apps/phase_dag_v2_cli.hpp) |
| `InvalidPhaseDagSpecDependency` | [apps/phase_dag_v2_cli.hpp:55](../../apps/phase_dag_v2_cli.hpp) |
| `InvalidPhaseDagSpecHeader` | [apps/phase_dag_v2_cli.hpp:118](../../apps/phase_dag_v2_cli.hpp) |
| `InvalidPhaseDagSpecNumber` | [apps/phase_dag_v2_cli.hpp:28](../../apps/phase_dag_v2_cli.hpp) |
| `InvalidPhaseOutputStatus` | [mods/mcp/src/mcp_inspector.cpp:22](../../mods/mcp/src/mcp_inspector.cpp) |
| `InvalidPhasePayloadNumber` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:46](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `InvalidPhaseReceiptOutcome` | [mods/mcp/src/mcp_inspector.cpp:32](../../mods/mcp/src/mcp_inspector.cpp) |
| `InvalidPhaseTraceEvent` | [mods/mcp/src/mcp_inspector.cpp:45](../../mods/mcp/src/mcp_inspector.cpp) |
| `InvalidPolynomialDegree` | [src/application/research_contract.cpp:57](../../src/application/research_contract.cpp) |
| `InvalidPolynomialSize` | [src/math/polynomial.cpp:7](../../src/math/polynomial.cpp) |
| `InvalidPortfolioDomainCache` | [src/application/portfolio_normalization.cpp:13](../../src/application/portfolio_normalization.cpp) |
| `InvalidPowerLimit` | [src/application/research_contract.cpp:72](../../src/application/research_contract.cpp) |
| `InvalidPowerLimit: expected 1..8` | [src/math/power_affine.cpp:6](../../src/math/power_affine.cpp) |
| `InvalidRationalLinearDomainOrPower` | [src/application/research_contract.cpp:56](../../src/application/research_contract.cpp) |
| `InvalidRationalLinearSystem` | [src/math/rational_linear.cpp:29](../../src/math/rational_linear.cpp) |
| `InvalidReleaseSignatureV1` | [src/release/release_signature_v1.cpp:17](../../src/release/release_signature_v1.cpp) |
| `InvalidRoutePattern` | [src/tree/registry.cpp:61](../../src/tree/registry.cpp) |
| `InvalidRouteReport` | [apps/runtime_mod_cli.hpp:222](../../apps/runtime_mod_cli.hpp) |
| `InvalidRouteStatus` | [apps/runtime_mod_cli.hpp:98](../../apps/runtime_mod_cli.hpp) |
| `InvalidRouteUtf8` | [src/tree/normalizer.cpp:34](../../src/tree/normalizer.cpp) |
| `InvalidRuleHistory` | [src/application/rule_history.cpp:41](../../src/application/rule_history.cpp) |
| `InvalidRuleProposal` | [src/application/rule_proposal.cpp:17](../../src/application/rule_proposal.cpp) |
| `InvalidRuleRow: expected 10 fields` | [src/compatibility/snapshot.cpp:64](../../src/compatibility/snapshot.cpp) |
| `InvalidRuleStage` | [src/application/rule_history.cpp:25](../../src/application/rule_history.cpp) |
| `InvalidRuleWeight` | [src/compatibility/snapshot.cpp:73](../../src/compatibility/snapshot.cpp) |
| `InvalidRuntimeModContainer` | [src/mod/runtime_package.cpp:238](../../src/mod/runtime_package.cpp) |
| `InvalidRuntimeModJournal` | [src/mod/runtime_registry.cpp:151](../../src/mod/runtime_registry.cpp) |
| `InvalidRuntimeModJournalGeneration` | [src/mod/runtime_registry.cpp:173](../../src/mod/runtime_registry.cpp) |
| `InvalidRuntimeModMember` | [src/mod/runtime_package.cpp:290](../../src/mod/runtime_package.cpp) |
| `InvalidRuntimeModRegistry` | [src/mod/runtime_registry.cpp:73](../../src/mod/runtime_registry.cpp) |
| `InvalidRuntimeModSchema` | [src/mod/runtime_package.cpp:18](../../src/mod/runtime_package.cpp) |
| `InvalidRuntimeModSignatureEncoding` | [src/mod/runtime_package.cpp:294](../../src/mod/runtime_package.cpp) |
| `InvalidRuntimeModState` | [apps/runtime_mod_cli.hpp:78](../../apps/runtime_mod_cli.hpp) |
| `InvalidRuntimeModTrustStore` | [src/mod/runtime_package.cpp:305](../../src/mod/runtime_package.cpp) |
| `InvalidRuntimeOrchestratorArguments` | [apps/runtime_orchestrator_v1_cli.hpp:71](../../apps/runtime_orchestrator_v1_cli.hpp) |
| `InvalidRuntimeOrchestratorJob` | [src/runtime/runtime_orchestrator_v1.cpp:45](../../src/runtime/runtime_orchestrator_v1.cpp) |
| `InvalidRuntimeOrchestratorLimits` | [src/runtime/runtime_orchestrator_v1.cpp:74](../../src/runtime/runtime_orchestrator_v1.cpp) |
| `InvalidRuntimeOrchestratorReservation` | [src/runtime/runtime_orchestrator_v1.cpp:89](../../src/runtime/runtime_orchestrator_v1.cpp) |
| `InvalidRuntimeOrchestratorRunId` | [src/runtime/runtime_orchestrator_v1.cpp:94](../../src/runtime/runtime_orchestrator_v1.cpp) |
| `InvalidRuntimeOrchestratorTransition` | [src/runtime/runtime_orchestrator_v1.cpp:52](../../src/runtime/runtime_orchestrator_v1.cpp) |
| `InvalidRuntimeTrustV2` | [src/mod/runtime_trust_store_v2.cpp:29](../../src/mod/runtime_trust_store_v2.cpp) |
| `InvalidSample` | [src/application/replay.cpp:35](../../src/application/replay.cpp) |
| `InvalidSampleCount` | [src/application/research_contract.cpp:78](../../src/application/research_contract.cpp) |
| `InvalidSampleCount: training 1..4096, holdout 0..4096` | [src/application/research.cpp:12](../../src/application/research.cpp) |
| `InvalidSampleHeader: expected x,y` | [src/application/exact_research.cpp:205](../../src/application/exact_research.cpp) |
| `InvalidSampleHeader: expected x,y (input,expected_output)` | [src/application/replay.cpp:89](../../src/application/replay.cpp) |
| `InvalidSampleRow` | [src/application/exact_research.cpp:210](../../src/application/exact_research.cpp) |
| `InvalidSampleRow: expected x,y` | [src/application/replay.cpp:94](../../src/application/replay.cpp) |
| `InvalidSearchRange: expected -1000000 <= min <= max <= 1000000` | [src/application/research.cpp:10](../../src/application/research.cpp) |
| `InvalidSignedAffineDagNodeId: ` | [src/signed_affine_dag/signed_affine_dag_v1.cpp:59](../../src/signed_affine_dag/signed_affine_dag_v1.cpp) |
| `InvalidSignedAffineDagOperator: ` | [src/signed_affine_dag/signed_affine_dag_v1.cpp:20](../../src/signed_affine_dag/signed_affine_dag_v1.cpp) |
| `InvalidSignedAffineDagPathWeight: ` | [src/signed_affine_dag/signed_affine_dag_v1.cpp:65](../../src/signed_affine_dag/signed_affine_dag_v1.cpp) |
| `InvalidSignedAffineDagSource: ` | [src/signed_affine_dag/signed_affine_dag_v1.cpp:68](../../src/signed_affine_dag/signed_affine_dag_v1.cpp) |
| `InvalidSignedAffineDagSource: no source` | [src/signed_affine_dag/signed_affine_dag_v1.cpp:76](../../src/signed_affine_dag/signed_affine_dag_v1.cpp) |
| `InvalidSignedAffineRelationSelection` | [src/signed_affine_relation/signed_affine_relation_v1.cpp:79](../../src/signed_affine_relation/signed_affine_relation_v1.cpp) |
| `InvalidSignedAffineRelationSourceCount` | [src/signed_affine_relation/signed_affine_relation_v1.cpp:76](../../src/signed_affine_relation/signed_affine_relation_v1.cpp) |
| `InvalidSignedAffineRelationTrace: ` | [src/signed_affine_relation/signed_affine_relation_v1.cpp:14](../../src/signed_affine_relation/signed_affine_relation_v1.cpp) |
| `InvalidSnapshotDigest` | [src/application/compatibility.cpp:252](../../src/application/compatibility.cpp) |
| `InvalidSnapshotId` | [src/compatibility/snapshot.cpp:58](../../src/compatibility/snapshot.cpp) |
| `InvalidStageRange` | [apps/bruce_cli.cpp:881](../../apps/bruce_cli.cpp) |
| `InvalidSweepVerificationSource` | [src/application/grid_stage_verify.cpp:33](../../src/application/grid_stage_verify.cpp) |
| `InvalidTaskSpecAlternative` | [src/token/task_spec.cpp:164](../../src/token/task_spec.cpp) |
| `InvalidTaskSpecAlternativeCount` | [src/token/task_spec.cpp:250](../../src/token/task_spec.cpp) |
| `InvalidTaskSpecAsset` | [src/token/task_spec.cpp:158](../../src/token/task_spec.cpp) |
| `InvalidTaskSpecAssetCount` | [src/token/task_spec.cpp:156](../../src/token/task_spec.cpp) |
| `InvalidTaskSpecField` | [src/token/task_spec.cpp:20](../../src/token/task_spec.cpp) |
| `InvalidTaskSpecHeader` | [src/token/task_spec.cpp:237](../../src/token/task_spec.cpp) |
| `InvalidTaskSpecLimit` | [src/token/task_spec.cpp:138](../../src/token/task_spec.cpp) |
| `InvalidTaskSpecNumber` | [src/token/task_spec.cpp:34](../../src/token/task_spec.cpp) |
| `InvalidTaskSpecRange` | [src/token/task_spec.cpp:143](../../src/token/task_spec.cpp) |
| `InvalidTaskSpecRole` | [src/token/task_spec.cpp:101](../../src/token/task_spec.cpp) |
| `InvalidTaskSpecRoles` | [src/token/task_spec.cpp:97](../../src/token/task_spec.cpp) |
| `InvalidTaskSpecRouteAsset` | [src/token/task_spec.cpp:108](../../src/token/task_spec.cpp) |
| `InvalidTaskSpecSelection` | [src/token/task_spec.cpp:171](../../src/token/task_spec.cpp) |
| `InvalidTaskSpecUtf8` | [src/token/task_spec.cpp:65](../../src/token/task_spec.cpp) |
| `InvalidTokenEntry` | [src/token/task_spec.cpp:149](../../src/token/task_spec.cpp) |
| `InvalidTokenIdentity` | [src/token/task_spec.cpp:151](../../src/token/task_spec.cpp) |
| `InvalidTokenNormalization` | [src/token/task_spec.cpp:196](../../src/token/task_spec.cpp) |
| `InvalidTokenOrdinal` | [src/token/task_spec.cpp:194](../../src/token/task_spec.cpp) |
| `InvalidTokenSequenceSize` | [src/token/task_spec.cpp:245](../../src/token/task_spec.cpp) |
| `InvalidTokenTableSize` | [src/token/task_spec.cpp:243](../../src/token/task_spec.cpp) |
| `InvalidTokenizedTaskNativePhases` | [src/application/tokenized_task_run.cpp:74](../../src/application/tokenized_task_run.cpp) |
| `InvalidTokenizedTaskRunField` | [src/application/tokenized_task_run.cpp:88](../../src/application/tokenized_task_run.cpp) |
| `InvalidTokenizedTaskRunHeader` | [src/application/tokenized_task_run.cpp:137](../../src/application/tokenized_task_run.cpp) |
| `InvalidTokenizedTaskRunPhases` | [src/application/tokenized_task_run.cpp:16](../../src/application/tokenized_task_run.cpp) |
| `InvalidToyRelationCoefficient` | [src/toy_relation/toy_relation_v1.cpp:21](../../src/toy_relation/toy_relation_v1.cpp) |
| `InvalidToyRelationGenerator` | [src/toy_relation/toy_relation_v1.cpp:14](../../src/toy_relation/toy_relation_v1.cpp) |
| `InvalidToyRelationModulus` | [src/toy_relation/toy_relation_v1.cpp:11](../../src/toy_relation/toy_relation_v1.cpp) |
| `InvalidToyRelationProvenanceId` | [src/toy_relation/toy_relation_v1.cpp:40](../../src/toy_relation/toy_relation_v1.cpp) |
| `InvalidToyRelationPublicPoint` | [src/toy_relation/toy_relation_v1.cpp:16](../../src/toy_relation/toy_relation_v1.cpp) |
| `InvalidTreeAddress` | [src/tree/registry.cpp:50](../../src/tree/registry.cpp) |
| `InvalidTreeKey` | [src/tree/registry.cpp:55](../../src/tree/registry.cpp) |
| `InvalidTreeOwner` | [src/tree/registry.cpp:42](../../src/tree/registry.cpp) |
| `InvalidUnifiedModCatalogV2` | [src/mod/unified_catalog_v2.cpp:12](../../src/mod/unified_catalog_v2.cpp) |
| `InvalidWorkerAdapter` | [apps/dag_scheduler_v5_cli.hpp:1371](../../apps/dag_scheduler_v5_cli.hpp) |
| `InvalidWorkerAttemptHandle` | [src/worker/worker_supervisor_v1.cpp:660](../../src/worker/worker_supervisor_v1.cpp) |
| `InvalidWorkerFailureCode` | [apps/dag_scheduler_v5_cli.hpp:1518](../../apps/dag_scheduler_v5_cli.hpp) |
| `InvalidWorkerFrameDigest` | [src/worker/worker_boundary_v1.cpp:101](../../src/worker/worker_boundary_v1.cpp) |
| `InvalidWorkerFrameHeader` | [apps/dag_scheduler_v5_cli.hpp:1299](../../apps/dag_scheduler_v5_cli.hpp) |
| `InvalidWorkerFrameLength` | [apps/dag_scheduler_v5_cli.hpp:1313](../../apps/dag_scheduler_v5_cli.hpp) |
| `InvalidWorkerFrameMagic` | [src/worker/worker_boundary_v1.cpp:85](../../src/worker/worker_boundary_v1.cpp) |
| `InvalidWorkerFrameSize` | [apps/dag_scheduler_v5_cli.hpp:1346](../../apps/dag_scheduler_v5_cli.hpp) |
| `InvalidWorkerFrameType` | [apps/dag_scheduler_v5_cli.hpp:1446](../../apps/dag_scheduler_v5_cli.hpp) |
| `InvalidWorkerInput` | [apps/dag_scheduler_v5_cli.hpp:1170](../../apps/dag_scheduler_v5_cli.hpp) |
| `InvalidWorkerPayload` | [src/worker/worker_supervisor_v1.cpp:72](../../src/worker/worker_supervisor_v1.cpp) |
| `InvalidWorkerRequest` | [apps/dag_scheduler_v5_cli.hpp:1366](../../apps/dag_scheduler_v5_cli.hpp) |
| `InvalidWorkerResultFrame` | [src/worker/worker_supervisor_v1.cpp:819](../../src/worker/worker_supervisor_v1.cpp) |
| `InvalidWorkerStatus` | [apps/dag_scheduler_v5_cli.hpp:1498](../../apps/dag_scheduler_v5_cli.hpp) |
| `InvalidZeroZeroPolicy` | [src/application/grid_spec.cpp:10](../../src/application/grid_spec.cpp) |
| `LanguageAliasOptionsRequireV2` | [apps/bruce_cli.cpp:1052](../../apps/bruce_cli.cpp) |
| `LanguageCoreByteLimit` | [mods/language/src/language_run.cpp:104](../../mods/language/src/language_run.cpp) |
| `LanguageCoreDigestMismatch` | [mods/language/src/language_run.cpp:202](../../mods/language/src/language_run.cpp) |
| `LanguageCoreEvidenceMismatch` | [mods/language/src/language_run.cpp:118](../../mods/language/src/language_run.cpp) |
| `LanguageCoreTaskMismatch` | [mods/language/src/language_run.cpp:65](../../mods/language/src/language_run.cpp) |
| `LanguageDraftByteLimit` | [mods/language/src/language.cpp:207](../../mods/language/src/language.cpp) |
| `LanguageDraftDigestMismatch` | [mods/language/src/language.cpp:217](../../mods/language/src/language.cpp) |
| `LanguageExplanationMismatch` | [mods/language/src/language_run.cpp:113](../../mods/language/src/language_run.cpp) |
| `LanguageInputDigestMismatch` | [mods/language/src/language.cpp:62](../../mods/language/src/language.cpp) |
| `LanguageModDraftRunMismatch` | [apps/bruce_cli.cpp:271](../../apps/bruce_cli.cpp) |
| `LanguageRunByteLimit` | [mods/language/src/language_run.cpp:183](../../mods/language/src/language_run.cpp) |
| `LanguageRunDigestMismatch` | [mods/language/src/language_run.cpp:192](../../mods/language/src/language_run.cpp) |
| `LocalModBuildFailed` | [src/mod/local_builder.cpp:22](../../src/mod/local_builder.cpp) |
| `LogicBudgetDenied` | [src/mod/logic_mod_artifact_chain_v1.cpp:129](../../src/mod/logic_mod_artifact_chain_v1.cpp) |
| `LogicCandidateStatusInvalid` | [src/mod/logic_mod_worker_v1.cpp:128](../../src/mod/logic_mod_worker_v1.cpp) |
| `LogicExecGateFailed` | [src/mod/logic_mod_worker_attempt_v6.cpp:139](../../src/mod/logic_mod_worker_attempt_v6.cpp) |
| `LogicExecGateSetupFailed` | [src/mod/logic_mod_worker_attempt_v6.cpp:131](../../src/mod/logic_mod_worker_attempt_v6.cpp) |
| `LogicFrameInvalid` | [src/mod/logic_mod_worker_v1.cpp:18](../../src/mod/logic_mod_worker_v1.cpp) |
| `LogicFrameTrailingBytes` | [src/mod/logic_mod_worker_v1.cpp:28](../../src/mod/logic_mod_worker_v1.cpp) |
| `LogicHostValidationFailed` | [src/mod/logic_mod_worker_v1.cpp:131](../../src/mod/logic_mod_worker_v1.cpp) |
| `LogicInputBudgetExceeded` | [src/mod/logic_mod_artifact_chain_v1.cpp:130](../../src/mod/logic_mod_artifact_chain_v1.cpp) |
| `LogicModAlreadyInstalled` | [src/mod/logic_mod_registry_v1.cpp:468](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicModArtifactDagV6PlanAlreadyRun` | [src/mod/logic_mod_artifact_dag_v6_plan.cpp:90](../../src/mod/logic_mod_artifact_dag_v6_plan.cpp) |
| `LogicModBindingDenied` | [src/mod/logic_mod_registry_v1.cpp:597](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicModCapabilityDenied` | [src/mod/logic_mod_plugin_v1.cpp:177](../../src/mod/logic_mod_plugin_v1.cpp) |
| `LogicModChainCapabilityDenied` | [src/mod/logic_mod_artifact_chain_v1.cpp:142](../../src/mod/logic_mod_artifact_chain_v1.cpp) |
| `LogicModChainFileInvalid` | [apps/logic_mod_artifact_chain_v1_cli.hpp:42](../../apps/logic_mod_artifact_chain_v1_cli.hpp) |
| `LogicModChainRecordInvalid` | [src/mod/logic_mod_artifact_chain_v1.cpp:12](../../src/mod/logic_mod_artifact_chain_v1.cpp) |
| `LogicModChainRecordTooLarge` | [src/mod/logic_mod_artifact_chain_v1.cpp:179](../../src/mod/logic_mod_artifact_chain_v1.cpp) |
| `LogicModChainStoreMismatch` | [src/mod/logic_mod_artifact_chain_v1.cpp:149](../../src/mod/logic_mod_artifact_chain_v1.cpp) |
| `LogicModCommitDurabilityUnknown` | [src/mod/logic_mod_registry_v1.cpp:277](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicModDagBudgetLimitInvalid` | [src/mod/logic_mod_artifact_dag_v4.cpp:224](../../src/mod/logic_mod_artifact_dag_v4.cpp) |
| `LogicModDagCapabilityDenied` | [src/mod/logic_mod_artifact_dag_v2.cpp:147](../../src/mod/logic_mod_artifact_dag_v2.cpp) |
| `LogicModDagInputTooLarge` | [src/mod/logic_mod_artifact_dag_v2.cpp:159](../../src/mod/logic_mod_artifact_dag_v2.cpp) |
| `LogicModDagInvalid` | [src/mod/logic_mod_artifact_dag_v2.cpp:56](../../src/mod/logic_mod_artifact_dag_v2.cpp) |
| `LogicModDagParallelLimitInvalid` | [src/mod/logic_mod_artifact_dag_v3.cpp:195](../../src/mod/logic_mod_artifact_dag_v3.cpp) |
| `LogicModDagRecordInvalid` | [src/mod/logic_mod_artifact_dag_v2.cpp:16](../../src/mod/logic_mod_artifact_dag_v2.cpp) |
| `LogicModDagRecordTooLarge` | [src/mod/logic_mod_artifact_dag_v2.cpp:184](../../src/mod/logic_mod_artifact_dag_v2.cpp) |
| `LogicModDagReportRequired` | [src/mod/logic_mod_artifact_dag_v6.cpp:57](../../src/mod/logic_mod_artifact_dag_v6.cpp) |
| `LogicModDagSpecInvalid` | [apps/logic_mod_artifact_dag_v2_cli.hpp:12](../../apps/logic_mod_artifact_dag_v2_cli.hpp) |
| `LogicModFileOpenFailed` | [src/mod/logic_mod_plugin_v1.cpp:19](../../src/mod/logic_mod_plugin_v1.cpp) |
| `LogicModFileReadFailed` | [src/mod/logic_mod_plugin_v1.cpp:61](../../src/mod/logic_mod_plugin_v1.cpp) |
| `LogicModFileStatFailed` | [src/mod/logic_mod_registry_v1.cpp:62](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicModGenerationLimit` | [src/mod/logic_mod_registry_v1.cpp:357](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicModGenerationOverflow` | [src/mod/installed_mod_host_v2.cpp:519](../../src/mod/installed_mod_host_v2.cpp) |
| `LogicModGoldenCaseFailed` | [src/mod/logic_mod_plugin_v1.cpp:121](../../src/mod/logic_mod_plugin_v1.cpp) |
| `LogicModGoldenCoverageMissing` | [src/mod/logic_mod_plugin_v1.cpp:124](../../src/mod/logic_mod_plugin_v1.cpp) |
| `LogicModInputOptionsMustPair` | [apps/logic_mod_plugin_v1_cli.hpp:61](../../apps/logic_mod_plugin_v1_cli.hpp) |
| `LogicModInstalledPackageMismatch` | [src/mod/installed_mod_host_v2.cpp:103](../../src/mod/installed_mod_host_v2.cpp) |
| `LogicModInstalledWorkerMismatch` | [src/mod/logic_mod_registry_v1.cpp:312](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicModJournalLimit` | [src/mod/logic_mod_registry_v1.cpp:269](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicModJournalRecoveryFailed` | [src/mod/logic_mod_registry_v1.cpp:246](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicModJournalWriteFailed` | [src/mod/logic_mod_registry_v1.cpp:274](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicModNotActive` | [src/mod/installed_mod_host_v2.cpp:436](../../src/mod/installed_mod_host_v2.cpp) |
| `LogicModNotInstalled` | [src/mod/logic_mod_registry_v1.cpp:546](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicModOutputCapabilityDenied` | [src/mod/logic_mod_result_artifact_v1.cpp:35](../../src/mod/logic_mod_result_artifact_v1.cpp) |
| `LogicModOutputLimitExceeded` | [src/mod/logic_mod_result_artifact_v1.cpp:36](../../src/mod/logic_mod_result_artifact_v1.cpp) |
| `LogicModOutputReplayMismatch` | [src/mod/logic_mod_artifact_dag_v5.cpp:24](../../src/mod/logic_mod_artifact_dag_v5.cpp) |
| `LogicModPayloadCapabilityDenied` | [src/mod/logic_mod_payload_v1.cpp:104](../../src/mod/logic_mod_payload_v1.cpp) |
| `LogicModPayloadLimitExceeded` | [src/mod/logic_mod_payload_v1.cpp:110](../../src/mod/logic_mod_payload_v1.cpp) |
| `LogicModPayloadManifestNotReady` | [src/mod/logic_mod_payload_v1.cpp:107](../../src/mod/logic_mod_payload_v1.cpp) |
| `LogicModPayloadObjectCorrupt` | [src/mod/logic_mod_payload_v1.cpp:50](../../src/mod/logic_mod_payload_v1.cpp) |
| `LogicModPayloadObjectIndexInvalid` | [src/mod/logic_mod_payload_v1.cpp:108](../../src/mod/logic_mod_payload_v1.cpp) |
| `LogicModPayloadOpenFailed` | [src/mod/logic_mod_payload_v1.cpp:17](../../src/mod/logic_mod_payload_v1.cpp) |
| `LogicModPayloadSealFailed` | [src/mod/logic_mod_payload_v1.cpp:72](../../src/mod/logic_mod_payload_v1.cpp) |
| `LogicModPayloadStorePathInvalid` | [src/mod/logic_mod_payload_v1.cpp:33](../../src/mod/logic_mod_payload_v1.cpp) |
| `LogicModPayloadStorePermissions` | [src/mod/logic_mod_payload_v1.cpp:26](../../src/mod/logic_mod_payload_v1.cpp) |
| `LogicModRecordLimit` | [src/mod/logic_mod_registry_v1.cpp:182](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicModRecoveryRequired` | [src/mod/logic_mod_registry_v1.cpp:242](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicModRegistryJournalMismatch` | [src/mod/logic_mod_registry_v1.cpp:250](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicModRegistryLimit` | [src/mod/logic_mod_plugin_v1.cpp:94](../../src/mod/logic_mod_plugin_v1.cpp) |
| `LogicModResultRecordInvalid` | [src/mod/logic_mod_result_inspector_v1.cpp:15](../../src/mod/logic_mod_result_inspector_v1.cpp) |
| `LogicModRevoked` | [src/mod/logic_mod_registry_v1.cpp:547](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicModStoreByteLimit` | [src/mod/logic_mod_registry_v1.cpp:89](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicModTemplateDenied` | [src/mod/logic_mod_plugin_v1.cpp:176](../../src/mod/logic_mod_plugin_v1.cpp) |
| `LogicModTemplateMismatch` | [src/mod/runtime_package.cpp:396](../../src/mod/runtime_package.cpp) |
| `LogicModVersionAlreadyActive` | [src/mod/logic_mod_plugin_v1.cpp:139](../../src/mod/logic_mod_plugin_v1.cpp) |
| `LogicModWorkerChanged` | [src/mod/logic_mod_plugin_v1.cpp:69](../../src/mod/logic_mod_plugin_v1.cpp) |
| `LogicModWorkerDigestMismatch` | [src/mod/logic_mod_plugin_v1.cpp:70](../../src/mod/logic_mod_plugin_v1.cpp) |
| `LogicModWorkerTooLarge` | [src/mod/logic_mod_plugin_v1.cpp:63](../../src/mod/logic_mod_plugin_v1.cpp) |
| `LogicModWorkerWriteFailed` | [src/mod/logic_mod_registry_v1.cpp:374](../../src/mod/logic_mod_registry_v1.cpp) |
| `LogicOutputAuthorizationChanged` | [src/mod/logic_mod_worker_v1.cpp:105](../../src/mod/logic_mod_worker_v1.cpp) |
| `LogicPackageExtractionFailed` | [apps/logic_mod_plugin_v1_cli.hpp:30](../../apps/logic_mod_plugin_v1_cli.hpp) |
| `LogicPayloadAuthorizationChanged` | [src/mod/logic_mod_worker_v1.cpp:92](../../src/mod/logic_mod_worker_v1.cpp) |
| `LogicRecordInvalid` | [src/mod/logic_mod_worker_v1.cpp:34](../../src/mod/logic_mod_worker_v1.cpp) |
| `LogicRecordTooLarge` | [src/mod/logic_mod_worker_v1.cpp:248](../../src/mod/logic_mod_worker_v1.cpp) |
| `LogicReplayMismatch` | [src/mod/logic_mod_worker_v1.cpp:272](../../src/mod/logic_mod_worker_v1.cpp) |
| `LogicResponseIdentityMismatch` | [src/mod/logic_mod_worker_v1.cpp:127](../../src/mod/logic_mod_worker_v1.cpp) |
| `LogicResultDigestInvalid` | [src/mod/logic_mod_worker_v1.cpp:130](../../src/mod/logic_mod_worker_v1.cpp) |
| `LogicSessionMustInstallFirst` | [apps/logic_mod_plugin_v1_cli.hpp:87](../../apps/logic_mod_plugin_v1_cli.hpp) |
| `LogicSupervisionUnavailable` | [src/mod/logic_mod_worker_attempt_v6.cpp:324](../../src/mod/logic_mod_worker_attempt_v6.cpp) |
| `LogicWorkerRequiresStaticELF` | [src/mod/logic_mod_artifact_chain_v1.cpp:85](../../src/mod/logic_mod_artifact_chain_v1.cpp) |
| `LogicWorkerSetupFailed` | [src/mod/logic_mod_worker_attempt_v6.cpp:47](../../src/mod/logic_mod_worker_attempt_v6.cpp) |
| `LogicWorkerSpawnFailed` | [src/mod/logic_mod_worker_attempt_v6.cpp:282](../../src/mod/logic_mod_worker_attempt_v6.cpp) |
| `ManagementCheckpointBudgetExceeded: checkpoints=` | [include/bruce/application/management_inspection_control.hpp:23](../../include/bruce/application/management_inspection_control.hpp) |
| `ManagementCheckpointCounterOverflow` | [include/bruce/application/management_inspection_control.hpp:27](../../include/bruce/application/management_inspection_control.hpp) |
| `ManagementInspectionCancelled` | [include/bruce/application/management_inspection_control.hpp:21](../../include/bruce/application/management_inspection_control.hpp) |
| `MaxDegreeRequiresPolynomialStrategy` | [apps/bruce_cli.cpp:1220](../../apps/bruce_cli.cpp) |
| `MaxNodesRequiresExpressionStrategy` | [apps/bruce_cli.cpp:1218](../../apps/bruce_cli.cpp) |
| `MaxPowerRequiresPowerStrategy` | [apps/bruce_cli.cpp:1219](../../apps/bruce_cli.cpp) |
| `Missing NanoCell length separator` | [mods/nanocell/src/nanocell.cpp:246](../../mods/nanocell/src/nanocell.cpp) |
| `MissingCandidateHeader` | [src/compatibility/snapshot.cpp:98](../../src/compatibility/snapshot.cpp) |
| `MissingDagSchedulerDemoCancellationBinding` | [apps/dag_scheduler_v1_cli.hpp:32](../../apps/dag_scheduler_v1_cli.hpp) |
| `MissingDagSchedulerNode` | [mods/mcp/src/mcp_inspector.cpp:277](../../mods/mcp/src/mcp_inspector.cpp) |
| `MissingDagSchedulerV2DemoNode` | [apps/dag_scheduler_v2_cli.hpp:108](../../apps/dag_scheduler_v2_cli.hpp) |
| `MissingDagSchedulerV3Node` | [apps/dag_scheduler_v3_cli.hpp:318](../../apps/dag_scheduler_v3_cli.hpp) |
| `MissingDagSchedulerV4Node` | [apps/dag_scheduler_v4_cli.hpp:24](../../apps/dag_scheduler_v4_cli.hpp) |
| `MissingExperienceAction` | [apps/bruce_cli.cpp:1326](../../apps/bruce_cli.cpp) |
| `MissingExperienceSnapshot` | [src/application/grid_research.cpp:131](../../src/application/grid_research.cpp) |
| `MissingGridExecutionBoundary` | [src/application/grid_batch.cpp:167](../../src/application/grid_batch.cpp) |
| `MissingLanguageAlias` | [mods/language/src/language.cpp:157](../../mods/language/src/language.cpp) |
| `MissingLogicModOption: ` | [apps/logic_mod_plugin_v1_cli.hpp:59](../../apps/logic_mod_plugin_v1_cli.hpp) |
| `MissingOption: ` | [apps/bruce_cli.cpp:124](../../apps/bruce_cli.cpp) |
| `MissingOptionValue` | [apps/runtime_mod_cli.hpp:309](../../apps/runtime_mod_cli.hpp) |
| `MissingOptionValue: ` | [apps/bruce_cli.cpp:98](../../apps/bruce_cli.cpp) |
| `MissingPhaseDagReceipt` | [mods/mcp/src/mcp_inspector.cpp:190](../../mods/mcp/src/mcp_inspector.cpp) |
| `MissingPhaseDagSpecGlobals` | [apps/phase_dag_v2_cli.hpp:133](../../apps/phase_dag_v2_cli.hpp) |
| `MissingPhaseDagSpecKey: ` | [apps/phase_dag_v2_cli.hpp:37](../../apps/phase_dag_v2_cli.hpp) |
| `MissingPortfolioNormalForm` | [src/application/portfolio_normalization.cpp:56](../../src/application/portfolio_normalization.cpp) |
| `MissingResult` | [src/application/exact_replay.cpp:339](../../src/application/exact_replay.cpp) |
| `MissingRouteReport` | [apps/runtime_mod_cli.hpp:453](../../apps/runtime_mod_cli.hpp) |
| `MissingRouteTemplate` | [src/tree/registry.cpp:56](../../src/tree/registry.cpp) |
| `MissingRuntimeModAction` | [apps/runtime_mod_cli.hpp:364](../../apps/runtime_mod_cli.hpp) |
| `MissingRuntimeModIdentity` | [apps/runtime_mod_cli.hpp:405](../../apps/runtime_mod_cli.hpp) |
| `MissingRuntimeModPackage` | [apps/runtime_mod_cli.hpp:368](../../apps/runtime_mod_cli.hpp) |
| `MissingRuntimeTrustAction` | [apps/runtime_mod_cli.hpp:294](../../apps/runtime_mod_cli.hpp) |
| `MissingRuntimeTrustStore` | [apps/runtime_mod_cli.hpp:303](../../apps/runtime_mod_cli.hpp) |
| `MissingSampleHeader` | [src/application/exact_research.cpp:203](../../src/application/exact_research.cpp) |
| `MissingSnapshotId` | [src/compatibility/snapshot.cpp:56](../../src/compatibility/snapshot.cpp) |
| `MissingTaskSpecAsset` | [src/token/task_spec.cpp:112](../../src/token/task_spec.cpp) |
| `MissingTaskSpecRoles` | [src/token/task_spec.cpp:160](../../src/token/task_spec.cpp) |
| `MixedCoordinateContext` | [src/compatibility/snapshot.cpp:30](../../src/compatibility/snapshot.cpp) |
| `ModManifestMismatch` | [src/mod/mod_api.cpp:114](../../src/mod/mod_api.cpp) |
| `ModNotFound` | [apps/runtime_mod_cli.hpp:128](../../apps/runtime_mod_cli.hpp) |
| `ModPackageFileRejected` | [src/mod/package_inspection_v2.cpp:41](../../src/mod/package_inspection_v2.cpp) |
| `ModRunByteLimit` | [src/mod/mod_api.cpp:87](../../src/mod/mod_api.cpp) |
| `ModRunContentDigestMismatch` | [src/mod/mod_api.cpp:83](../../src/mod/mod_api.cpp) |
| `ModRunDigestMismatch` | [src/mod/mod_api.cpp:144](../../src/mod/mod_api.cpp) |
| `ModTrustStoreFileRejected` | [src/mod/package_inspection_v2.cpp:42](../../src/mod/package_inspection_v2.cpp) |
| `ModpowRequiresBigint` | [src/application/exact_research.cpp:236](../../src/application/exact_research.cpp) |
| `ModulusRequiresModpow` | [src/application/exact_research.cpp:226](../../src/application/exact_research.cpp) |
| `ModulusRequiresModularMode` | [apps/bruce_cli.cpp:859](../../apps/bruce_cli.cpp) |
| `NanoCell NOT operand must be zero` | [mods/nanocell/src/nanocell.cpp:30](../../mods/nanocell/src/nanocell.cpp) |
| `NanoCell ROTL width must be 0..7` | [mods/nanocell/src/nanocell.cpp:33](../../mods/nanocell/src/nanocell.cpp) |
| `NanoCell SHL width must be 0..7` | [mods/nanocell/src/nanocell.cpp:37](../../mods/nanocell/src/nanocell.cpp) |
| `NanoCell SHR width must be 0..7` | [mods/nanocell/src/nanocell.cpp:40](../../mods/nanocell/src/nanocell.cpp) |
| `NanoCell bundle digest mismatch` | [mods/nanocell/src/nanocell.cpp:423](../../mods/nanocell/src/nanocell.cpp) |
| `NanoCell context too large` | [mods/nanocell/src/nanocell.cpp:145](../../mods/nanocell/src/nanocell.cpp) |
| `NanoCell direct V1 run exceeds byte limit` | [apps/nanocell_cli.hpp:125](../../apps/nanocell_cli.hpp) |
| `NanoCell number exceeds limit` | [mods/nanocell/src/nanocell.cpp:235](../../mods/nanocell/src/nanocell.cpp) |
| `NanoCell replay result mismatch` | [mods/nanocell/src/nanocell.cpp:435](../../mods/nanocell/src/nanocell.cpp) |
| `NanoCell request digest mismatch` | [mods/nanocell/src/nanocell.cpp:427](../../mods/nanocell/src/nanocell.cpp) |
| `NanoCell shift/rotation width must be 0..7` | [mods/nanocell/src/nanocell.cpp:135](../../mods/nanocell/src/nanocell.cpp) |
| `NanoCellCancelled` | [mods/nanocell/src/nanocell.cpp:192](../../mods/nanocell/src/nanocell.cpp) |
| `NanoCellModRequestRunMismatch` | [mods/nanocell/src/mod.cpp:58](../../mods/nanocell/src/mod.cpp) |
| `NanoCellPhaseCodecTooLarge` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:32](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `NanoCellPhaseFieldMissing` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:36](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `NanoCellPhaseFieldTruncated` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:51](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `NanoCellPhaseTrailingBytes` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:69](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `NoResearchCandidatesToRank` | [src/application/compatibility.cpp:125](../../src/application/compatibility.cpp) |
| `NoSignedAffineRelationCollision` | [src/signed_affine_relation/signed_affine_relation_v1.cpp:138](../../src/signed_affine_relation/signed_affine_relation_v1.cpp) |
| `NoToyRelationCollision` | [src/toy_relation/toy_relation_v1.cpp:79](../../src/toy_relation/toy_relation_v1.cpp) |
| `NonExactPolynomialDivision` | [src/math/expression_normal_form.cpp:53](../../src/math/expression_normal_form.cpp) |
| `NonIntegralDivision: quotient=` | [src/math/numbers.cpp:58](../../src/math/numbers.cpp) |
| `NonIntegralValue` | [src/math/numbers.cpp:37](../../src/math/numbers.cpp) |
| `NonInvertibleToyRelationCoefficient` | [src/toy_relation/toy_relation_v1.cpp:63](../../src/toy_relation/toy_relation_v1.cpp) |
| `Noncanonical NanoCell bundle` | [mods/nanocell/src/nanocell.cpp:436](../../mods/nanocell/src/nanocell.cpp) |
| `Noncanonical NanoCell number` | [mods/nanocell/src/nanocell.cpp:230](../../mods/nanocell/src/nanocell.cpp) |
| `Noncanonical NanoCell request` | [mods/nanocell/src/nanocell.cpp:320](../../mods/nanocell/src/nanocell.cpp) |
| `NoncanonicalEvidenceGateMetadata` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:251](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `NoncanonicalExperienceSnapshot` | [src/experience/store.cpp:125](../../src/experience/store.cpp) |
| `NoncanonicalExpressionOperators` | [src/application/exact_replay.cpp:229](../../src/application/exact_replay.cpp) |
| `NoncanonicalLanguageDraft` | [mods/language/src/language.cpp:82](../../mods/language/src/language.cpp) |
| `NoncanonicalLanguageRun` | [mods/language/src/language_run.cpp:128](../../mods/language/src/language_run.cpp) |
| `NoncanonicalModCapabilities` | [src/mod/mod_api.cpp:105](../../src/mod/mod_api.cpp) |
| `NoncanonicalModCatalog` | [src/mod/catalog.cpp:20](../../src/mod/catalog.cpp) |
| `NoncanonicalModRun` | [src/mod/mod_api.cpp:61](../../src/mod/mod_api.cpp) |
| `NoncanonicalNanoCellModRun` | [mods/nanocell/src/mod.cpp:61](../../mods/nanocell/src/mod.cpp) |
| `NoncanonicalNanoCellPhaseMetadata` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:327](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `NoncanonicalNanoCellPhasePayload` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:210](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `NoncanonicalNanoCellPhaseRequest` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:281](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `NoncanonicalPayloadGateMetadata` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:240](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `NoncanonicalPayloadGatePayload` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:263](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `NoncanonicalPayloadGateRequest` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:222](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `NoncanonicalPhaseDagBundle` | [src/phase_dag/phase_dag.cpp:541](../../src/phase_dag/phase_dag.cpp) |
| `NoncanonicalPhaseDagDependencies` | [src/phase_dag/phase_dag.cpp:398](../../src/phase_dag/phase_dag.cpp) |
| `NoncanonicalPhaseDagNodes` | [src/phase_dag/phase_dag.cpp:390](../../src/phase_dag/phase_dag.cpp) |
| `NoncanonicalRuntimeModJson` | [src/mod/runtime_package.cpp:140](../../src/mod/runtime_package.cpp) |
| `NoncanonicalTaskSpec` | [src/token/task_spec.cpp:260](../../src/token/task_spec.cpp) |
| `NoncanonicalTaskSpecNumber` | [src/token/task_spec.cpp:29](../../src/token/task_spec.cpp) |
| `NoncanonicalTaskSpecV1RoleAssetBinding` | [src/token/task_spec.cpp:110](../../src/token/task_spec.cpp) |
| `NoncanonicalTaskSpecV1RoleBinding` | [src/token/task_spec.cpp:106](../../src/token/task_spec.cpp) |
| `NoncanonicalTaskSpecV1Table` | [src/token/task_spec.cpp:87](../../src/token/task_spec.cpp) |
| `NoncanonicalTaskSpecV1TokenOrder` | [src/token/task_spec.cpp:92](../../src/token/task_spec.cpp) |
| `NoncanonicalTokenizedTaskRun` | [src/application/tokenized_task_run.cpp:152](../../src/application/tokenized_task_run.cpp) |
| `OperationBudgetExceeded` | [src/application/exact_research.cpp:73](../../src/application/exact_research.cpp) |
| `OperatorsRequireExpressionStrategy` | [apps/bruce_cli.cpp:1215](../../apps/bruce_cli.cpp) |
| `PROTOCOL_ERROR` | [apps/artifact_store_v2_cli.hpp:40](../../apps/artifact_store_v2_cli.hpp) |
| `PayloadFlowV2Invalid` | [src/artifact/payload_flow_v2.cpp:15](../../src/artifact/payload_flow_v2.cpp) |
| `PhaseDagBundleDigestMismatch` | [src/phase_dag/phase_dag.cpp:538](../../src/phase_dag/phase_dag.cpp) |
| `PhaseDagBundleTooLarge` | [src/phase_dag/phase_dag_v2.cpp:504](../../src/phase_dag/phase_dag_v2.cpp) |
| `PhaseDagBundleTrailingBytes` | [src/phase_dag/phase_dag.cpp:367](../../src/phase_dag/phase_dag.cpp) |
| `PhaseDagBundleTruncated` | [src/phase_dag/phase_dag.cpp:229](../../src/phase_dag/phase_dag.cpp) |
| `PhaseDagCycle` | [src/phase_dag/phase_dag.cpp:428](../../src/phase_dag/phase_dag.cpp) |
| `PhaseDagEdgeLimit` | [src/phase_dag/phase_dag.cpp:407](../../src/phase_dag/phase_dag.cpp) |
| `PhaseDagReplayMismatch` | [src/phase_dag/phase_dag.cpp:543](../../src/phase_dag/phase_dag.cpp) |
| `PhaseDagRunNotReplayable` | [src/phase_dag/phase_dag.cpp:524](../../src/phase_dag/phase_dag.cpp) |
| `PhaseDagSchedulingStalled` | [src/phase_dag/phase_dag.cpp:468](../../src/phase_dag/phase_dag.cpp) |
| `PhasePayloadCodecTooLarge` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:26](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `PhasePayloadFieldMissing` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:30](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `PhasePayloadFieldTruncated` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:33](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `PhasePayloadTrailingBytes` | [mods/nanocell/src/phase_dag_adapter_v3.cpp:40](../../mods/nanocell/src/phase_dag_adapter_v3.cpp) |
| `PolynomialInputByteLimit` | [src/application/fraction_compare.cpp:33](../../src/application/fraction_compare.cpp) |
| `PolynomialRequiresInterpolationPlanner` | [src/application/research_contract.cpp:42](../../src/application/research_contract.cpp) |
| `PortfolioNormalizationMismatch` | [src/application/portfolio_normalization.cpp:53](../../src/application/portfolio_normalization.cpp) |
| `PortfolioRequiresPortfolioPlanner` | [src/application/research_contract.cpp:41](../../src/application/research_contract.cpp) |
| `ProjectionRequiresResearchOrigin` | [src/application/compatibility.cpp:130](../../src/application/compatibility.cpp) |
| `QuadraticParameterSlotLimit` | [src/application/research_contract.cpp:90](../../src/application/research_contract.cpp) |
| `QueryBudgetExceeded` | [src/application/grid_batch.cpp:83](../../src/application/grid_batch.cpp) |
| `RESULT_DECISION_REQUIRED` | [src/application/common_runtime_host_v2_batch.cpp:349](../../src/application/common_runtime_host_v2_batch.cpp) |
| `RESULT_NOT_ACCEPTABLE` | [src/application/common_runtime_host_v2_batch.cpp:361](../../src/application/common_runtime_host_v2_batch.cpp) |
| `RESULT_NOT_REJECTABLE` | [src/application/common_runtime_host_v2_batch.cpp:384](../../src/application/common_runtime_host_v2_batch.cpp) |
| `RESULT_NOT_RETRYABLE` | [src/application/common_runtime_host_v2_batch.cpp:416](../../src/application/common_runtime_host_v2_batch.cpp) |
| `RationalFunctionPole` | [include/bruce/math/rational_linear.hpp:7](../../include/bruce/math/rational_linear.hpp) |
| `RationalLinearRequiresSolvePlanner` | [src/application/research_contract.cpp:40](../../src/application/research_contract.cpp) |
| `RationalPortfolioRequiresRationalDomain` | [src/application/research_contract.cpp:55](../../src/application/research_contract.cpp) |
| `ReleaseCryptoFailure` | [src/release/release_signature_v1.cpp:155](../../src/release/release_signature_v1.cpp) |
| `ReleaseKeyUntrusted` | [src/release/release_signature_v1.cpp:18](../../src/release/release_signature_v1.cpp) |
| `ReleaseSignatureInvalid` | [src/release/release_signature_v1.cpp:185](../../src/release/release_signature_v1.cpp) |
| `ReplayBudgetRequiresVerification` | [apps/bruce_cli.cpp:454](../../apps/bruce_cli.cpp) |
| `ReplayByteBudgetExceeded: bytes=` | [src/application/execution_session.cpp:77](../../src/application/execution_session.cpp) |
| `ReplayMismatch: compatibility trace or result differs` | [src/application/compatibility.cpp:257](../../src/application/compatibility.cpp) |
| `ReplayMismatch: evidence chain differs` | [src/evidence/chain.cpp:154](../../src/evidence/chain.cpp) |
| `ReplayMismatch: exact input, result or trace differs` | [src/application/exact_replay.cpp:348](../../src/application/exact_replay.cpp) |
| `ReplayMismatch: grid batch, cache, result or trace differs` | [src/application/grid_batch.cpp:189](../../src/application/grid_batch.cpp) |
| `ReplayMismatch: grid continuation differs` | [src/application/grid_resume.cpp:99](../../src/application/grid_resume.cpp) |
| `ReplayMismatch: grid continuation stage differs from parent` | [src/application/grid_resume.cpp:97](../../src/application/grid_resume.cpp) |
| `ReplayMismatch: grid research origin or result differs` | [src/application/grid_research.cpp:221](../../src/application/grid_research.cpp) |
| `ReplayMismatch: grid stage sweep task, records or diagnostics differ` | [src/application/grid_stage_sweep.cpp:118](../../src/application/grid_stage_sweep.cpp) |
| `ReplayMismatch: grid stage verification differs` | [src/application/grid_stage_verify.cpp:102](../../src/application/grid_stage_verify.cpp) |
| `ReplayMismatch: grid task, result or trace differs` | [src/application/grid.cpp:124](../../src/application/grid.cpp) |
| `ReplayMismatch: grid work budget differs` | [src/application/grid_research.cpp:193](../../src/application/grid_research.cpp) |
| `ReplayMismatch: input, result or logical trace differs` | [src/application/fraction_compare.cpp:192](../../src/application/fraction_compare.cpp) |
| `ReplayMismatch: invalid embedded sweep` | [src/application/grid_stage_verify.cpp:92](../../src/application/grid_stage_verify.cpp) |
| `ReplayMismatch: truncated embedded sweep` | [src/application/grid_stage_verify.cpp:94](../../src/application/grid_stage_verify.cpp) |
| `ReplayOperationLimit` | [src/application/exact_replay.cpp:340](../../src/application/exact_replay.cpp) |
| `ResearchProjectionMismatch` | [src/application/compatibility.cpp:127](../../src/application/compatibility.cpp) |
| `ResearchReplayCheckpointsRequireCallback` | [src/application/execution_session.cpp:43](../../src/application/execution_session.cpp) |
| `ResearchReplayCheckpointsRequireResearchTask` | [src/application/execution_session.cpp:45](../../src/application/execution_session.cpp) |
| `ResultBudgetExceeded` | [src/application/exact_research.cpp:129](../../src/application/exact_research.cpp) |
| `RouteInputTooLarge` | [src/mod/installed_mod_host_v2.cpp:395](../../src/mod/installed_mod_host_v2.cpp) |
| `RouteOwnerNotFound` | [apps/runtime_mod_cli.hpp:171](../../apps/runtime_mod_cli.hpp) |
| `RouteRegistryDigestMismatch` | [apps/runtime_mod_cli.hpp:269](../../apps/runtime_mod_cli.hpp) |
| `RouteReportMismatch` | [apps/runtime_mod_cli.hpp:271](../../apps/runtime_mod_cli.hpp) |
| `RouteReportTooLarge` | [apps/runtime_mod_cli.hpp:144](../../apps/runtime_mod_cli.hpp) |
| `RuleApprovalDeclined` | [apps/rule_review.hpp:32](../../apps/rule_review.hpp) |
| `RuleApprovalRequiresTerminal` | [apps/rule_review.hpp:24](../../apps/rule_review.hpp) |
| `RuleBaseChanged` | [src/application/rule_proposal.cpp:30](../../src/application/rule_proposal.cpp) |
| `RuleBudgetExceeded` | [src/compatibility/snapshot.cpp:78](../../src/compatibility/snapshot.cpp) |
| `RuleHistoryByteLimit` | [src/application/rule_history.cpp:10](../../src/application/rule_history.cpp) |
| `RuleHistoryDecisionLimit` | [src/application/rule_history.cpp:52](../../src/application/rule_history.cpp) |
| `RuleHistoryDigestMismatch` | [src/application/rule_history.cpp:12](../../src/application/rule_history.cpp) |
| `RuleProposalDigestMismatch` | [src/application/rule_proposal.cpp:12](../../src/application/rule_proposal.cpp) |
| `RuleProposalUnchanged` | [src/application/rule_proposal.cpp:23](../../src/application/rule_proposal.cpp) |
| `RuleStoreBaseChanged` | [src/application/rule_history.cpp:50](../../src/application/rule_history.cpp) |
| `RuleStoreBusy` | [src/io/local_files.cpp:139](../../src/io/local_files.cpp) |
| `RuleStoreCommitDurabilityUnknown` | [src/io/local_files.cpp:38](../../src/io/local_files.cpp) |
| `RuleStoreFileOpenFailed` | [src/io/local_files.cpp:18](../../src/io/local_files.cpp) |
| `RuleStoreInvalidClock` | [src/io/local_files.cpp:142](../../src/io/local_files.cpp) |
| `RuleStoreReplaceFailed` | [src/io/local_files.cpp:37](../../src/io/local_files.cpp) |
| `RuleStoreSyncFailed` | [src/io/local_files.cpp:36](../../src/io/local_files.cpp) |
| `RuleStoreTargetMismatch` | [src/application/rule_history.cpp:36](../../src/application/rule_history.cpp) |
| `RuleStoreWriteFailed` | [src/io/local_files.cpp:33](../../src/io/local_files.cpp) |
| `RuntimeModAlreadyInstalled` | [src/mod/runtime_registry.cpp:328](../../src/mod/runtime_registry.cpp) |
| `RuntimeModCommitDurabilityUnknown` | [src/mod/runtime_registry.cpp:145](../../src/mod/runtime_registry.cpp) |
| `RuntimeModCryptoFailure` | [src/mod/runtime_package.cpp:329](../../src/mod/runtime_package.cpp) |
| `RuntimeModFileOpenFailed` | [src/mod/installed_mod_host_v2.cpp:25](../../src/mod/installed_mod_host_v2.cpp) |
| `RuntimeModFileReadFailed` | [src/mod/installed_mod_host_v2.cpp:37](../../src/mod/installed_mod_host_v2.cpp) |
| `RuntimeModFileStatFailed` | [src/mod/runtime_registry.cpp:42](../../src/mod/runtime_registry.cpp) |
| `RuntimeModGenerationLimit` | [src/mod/runtime_registry.cpp:252](../../src/mod/runtime_registry.cpp) |
| `RuntimeModGenerationOverflow` | [src/mod/installed_mod_host_v2.cpp:514](../../src/mod/installed_mod_host_v2.cpp) |
| `RuntimeModGoldenCaseFailed` | [src/mod/runtime_registry.cpp:224](../../src/mod/runtime_registry.cpp) |
| `RuntimeModInstalledPackageMismatch` | [src/mod/installed_mod_host_v2.cpp:48](../../src/mod/installed_mod_host_v2.cpp) |
| `RuntimeModJournalLimit` | [src/mod/runtime_registry.cpp:200](../../src/mod/runtime_registry.cpp) |
| `RuntimeModJournalMissing` | [src/mod/runtime_registry.cpp:303](../../src/mod/runtime_registry.cpp) |
| `RuntimeModJournalRecoveryFailed` | [src/mod/runtime_registry.cpp:181](../../src/mod/runtime_registry.cpp) |
| `RuntimeModJournalWriteFailed` | [src/mod/runtime_registry.cpp:205](../../src/mod/runtime_registry.cpp) |
| `RuntimeModNotActive` | [src/mod/runtime_registry.cpp:405](../../src/mod/runtime_registry.cpp) |
| `RuntimeModNotInstalled` | [src/mod/runtime_registry.cpp:390](../../src/mod/runtime_registry.cpp) |
| `RuntimeModPackageTooLarge` | [src/mod/package_inspection_v2.cpp:132](../../src/mod/package_inspection_v2.cpp) |
| `RuntimeModPayloadDigestMismatch` | [src/mod/runtime_package.cpp:297](../../src/mod/runtime_package.cpp) |
| `RuntimeModRecordLimit` | [src/mod/runtime_registry.cpp:125](../../src/mod/runtime_registry.cpp) |
| `RuntimeModRecoveryRequired` | [src/mod/runtime_registry.cpp:179](../../src/mod/runtime_registry.cpp) |
| `RuntimeModRegistryJournalMismatch` | [src/mod/runtime_registry.cpp:185](../../src/mod/runtime_registry.cpp) |
| `RuntimeModRevoked` | [src/mod/runtime_registry.cpp:391](../../src/mod/runtime_registry.cpp) |
| `RuntimeModRootCreateFailed` | [src/mod/runtime_mod_store_internal.cpp:31](../../src/mod/runtime_mod_store_internal.cpp) |
| `RuntimeModRootRequired` | [src/mod/installed_mod_host_v2.cpp:207](../../src/mod/installed_mod_host_v2.cpp) |
| `RuntimeModSignatureInvalid` | [src/mod/runtime_package.cpp:330](../../src/mod/runtime_package.cpp) |
| `RuntimeModSigningKeyUntrusted` | [src/mod/runtime_package.cpp:324](../../src/mod/runtime_package.cpp) |
| `RuntimeModStoreByteLimit` | [src/mod/installed_mod_host_v2.cpp:32](../../src/mod/installed_mod_host_v2.cpp) |
| `RuntimeModStoreLockFailed` | [src/mod/runtime_mod_store_internal.cpp:77](../../src/mod/runtime_mod_store_internal.cpp) |
| `RuntimeModTestFailpoint` | [src/mod/runtime_mod_store_internal.cpp:19](../../src/mod/runtime_mod_store_internal.cpp) |
| `RuntimeModTrustStoreChanged` | [src/mod/installed_mod_host_v2.cpp:222](../../src/mod/installed_mod_host_v2.cpp) |
| `RuntimeModTrustStoreMissing` | [src/mod/runtime_registry.cpp:298](../../src/mod/runtime_registry.cpp) |
| `RuntimeOrchestratorInputUnavailable` | [apps/runtime_orchestrator_v1_cli.hpp:199](../../apps/runtime_orchestrator_v1_cli.hpp) |
| `RuntimeOrchestratorProtocolError` | [apps/runtime_orchestrator_v1_cli.hpp:21](../../apps/runtime_orchestrator_v1_cli.hpp) |
| `RuntimeOrchestratorQueueFull` | [src/runtime/runtime_orchestrator_v1.cpp:91](../../src/runtime/runtime_orchestrator_v1.cpp) |
| `RuntimeOrchestratorReservationPlanMismatch` | [src/runtime/runtime_orchestrator_v1.cpp:83](../../src/runtime/runtime_orchestrator_v1.cpp) |
| `RuntimeOrchestratorRunNotFound` | [src/runtime/runtime_orchestrator_v1.cpp:180](../../src/runtime/runtime_orchestrator_v1.cpp) |
| `RuntimeOrchestratorSequenceOverflow` | [src/runtime/runtime_orchestrator_v1.cpp:92](../../src/runtime/runtime_orchestrator_v1.cpp) |
| `RuntimeTrustActiveModsIncompatible` | [include/bruce/mod/installed_mod_host_v2.hpp:27](../../include/bruce/mod/installed_mod_host_v2.hpp) |
| `RuntimeTrustAlreadyImported` | [src/mod/runtime_trust_store_v2.cpp:1020](../../src/mod/runtime_trust_store_v2.cpp) |
| `RuntimeTrustClockFailed` | [apps/runtime_mod_cli.hpp:27](../../apps/runtime_mod_cli.hpp) |
| `RuntimeTrustCoordinatorMissing` | [src/mod/logic_mod_registry_v1.cpp:222](../../src/mod/logic_mod_registry_v1.cpp) |
| `RuntimeTrustCoordinatorRequired` | [src/mod/runtime_registry.cpp:419](../../src/mod/runtime_registry.cpp) |
| `RuntimeTrustDurabilityUnknown` | [src/mod/runtime_trust_store_v2.cpp:317](../../src/mod/runtime_trust_store_v2.cpp) |
| `RuntimeTrustFileOpenFailed` | [src/mod/runtime_trust_store_v2.cpp:291](../../src/mod/runtime_trust_store_v2.cpp) |
| `RuntimeTrustFileReadFailed` | [src/mod/runtime_trust_store_v2.cpp:308](../../src/mod/runtime_trust_store_v2.cpp) |
| `RuntimeTrustFileStatFailed` | [src/mod/runtime_trust_store_v2.cpp:297](../../src/mod/runtime_trust_store_v2.cpp) |
| `RuntimeTrustGenerationCollision` | [src/mod/runtime_trust_store_v2.cpp:577](../../src/mod/runtime_trust_store_v2.cpp) |
| `RuntimeTrustGenerationLimit` | [src/mod/runtime_trust_store_v2.cpp:1069](../../src/mod/runtime_trust_store_v2.cpp) |
| `RuntimeTrustInstalledPackageIntegrity` | [src/mod/installed_mod_host_v2.cpp:503](../../src/mod/installed_mod_host_v2.cpp) |
| `RuntimeTrustJournalCapacity` | [src/mod/runtime_trust_store_v2.cpp:501](../../src/mod/runtime_trust_store_v2.cpp) |
| `RuntimeTrustJournalWriteFailed` | [src/mod/runtime_trust_store_v2.cpp:332](../../src/mod/runtime_trust_store_v2.cpp) |
| `RuntimeTrustNotImported` | [src/mod/runtime_trust_store_v2.cpp:856](../../src/mod/runtime_trust_store_v2.cpp) |
| `RuntimeTrustOperationFailed` | [apps/runtime_mod_cli.hpp:353](../../apps/runtime_mod_cli.hpp) |
| `RuntimeTrustRecoveryFailed` | [src/mod/runtime_trust_store_v2.cpp:554](../../src/mod/runtime_trust_store_v2.cpp) |
| `RuntimeTrustRecoveryRequired` | [src/mod/runtime_trust_store_v2.cpp:552](../../src/mod/runtime_trust_store_v2.cpp) |
| `RuntimeTrustReportTooLarge` | [apps/runtime_mod_cli.hpp:345](../../apps/runtime_mod_cli.hpp) |
| `RuntimeTrustTransitionMissing` | [src/mod/runtime_trust_store_v2.cpp:884](../../src/mod/runtime_trust_store_v2.cpp) |
| `SHA-256 requires Web Crypto in a secure browser context.` | [mods/cockpit/src/cockpit_assets.cpp:243](../../mods/cockpit/src/cockpit_assets.cpp) |
| `SampleBudgetExceeded` | [src/application/exact_replay.cpp:152](../../src/application/exact_replay.cpp) |
| `SampleByteLimit` | [src/application/research_contract.cpp:86](../../src/application/research_contract.cpp) |
| `SerializationBudgetRequiresVerification` | [apps/bruce_cli.cpp:458](../../apps/bruce_cli.cpp) |
| `SerializationByteBudgetExceeded: bytes=` | [src/application/execution_session.cpp:73](../../src/application/execution_session.cpp) |
| `SerializationCancellationRequiresReplay` | [apps/bruce_cli.cpp:438](../../apps/bruce_cli.cpp) |
| `SerializationCheckpointsRequireCallback` | [src/application/execution_session.cpp:54](../../src/application/execution_session.cpp) |
| `SerializationRecordsRequireBatchOrResume` | [src/application/application_task.cpp:108](../../src/application/application_task.cpp) |
| `SerializationRecordsRequireBatchResumeOrResearch` | [src/application/execution_session.cpp:53](../../src/application/execution_session.cpp) |
| `SessionBudgetNotEnabled` | [src/application/session_budget.cpp:18](../../src/application/session_budget.cpp) |
| `SessionBudgetRequiresVerification` | [apps/bruce_cli.cpp:463](../../apps/bruce_cli.cpp) |
| `SessionCheckpointBudgetExceeded: phase=` | [src/application/session_budget.cpp:13](../../src/application/session_budget.cpp) |
| `SessionCheckpointOutsideWorkPhase` | [src/application/session_budget.cpp:10](../../src/application/session_budget.cpp) |
| `SignedAffineDagArithmeticOverflow: ` | [src/signed_affine_dag/signed_affine_dag_v1.cpp:24](../../src/signed_affine_dag/signed_affine_dag_v1.cpp) |
| `SignedAffineDagCycle` | [src/signed_affine_dag/signed_affine_dag_v1.cpp:106](../../src/signed_affine_dag/signed_affine_dag_v1.cpp) |
| `SignedAffineRelationCoefficientOverflow: ` | [src/signed_affine_relation/signed_affine_relation_v1.cpp:17](../../src/signed_affine_relation/signed_affine_relation_v1.cpp) |
| `SnapshotByteLimit` | [src/compatibility/snapshot.cpp:51](../../src/compatibility/snapshot.cpp) |
| `StageRequiresScaledExponentialRule` | [src/application/grid_spec.cpp:7](../../src/application/grid_spec.cpp) |
| `StrategyRequiresExactDomain` | [apps/bruce_cli.cpp:1223](../../apps/bruce_cli.cpp) |
| `StrategyResultConflict` | [src/application/portfolio_normalization.cpp:101](../../src/application/portfolio_normalization.cpp) |
| `TIMEOUT` | [src/application/common_runtime_host_v2_batch.cpp:292](../../src/application/common_runtime_host_v2_batch.cpp) |
| `TaskSpecByteLimit` | [src/token/task_spec.cpp:231](../../src/token/task_spec.cpp) |
| `TaskSpecDigestMismatch` | [src/token/task_spec.cpp:239](../../src/token/task_spec.cpp) |
| `TokenTableDigestMismatch` | [src/token/task_spec.cpp:153](../../src/token/task_spec.cpp) |
| `TokenizedTaskCompiledTaskMismatch` | [src/application/tokenized_task_run.cpp:38](../../src/application/tokenized_task_run.cpp) |
| `TokenizedTaskExplanationMismatch` | [src/application/tokenized_task_run.cpp:73](../../src/application/tokenized_task_run.cpp) |
| `TokenizedTaskNativeByteLimit` | [src/application/tokenized_task_run.cpp:52](../../src/application/tokenized_task_run.cpp) |
| `TokenizedTaskNativeDigestMismatch` | [src/application/tokenized_task_run.cpp:71](../../src/application/tokenized_task_run.cpp) |
| `TokenizedTaskNativeEvidenceMismatch` | [src/application/tokenized_task_run.cpp:78](../../src/application/tokenized_task_run.cpp) |
| `TokenizedTaskRunByteLimit` | [src/application/tokenized_task_run.cpp:130](../../src/application/tokenized_task_run.cpp) |
| `TokenizedTaskRunDigestMismatch` | [src/application/tokenized_task_run.cpp:139](../../src/application/tokenized_task_run.cpp) |
| `TokenizedTaskSpecDigestMismatch` | [src/application/tokenized_task_run.cpp:67](../../src/application/tokenized_task_run.cpp) |
| `TotalBudgetRequiresGridTraining` | [apps/bruce_cli.cpp:1207](../../apps/bruce_cli.cpp) |
| `TraceBudgetExceeded` | [src/application/exact_research.cpp:60](../../src/application/exact_research.cpp) |
| `TraceByteBudgetExceeded` | [src/application/exact_research.cpp:62](../../src/application/exact_research.cpp) |
| `Trailing NanoCell bytes` | [mods/nanocell/src/nanocell.cpp:262](../../mods/nanocell/src/nanocell.cpp) |
| `TrailingExperienceEventData` | [src/experience/store.cpp:91](../../src/experience/store.cpp) |
| `TransferCancelled` | [include/bruce/application/transfer_control.hpp:13](../../include/bruce/application/transfer_control.hpp) |
| `TransferCheckpointBudgetExceeded: checkpoints=` | [include/bruce/application/transfer_control.hpp:14](../../include/bruce/application/transfer_control.hpp) |
| `TransferCheckpointCounterOverflow` | [include/bruce/application/transfer_control.hpp:15](../../include/bruce/application/transfer_control.hpp) |
| `TreeMountNotFound` | [src/mod/installed_mod_host_v2.cpp:142](../../src/mod/installed_mod_host_v2.cpp) |
| `TreeNodeOutsideMount` | [src/tree/registry.cpp:53](../../src/tree/registry.cpp) |
| `TreeParentNotFound` | [src/tree/registry.cpp:74](../../src/tree/registry.cpp) |
| `Truncated NanoCell field` | [mods/nanocell/src/nanocell.cpp:249](../../mods/nanocell/src/nanocell.cpp) |
| `TruncatedBundle` | [src/application/fraction_compare.cpp:68](../../src/application/fraction_compare.cpp) |
| `TruncatedCompatibilityBundle` | [src/application/compatibility.cpp:57](../../src/application/compatibility.cpp) |
| `TruncatedExactBundle` | [src/application/exact_replay.cpp:145](../../src/application/exact_replay.cpp) |
| `TruncatedExactSamples` | [src/application/exact_replay.cpp:157](../../src/application/exact_replay.cpp) |
| `TruncatedExperienceSnapshot` | [src/application/exact_replay.cpp:336](../../src/application/exact_replay.cpp) |
| `TruncatedSamples` | [src/application/replay.cpp:33](../../src/application/replay.cpp) |
| `UnchangedExperienceReplacement` | [src/experience/store.cpp:67](../../src/experience/store.cpp) |
| `Unexpected NanoCell final state` | [mods/nanocell/src/nanocell.cpp:351](../../mods/nanocell/src/nanocell.cpp) |
| `UnexpectedDagSchedulerDemoCallbackStart` | [apps/dag_scheduler_v1_cli.hpp:46](../../apps/dag_scheduler_v1_cli.hpp) |
| `UnexpectedLogicModMember` | [src/mod/runtime_package.cpp:378](../../src/mod/runtime_package.cpp) |
| `UnexpectedRuntimeModMember` | [src/mod/runtime_package.cpp:293](../../src/mod/runtime_package.cpp) |
| `UnexpectedWorkerControlFrame` | [apps/dag_scheduler_v5_cli.hpp:1462](../../apps/dag_scheduler_v5_cli.hpp) |
| `UnexpectedWorkerFrame` | [src/worker/worker_supervisor_v1.cpp:691](../../src/worker/worker_supervisor_v1.cpp) |
| `Unknown NanoCell byte opcode` | [mods/nanocell/src/nanocell.cpp:43](../../mods/nanocell/src/nanocell.cpp) |
| `Unknown NanoCell encoded opcode` | [mods/nanocell/src/nanocell.cpp:311](../../mods/nanocell/src/nanocell.cpp) |
| `Unknown NanoCell reference technique` | [mods/nanocell/src/nanocell.cpp:71](../../mods/nanocell/src/nanocell.cpp) |
| `Unknown NanoCell technique id` | [mods/nanocell/src/nanocell.cpp:160](../../mods/nanocell/src/nanocell.cpp) |
| `UnknownBundleVersion` | [src/application/compatibility.cpp:233](../../src/application/compatibility.cpp) |
| `UnknownCommand: ` | [apps/bruce_cli.cpp:1458](../../apps/bruce_cli.cpp) |
| `UnknownCommonRuntimeCompatibilityV1Family` | [src/application/common_runtime_compatibility_v1.cpp:126](../../src/application/common_runtime_compatibility_v1.cpp) |
| `UnknownCompatibilityVersion` | [src/application/compatibility.cpp:240](../../src/application/compatibility.cpp) |
| `UnknownDagSchedulerCallback: ` | [src/dag_scheduler/dag_scheduler_v1.cpp:54](../../src/dag_scheduler/dag_scheduler_v1.cpp) |
| `UnknownDagSchedulerDependency: ` | [src/dag_scheduler/dag_scheduler_v1.cpp:68](../../src/dag_scheduler/dag_scheduler_v1.cpp) |
| `UnknownDagSchedulerV2Callback: ` | [src/dag_scheduler/dag_scheduler_v2.cpp:61](../../src/dag_scheduler/dag_scheduler_v2.cpp) |
| `UnknownDagSchedulerV2Dependency: ` | [src/dag_scheduler/dag_scheduler_v2.cpp:75](../../src/dag_scheduler/dag_scheduler_v2.cpp) |
| `UnknownDagSchedulerV3SpecKey` | [apps/dag_scheduler_v3_cli.hpp:115](../../apps/dag_scheduler_v3_cli.hpp) |
| `UnknownDialogueAction: ` | [apps/bruce_cli.cpp:1166](../../apps/bruce_cli.cpp) |
| `UnknownDomainCache` | [src/application/research_contract.cpp:60](../../src/application/research_contract.cpp) |
| `UnknownDomainCachePolicy` | [src/application/exact_replay.cpp:253](../../src/application/exact_replay.cpp) |
| `UnknownEquivalenceVersion` | [src/application/exact_replay.cpp:248](../../src/application/exact_replay.cpp) |
| `UnknownEvidenceAction: ` | [apps/bruce_cli.cpp:1202](../../apps/bruce_cli.cpp) |
| `UnknownEvidenceChainPolicy` | [src/evidence/chain.cpp:145](../../src/evidence/chain.cpp) |
| `UnknownExactOperation` | [src/application/exact_research.cpp:244](../../src/application/exact_research.cpp) |
| `UnknownExactVersion` | [src/application/exact_replay.cpp:220](../../src/application/exact_replay.cpp) |
| `UnknownExecutionCancellation` | [apps/bruce_cli.cpp:425](../../apps/bruce_cli.cpp) |
| `UnknownExecutionPhase` | [src/application/execution_session.cpp:7](../../src/application/execution_session.cpp) |
| `UnknownExecutionVerification` | [apps/bruce_cli.cpp:422](../../apps/bruce_cli.cpp) |
| `UnknownExperienceAction` | [apps/bruce_cli.cpp:1416](../../apps/bruce_cli.cpp) |
| `UnknownExperienceEvent` | [src/experience/store.cpp:90](../../src/experience/store.cpp) |
| `UnknownExperiencePolicy` | [src/experience/store.cpp:116](../../src/experience/store.cpp) |
| `UnknownExperienceVersion` | [src/experience/store.cpp:109](../../src/experience/store.cpp) |
| `UnknownExpressionCachePolicy` | [src/application/exact_replay.cpp:230](../../src/application/exact_replay.cpp) |
| `UnknownExpressionEquivalence` | [src/application/research_contract.cpp:66](../../src/application/research_contract.cpp) |
| `UnknownExpressionExponentPolicy` | [src/application/exact_replay.cpp:247](../../src/application/exact_replay.cpp) |
| `UnknownExpressionPlanner` | [src/application/research_contract.cpp:64](../../src/application/research_contract.cpp) |
| `UnknownExpressionPlannerVersion` | [src/application/exact_replay.cpp:226](../../src/application/exact_replay.cpp) |
| `UnknownGateCancellation` | [apps/bruce_cli.cpp:1430](../../apps/bruce_cli.cpp) |
| `UnknownGridExecutionPolicy` | [src/application/grid_batch.cpp:163](../../src/application/grid_batch.cpp) |
| `UnknownGridResearchAxis` | [src/application/grid_research.cpp:27](../../src/application/grid_research.cpp) |
| `UnknownGridResearchVersion` | [src/application/grid_research.cpp:205](../../src/application/grid_research.cpp) |
| `UnknownGridResumePolicy` | [src/application/grid_resume.cpp:76](../../src/application/grid_resume.cpp) |
| `UnknownGridSpecOrPolicy` | [src/application/grid_batch.cpp:156](../../src/application/grid_batch.cpp) |
| `UnknownGridWorkPolicy` | [src/application/grid_research.cpp:180](../../src/application/grid_research.cpp) |
| `UnknownLanguageAction: ` | [apps/bruce_cli.cpp:1127](../../apps/bruce_cli.cpp) |
| `UnknownLanguageAlternative` | [apps/bruce_cli.cpp:185](../../apps/bruce_cli.cpp) |
| `UnknownLogicModPackage` | [src/mod/logic_mod_plugin_v1.cpp:84](../../src/mod/logic_mod_plugin_v1.cpp) |
| `UnknownManagementCancellation` | [apps/management_inspection_cli.hpp:50](../../apps/management_inspection_cli.hpp) |
| `UnknownNanoCellCommand: ` | [apps/nanocell_cli.hpp:133](../../apps/nanocell_cli.hpp) |
| `UnknownNanoCellPhaseTechnique` | [mods/nanocell/src/phase_dag_adapter_v2.cpp:81](../../mods/nanocell/src/phase_dag_adapter_v2.cpp) |
| `UnknownNanoCellTechnique` | [apps/nanocell_cli.hpp:62](../../apps/nanocell_cli.hpp) |
| `UnknownNumberDomain` | [src/math/numbers.cpp:8](../../src/math/numbers.cpp) |
| `UnknownOption: ` | [apps/bruce_cli.cpp:93](../../apps/bruce_cli.cpp) |
| `UnknownOrRevokedExperience` | [src/experience/store.cpp:52](../../src/experience/store.cpp) |
| `UnknownPhaseDagAdapter` | [src/phase_dag/phase_dag.cpp:90](../../src/phase_dag/phase_dag.cpp) |
| `UnknownPhaseDagSpecAdapter` | [apps/phase_dag_v2_cli.hpp:43](../../apps/phase_dag_v2_cli.hpp) |
| `UnknownPhaseDagSpecKey` | [apps/phase_dag_v2_cli.hpp:38](../../apps/phase_dag_v2_cli.hpp) |
| `UnknownPlannerVersion` | [src/application/exact_replay.cpp:223](../../src/application/exact_replay.cpp) |
| `UnknownResearchGateReason` | [src/application/research_gates.cpp:19](../../src/application/research_gates.cpp) |
| `UnknownResearchStrategy` | [src/application/research_contract.cpp:30](../../src/application/research_contract.cpp) |
| `UnknownRuleField` | [src/compatibility/snapshot.cpp:69](../../src/compatibility/snapshot.cpp) |
| `UnknownRuleKind` | [src/compatibility/snapshot.cpp:68](../../src/compatibility/snapshot.cpp) |
| `UnknownRuleOperator` | [src/compatibility/snapshot.cpp:70](../../src/compatibility/snapshot.cpp) |
| `UnknownRuleOrEngineVersion` | [src/application/fraction_compare.cpp:176](../../src/application/fraction_compare.cpp) |
| `UnknownRuleVersion` | [src/compatibility/snapshot.cpp:55](../../src/compatibility/snapshot.cpp) |
| `UnknownRuntimeModAction` | [apps/runtime_mod_cli.hpp:450](../../apps/runtime_mod_cli.hpp) |
| `UnknownRuntimeTrustAction` | [apps/runtime_mod_cli.hpp:298](../../apps/runtime_mod_cli.hpp) |
| `UnknownSearchPlanner` | [src/application/research_contract.cpp:35](../../src/application/research_contract.cpp) |
| `UnknownSerializationCancellation` | [apps/bruce_cli.cpp:437](../../apps/bruce_cli.cpp) |
| `UnknownSignedAffineDagPredecessor: ` | [src/signed_affine_dag/signed_affine_dag_v1.cpp:91](../../src/signed_affine_dag/signed_affine_dag_v1.cpp) |
| `UnknownSignedAffineRelationNode: ` | [src/signed_affine_relation/signed_affine_relation_v1.cpp:81](../../src/signed_affine_relation/signed_affine_relation_v1.cpp) |
| `UnknownTaskSpecAction: ` | [apps/bruce_cli.cpp:1009](../../apps/bruce_cli.cpp) |
| `UnknownTaskSpecAlternative` | [src/token/task_spec.cpp:173](../../src/token/task_spec.cpp) |
| `UnknownTaskSpecToken` | [src/token/task_spec.cpp:103](../../src/token/task_spec.cpp) |
| `UnknownTransferCancellation` | [apps/bruce_cli.cpp:130](../../apps/bruce_cli.cpp) |
| `UnknownVerificationCancellation` | [apps/bruce_cli.cpp:435](../../apps/bruce_cli.cpp) |
| `UnsafeLogicModFilePermissions` | [src/mod/logic_mod_plugin_v1.cpp:43](../../src/mod/logic_mod_plugin_v1.cpp) |
| `UnsafeRuntimeModFilePermissions` | [src/mod/runtime_mod_store_internal.cpp:26](../../src/mod/runtime_mod_store_internal.cpp) |
| `UnselectedLanguageAlternative` | [mods/language/src/language.cpp:643](../../mods/language/src/language.cpp) |
| `UnselectedTaskSpecAlternative` | [src/token/task_spec.cpp:170](../../src/token/task_spec.cpp) |
| `UnsupportedDialogueText` | [apps/bruce_cli.cpp:1139](../../apps/bruce_cli.cpp) |
| `UnsupportedEvidenceChainDomain` | [apps/bruce_cli.cpp:1176](../../apps/bruce_cli.cpp) |
| `UnsupportedEvidenceChainRoute` | [src/evidence/chain.cpp:15](../../src/evidence/chain.cpp) |
| `UnsupportedLanguageDraft` | [mods/language/src/language.cpp:370](../../mods/language/src/language.cpp) |
| `UnsupportedLanguageGrammar` | [apps/bruce_cli.cpp:1073](../../apps/bruce_cli.cpp) |
| `UnsupportedLanguageText` | [apps/bruce_cli.cpp:1032](../../apps/bruce_cli.cpp) |
| `UnsupportedLogicModCapability` | [src/mod/runtime_package.cpp:357](../../src/mod/runtime_package.cpp) |
| `UnsupportedRouteNormalization` | [src/tree/registry.cpp:80](../../src/tree/registry.cpp) |
| `UnsupportedRuleSource` | [src/compatibility/snapshot.cpp:71](../../src/compatibility/snapshot.cpp) |
| `UnsupportedRuntimeModCapability` | [src/mod/runtime_package.cpp:230](../../src/mod/runtime_package.cpp) |
| `UnsupportedRuntimeModVersion` | [src/mod/runtime_package.cpp:221](../../src/mod/runtime_package.cpp) |
| `UnsupportedTaskSpecNormalization` | [src/token/task_spec.cpp:133](../../src/token/task_spec.cpp) |
| `UnsupportedTaskSpecRoute` | [apps/bruce_cli.cpp:963](../../apps/bruce_cli.cpp) |
| `UnsupportedTaskSpecVersion` | [src/token/task_spec.cpp:132](../../src/token/task_spec.cpp) |
| `UnsupportedTokenTableVersion` | [apps/bruce_cli.cpp:964](../../apps/bruce_cli.cpp) |
| `UnusedRuleArgumentMustBeZero` | [src/compatibility/snapshot.cpp:76](../../src/compatibility/snapshot.cpp) |
| `Usage: bruce assess-run RUN.bruce` | [apps/bruce_cli.cpp:1450](../../apps/bruce_cli.cpp) |
| `Usage: bruce contracts` | [apps/bruce_cli.cpp:945](../../apps/bruce_cli.cpp) |
| `Usage: bruce dialogue confirm DRAFT --output DIALOGUE_RUN` | [apps/bruce_cli.cpp:1150](../../apps/bruce_cli.cpp) |
| `Usage: bruce dialogue interpret&#124;select&#124;confirm&#124;replay` | [apps/bruce_cli.cpp:1134](../../apps/bruce_cli.cpp) |
| `Usage: bruce dialogue replay DIALOGUE_DRAFT` | [apps/bruce_cli.cpp:1157](../../apps/bruce_cli.cpp) |
| `Usage: bruce dialogue select DRAFT --alternative ID --output SELECTED_DRAFT` | [apps/bruce_cli.cpp:1143](../../apps/bruce_cli.cpp) |
| `Usage: bruce dispatch inspect&#124;replay BUNDLE` | [apps/bruce_cli.cpp:781](../../apps/bruce_cli.cpp) |
| `Usage: bruce evidence create&#124;replay` | [apps/bruce_cli.cpp:1170](../../apps/bruce_cli.cpp) |
| `Usage: bruce evidence replay CHAIN` | [apps/bruce_cli.cpp:1199](../../apps/bruce_cli.cpp) |
| `Usage: bruce experience inspect SNAPSHOT` | [apps/bruce_cli.cpp:1358](../../apps/bruce_cli.cpp) |
| `Usage: bruce gate-contracts` | [apps/bruce_cli.cpp:942](../../apps/bruce_cli.cpp) |
| `Usage: bruce gate-run RUN.bruce [--compatibility-family gate] [--max-session-checkpoints N] [--max-replay-bytes N] [--cancellation none&#124;checkpoints]` | [apps/bruce_cli.cpp:1418](../../apps/bruce_cli.cpp) |
| `Usage: bruce inspect-run RUN.bruce` | [apps/bruce_cli.cpp:1453](../../apps/bruce_cli.cpp) |
| `Usage: bruce language execute DRAFT --output LANGUAGE_RUN` | [apps/bruce_cli.cpp:1095](../../apps/bruce_cli.cpp) |
| `Usage: bruce language interpret&#124;select&#124;execute&#124;replay` | [apps/bruce_cli.cpp:1012](../../apps/bruce_cli.cpp) |
| `Usage: bruce language replay LANGUAGE_RUN` | [apps/bruce_cli.cpp:1115](../../apps/bruce_cli.cpp) |
| `Usage: bruce language select DRAFT --alternative ID --output SELECTED_DRAFT` | [apps/bruce_cli.cpp:1075](../../apps/bruce_cli.cpp) |
| `Usage: bruce nanocell replay FILE` | [apps/nanocell_cli.hpp:114](../../apps/nanocell_cli.hpp) |
| `Usage: bruce nanocell run&#124;replay ...` | [apps/nanocell_cli.hpp:111](../../apps/nanocell_cli.hpp) |
| `Usage: bruce phase-dag inspect BUNDLE` | [apps/bruce_cli.cpp:799](../../apps/bruce_cli.cpp) |
| `Usage: bruce phase-dag-v2 ` | [apps/phase_dag_v2_cli.hpp:188](../../apps/phase_dag_v2_cli.hpp) |
| `Usage: bruce phase-dag-v2 execute SPEC --output BUNDLE` | [apps/phase_dag_v2_cli.hpp:197](../../apps/phase_dag_v2_cli.hpp) |
| `Usage: bruce phase-dag-v2 execute&#124;inspect&#124;replay` | [apps/phase_dag_v2_cli.hpp:184](../../apps/phase_dag_v2_cli.hpp) |
| `Usage: bruce phase-dag-v3 ` | [apps/phase_dag_v3_cli.hpp:197](../../apps/phase_dag_v3_cli.hpp) |
| `Usage: bruce phase-dag-v3 execute SPEC --output BUNDLE [--max-parallel-nodes N]` | [apps/phase_dag_v3_cli.hpp:207](../../apps/phase_dag_v3_cli.hpp) |
| `Usage: bruce phase-dag-v3 execute&#124;inspect&#124;replay` | [apps/phase_dag_v3_cli.hpp:193](../../apps/phase_dag_v3_cli.hpp) |
| `Usage: bruce replay RUN.bruce` | [apps/bruce_cli.cpp:1456](../../apps/bruce_cli.cpp) |
| `Usage: bruce signed-affine-dag-v1 demo merge&#124;cancellation` | [apps/signed_affine_dag_v1_cli.hpp:48](../../apps/signed_affine_dag_v1_cli.hpp) |
| `Usage: bruce signed-affine-relation-v1 demo merge` | [apps/signed_affine_relation_v1_cli.hpp:67](../../apps/signed_affine_relation_v1_cli.hpp) |
| `Usage: bruce task-contracts` | [apps/bruce_cli.cpp:939](../../apps/bruce_cli.cpp) |
| `Usage: bruce task-spec create&#124;inspect&#124;execute&#124;replay` | [apps/bruce_cli.cpp:952](../../apps/bruce_cli.cpp) |
| `Usage: bruce task-spec execute SPEC --output RUN` | [apps/bruce_cli.cpp:998](../../apps/bruce_cli.cpp) |
| `Usage: bruce task-spec inspect SPEC` | [apps/bruce_cli.cpp:994](../../apps/bruce_cli.cpp) |
| `Usage: bruce task-spec replay RUN` | [apps/bruce_cli.cpp:1007](../../apps/bruce_cli.cpp) |
| `Usage: bruce toy-relation-v1 demo collision&#124;noninvertible` | [apps/toy_relation_v1_cli.hpp:75](../../apps/toy_relation_v1_cli.hpp) |
| `Usage: compatibility-apply PLAN --from-run RUN --rules RULES [--output RUN]` | [apps/bruce_cli.cpp:1276](../../apps/bruce_cli.cpp) |
| `Usage: experience apply-add PLAN --from-run RUN [--store STORE] --output NEW` | [apps/bruce_cli.cpp:1329](../../apps/bruce_cli.cpp) |
| `Usage: rules-activate STORE PROPOSAL` | [apps/rule_store.hpp:30](../../apps/rule_store.hpp) |
| `Usage: rules-propose OLD NEW PROPOSAL &#124; rules-approve PROPOSAL CURRENT OUTPUT` | [apps/rule_review.hpp:13](../../apps/rule_review.hpp) |
| `Usage: rules-stage STORE NEW PROPOSAL` | [apps/rule_store.hpp:25](../../apps/rule_store.hpp) |
| `Usage: rules-status STORE` | [apps/rule_store.hpp:16](../../apps/rule_store.hpp) |
| `UseCandidatesOrResearchRun` | [apps/bruce_cli.cpp:1308](../../apps/bruce_cli.cpp) |
| `UseEitherZOrXY` | [apps/bruce_cli.cpp:842](../../apps/bruce_cli.cpp) |
| `UseGridOrCsvSamples` | [apps/bruce_cli.cpp:1210](../../apps/bruce_cli.cpp) |
| `UseGridOrExplicitSamples` | [src/application/grid_research.cpp:127](../../src/application/grid_research.cpp) |
| `UseRulesOrRuleStore` | [apps/bruce_cli.cpp:1283](../../apps/bruce_cli.cpp) |
| `VerificationCancellationRequiresReplay` | [apps/bruce_cli.cpp:443](../../apps/bruce_cli.cpp) |
| `VerificationCancelled` | [src/application/execution_session.cpp:28](../../src/application/execution_session.cpp) |
| `VersionTakesNoArguments` | [apps/bruce_cli.cpp:813](../../apps/bruce_cli.cpp) |
| `WORKER_LIMIT_EXCEEDED` | [src/application/common_runtime_host_v2_batch.cpp:290](../../src/application/common_runtime_host_v2_batch.cpp) |
| `WorkerAdapterFailed` | [apps/dag_scheduler_v5_cli.hpp:1388](../../apps/dag_scheduler_v5_cli.hpp) |
| `WorkerAttemptDirectoryUnavailable` | [src/worker/worker_supervisor_v1.cpp:543](../../src/worker/worker_supervisor_v1.cpp) |
| `WorkerControlFrameLimitExceeded` | [apps/dag_scheduler_v5_cli.hpp:1325](../../apps/dag_scheduler_v5_cli.hpp) |
| `WorkerControlFrameReadFailed` | [apps/dag_scheduler_v5_cli.hpp:1332](../../apps/dag_scheduler_v5_cli.hpp) |
| `WorkerDigestMismatch` | [apps/dag_scheduler_v5_cli.hpp:1148](../../apps/dag_scheduler_v5_cli.hpp) |
| `WorkerExecutableUnavailable` | [src/worker/worker_supervisor_v1.cpp:532](../../src/worker/worker_supervisor_v1.cpp) |
| `WorkerFramePayloadLimitExceeded` | [src/worker/worker_boundary_v1.cpp:95](../../src/worker/worker_boundary_v1.cpp) |
| `WorkerFrameTooLarge` | [src/worker/worker_boundary_v1.cpp:67](../../src/worker/worker_boundary_v1.cpp) |
| `WorkerFrameWriteFailed` | [apps/dag_scheduler_v5_cli.hpp:1288](../../apps/dag_scheduler_v5_cli.hpp) |
| `WorkerInputLimitExceeded` | [apps/dag_scheduler_v5_cli.hpp:1246](../../apps/dag_scheduler_v5_cli.hpp) |
| `WorkerOperationLimitExceeded` | [apps/dag_scheduler_v5_cli.hpp:1386](../../apps/dag_scheduler_v5_cli.hpp) |
| `WorkerOutputLimitExceeded` | [apps/dag_scheduler_v5_cli.hpp:1391](../../apps/dag_scheduler_v5_cli.hpp) |
| `WorkerOutputUnavailable` | [apps/dag_scheduler_v5_cli.hpp:1394](../../apps/dag_scheduler_v5_cli.hpp) |
| `WorkerRunRootUnavailable` | [src/worker/worker_supervisor_v1.cpp:537](../../src/worker/worker_supervisor_v1.cpp) |
| `ZeroDenominator` | [src/math/expression.cpp:60](../../src/math/expression.cpp) |
| `ZeroNormalFormDenominator` | [src/math/expression_normal_form.cpp:74](../../src/math/expression_normal_form.cpp) |
| `ZeroPolynomialDivisor` | [src/math/expression_normal_form.cpp:37](../../src/math/expression_normal_form.cpp) |
