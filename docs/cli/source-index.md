# Quellenbasierter Options- und Diagnoseindex

[Handbuch](README.md)

Stand Core 0.102.0. Automatisch durch `verify_examples.py` aus dem lokalen Quellbaum ermittelt.

## Optionen nach Parserzweig

Gemeinsame Optionen sind absichtlich pro Befehl aufgeführt. Die Liste beschreibt Annahme durch den Parser, nicht die Zulässigkeit jeder Wertkombination; diese steht in der Befehlsreferenz.

### `compare`

`--execution-cancellation`, `--serialization-cancellation`, `--verification`, `--verification-cancellation`, `--max-replay-bytes`, `--max-serialized-bytes`, `--max-session-checkpoints`, `--left-numerator`, `--left-denominator`, `--right-numerator`, `--right-denominator`, `--domain-policy`, `--max-bits`, `--max-operations`, `--output`

### `coordinate`

`--x`, `--y`, `--z`

### `grid`

`--execution-cancellation`, `--serialization-cancellation`, `--verification`, `--verification-cancellation`, `--max-replay-bytes`, `--max-serialized-bytes`, `--max-session-checkpoints`, `--path`, `--mode`, `--modulus`, `--max-bits`, `--zero-zero`, `--output`

### `grid-batch`

`--execution-cancellation`, `--serialization-cancellation`, `--verification`, `--verification-cancellation`, `--max-replay-bytes`, `--max-serialized-bytes`, `--max-session-checkpoints`, `--paths`, `--mode`, `--modulus`, `--max-bits`, `--zero-zero`, `--cache-bytes`, `--max-query-steps`, `--output`

### `grid-resume`

`--execution-cancellation`, `--serialization-cancellation`, `--verification`, `--verification-cancellation`, `--max-replay-bytes`, `--max-serialized-bytes`, `--max-session-checkpoints`, `--from-run`, `--additional-queries`, `--output`

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

`--execution-cancellation`, `--serialization-cancellation`, `--verification`, `--verification-cancellation`, `--max-replay-bytes`, `--max-serialized-bytes`, `--max-session-checkpoints`, `--train`, `--holdout`, `--min`, `--max`, `--max-candidates`, `--max-operations`, `--max-trace`, `--max-results`, `--output`, `--domain`, `--max-bits`, `--max-denominator`, `--max-trace-bytes`, `--strategy`, `--max-power`, `--planner`, `--max-degree`, `--experience`, `--max-nodes`, `--equivalence`, `--domain-cache`, `--expression-planner`, `--operators`, `--grid-train`, `--grid-holdout`, `--grid-axis`, `--max-total-operations`

### `compatibility`

`--execution-cancellation`, `--serialization-cancellation`, `--verification`, `--verification-cancellation`, `--max-replay-bytes`, `--max-serialized-bytes`, `--max-session-checkpoints`, `--rules`, `--rule-store`, `--candidates`, `--from-run`, `--at`, `--max-candidates`, `--max-checks`, `--max-trace`, `--output`

### `gate-run`

`--max-session-checkpoints`, `--max-replay-bytes`, `--cancellation`

### `compatibility-plan`

`--from-run`, `--rules`, `--rule-store`, `--at`, `--max-source-bytes`, `--max-candidates`, `--max-checks`, `--max-trace`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`.

### `compatibility-apply`

Plan als Positionsargument, danach `--from-run`, `--rules`, `--rule-store`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`.

### `experience`

`add`: `--store`, `--from-run`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`.

`revoke`: `--store`, `--id`, `--reason`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`.

`replace`: `--store`, `--id`, `--reason`, `--from-run`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`.

`inspect`: genau ein Positionsargument; keine Optionen.

`plan-add`: `--from-run`, `--store`, `--max-source-bytes`, `--max-store-bytes`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`.

`apply-add`: Plan als Positionsargument, danach `--from-run`, `--store`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`.

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

### `mods` und `route`

`mods verify`: Paket als Positionsargument; `--trust-store`.

`mods install`: Paket als Positionsargument; `--mod-root`, `--trust-store`.

`mods list`: `--mod-root`.

`mods activate`: Mod-ID und Version als Positionsargumente; `--mod-root`.

`mods disable`: Mod-ID als Positionsargument; `--mod-root`.

`route`: `--text`, `--mod-root`, optional `--output`.

`route replay`: Report als Positionsargument; `--mod-root`.

## Diagnose-Literale

Aus `Error("…")`, Stopaufrufen und Zuweisungen von `stop_reason` in `src/`, `include/` und der CLI. Dies sind vollständige gefundene Textliterale bzw. Präfixe; dynamisch zusammengesetzte Suffixe und Betriebssystemmeldungen sind nicht enumerierbar. Nicht jeder interne Fehler ist bei jedem CLI-Aufruf erreichbar. Quellverweise zeigen die Entstehungsstelle.

| Literal / Präfix | Quelle |
| --- | --- |
| `AdaptiveOrderRequiresRationalPortfolio` | [src/application/research_contract.cpp:38](../../src/application/research_contract.cpp) |
| `AdaptiveRequiresPowerStrategy` | [src/application/research_contract.cpp:43](../../src/application/research_contract.cpp) |
| `ApplicationDispatchPayloadTooLarge` | [mods/cockpit/src/cockpit_assets.cpp:221](../../mods/cockpit/src/cockpit_assets.cpp) |
| `ApplicationReplayMismatch` | [src/application/execution_session.cpp:83](../../src/application/execution_session.cpp) |
| `AtRequiresResearchRun` | [apps/bruce_cli.cpp:1089](../../apps/bruce_cli.cpp) |
| `BundleByteLimit` | [src/application/compatibility.cpp:224](../../src/application/compatibility.cpp) |
| `Cancelled` | [src/application/exact_research.cpp:51](../../src/application/exact_research.cpp) |
| `CandidateBudgetExceeded` | [src/application/exact_research.cpp:95](../../src/application/exact_research.cpp) |
| `CandidateByteLimit` | [src/compatibility/snapshot.cpp:95](../../src/compatibility/snapshot.cpp) |
| `CandidateInputLimit` | [src/compatibility/snapshot.cpp:116](../../src/compatibility/snapshot.cpp) |
| `CannotCreateExport: ` | [src/io/local_files.cpp:68](../../src/io/local_files.cpp) |
| `CannotReadFile: ` | [src/io/local_files.cpp:49](../../src/io/local_files.cpp) |
| `CheckpointRequiresAdaptivePlanner` | [src/application/research_contract.cpp:70](../../src/application/research_contract.cpp) |
| `CockpitImportTooLarge` | [mods/cockpit/src/cockpit_assets.cpp:317](../../mods/cockpit/src/cockpit_assets.cpp) |
| `ComparisonReplayCheckpointsRequireCallback` | [src/application/execution_session.cpp:39](../../src/application/execution_session.cpp) |
| `ComparisonReplayCheckpointsRequireComparisonTask` | [src/application/execution_session.cpp:40](../../src/application/execution_session.cpp) |
| `CompatibilityProjectionRequiresInt64: fractional value` | [src/application/compatibility.cpp:108](../../src/application/compatibility.cpp) |
| `CompatibilityProjectionRequiresInt64: out of range` | [src/application/compatibility.cpp:111](../../src/application/compatibility.cpp) |
| `CompatibilityReplayCheckpointsRequireCallback` | [src/application/execution_session.cpp:35](../../src/application/execution_session.cpp) |
| `CompatibilityReplayCheckpointsRequireCompatibilityTask` | [src/application/execution_session.cpp:36](../../src/application/execution_session.cpp) |
| `CompatibilityTransferByteLimit` | [src/application/compatibility_transfer.cpp:15](../../src/application/compatibility_transfer.cpp) |
| `CompatibilityTransferDigestMismatch` | [src/application/compatibility_transfer.cpp:27](../../src/application/compatibility_transfer.cpp) |
| `CompatibilityTransferInvalidBudget` | [apps/bruce_cli.cpp:1061](../../apps/bruce_cli.cpp) |
| `CompatibilityTransferPlanByteLimit` | [src/application/compatibility_transfer.cpp:25](../../src/application/compatibility_transfer.cpp) |
| `CompatibilityTransferRulesChanged` | [src/application/compatibility_transfer.cpp:39](../../src/application/compatibility_transfer.cpp) |
| `CompatibilityTransferSourceChanged` | [src/application/compatibility_transfer.cpp:37](../../src/application/compatibility_transfer.cpp) |
| `Completed` | [include/bruce/application/exact_research.hpp:47](../../include/bruce/application/exact_research.hpp) |
| `ConflictingReplayCheckpoints` | [src/application/execution_session.cpp:33](../../src/application/execution_session.cpp) |
| `ConflictingSerializationCheckpoints` | [src/application/execution_session.cpp:52](../../src/application/execution_session.cpp) |
| `ConflictingTaskSpecRoles` | [src/token/task_spec.cpp:168](../../src/token/task_spec.cpp) |
| `CoordinateOverflow` | [src/coordinate/address.cpp:13](../../src/coordinate/address.cpp) |
| `CountercheckMismatch` | [src/application/exact_research.cpp:120](../../src/application/exact_research.cpp) |
| `DepthBudgetExceeded` | [src/coordinate/address.cpp:46](../../src/coordinate/address.cpp) |
| `DigestFailure` | [src/evidence/bundle.cpp:14](../../src/evidence/bundle.cpp) |
| `DivisionByZero` | [src/math/numbers.cpp:45](../../src/math/numbers.cpp) |
| `DivmodRequiresBigint` | [src/application/exact_research.cpp:228](../../src/application/exact_research.cpp) |
| `DomainCacheRequiresModularExpression` | [apps/bruce_cli.cpp:987](../../apps/bruce_cli.cpp) |
| `DomainWitnessCountercheckFailed` | [src/math/expression_normal_form.cpp:279](../../src/math/expression_normal_form.cpp) |
| `DuplicateCandidateId` | [src/compatibility/selection.cpp:11](../../src/compatibility/selection.cpp) |
| `DuplicateExperienceTrainingInput` | [src/experience/store.cpp:64](../../src/experience/store.cpp) |
| `DuplicateExpressionConstant` | [src/math/expression.cpp:62](../../src/math/expression.cpp) |
| `DuplicateInterpolationInput` | [src/math/polynomial.cpp:19](../../src/math/polynomial.cpp) |
| `DuplicateLanguageAlias` | [mods/language/src/language.cpp:65](../../mods/language/src/language.cpp) |
| `DuplicateOption: ` | [apps/bruce_cli.cpp:68](../../apps/bruce_cli.cpp) |
| `DuplicateOrOverlappingGridInput` | [src/application/grid_research.cpp:61](../../src/application/grid_research.cpp) |
| `DuplicateOrOverlappingSampleInput` | [src/application/research_contract.cpp:87](../../src/application/research_contract.cpp) |
| `DuplicateOrUnsortedTaskSpecAlias` | [src/token/task_spec.cpp:159](../../src/token/task_spec.cpp) |
| `DuplicateRuntimeModJsonKey` | [src/mod/runtime_package.cpp:104](../../src/mod/runtime_package.cpp) |
| `DuplicateTaskSpecFileAlias` | [apps/bruce_cli.cpp:741](../../apps/bruce_cli.cpp) |
| `DuplicateTaskSpecRoleAsset` | [src/token/task_spec.cpp:117](../../src/token/task_spec.cpp) |
| `DuplicateTrainingInput` | [src/application/research.cpp:19](../../src/application/research.cpp) |
| `DuplicateTreeAddress` | [src/tree/registry.cpp:54](../../src/tree/registry.cpp) |
| `DuplicateTreeSiblingKey` | [src/tree/registry.cpp:75](../../src/tree/registry.cpp) |
| `EmptyGridHoldoutSource` | [apps/bruce_cli.cpp:1027](../../apps/bruce_cli.cpp) |
| `EmptySamples` | [src/application/exact_research.cpp:214](../../src/application/exact_research.cpp) |
| `EmptyTokenTable` | [src/token/task_spec.cpp:144](../../src/token/task_spec.cpp) |
| `EmptyTreeFragment` | [src/tree/registry.cpp:44](../../src/tree/registry.cpp) |
| `EquivalenceRequiresExpressionStrategy` | [apps/bruce_cli.cpp:992](../../apps/bruce_cli.cpp) |
| `EvidenceChainNativeResultMismatch` | [src/evidence/chain.cpp:123](../../src/evidence/chain.cpp) |
| `EvidenceChainRequiresExactNativeBundle` | [src/evidence/chain.cpp:120](../../src/evidence/chain.cpp) |
| `EvidenceChainSampleBindingMismatch` | [src/evidence/chain.cpp:35](../../src/evidence/chain.cpp) |
| `ExactOptionsRequireBigintOrRationalDomain` | [apps/bruce_cli.cpp:1037](../../apps/bruce_cli.cpp) |
| `ExecutionCancellationRequiresReplay` | [apps/bruce_cli.cpp:369](../../apps/bruce_cli.cpp) |
| `ExecutionCompatibilitySourceCancelled` | [src/application/application_task.cpp:48](../../src/application/application_task.cpp) |
| `ExecutionCompatibilitySourceRequiresCompatibilityTask` | [src/application/application_task.cpp:57](../../src/application/application_task.cpp) |
| `ExecutionCompatibilitySourceRequiresOrigin` | [src/application/compatibility.cpp:141](../../src/application/compatibility.cpp) |
| `ExecutionExperienceCancelled` | [src/application/application_task.cpp:44](../../src/application/application_task.cpp) |
| `ExecutionExperienceCheckpointsRequireLearnedResearch` | [src/application/application_task.cpp:68](../../src/application/application_task.cpp) |
| `ExecutionGridSourceCancelled` | [src/application/application_task.cpp:46](../../src/application/application_task.cpp) |
| `ExecutionGridSourceCheckpointsRequireGridResearch` | [src/application/application_task.cpp:60](../../src/application/application_task.cpp) |
| `ExecutionResumeImportCancelled` | [src/application/application_task.cpp:50](../../src/application/application_task.cpp) |
| `ExecutionResumeImportRequiresResumeTask` | [src/application/application_task.cpp:54](../../src/application/application_task.cpp) |
| `ExecutionSessionFailed at ` | [src/application/execution_session.cpp:19](../../src/application/execution_session.cpp) |
| `ExecutionSessionNotReady` | [src/application/execution_session.cpp:92](../../src/application/execution_session.cpp) |
| `ExperienceByteLimit` | [src/application/exact_replay.cpp:334](../../src/application/exact_replay.cpp) |
| `ExperienceEventLimit` | [src/experience/store.cpp:34](../../src/experience/store.cpp) |
| `ExperienceRecordLimit` | [src/experience/store.cpp:61](../../src/experience/store.cpp) |
| `ExperienceRequiresCompletedHoldoutRun` | [src/application/experience_source.cpp:57](../../src/application/experience_source.cpp) |
| `ExperienceRequiresIndependentPortfolioSource` | [src/application/experience_source.cpp:22](../../src/application/experience_source.cpp) |
| `ExperienceRequiresRationalPortfolio` | [apps/bruce_cli.cpp:1035](../../apps/bruce_cli.cpp) |
| `ExperienceRequiresVerifiedHypothesis` | [src/application/experience_source.cpp:67](../../src/application/experience_source.cpp) |
| `ExperienceSourceOperationLimit` | [src/application/experience_source.cpp:25](../../src/application/experience_source.cpp) |
| `ExperienceTransferBaseChanged` | [src/application/experience_transfer.cpp:38](../../src/application/experience_transfer.cpp) |
| `ExperienceTransferByteLimit` | [src/application/experience_transfer.cpp:11](../../src/application/experience_transfer.cpp) |
| `ExperienceTransferDigestMismatch` | [src/application/experience_transfer.cpp:28](../../src/application/experience_transfer.cpp) |
| `ExperienceTransferInvalidBudget` | [apps/bruce_cli.cpp:1110](../../apps/bruce_cli.cpp) |
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
| `ExpressionPlannerRequiresFiveFamilyPortfolio` | [apps/bruce_cli.cpp:991](../../apps/bruce_cli.cpp) |
| `ExpressionRankOutOfRange` | [src/math/expression.cpp:117](../../src/math/expression.cpp) |
| `ExpressionRequiresGrammarPlanner` | [src/application/research_contract.cpp:39](../../src/application/research_contract.cpp) |
| `FileReadError` | [src/io/local_files.cpp:59](../../src/io/local_files.cpp) |
| `Forged NanoCell technique` | [mods/nanocell/src/nanocell.cpp:164](../../mods/nanocell/src/nanocell.cpp) |
| `FractionRequiresRationalDomain` | [src/math/numbers.cpp:25](../../src/math/numbers.cpp) |
| `GateInspectionCancelled` | [apps/bruce_cli.cpp:1190](../../apps/bruce_cli.cpp) |
| `GridAxisRequiresGridTraining` | [apps/bruce_cli.cpp:983](../../apps/bruce_cli.cpp) |
| `GridBatchInputByteLimit` | [apps/bruce_cli.cpp:697](../../apps/bruce_cli.cpp) |
| `GridBudgetRequiresUnbudgetedOrigin` | [src/application/grid_research.cpp:189](../../src/application/grid_research.cpp) |
| `GridCacheByteLimit` | [src/application/grid_batch.cpp:45](../../src/application/grid_batch.cpp) |
| `GridExperienceBudgetMismatch` | [src/application/grid_research.cpp:148](../../src/application/grid_research.cpp) |
| `GridHoldoutRequiresGridTraining` | [apps/bruce_cli.cpp:984](../../apps/bruce_cli.cpp) |
| `GridInputByteLimit` | [src/application/grid.cpp:33](../../src/application/grid.cpp) |
| `GridProjectionInputOutOfRange` | [src/application/compatibility.cpp:84](../../src/application/compatibility.cpp) |
| `GridQueryLimit` | [apps/bruce_cli.cpp:702](../../apps/bruce_cli.cpp) |
| `GridReplayCheckpointsRequireBatchOrResume` | [src/application/execution_session.cpp:50](../../src/application/execution_session.cpp) |
| `GridReplayCheckpointsRequireCallback` | [src/application/execution_session.cpp:48](../../src/application/execution_session.cpp) |
| `GridResearchRequiresExactDomain` | [apps/bruce_cli.cpp:997](../../apps/bruce_cli.cpp) |
| `GridResearchRequiresExactValues` | [src/application/grid_research.cpp:40](../../src/application/grid_research.cpp) |
| `GridResearchRequiresFixedSlice` | [src/application/grid_research.cpp:60](../../src/application/grid_research.cpp) |
| `GridResearchSampleBindingMismatch` | [src/application/grid_research.cpp:217](../../src/application/grid_research.cpp) |
| `GridResearchSourceByteLimit` | [src/application/grid_research.cpp:19](../../src/application/grid_research.cpp) |
| `GridResearchSpecMismatch` | [src/application/grid_research.cpp:46](../../src/application/grid_research.cpp) |
| `GridResearchStoppedSample` | [src/application/grid_research.cpp:50](../../src/application/grid_research.cpp) |
| `GridResumeGenerationLimit` | [src/application/grid_resume.cpp:43](../../src/application/grid_resume.cpp) |
| `GridResumeRequiresIncompleteSource` | [src/application/grid_resume.cpp:56](../../src/application/grid_resume.cpp) |
| `GridWorkBudgetMismatch` | [src/application/grid_research.cpp:150](../../src/application/grid_research.cpp) |
| `HoldoutOverlapOrDuplicate` | [src/application/research.cpp:21](../../src/application/research.cpp) |
| `IncompleteGridSource` | [src/application/grid_research.cpp:45](../../src/application/grid_research.cpp) |
| `IncompleteResearchCannotBeRanked` | [src/application/compatibility.cpp:89](../../src/application/compatibility.cpp) |
| `Inconsistent NanoCell run` | [mods/nanocell/src/nanocell.cpp:407](../../mods/nanocell/src/nanocell.cpp) |
| `InputByteLimit` | [src/application/exact_research.cpp:200](../../src/application/exact_research.cpp) |
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
| `InvalidBitLimit` | [src/application/grid_spec.cpp:8](../../src/application/grid_spec.cpp) |
| `InvalidBitLimit: expected 1..16384` | [src/math/numbers.cpp:9](../../src/math/numbers.cpp) |
| `InvalidBitLimit: expected 1..64` | [src/math/exact.cpp:5](../../src/math/exact.cpp) |
| `InvalidBudget: candidates 1..1000000, operations 1..10000000, trace 2..100002, results 1..4096` | [src/application/research.cpp:16](../../src/application/research.cpp) |
| `InvalidBundleBlockSize` | [src/application/compatibility.cpp:66](../../src/application/compatibility.cpp) |
| `InvalidBundleField: ` | [src/application/fraction_compare.cpp:70](../../src/application/fraction_compare.cpp) |
| `InvalidCandidateCount: expected 1..4096` | [src/compatibility/snapshot.cpp:22](../../src/compatibility/snapshot.cpp) |
| `InvalidCandidateHeader` | [src/compatibility/snapshot.cpp:100](../../src/compatibility/snapshot.cpp) |
| `InvalidCandidateRow` | [src/compatibility/snapshot.cpp:112](../../src/compatibility/snapshot.cpp) |
| `InvalidComparisonCancellation` | [src/application/fraction_compare.cpp:78](../../src/application/fraction_compare.cpp) |
| `InvalidComparisonOperationBudget: expected 1..1000000` | [src/application/fraction_compare.cpp:77](../../src/application/fraction_compare.cpp) |
| `InvalidCompatibilityBudget` | [src/compatibility/snapshot.cpp:133](../../src/compatibility/snapshot.cpp) |
| `InvalidCompatibilityBundleField` | [src/application/compatibility.cpp:60](../../src/application/compatibility.cpp) |
| `InvalidCompatibilityTransfer` | [src/application/compatibility_transfer.cpp:29](../../src/application/compatibility_transfer.cpp) |
| `InvalidDenominatorLimit` | [src/application/research_contract.cpp:47](../../src/application/research_contract.cpp) |
| `InvalidDialogueStatus` | [apps/bruce_cli.cpp:284](../../apps/bruce_cli.cpp) |
| `InvalidDigestHeader` | [src/application/compatibility.cpp:235](../../src/application/compatibility.cpp) |
| `InvalidDomainId` | [src/compatibility/snapshot.cpp:28](../../src/compatibility/snapshot.cpp) |
| `InvalidDomainPolicy` | [src/application/fraction_compare.cpp:75](../../src/application/fraction_compare.cpp) |
| `InvalidEvaluationMode` | [src/application/grid_spec.cpp:6](../../src/application/grid_spec.cpp) |
| `InvalidEvidence: digest mismatch` | [src/application/compatibility.cpp:237](../../src/application/compatibility.cpp) |
| `InvalidEvidenceChainBlockSize` | [src/evidence/chain.cpp:87](../../src/evidence/chain.cpp) |
| `InvalidEvidenceChainField` | [src/evidence/chain.cpp:80](../../src/evidence/chain.cpp) |
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
| `InvalidGridAdditionalQueries` | [src/application/grid_resume.cpp:44](../../src/application/grid_resume.cpp) |
| `InvalidGridCancellation` | [src/application/grid_batch.cpp:96](../../src/application/grid_batch.cpp) |
| `InvalidGridParentSize` | [src/application/grid_resume.cpp:76](../../src/application/grid_resume.cpp) |
| `InvalidGridPath: expected x,y/x,y` | [src/coordinate/address.cpp:41](../../src/coordinate/address.cpp) |
| `InvalidGridQueryBudget` | [src/application/grid_batch.cpp:95](../../src/application/grid_batch.cpp) |
| `InvalidGridResearchBlockSize` | [src/application/grid_research.cpp:120](../../src/application/grid_research.cpp) |
| `InvalidGridResearchField` | [src/application/grid_research.cpp:114](../../src/application/grid_research.cpp) |
| `InvalidGridResumeField` | [src/application/grid_resume.cpp:17](../../src/application/grid_resume.cpp) |
| `InvalidGridTotalBudget` | [src/application/grid_research.cpp:20](../../src/application/grid_research.cpp) |
| `InvalidInteger: ` | [include/bruce/core/types.hpp:19](../../include/bruce/core/types.hpp) |
| `InvalidInterpolationInput` | [src/math/polynomial.cpp:14](../../src/math/polynomial.cpp) |
| `InvalidLanguageAlias` | [mods/language/src/language.cpp:61](../../mods/language/src/language.cpp) |
| `InvalidLanguageAlternativeCount` | [mods/language/src/language.cpp:241](../../mods/language/src/language.cpp) |
| `InvalidLanguageCorePhases` | [mods/language/src/language_run.cpp:114](../../mods/language/src/language_run.cpp) |
| `InvalidLanguageDraftField` | [mods/language/src/language.cpp:80](../../mods/language/src/language.cpp) |
| `InvalidLanguageDraftHeader` | [mods/language/src/language.cpp:214](../../mods/language/src/language.cpp) |
| `InvalidLanguageInputCount` | [mods/language/src/language.cpp:59](../../mods/language/src/language.cpp) |
| `InvalidLanguageModCodec` | [apps/bruce_cli.cpp:209](../../apps/bruce_cli.cpp) |
| `InvalidLanguageRunField` | [mods/language/src/language_run.cpp:126](../../mods/language/src/language_run.cpp) |
| `InvalidLanguageRunHeader` | [mods/language/src/language_run.cpp:190](../../mods/language/src/language_run.cpp) |
| `InvalidLanguageRunPhases` | [mods/language/src/language_run.cpp:87](../../mods/language/src/language_run.cpp) |
| `InvalidLanguageSelection` | [mods/language/src/language.cpp:134](../../mods/language/src/language.cpp) |
| `InvalidLanguageStatus` | [apps/bruce_cli.cpp:102](../../apps/bruce_cli.cpp) |
| `InvalidLanguageUtf8` | [mods/language/src/language.cpp:40](../../mods/language/src/language.cpp) |
| `InvalidLocalAddress` | [src/coordinate/address.cpp:32](../../src/coordinate/address.cpp) |
| `InvalidLocalCoordinate` | [src/coordinate/address.cpp:28](../../src/coordinate/address.cpp) |
| `InvalidModCapabilities` | [src/mod/mod_api.cpp:99](../../src/mod/mod_api.cpp) |
| `InvalidModCapability` | [src/mod/mod_api.cpp:32](../../src/mod/mod_api.cpp) |
| `InvalidModManifest` | [src/mod/mod_api.cpp:96](../../src/mod/mod_api.cpp) |
| `InvalidModRunCodec` | [src/mod/mod_api.cpp:79](../../src/mod/mod_api.cpp) |
| `InvalidModRunField` | [src/mod/mod_api.cpp:59](../../src/mod/mod_api.cpp) |
| `InvalidModRunHeader` | [src/mod/mod_api.cpp:140](../../src/mod/mod_api.cpp) |
| `InvalidModularPredicate` | [src/compatibility/snapshot.cpp:75](../../src/compatibility/snapshot.cpp) |
| `InvalidModulus` | [src/application/grid_spec.cpp:10](../../src/application/grid_spec.cpp) |
| `InvalidNanoCellCoreHex` | [apps/nanocell_cli.hpp:47](../../apps/nanocell_cli.hpp) |
| `InvalidNanoCellModCodec` | [mods/nanocell/src/mod.cpp:16](../../mods/nanocell/src/mod.cpp) |
| `InvalidNanoCellStatus` | [apps/nanocell_cli.hpp:72](../../apps/nanocell_cli.hpp) |
| `InvalidNanoCellTraceKind` | [apps/nanocell_cli.hpp:81](../../apps/nanocell_cli.hpp) |
| `InvalidNormalizationConfiguration` | [src/math/domain_cache.cpp:5](../../src/math/domain_cache.cpp) |
| `InvalidNormalizedRouteInput` | [src/tree/registry.cpp:83](../../src/tree/registry.cpp) |
| `InvalidOrDuplicateRuleId` | [src/compatibility/snapshot.cpp:67](../../src/compatibility/snapshot.cpp) |
| `InvalidPolynomialDegree` | [src/application/research_contract.cpp:57](../../src/application/research_contract.cpp) |
| `InvalidPolynomialSize` | [src/math/polynomial.cpp:7](../../src/math/polynomial.cpp) |
| `InvalidPortfolioDomainCache` | [src/application/portfolio_normalization.cpp:13](../../src/application/portfolio_normalization.cpp) |
| `InvalidPowerLimit` | [src/application/research_contract.cpp:72](../../src/application/research_contract.cpp) |
| `InvalidPowerLimit: expected 1..8` | [src/math/power_affine.cpp:6](../../src/math/power_affine.cpp) |
| `InvalidRationalLinearDomainOrPower` | [src/application/research_contract.cpp:56](../../src/application/research_contract.cpp) |
| `InvalidRationalLinearSystem` | [src/math/rational_linear.cpp:29](../../src/math/rational_linear.cpp) |
| `InvalidRoutePattern` | [src/tree/registry.cpp:61](../../src/tree/registry.cpp) |
| `InvalidRouteReport` | [apps/runtime_mod_cli.hpp:96](../../apps/runtime_mod_cli.hpp) |
| `InvalidRouteStatus` | [apps/runtime_mod_cli.hpp:44](../../apps/runtime_mod_cli.hpp) |
| `InvalidRouteUtf8` | [src/tree/normalizer.cpp:34](../../src/tree/normalizer.cpp) |
| `InvalidRuleHistory` | [src/application/rule_history.cpp:41](../../src/application/rule_history.cpp) |
| `InvalidRuleProposal` | [src/application/rule_proposal.cpp:17](../../src/application/rule_proposal.cpp) |
| `InvalidRuleRow: expected 10 fields` | [src/compatibility/snapshot.cpp:64](../../src/compatibility/snapshot.cpp) |
| `InvalidRuleStage` | [src/application/rule_history.cpp:25](../../src/application/rule_history.cpp) |
| `InvalidRuleWeight` | [src/compatibility/snapshot.cpp:73](../../src/compatibility/snapshot.cpp) |
| `InvalidRuntimeModContainer` | [src/mod/runtime_package.cpp:237](../../src/mod/runtime_package.cpp) |
| `InvalidRuntimeModJournal` | [src/mod/runtime_registry.cpp:158](../../src/mod/runtime_registry.cpp) |
| `InvalidRuntimeModJournalGeneration` | [src/mod/runtime_registry.cpp:176](../../src/mod/runtime_registry.cpp) |
| `InvalidRuntimeModMember` | [src/mod/runtime_package.cpp:289](../../src/mod/runtime_package.cpp) |
| `InvalidRuntimeModRegistry` | [src/mod/runtime_registry.cpp:81](../../src/mod/runtime_registry.cpp) |
| `InvalidRuntimeModSchema` | [src/mod/runtime_package.cpp:17](../../src/mod/runtime_package.cpp) |
| `InvalidRuntimeModSignatureEncoding` | [src/mod/runtime_package.cpp:293](../../src/mod/runtime_package.cpp) |
| `InvalidRuntimeModState` | [apps/runtime_mod_cli.hpp:36](../../apps/runtime_mod_cli.hpp) |
| `InvalidRuntimeModTrustStore` | [src/mod/runtime_package.cpp:304](../../src/mod/runtime_package.cpp) |
| `InvalidSample` | [src/application/replay.cpp:35](../../src/application/replay.cpp) |
| `InvalidSampleCount` | [src/application/research_contract.cpp:78](../../src/application/research_contract.cpp) |
| `InvalidSampleCount: training 1..4096, holdout 0..4096` | [src/application/research.cpp:12](../../src/application/research.cpp) |
| `InvalidSampleHeader: expected x,y` | [src/application/exact_research.cpp:205](../../src/application/exact_research.cpp) |
| `InvalidSampleHeader: expected x,y (input,expected_output)` | [src/application/replay.cpp:89](../../src/application/replay.cpp) |
| `InvalidSampleRow` | [src/application/exact_research.cpp:210](../../src/application/exact_research.cpp) |
| `InvalidSampleRow: expected x,y` | [src/application/replay.cpp:94](../../src/application/replay.cpp) |
| `InvalidSearchRange: expected -1000000 <= min <= max <= 1000000` | [src/application/research.cpp:10](../../src/application/research.cpp) |
| `InvalidSnapshotDigest` | [src/application/compatibility.cpp:252](../../src/application/compatibility.cpp) |
| `InvalidSnapshotId` | [src/compatibility/snapshot.cpp:58](../../src/compatibility/snapshot.cpp) |
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
| `InvalidTreeAddress` | [src/tree/registry.cpp:50](../../src/tree/registry.cpp) |
| `InvalidTreeKey` | [src/tree/registry.cpp:55](../../src/tree/registry.cpp) |
| `InvalidTreeOwner` | [src/tree/registry.cpp:42](../../src/tree/registry.cpp) |
| `InvalidZeroZeroPolicy` | [src/application/grid_spec.cpp:7](../../src/application/grid_spec.cpp) |
| `LanguageAliasOptionsRequireV2` | [apps/bruce_cli.cpp:828](../../apps/bruce_cli.cpp) |
| `LanguageCoreByteLimit` | [mods/language/src/language_run.cpp:104](../../mods/language/src/language_run.cpp) |
| `LanguageCoreDigestMismatch` | [mods/language/src/language_run.cpp:202](../../mods/language/src/language_run.cpp) |
| `LanguageCoreEvidenceMismatch` | [mods/language/src/language_run.cpp:118](../../mods/language/src/language_run.cpp) |
| `LanguageCoreTaskMismatch` | [mods/language/src/language_run.cpp:65](../../mods/language/src/language_run.cpp) |
| `LanguageDraftByteLimit` | [mods/language/src/language.cpp:207](../../mods/language/src/language.cpp) |
| `LanguageDraftDigestMismatch` | [mods/language/src/language.cpp:217](../../mods/language/src/language.cpp) |
| `LanguageExplanationMismatch` | [mods/language/src/language_run.cpp:113](../../mods/language/src/language_run.cpp) |
| `LanguageInputDigestMismatch` | [mods/language/src/language.cpp:62](../../mods/language/src/language.cpp) |
| `LanguageModDraftRunMismatch` | [apps/bruce_cli.cpp:213](../../apps/bruce_cli.cpp) |
| `LanguageRunByteLimit` | [mods/language/src/language_run.cpp:183](../../mods/language/src/language_run.cpp) |
| `LanguageRunDigestMismatch` | [mods/language/src/language_run.cpp:192](../../mods/language/src/language_run.cpp) |
| `LocalModBuildFailed` | [src/mod/local_builder.cpp:20](../../src/mod/local_builder.cpp) |
| `ManagementCheckpointBudgetExceeded: checkpoints=` | [include/bruce/application/management_inspection_control.hpp:23](../../include/bruce/application/management_inspection_control.hpp) |
| `ManagementCheckpointCounterOverflow` | [include/bruce/application/management_inspection_control.hpp:27](../../include/bruce/application/management_inspection_control.hpp) |
| `ManagementInspectionCancelled` | [include/bruce/application/management_inspection_control.hpp:21](../../include/bruce/application/management_inspection_control.hpp) |
| `MaxDegreeRequiresPolynomialStrategy` | [apps/bruce_cli.cpp:995](../../apps/bruce_cli.cpp) |
| `MaxNodesRequiresExpressionStrategy` | [apps/bruce_cli.cpp:993](../../apps/bruce_cli.cpp) |
| `MaxPowerRequiresPowerStrategy` | [apps/bruce_cli.cpp:994](../../apps/bruce_cli.cpp) |
| `Missing NanoCell length separator` | [mods/nanocell/src/nanocell.cpp:246](../../mods/nanocell/src/nanocell.cpp) |
| `MissingCandidateHeader` | [src/compatibility/snapshot.cpp:98](../../src/compatibility/snapshot.cpp) |
| `MissingExperienceAction` | [apps/bruce_cli.cpp:1099](../../apps/bruce_cli.cpp) |
| `MissingExperienceSnapshot` | [src/application/grid_research.cpp:131](../../src/application/grid_research.cpp) |
| `MissingGridExecutionBoundary` | [src/application/grid_batch.cpp:158](../../src/application/grid_batch.cpp) |
| `MissingLanguageAlias` | [mods/language/src/language.cpp:157](../../mods/language/src/language.cpp) |
| `MissingOption: ` | [apps/bruce_cli.cpp:78](../../apps/bruce_cli.cpp) |
| `MissingOptionValue: ` | [apps/bruce_cli.cpp:67](../../apps/bruce_cli.cpp) |
| `MissingPortfolioNormalForm` | [src/application/portfolio_normalization.cpp:56](../../src/application/portfolio_normalization.cpp) |
| `MissingResult` | [src/application/exact_replay.cpp:339](../../src/application/exact_replay.cpp) |
| `MissingRouteReport` | [apps/runtime_mod_cli.hpp:184](../../apps/runtime_mod_cli.hpp) |
| `MissingRouteTemplate` | [src/tree/registry.cpp:56](../../src/tree/registry.cpp) |
| `MissingRuntimeModAction` | [apps/runtime_mod_cli.hpp:153](../../apps/runtime_mod_cli.hpp) |
| `MissingRuntimeModIdentity` | [apps/runtime_mod_cli.hpp:175](../../apps/runtime_mod_cli.hpp) |
| `MissingRuntimeModPackage` | [apps/runtime_mod_cli.hpp:156](../../apps/runtime_mod_cli.hpp) |
| `MissingSampleHeader` | [src/application/exact_research.cpp:203](../../src/application/exact_research.cpp) |
| `MissingSnapshotId` | [src/compatibility/snapshot.cpp:56](../../src/compatibility/snapshot.cpp) |
| `MissingTaskSpecAsset` | [src/token/task_spec.cpp:112](../../src/token/task_spec.cpp) |
| `MissingTaskSpecRoles` | [src/token/task_spec.cpp:160](../../src/token/task_spec.cpp) |
| `MixedCoordinateContext` | [src/compatibility/snapshot.cpp:30](../../src/compatibility/snapshot.cpp) |
| `ModManifestMismatch` | [src/mod/mod_api.cpp:114](../../src/mod/mod_api.cpp) |
| `ModRunByteLimit` | [src/mod/mod_api.cpp:87](../../src/mod/mod_api.cpp) |
| `ModRunContentDigestMismatch` | [src/mod/mod_api.cpp:83](../../src/mod/mod_api.cpp) |
| `ModRunDigestMismatch` | [src/mod/mod_api.cpp:144](../../src/mod/mod_api.cpp) |
| `ModpowRequiresBigint` | [src/application/exact_research.cpp:236](../../src/application/exact_research.cpp) |
| `ModulusRequiresModpow` | [src/application/exact_research.cpp:226](../../src/application/exact_research.cpp) |
| `ModulusRequiresModularMode` | [apps/bruce_cli.cpp:685](../../apps/bruce_cli.cpp) |
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
| `NoResearchCandidatesToRank` | [src/application/compatibility.cpp:125](../../src/application/compatibility.cpp) |
| `NonExactPolynomialDivision` | [src/math/expression_normal_form.cpp:53](../../src/math/expression_normal_form.cpp) |
| `NonIntegralDivision: quotient=` | [src/math/numbers.cpp:58](../../src/math/numbers.cpp) |
| `NonIntegralValue` | [src/math/numbers.cpp:37](../../src/math/numbers.cpp) |
| `Noncanonical NanoCell bundle` | [mods/nanocell/src/nanocell.cpp:436](../../mods/nanocell/src/nanocell.cpp) |
| `Noncanonical NanoCell number` | [mods/nanocell/src/nanocell.cpp:230](../../mods/nanocell/src/nanocell.cpp) |
| `Noncanonical NanoCell request` | [mods/nanocell/src/nanocell.cpp:320](../../mods/nanocell/src/nanocell.cpp) |
| `NoncanonicalExperienceSnapshot` | [src/experience/store.cpp:125](../../src/experience/store.cpp) |
| `NoncanonicalExpressionOperators` | [src/application/exact_replay.cpp:229](../../src/application/exact_replay.cpp) |
| `NoncanonicalLanguageDraft` | [mods/language/src/language.cpp:82](../../mods/language/src/language.cpp) |
| `NoncanonicalLanguageRun` | [mods/language/src/language_run.cpp:128](../../mods/language/src/language_run.cpp) |
| `NoncanonicalModCapabilities` | [src/mod/mod_api.cpp:105](../../src/mod/mod_api.cpp) |
| `NoncanonicalModCatalog` | [src/mod/catalog.cpp:17](../../src/mod/catalog.cpp) |
| `NoncanonicalModRun` | [src/mod/mod_api.cpp:61](../../src/mod/mod_api.cpp) |
| `NoncanonicalNanoCellModRun` | [mods/nanocell/src/mod.cpp:61](../../mods/nanocell/src/mod.cpp) |
| `NoncanonicalRuntimeModJson` | [src/mod/runtime_package.cpp:139](../../src/mod/runtime_package.cpp) |
| `NoncanonicalTaskSpec` | [src/token/task_spec.cpp:260](../../src/token/task_spec.cpp) |
| `NoncanonicalTaskSpecNumber` | [src/token/task_spec.cpp:29](../../src/token/task_spec.cpp) |
| `NoncanonicalTaskSpecV1RoleAssetBinding` | [src/token/task_spec.cpp:110](../../src/token/task_spec.cpp) |
| `NoncanonicalTaskSpecV1RoleBinding` | [src/token/task_spec.cpp:106](../../src/token/task_spec.cpp) |
| `NoncanonicalTaskSpecV1Table` | [src/token/task_spec.cpp:87](../../src/token/task_spec.cpp) |
| `NoncanonicalTaskSpecV1TokenOrder` | [src/token/task_spec.cpp:92](../../src/token/task_spec.cpp) |
| `NoncanonicalTokenizedTaskRun` | [src/application/tokenized_task_run.cpp:152](../../src/application/tokenized_task_run.cpp) |
| `OperationBudgetExceeded` | [src/application/exact_research.cpp:73](../../src/application/exact_research.cpp) |
| `OperatorsRequireExpressionStrategy` | [apps/bruce_cli.cpp:990](../../apps/bruce_cli.cpp) |
| `PolynomialInputByteLimit` | [src/application/fraction_compare.cpp:33](../../src/application/fraction_compare.cpp) |
| `PolynomialRequiresInterpolationPlanner` | [src/application/research_contract.cpp:42](../../src/application/research_contract.cpp) |
| `PortfolioNormalizationMismatch` | [src/application/portfolio_normalization.cpp:53](../../src/application/portfolio_normalization.cpp) |
| `PortfolioRequiresPortfolioPlanner` | [src/application/research_contract.cpp:41](../../src/application/research_contract.cpp) |
| `ProjectionRequiresResearchOrigin` | [src/application/compatibility.cpp:130](../../src/application/compatibility.cpp) |
| `QuadraticParameterSlotLimit` | [src/application/research_contract.cpp:90](../../src/application/research_contract.cpp) |
| `QueryBudgetExceeded` | [src/application/grid_batch.cpp:80](../../src/application/grid_batch.cpp) |
| `RationalFunctionPole` | [include/bruce/math/rational_linear.hpp:7](../../include/bruce/math/rational_linear.hpp) |
| `RationalLinearRequiresSolvePlanner` | [src/application/research_contract.cpp:40](../../src/application/research_contract.cpp) |
| `RationalPortfolioRequiresRationalDomain` | [src/application/research_contract.cpp:55](../../src/application/research_contract.cpp) |
| `ReplayBudgetRequiresVerification` | [apps/bruce_cli.cpp:396](../../apps/bruce_cli.cpp) |
| `ReplayByteBudgetExceeded: bytes=` | [src/application/execution_session.cpp:77](../../src/application/execution_session.cpp) |
| `ReplayMismatch: compatibility trace or result differs` | [src/application/compatibility.cpp:257](../../src/application/compatibility.cpp) |
| `ReplayMismatch: evidence chain differs` | [src/evidence/chain.cpp:154](../../src/evidence/chain.cpp) |
| `ReplayMismatch: exact input, result or trace differs` | [src/application/exact_replay.cpp:348](../../src/application/exact_replay.cpp) |
| `ReplayMismatch: grid batch, cache, result or trace differs` | [src/application/grid_batch.cpp:169](../../src/application/grid_batch.cpp) |
| `ReplayMismatch: grid continuation differs` | [src/application/grid_resume.cpp:83](../../src/application/grid_resume.cpp) |
| `ReplayMismatch: grid research origin or result differs` | [src/application/grid_research.cpp:221](../../src/application/grid_research.cpp) |
| `ReplayMismatch: grid task, result or trace differs` | [src/application/grid.cpp:84](../../src/application/grid.cpp) |
| `ReplayMismatch: grid work budget differs` | [src/application/grid_research.cpp:193](../../src/application/grid_research.cpp) |
| `ReplayMismatch: input, result or logical trace differs` | [src/application/fraction_compare.cpp:192](../../src/application/fraction_compare.cpp) |
| `ReplayOperationLimit` | [src/application/exact_replay.cpp:340](../../src/application/exact_replay.cpp) |
| `ResearchProjectionMismatch` | [src/application/compatibility.cpp:127](../../src/application/compatibility.cpp) |
| `ResearchReplayCheckpointsRequireCallback` | [src/application/execution_session.cpp:43](../../src/application/execution_session.cpp) |
| `ResearchReplayCheckpointsRequireResearchTask` | [src/application/execution_session.cpp:45](../../src/application/execution_session.cpp) |
| `ResultBudgetExceeded` | [src/application/exact_research.cpp:129](../../src/application/exact_research.cpp) |
| `RouteInputTooLarge` | [apps/runtime_mod_cli.hpp:60](../../apps/runtime_mod_cli.hpp) |
| `RouteRegistryDigestMismatch` | [apps/runtime_mod_cli.hpp:142](../../apps/runtime_mod_cli.hpp) |
| `RouteReportMismatch` | [apps/runtime_mod_cli.hpp:144](../../apps/runtime_mod_cli.hpp) |
| `RouteReportTooLarge` | [apps/runtime_mod_cli.hpp:73](../../apps/runtime_mod_cli.hpp) |
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
| `RuntimeModAlreadyInstalled` | [src/mod/runtime_registry.cpp:282](../../src/mod/runtime_registry.cpp) |
| `RuntimeModCommitDurabilityUnknown` | [src/mod/runtime_registry.cpp:153](../../src/mod/runtime_registry.cpp) |
| `RuntimeModCryptoFailure` | [src/mod/runtime_package.cpp:328](../../src/mod/runtime_package.cpp) |
| `RuntimeModFileOpenFailed` | [src/mod/runtime_registry.cpp:26](../../src/mod/runtime_registry.cpp) |
| `RuntimeModFileReadFailed` | [src/mod/runtime_registry.cpp:56](../../src/mod/runtime_registry.cpp) |
| `RuntimeModFileStatFailed` | [src/mod/runtime_registry.cpp:45](../../src/mod/runtime_registry.cpp) |
| `RuntimeModGenerationLimit` | [src/mod/runtime_registry.cpp:248](../../src/mod/runtime_registry.cpp) |
| `RuntimeModGoldenCaseFailed` | [src/mod/runtime_registry.cpp:220](../../src/mod/runtime_registry.cpp) |
| `RuntimeModInstalledPackageMismatch` | [src/mod/runtime_registry.cpp:212](../../src/mod/runtime_registry.cpp) |
| `RuntimeModJournalLimit` | [src/mod/runtime_registry.cpp:198](../../src/mod/runtime_registry.cpp) |
| `RuntimeModJournalMissing` | [src/mod/runtime_registry.cpp:264](../../src/mod/runtime_registry.cpp) |
| `RuntimeModJournalRecoveryFailed` | [src/mod/runtime_registry.cpp:183](../../src/mod/runtime_registry.cpp) |
| `RuntimeModJournalWriteFailed` | [src/mod/runtime_registry.cpp:203](../../src/mod/runtime_registry.cpp) |
| `RuntimeModNotActive` | [src/mod/runtime_registry.cpp:312](../../src/mod/runtime_registry.cpp) |
| `RuntimeModNotInstalled` | [src/mod/runtime_registry.cpp:300](../../src/mod/runtime_registry.cpp) |
| `RuntimeModPackageTooLarge` | [src/mod/runtime_package.cpp:275](../../src/mod/runtime_package.cpp) |
| `RuntimeModPayloadDigestMismatch` | [src/mod/runtime_package.cpp:296](../../src/mod/runtime_package.cpp) |
| `RuntimeModRecordLimit` | [src/mod/runtime_registry.cpp:133](../../src/mod/runtime_registry.cpp) |
| `RuntimeModRegistryJournalMismatch` | [src/mod/runtime_registry.cpp:187](../../src/mod/runtime_registry.cpp) |
| `RuntimeModRevoked` | [src/mod/runtime_registry.cpp:301](../../src/mod/runtime_registry.cpp) |
| `RuntimeModRootCreateFailed` | [src/mod/runtime_registry.cpp:38](../../src/mod/runtime_registry.cpp) |
| `RuntimeModRootRequired` | [src/mod/runtime_registry.cpp:254](../../src/mod/runtime_registry.cpp) |
| `RuntimeModSignatureInvalid` | [src/mod/runtime_package.cpp:329](../../src/mod/runtime_package.cpp) |
| `RuntimeModSigningKeyUntrusted` | [src/mod/runtime_package.cpp:323](../../src/mod/runtime_package.cpp) |
| `RuntimeModStoreByteLimit` | [src/mod/runtime_registry.cpp:58](../../src/mod/runtime_registry.cpp) |
| `RuntimeModStoreLockFailed` | [src/mod/runtime_registry.cpp:67](../../src/mod/runtime_registry.cpp) |
| `RuntimeModTrustStoreChanged` | [src/mod/runtime_registry.cpp:76](../../src/mod/runtime_registry.cpp) |
| `RuntimeModTrustStoreMissing` | [src/mod/runtime_registry.cpp:259](../../src/mod/runtime_registry.cpp) |
| `SHA-256 requires Web Crypto in a secure browser context.` | [mods/cockpit/src/cockpit_assets.cpp:243](../../mods/cockpit/src/cockpit_assets.cpp) |
| `SampleBudgetExceeded` | [src/application/exact_replay.cpp:152](../../src/application/exact_replay.cpp) |
| `SampleByteLimit` | [src/application/research_contract.cpp:86](../../src/application/research_contract.cpp) |
| `SerializationBudgetRequiresVerification` | [apps/bruce_cli.cpp:400](../../apps/bruce_cli.cpp) |
| `SerializationByteBudgetExceeded: bytes=` | [src/application/execution_session.cpp:73](../../src/application/execution_session.cpp) |
| `SerializationCancellationRequiresReplay` | [apps/bruce_cli.cpp:380](../../apps/bruce_cli.cpp) |
| `SerializationCheckpointsRequireCallback` | [src/application/execution_session.cpp:54](../../src/application/execution_session.cpp) |
| `SerializationRecordsRequireBatchOrResume` | [src/application/application_task.cpp:104](../../src/application/application_task.cpp) |
| `SerializationRecordsRequireBatchResumeOrResearch` | [src/application/execution_session.cpp:53](../../src/application/execution_session.cpp) |
| `SessionBudgetNotEnabled` | [src/application/session_budget.cpp:18](../../src/application/session_budget.cpp) |
| `SessionBudgetRequiresVerification` | [apps/bruce_cli.cpp:405](../../apps/bruce_cli.cpp) |
| `SessionCheckpointBudgetExceeded: phase=` | [src/application/session_budget.cpp:13](../../src/application/session_budget.cpp) |
| `SessionCheckpointOutsideWorkPhase` | [src/application/session_budget.cpp:10](../../src/application/session_budget.cpp) |
| `SnapshotByteLimit` | [src/compatibility/snapshot.cpp:51](../../src/compatibility/snapshot.cpp) |
| `StrategyRequiresExactDomain` | [apps/bruce_cli.cpp:998](../../apps/bruce_cli.cpp) |
| `StrategyResultConflict` | [src/application/portfolio_normalization.cpp:101](../../src/application/portfolio_normalization.cpp) |
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
| `TotalBudgetRequiresGridTraining` | [apps/bruce_cli.cpp:982](../../apps/bruce_cli.cpp) |
| `TraceBudgetExceeded` | [src/application/exact_research.cpp:60](../../src/application/exact_research.cpp) |
| `TraceByteBudgetExceeded` | [src/application/exact_research.cpp:62](../../src/application/exact_research.cpp) |
| `Trailing NanoCell bytes` | [mods/nanocell/src/nanocell.cpp:262](../../mods/nanocell/src/nanocell.cpp) |
| `TrailingExperienceEventData` | [src/experience/store.cpp:91](../../src/experience/store.cpp) |
| `TransferCancelled` | [include/bruce/application/transfer_control.hpp:13](../../include/bruce/application/transfer_control.hpp) |
| `TransferCheckpointBudgetExceeded: checkpoints=` | [include/bruce/application/transfer_control.hpp:14](../../include/bruce/application/transfer_control.hpp) |
| `TransferCheckpointCounterOverflow` | [include/bruce/application/transfer_control.hpp:15](../../include/bruce/application/transfer_control.hpp) |
| `TreeMountNotFound` | [src/mod/runtime_registry.cpp:239](../../src/mod/runtime_registry.cpp) |
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
| `UnexpectedRuntimeModMember` | [src/mod/runtime_package.cpp:292](../../src/mod/runtime_package.cpp) |
| `Unknown NanoCell byte opcode` | [mods/nanocell/src/nanocell.cpp:43](../../mods/nanocell/src/nanocell.cpp) |
| `Unknown NanoCell encoded opcode` | [mods/nanocell/src/nanocell.cpp:311](../../mods/nanocell/src/nanocell.cpp) |
| `Unknown NanoCell reference technique` | [mods/nanocell/src/nanocell.cpp:71](../../mods/nanocell/src/nanocell.cpp) |
| `Unknown NanoCell technique id` | [mods/nanocell/src/nanocell.cpp:160](../../mods/nanocell/src/nanocell.cpp) |
| `UnknownBundleVersion` | [src/application/compatibility.cpp:233](../../src/application/compatibility.cpp) |
| `UnknownCommand: ` | [apps/bruce_cli.cpp:1212](../../apps/bruce_cli.cpp) |
| `UnknownCompatibilityVersion` | [src/application/compatibility.cpp:240](../../src/application/compatibility.cpp) |
| `UnknownDialogueAction: ` | [apps/bruce_cli.cpp:942](../../apps/bruce_cli.cpp) |
| `UnknownDomainCache` | [src/application/research_contract.cpp:60](../../src/application/research_contract.cpp) |
| `UnknownDomainCachePolicy` | [src/application/exact_replay.cpp:253](../../src/application/exact_replay.cpp) |
| `UnknownEquivalenceVersion` | [src/application/exact_replay.cpp:248](../../src/application/exact_replay.cpp) |
| `UnknownEvidenceAction: ` | [apps/bruce_cli.cpp:978](../../apps/bruce_cli.cpp) |
| `UnknownEvidenceChainPolicy` | [src/evidence/chain.cpp:145](../../src/evidence/chain.cpp) |
| `UnknownExactOperation` | [src/application/exact_research.cpp:244](../../src/application/exact_research.cpp) |
| `UnknownExactVersion` | [src/application/exact_replay.cpp:220](../../src/application/exact_replay.cpp) |
| `UnknownExecutionCancellation` | [apps/bruce_cli.cpp:367](../../apps/bruce_cli.cpp) |
| `UnknownExecutionPhase` | [src/application/execution_session.cpp:7](../../src/application/execution_session.cpp) |
| `UnknownExecutionVerification` | [apps/bruce_cli.cpp:364](../../apps/bruce_cli.cpp) |
| `UnknownExperienceAction` | [apps/bruce_cli.cpp:1174](../../apps/bruce_cli.cpp) |
| `UnknownExperienceEvent` | [src/experience/store.cpp:90](../../src/experience/store.cpp) |
| `UnknownExperiencePolicy` | [src/experience/store.cpp:116](../../src/experience/store.cpp) |
| `UnknownExperienceVersion` | [src/experience/store.cpp:109](../../src/experience/store.cpp) |
| `UnknownExpressionCachePolicy` | [src/application/exact_replay.cpp:230](../../src/application/exact_replay.cpp) |
| `UnknownExpressionEquivalence` | [src/application/research_contract.cpp:66](../../src/application/research_contract.cpp) |
| `UnknownExpressionExponentPolicy` | [src/application/exact_replay.cpp:247](../../src/application/exact_replay.cpp) |
| `UnknownExpressionPlanner` | [src/application/research_contract.cpp:64](../../src/application/research_contract.cpp) |
| `UnknownExpressionPlannerVersion` | [src/application/exact_replay.cpp:226](../../src/application/exact_replay.cpp) |
| `UnknownGateCancellation` | [apps/bruce_cli.cpp:1184](../../apps/bruce_cli.cpp) |
| `UnknownGridExecutionPolicy` | [src/application/grid_batch.cpp:154](../../src/application/grid_batch.cpp) |
| `UnknownGridResearchAxis` | [src/application/grid_research.cpp:27](../../src/application/grid_research.cpp) |
| `UnknownGridResearchVersion` | [src/application/grid_research.cpp:205](../../src/application/grid_research.cpp) |
| `UnknownGridResumePolicy` | [src/application/grid_resume.cpp:72](../../src/application/grid_resume.cpp) |
| `UnknownGridSpecOrPolicy` | [src/application/grid_batch.cpp:151](../../src/application/grid_batch.cpp) |
| `UnknownGridWorkPolicy` | [src/application/grid_research.cpp:180](../../src/application/grid_research.cpp) |
| `UnknownLanguageAction: ` | [apps/bruce_cli.cpp:903](../../apps/bruce_cli.cpp) |
| `UnknownLanguageAlternative` | [apps/bruce_cli.cpp:127](../../apps/bruce_cli.cpp) |
| `UnknownManagementCancellation` | [apps/management_inspection_cli.hpp:47](../../apps/management_inspection_cli.hpp) |
| `UnknownNanoCellCommand: ` | [apps/nanocell_cli.hpp:133](../../apps/nanocell_cli.hpp) |
| `UnknownNanoCellTechnique` | [apps/nanocell_cli.hpp:62](../../apps/nanocell_cli.hpp) |
| `UnknownNumberDomain` | [src/math/numbers.cpp:8](../../src/math/numbers.cpp) |
| `UnknownOption: ` | [apps/bruce_cli.cpp:66](../../apps/bruce_cli.cpp) |
| `UnknownOrRevokedExperience` | [src/experience/store.cpp:52](../../src/experience/store.cpp) |
| `UnknownPlannerVersion` | [src/application/exact_replay.cpp:223](../../src/application/exact_replay.cpp) |
| `UnknownResearchGateReason` | [src/application/research_gates.cpp:19](../../src/application/research_gates.cpp) |
| `UnknownResearchStrategy` | [src/application/research_contract.cpp:30](../../src/application/research_contract.cpp) |
| `UnknownRuleField` | [src/compatibility/snapshot.cpp:69](../../src/compatibility/snapshot.cpp) |
| `UnknownRuleKind` | [src/compatibility/snapshot.cpp:68](../../src/compatibility/snapshot.cpp) |
| `UnknownRuleOperator` | [src/compatibility/snapshot.cpp:70](../../src/compatibility/snapshot.cpp) |
| `UnknownRuleOrEngineVersion` | [src/application/fraction_compare.cpp:176](../../src/application/fraction_compare.cpp) |
| `UnknownRuleVersion` | [src/compatibility/snapshot.cpp:55](../../src/compatibility/snapshot.cpp) |
| `UnknownRuntimeModAction` | [apps/runtime_mod_cli.hpp:181](../../apps/runtime_mod_cli.hpp) |
| `UnknownSearchPlanner` | [src/application/research_contract.cpp:35](../../src/application/research_contract.cpp) |
| `UnknownSerializationCancellation` | [apps/bruce_cli.cpp:379](../../apps/bruce_cli.cpp) |
| `UnknownTaskSpecAction: ` | [apps/bruce_cli.cpp:785](../../apps/bruce_cli.cpp) |
| `UnknownTaskSpecAlternative` | [src/token/task_spec.cpp:173](../../src/token/task_spec.cpp) |
| `UnknownTaskSpecToken` | [src/token/task_spec.cpp:103](../../src/token/task_spec.cpp) |
| `UnknownTransferCancellation` | [apps/bruce_cli.cpp:84](../../apps/bruce_cli.cpp) |
| `UnknownVerificationCancellation` | [apps/bruce_cli.cpp:377](../../apps/bruce_cli.cpp) |
| `UnsafeRuntimeModFilePermissions` | [src/mod/runtime_registry.cpp:35](../../src/mod/runtime_registry.cpp) |
| `UnselectedLanguageAlternative` | [mods/language/src/language.cpp:643](../../mods/language/src/language.cpp) |
| `UnselectedTaskSpecAlternative` | [src/token/task_spec.cpp:170](../../src/token/task_spec.cpp) |
| `UnsupportedDialogueText` | [apps/bruce_cli.cpp:915](../../apps/bruce_cli.cpp) |
| `UnsupportedEvidenceChainDomain` | [apps/bruce_cli.cpp:952](../../apps/bruce_cli.cpp) |
| `UnsupportedEvidenceChainRoute` | [src/evidence/chain.cpp:15](../../src/evidence/chain.cpp) |
| `UnsupportedLanguageDraft` | [mods/language/src/language.cpp:370](../../mods/language/src/language.cpp) |
| `UnsupportedLanguageGrammar` | [apps/bruce_cli.cpp:849](../../apps/bruce_cli.cpp) |
| `UnsupportedLanguageText` | [apps/bruce_cli.cpp:808](../../apps/bruce_cli.cpp) |
| `UnsupportedRouteNormalization` | [src/tree/registry.cpp:80](../../src/tree/registry.cpp) |
| `UnsupportedRuleSource` | [src/compatibility/snapshot.cpp:71](../../src/compatibility/snapshot.cpp) |
| `UnsupportedRuntimeModCapability` | [src/mod/runtime_package.cpp:229](../../src/mod/runtime_package.cpp) |
| `UnsupportedRuntimeModVersion` | [src/mod/runtime_package.cpp:220](../../src/mod/runtime_package.cpp) |
| `UnsupportedTaskSpecNormalization` | [src/token/task_spec.cpp:133](../../src/token/task_spec.cpp) |
| `UnsupportedTaskSpecRoute` | [apps/bruce_cli.cpp:739](../../apps/bruce_cli.cpp) |
| `UnsupportedTaskSpecVersion` | [src/token/task_spec.cpp:132](../../src/token/task_spec.cpp) |
| `UnsupportedTokenTableVersion` | [apps/bruce_cli.cpp:740](../../apps/bruce_cli.cpp) |
| `UnusedRuleArgumentMustBeZero` | [src/compatibility/snapshot.cpp:76](../../src/compatibility/snapshot.cpp) |
| `Usage: bruce assess-run RUN.bruce` | [apps/bruce_cli.cpp:1204](../../apps/bruce_cli.cpp) |
| `Usage: bruce contracts` | [apps/bruce_cli.cpp:721](../../apps/bruce_cli.cpp) |
| `Usage: bruce dialogue confirm DRAFT --output DIALOGUE_RUN` | [apps/bruce_cli.cpp:926](../../apps/bruce_cli.cpp) |
| `Usage: bruce dialogue interpret&#124;select&#124;confirm&#124;replay` | [apps/bruce_cli.cpp:910](../../apps/bruce_cli.cpp) |
| `Usage: bruce dialogue replay DIALOGUE_DRAFT` | [apps/bruce_cli.cpp:933](../../apps/bruce_cli.cpp) |
| `Usage: bruce dialogue select DRAFT --alternative ID --output SELECTED_DRAFT` | [apps/bruce_cli.cpp:919](../../apps/bruce_cli.cpp) |
| `Usage: bruce dispatch inspect&#124;replay BUNDLE` | [apps/bruce_cli.cpp:620](../../apps/bruce_cli.cpp) |
| `Usage: bruce evidence create&#124;replay` | [apps/bruce_cli.cpp:946](../../apps/bruce_cli.cpp) |
| `Usage: bruce evidence replay CHAIN` | [apps/bruce_cli.cpp:975](../../apps/bruce_cli.cpp) |
| `Usage: bruce experience inspect SNAPSHOT` | [apps/bruce_cli.cpp:1130](../../apps/bruce_cli.cpp) |
| `Usage: bruce gate-contracts` | [apps/bruce_cli.cpp:718](../../apps/bruce_cli.cpp) |
| `Usage: bruce gate-run RUN.bruce [--max-session-checkpoints N] [--max-replay-bytes N] [--cancellation none&#124;checkpoints]` | [apps/bruce_cli.cpp:1176](../../apps/bruce_cli.cpp) |
| `Usage: bruce inspect-run RUN.bruce` | [apps/bruce_cli.cpp:1207](../../apps/bruce_cli.cpp) |
| `Usage: bruce language execute DRAFT --output LANGUAGE_RUN` | [apps/bruce_cli.cpp:871](../../apps/bruce_cli.cpp) |
| `Usage: bruce language interpret&#124;select&#124;execute&#124;replay` | [apps/bruce_cli.cpp:788](../../apps/bruce_cli.cpp) |
| `Usage: bruce language replay LANGUAGE_RUN` | [apps/bruce_cli.cpp:891](../../apps/bruce_cli.cpp) |
| `Usage: bruce language select DRAFT --alternative ID --output SELECTED_DRAFT` | [apps/bruce_cli.cpp:851](../../apps/bruce_cli.cpp) |
| `Usage: bruce nanocell replay FILE` | [apps/nanocell_cli.hpp:114](../../apps/nanocell_cli.hpp) |
| `Usage: bruce nanocell run&#124;replay ...` | [apps/nanocell_cli.hpp:111](../../apps/nanocell_cli.hpp) |
| `Usage: bruce phase-dag inspect BUNDLE` | [apps/bruce_cli.cpp:638](../../apps/bruce_cli.cpp) |
| `Usage: bruce replay RUN.bruce` | [apps/bruce_cli.cpp:1210](../../apps/bruce_cli.cpp) |
| `Usage: bruce task-contracts` | [apps/bruce_cli.cpp:715](../../apps/bruce_cli.cpp) |
| `Usage: bruce task-spec create&#124;inspect&#124;execute&#124;replay` | [apps/bruce_cli.cpp:728](../../apps/bruce_cli.cpp) |
| `Usage: bruce task-spec execute SPEC --output RUN` | [apps/bruce_cli.cpp:774](../../apps/bruce_cli.cpp) |
| `Usage: bruce task-spec inspect SPEC` | [apps/bruce_cli.cpp:770](../../apps/bruce_cli.cpp) |
| `Usage: bruce task-spec replay RUN` | [apps/bruce_cli.cpp:783](../../apps/bruce_cli.cpp) |
| `Usage: compatibility-apply PLAN --from-run RUN --rules RULES [--output RUN]` | [apps/bruce_cli.cpp:1051](../../apps/bruce_cli.cpp) |
| `Usage: experience apply-add PLAN --from-run RUN [--store STORE] --output NEW` | [apps/bruce_cli.cpp:1102](../../apps/bruce_cli.cpp) |
| `Usage: rules-activate STORE PROPOSAL` | [apps/rule_store.hpp:30](../../apps/rule_store.hpp) |
| `Usage: rules-propose OLD NEW PROPOSAL &#124; rules-approve PROPOSAL CURRENT OUTPUT` | [apps/rule_review.hpp:13](../../apps/rule_review.hpp) |
| `Usage: rules-stage STORE NEW PROPOSAL` | [apps/rule_store.hpp:25](../../apps/rule_store.hpp) |
| `Usage: rules-status STORE` | [apps/rule_store.hpp:16](../../apps/rule_store.hpp) |
| `UseCandidatesOrResearchRun` | [apps/bruce_cli.cpp:1081](../../apps/bruce_cli.cpp) |
| `UseEitherZOrXY` | [apps/bruce_cli.cpp:671](../../apps/bruce_cli.cpp) |
| `UseGridOrCsvSamples` | [apps/bruce_cli.cpp:985](../../apps/bruce_cli.cpp) |
| `UseGridOrExplicitSamples` | [src/application/grid_research.cpp:127](../../src/application/grid_research.cpp) |
| `UseRulesOrRuleStore` | [apps/bruce_cli.cpp:1057](../../apps/bruce_cli.cpp) |
| `VerificationCancellationRequiresReplay` | [apps/bruce_cli.cpp:385](../../apps/bruce_cli.cpp) |
| `VerificationCancelled` | [src/application/execution_session.cpp:28](../../src/application/execution_session.cpp) |
| `VersionTakesNoArguments` | [apps/bruce_cli.cpp:652](../../apps/bruce_cli.cpp) |
| `ZeroDenominator` | [src/math/expression.cpp:60](../../src/math/expression.cpp) |
| `ZeroNormalFormDenominator` | [src/math/expression_normal_form.cpp:74](../../src/math/expression_normal_form.cpp) |
| `ZeroPolynomialDivisor` | [src/math/expression_normal_form.cpp:37](../../src/math/expression_normal_form.cpp) |
