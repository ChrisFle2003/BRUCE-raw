import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())
ROOT = Path(__file__).resolve().parents[2]
IGNORE = shutil.ignore_patterns('.git', '.agents', '.codex', '.worktrees', 'build*', 'runs', '.env', '.env.*',
                              '__pycache__', '*.pyc')


class CliDocumentationTests(unittest.TestCase):
    def test_artifact_store_v1_documentation_matches_implemented_boundary(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        heading = '## `artifact-store-v1`'
        self.assertIn(heading, manual, 'Artifact Store V1 manual section is missing')
        section = manual.partition(heading)[2].split('\n## ', 1)[0]
        for contract in (
            'IMPLEMENTIERT UND BELEGT',
            'bruce artifact-store-v1 import FILE --store STORE',
            'bruce artifact-store-v1 inspect DIGEST --store STORE',
            'bruce artifact-store-v1 verify DIGEST --store STORE',
            'bruce artifact-store-v1 resolve MANIFEST_ID --store STORE',
            '32 MiB', '33.554.432 Bytes', 'content-addressed',
            'metadata-only', 'sha256', 'size_bytes', 'PROTOCOL_ERROR',
            'Exit 0', 'Exit 2', 'O_NOFOLLOW', 'fsync', 'idempotent',
            'BRUCE_ARTIFACT_MANIFEST_V1', 'Payloadbytes', 'keine Verschlüsselung',
            'Payload-Download', 'Garbage-Collection', 'Netzwerktransport',
            'DOKUMENTIERT / KONZEPT',
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, section)

    def test_artifact_store_v1_documentation_is_cross_referenced(self):
        references = {
            'docs/cli/formats.md': (
                '## Artifact Store V1', '32 MiB', '33.554.432 Bytes',
                'BRUCE_ARTIFACT_MANIFEST_V1', 'metadata-only',
                'PROTOCOL_ERROR', 'O_NOFOLLOW', 'DOKUMENTIERT / KONZEPT',
            ),
            'docs/implementation/README.md': (
                '## Artifact Store V1', 'artifact-store-v1 import FILE',
                'bruce::max_file_bytes', 'BRUCE_ARTIFACT_MANIFEST_V1',
                'metadata-only', 'PROTOCOL_ERROR', 'DOKUMENTIERT / KONZEPT',
            ),
            'docs/architecture/03-blueprint-consolidation.md': (
                '## Artifact Store V1', 'artifact-store-v1 verify DIGEST',
                '33,554,432 bytes', 'O_NOFOLLOW', 'metadata',
                'Payload download', 'DOKUMENTIERT / KONZEPT',
            ),
        }
        for filename, contracts in references.items():
            text = (ROOT / filename).read_text()
            for contract in contracts:
                with self.subTest(reference=filename, contract=contract):
                    self.assertIn(contract, text)

    def test_artifact_store_resolve_v1_documentation_matches_implemented_boundary(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        heading = '## `artifact-store-v1`'
        self.assertIn(heading, manual)
        section = manual.partition(heading)[2].split('\n## ', 1)[0]
        for contract in (
            'IMPLEMENTIERT UND BELEGT',
            'Kontrollierte Manifestauflösung V1.2',
            'bruce artifact-store-v1 resolve MANIFEST_ID --store STORE',
            'artifact-store-resolve-v1', 'status":"RESOLVED"',
            'manifest_status', 'result_code', 'objects', 'sha256',
            'size_bytes', 'origin', 'media_type', 'objects":[]',
            'Manifestreihenfolge', 'vollständig', 'read-only',
            'Payloadbytes', 'private Pfade', 'PROTOCOL_ERROR', 'Exit 2',
            'kein neues Zeit-, CPU-, Speicher- oder I/O-Diagnosebudget',
            'Resolve-Workflow',
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, section)

    def test_artifact_store_resolve_v1_contract_is_cross_referenced(self):
        references = {
            'docs/cli/formats.md': (
                'bruce artifact-store-v1 resolve MANIFEST_ID --store STORE',
                'artifact-store-resolve-v1', 'manifest_status',
                'leeres `origin`', 'objects:[]', 'PROTOCOL_ERROR',
                'keine Payload', 'kein neues Diagnosebudget',
                'test_artifact_store_resolve_v1.py',
            ),
            'docs/implementation/README.md': (
                'Artifact Store V1.2 Resolve',
                'bruce artifact-store-v1 resolve MANIFEST_ID --store STORE',
                'artifact-store-resolve-v1', 'manifest_status',
                'objects:[]', 'Payloadausgabe', 'PROTOCOL_ERROR',
                'Resolve-Workflow',
            ),
            'docs/architecture/03-blueprint-consolidation.md': (
                'Artifact Store V1.2 Resolve',
                'bruce artifact-store-v1 resolve MANIFEST_ID --store STORE',
                'artifact-store-resolve-v1', 'manifest_status',
                'objects:[]', 'PROTOCOL_ERROR', 'no new diagnostic',
                'resolve filesystem workflows',
            ),
        }
        for filename, contracts in references.items():
            text = (ROOT / filename).read_text()
            for contract in contracts:
                with self.subTest(reference=filename, contract=contract):
                    self.assertIn(contract, text)

    def test_dag_scheduler_v5_inspector_manual_matches_read_only_contract(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        heading = '## `dag-scheduler-v5`'
        self.assertIn(heading, manual, 'V5 manual section is missing')
        section = manual.partition(heading)[2].split('\n## ', 1)[0]
        for contract in (
            'IMPLEMENTIERT UND BELEGT',
            'bruce dag-scheduler-v5 execute SPEC --output BUNDLE',
            'bruce dag-scheduler-v5 inspect BUNDLE',
            'BUNDLE.run/', 'lexikalisch normalisiert', 'read-only',
            'dag-scheduler-v5-inspect-v1', 'RUN_FINISHED', 'VALID',
            'INCOMPLETE', 'PROTOCOL_ERROR', 'Exit 0', 'Exit 1', 'Exit 2',
            'keinen Worker', 'keinen Adapter', 'keine Datei',
            'Payloadbytes', 'absolute Pfade', 'attempts/...',
            'SHA-256', '64 Verschachtelungsebenen', 'Retry', 'Workerzahl',
            'Budgetobergrenzen', 'Attempt-Historie', 'Usage',
            'kein Replay', 'kein Artifact Store', 'Recovery-Executor',
            'test_dag_scheduler_v5_inspector.py', 'worker_run_inspector_v1.cpp',
            'DOKUMENTIERT / KONZEPT',
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, section)

    def test_dag_scheduler_v5_inspector_contract_is_cross_referenced(self):
        references = {
            'docs/cli/formats.md': (
                'bruce dag-scheduler-v5 inspect BUNDLE',
                'dag-scheduler-v5-inspect-v1', 'BUNDLE.run/',
                'RUN_FINISHED', 'INCOMPLETE', 'PROTOCOL_ERROR',
                'O_NOFOLLOW', '64 Ebenen', 'keinen Replay',
            ),
            'docs/architecture/03-blueprint-consolidation.md': (
                'bruce dag-scheduler-v5 inspect BUNDLE',
                'dag-scheduler-v5-inspect-v1', 'WorkerRunInspectorV1',
                'O_NOFOLLOW', 'no replay', 'DOKUMENTIERT / KONZEPT',
            ),
            'docs/implementation/README.md': (
                'bruce dag-scheduler-v5 inspect BUNDLE',
                'dag-scheduler-v5-inspect-v1', 'PROTOCOL_ERROR',
                'worker_run_inspector_v1.cpp', 'DOKUMENTIERT / KONZEPT',
            ),
        }
        for filename, contracts in references.items():
            text = (ROOT / filename).read_text()
            for contract in contracts:
                with self.subTest(reference=filename, contract=contract):
                    self.assertIn(contract, text)

    def test_dag_scheduler_v5_publish_manual_matches_implemented_boundary(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        heading = '## `dag-scheduler-v5`'
        section = manual.partition(heading)[2].split('\n## ', 1)[0]
        for contract in (
            'V5-Artifact-Publish',
            'IMPLEMENTIERT UND BELEGT',
            'bruce dag-scheduler-v5 publish BUNDLE --store STORE',
            'Inspector-first', 'RUN_FINISHED', 'VALID', 'READY',
            'ARTIFACT_ACCEPTED', 'dag-scheduler-v5-publish-v1',
            'contract', 'action', 'status', 'run_id', 'manifests',
            'node_id', 'attempt_id', 'reference', 'manifest_id', 'sha256',
            'size_bytes', 'BRUCE_ARTIFACT_MANIFEST_V1', 'artifact/<node>',
            'idempotent', 'nicht überschrieben', 'Teilpublikation',
            'keinen Worker', 'keinen Adapter', 'kein Replay',
            'keine GC-Route', 'PROTOCOL_ERROR', 'Exit 0', 'Exit 2',
            'test_dag_scheduler_v5_publish.py',
            'worker-artifact-publish-v1-design.md',
            'DOKUMENTIERT / KONZEPT',
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, section)

    def test_dag_scheduler_v5_publish_contract_is_cross_referenced(self):
        references = {
            'docs/cli/formats.md': (
                'V5-Artifact-Publish-Bericht',
                'bruce dag-scheduler-v5 publish BUNDLE --store STORE',
                'dag-scheduler-v5-publish-v1', 'ARTIFACT_ACCEPTED',
                'BRUCE_ARTIFACT_MANIFEST_V1', 'manifest_id',
                'nicht transaktional', 'Garbage-Collection',
            ),
            'docs/implementation/README.md': (
                '## V5 Artifact Publish V1',
                'dag-scheduler-v5 publish BUNDLE --store STORE',
                'dag-scheduler-v5-publish-v1', 'ARTIFACT_ACCEPTED',
                'BRUCE_ARTIFACT_MANIFEST_V1', 'unreferenced object',
                'DOKUMENTIERT / KONZEPT',
            ),
            'docs/architecture/03-blueprint-consolidation.md': (
                '## V5 Artifact Publish V1',
                'dag-scheduler-v5 publish BUNDLE --store STORE',
                'dag-scheduler-v5-publish-v1', 'ARTIFACT_ACCEPTED',
                'BRUCE_ARTIFACT_MANIFEST_V1', 'not automatic Execute integration',
                'DOKUMENTIERT / KONZEPT',
            ),
        }
        for filename, contracts in references.items():
            text = (ROOT / filename).read_text()
            for contract in contracts:
                with self.subTest(reference=filename, contract=contract):
                    self.assertIn(contract, text)

    def test_dag_scheduler_v5_payload_manual_matches_implemented_boundary(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        section = manual.partition('## `dag-scheduler-v5`')[2].split('\n## ', 1)[0]
        for contract in (
            'V5-Payload-Flow V1.3',
            'IMPLEMENTIERT UND BELEGT',
            'bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE',
            'dag-scheduler-v5-payload-v13', 'action="payload"',
            'ArtifactInputRefV13', 'manifest_id', 'object_index', 'sha256',
            'size_bytes', 'status=READY', 'direkten DAG-Kante', 'origin',
            'O_NOFOLLOW', 'max-wall-ms', 'max-input-bytes', 'max-output-bytes',
            'max-operations', 'max-memory-bytes', 'workers', '30000', '600000',
            '1048576', '5242880', '10000000', '2147483648', '15',
            '16 Gesamtinputs', 'FAILED', 'BLOCKED', 'TIMEOUT', 'attempt_id',
            'Payloadbytes', 'private Pfade', 'Exit 0', 'Exit 1', 'Exit 2',
            'Payload-Workflow', 'DOKUMENTIERT / KONZEPT',
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, section)

    def test_dag_scheduler_v5_payload_contract_is_cross_referenced(self):
        references = {
            'docs/cli/formats.md': (
                '## DAG-Scheduler V5 Payload Flow V1.3',
                'dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE',
                'dag-scheduler-v5-payload-v13', 'ArtifactInputRefV13',
                'status=READY', 'O_NOFOLLOW', 'End-to-End-Deadline',
                '15', '16 Gesamtinputs', 'FAILED', 'BLOCKED',
                'test_dag_scheduler_v5_payload.py', 'DOKUMENTIERT / KONZEPT',
            ),
            'docs/implementation/README.md': (
                '## V5 Artifact Payload Flow V1.3',
                'dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE',
                'ArtifactInputRefV13', 'READY', '15 direct payload objects',
                'max_wall_ms', 'dag-scheduler-v5-payload-v13',
                'artifact_payload_v1.cpp', 'test_dag_scheduler_v5_payload.py',
                'DOKUMENTIERT / KONZEPT',
            ),
            'docs/architecture/03-blueprint-consolidation.md': (
                '## DAG-Scheduler V5 Artifact Payload Flow V1.3',
                'dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE',
                'ArtifactInputRefV13', 'O_NOFOLLOW', '15 direct payload objects',
                'FAILED', 'BLOCKED', 'test_dag_scheduler_v5_payload.py',
                'DOKUMENTIERT / KONZEPT',
            ),
        }
        for filename, contracts in references.items():
            text = (ROOT / filename).read_text()
            for contract in contracts:
                with self.subTest(reference=filename, contract=contract):
                    self.assertIn(contract, text)

    def test_dag_scheduler_v5_payload_v14_manual_matches_external_input_boundary(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        section = manual.partition('### V5-Payload-Flow V1.4')[2].split('\n## ', 1)[0]
        for contract in (
            'IMPLEMENTIERT UND BELEGT',
            '--external-inputs INPUTS',
            'bruce-dag-scheduler-v5-external-inputs-v14',
            'dag-scheduler-v5-payload-v13', 'dag-scheduler-v5-payload-v14',
            'manifest_id', 'object_index', 'sha256', 'size_bytes',
            'source_run_id', 'source_node_id', 'source_attempt_id',
            'external_inputs_digest', 'external_input_count',
            'external_input_bytes', 'READY', '15 Payloadobjekte',
            '16 Gesamtinputs', 'EXTERNAL_MANIFEST_INVALID', 'FAILED',
            'BLOCKED', 'Exit 1', 'Exit 2', '--retry N',
            'DOKUMENTIERT / KONZEPT',
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, section)

    def test_dag_scheduler_v5_payload_v14_contract_is_cross_referenced(self):
        references = {
            'docs/cli/formats.md': (
                '## DAG-Scheduler V5 Payload Flow V1.4',
                '--external-inputs INPUTS',
                'bruce-dag-scheduler-v5-external-inputs-v14',
                'dag-scheduler-v5-payload-v13', 'dag-scheduler-v5-payload-v14',
                'external_inputs_digest', 'external_input_count',
                'external_input_bytes', '15 Payloadobjekte', 'Exit 1',
                'Exit 2', 'V1.4-Spezifikation',
            ),
            'docs/implementation/README.md': (
                '## V5 Artifact Payload Flow V1.4: external local inputs',
                '--external-inputs INPUTS',
                'bruce-dag-scheduler-v5-external-inputs-v14',
                'external_inputs_digest', 'external_input_count',
                'external_input_bytes', '15 payload objects', 'exit 1',
                'exit 2', 'V1.4 design',
            ),
            'docs/architecture/03-blueprint-consolidation.md': (
                '## DAG-Scheduler V5 Artifact Payload Flow V1.4',
                '--external-inputs INPUTS',
                'bruce-dag-scheduler-v5-external-inputs-v14',
                'source_run_id', 'external_inputs_digest',
                'external_input_count', 'external_input_bytes',
                '15\npayload objects', 'Exit 1', 'Exit 2', 'V1.4 design',
            ),
        }
        for filename, contracts in references.items():
            text = (ROOT / filename).read_text()
            for contract in contracts:
                with self.subTest(reference=filename, contract=contract):
                    self.assertIn(contract, text)

    def test_payload_stream_v15_documentation_preserves_worker_and_report_boundary(self):
        sections = {
            'docs/cli/commands.md': '### V5-Payload-Stream V1.5',
            'docs/cli/formats.md': '## DAG-Scheduler V5 Payload Stream V1.5',
            'docs/implementation/README.md': '## V5 Artifact Payload Stream V1.5',
            'docs/architecture/03-blueprint-consolidation.md':
                '## DAG-Scheduler V5 Artifact Payload Stream V1.5',
        }
        for filename, heading in sections.items():
            document = (ROOT / filename).read_text()
            self.assertIn(heading, document)
            section = document.partition(heading)[2].split('\n## ', 1)[0]
            for contract in (
                'IMPLEMENTIERT UND BELEGT',
                'bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE --stream-v15',
                'payload-only', '65,536', 'header.jsonl', 'seal.json',
                'chunk', 'owner-only', '0700', '0600', 'O_NOFOLLOW',
                'SHA-256', 'input.bin', 'WorkerRequest', 'worker-v1',
                'V1.3', 'V1.4', 'metadata-only', 'ATTEMPT_STARTED',
                'DOKUMENTIERT / KONZEPT',
            ):
                with self.subTest(reference=filename, contract=contract):
                    self.assertIn(contract, section)
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        section = manual.partition('### V5-Payload-Stream V1.5')[2].split('\n## ', 1)[0]
        for contract in (
            'wertlos', 'genau einmal', 'keine Streamfelder',
            'WorkerRequest bindet diese reguläre Datei',
            'letzte Deadlineprüfung', 'gesamten aktuellen Attempt-Inputbereich',
            'Payloadbytes', 'Hexdarstellungen', 'private',
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, section)

    def test_payload_run_inspector_manual_explains_evidence_and_exit_contract(self):
        document = (ROOT / 'docs/cli/commands.md').read_text()
        heading = '### V5-Payload-Run-Inspector V1'
        self.assertIn(heading, document)
        section = document.partition(heading)[2].split('\n## ', 1)[0]
        for contract in (
            'bruce dag-scheduler-v5 payload-inspect BUNDLE [--stream-v15]',
            'IMPLEMENTIERT UND BELEGT', 'VALID', 'INCOMPLETE', 'PROTOCOL_ERROR',
            'Exit 0', 'Exit 1', 'Exit 2', 'run_status', 'FAILED',
            'Prüfanforderung', 'kein rückwirkender Modusnachweis',
            'NOT_PROVABLE', 'NOT_CHECKED', 'VERIFIED_PRESENT', 'NOT_REQUESTED',
            'keine historische Vollständigkeit', 'read-only', 'kein Repair',
            'keine Publikation', 'V1.3', 'V1.4', '30.000 ms', 'atime',
            'test_dag_scheduler_v5_payload_inspector.py',
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, section)

    def test_payload_run_inspector_contract_is_cross_referenced(self):
        for filename in (
            'docs/cli/formats.md', 'docs/implementation/README.md',
            'docs/architecture/03-blueprint-consolidation.md',
        ):
            document = (ROOT / filename).read_text()
            heading = '## Payload Run Inspector V1'
            self.assertIn(heading, document)
            section = document.partition(heading)[2].split('\n## ', 1)[0]
            for contract in (
                'bruce dag-scheduler-v5 payload-inspect BUNDLE [--stream-v15]',
                'dag-scheduler-v5-payload-inspect-v1', 'VALID', 'FAILED',
                'INCOMPLETE', 'PROTOCOL_ERROR', 'NOT_PROVABLE', 'NOT_CHECKED',
                'VERIFIED_PRESENT', 'NOT_REQUESTED', 'V1.3', 'V1.4',
                'read-only', 'payload_run_inspector_v1.cpp',
            ):
                with self.subTest(reference=filename, contract=contract):
                    self.assertIn(contract, section)

    def test_dag_scheduler_v5_payload_flow_v2_manual_matches_implemented_boundary(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        heading = '### V5-Payload-Flow V2 – persistierte Kanten-Lineage'
        self.assertIn(heading, manual)
        section = manual.partition(heading)[2].split('\n## ', 1)[0]
        for contract in (
            'IMPLEMENTIERT UND BELEGT',
            'bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE --flow-v2',
            'bruce dag-scheduler-v5 payload-inspect BUNDLE --flow-v2',
            'BUNDLE.flow-v2.json', 'Mode 0600', 'contract', 'run_id',
            'spec_digest', 'report_digest', 'stream_v15', 'nodes', 'attempts',
            'ATTEMPT_STARTED', 'inputs:[]', 'FLOW_V2_EXTERNAL_INPUTS_UNSUPPORTED',
            'FLOW_RECORD_INVALID', 'dag-scheduler-v5-payload-flow-inspect-v2',
            'VALID', 'INCOMPLETE', 'PROTOCOL_ERROR', 'Exit 0', 'Exit 1', 'Exit 2',
            'metadata-only', 'LOCAL_CONSISTENCY', 'Payloadbytes', 'PIDs',
            'test_dag_scheduler_v5_payload_flow_inspector.py',
            'DOKUMENTIERT / KONZEPT',
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, section)

    def test_dag_scheduler_v5_payload_flow_v2_contract_is_cross_referenced(self):
        references = {
            'docs/cli/formats.md': (
                '## DAG-Scheduler V5 Payload Flow V2',
                'dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE --flow-v2',
                'BUNDLE.flow-v2.json', 'bruce-payload-flow-v2', 'report_digest',
                'ATTEMPT_STARTED', 'object_index', 'input_index',
                'dag-scheduler-v5-payload-flow-inspect-v2', 'LOCAL_CONSISTENCY',
                'PROTOCOL_ERROR', 'FLOW_V2_EXTERNAL_INPUTS_UNSUPPORTED',
            ),
            'docs/implementation/README.md': (
                '## V5 Artifact Payload Flow V2',
                'BUNDLE.flow-v2.json', 'dag-scheduler-v5-payload-flow-inspect-v2',
                'FLOW_RECORD_INVALID', 'FLOW_V2_EXTERNAL_INPUTS_UNSUPPORTED',
                'object index 0', 'ATTEMPT_STARTED', 'LOCAL_CONSISTENCY',
                'DOCUMENTED / CONCEPT',
            ),
            'docs/architecture/03-blueprint-consolidation.md': (
                '## DAG-Scheduler V5 Artifact Payload Flow V2',
                'BUNDLE.flow-v2.json', 'dag-scheduler-v5-payload-flow-inspect-v2',
                'FLOW_RECORD_INVALID', 'FLOW_V2_EXTERNAL_INPUTS_UNSUPPORTED',
                'ATTEMPT_STARTED', 'Objektindex 0', 'DOKUMENTIERT / KONZEPT',
            ),
        }
        for filename, contracts in references.items():
            text = (ROOT / filename).read_text()
            for contract in contracts:
                with self.subTest(reference=filename, contract=contract):
                    self.assertIn(contract, text)

    def test_dag_scheduler_v4_manual_matches_bundle_routes_and_limits(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        heading = '## `dag-scheduler-v4`'
        self.assertIn(heading, manual, 'V4 bundle manual section is missing')
        section = manual.partition(heading)[2].split('\n## ', 1)[0]
        for contract in (
            'IMPLEMENTIERT UND BELEGT',
            'bruce dag-scheduler-v4 execute SPEC --output BUNDLE',
            'bruce dag-scheduler-v4 inspect BUNDLE',
            'bruce dag-scheduler-v4 replay BUNDLE',
            'NanoCell und MCP OFF', 'atomar und exklusiv',
            'vorhandenes Ziel', 'nicht überschrieben', 'schreiben keine Datei',
            '5 MiB', '5.242.880 Bytes', 'BRUCE_DAG_SCHEDULER_RUN_V4',
            'dag-scheduler-v4-execute-v1', 'dag-scheduler-v4-inspect-v1',
            'dag-scheduler-v4-replay-v1', 'bundle_digest', 'dag-scheduler-v2',
            'literal-v1', 'append-v1', 'join-v1',
            'keine rohen Request- oder Payload-Bytes', 'keine Hex-Kodierungen',
            'lokale vertrauliche Artefakte', 'keine Verschlüsselung',
            'Exit 0', 'leeres stderr', 'Exit 2', 'leeres stdout',
            'Usage: bruce dag-scheduler-v4 execute SPEC --output BUNDLE | inspect BUNDLE | replay BUNDLE',
            'keine freien Callbacks', 'keine Netzwerk', 'keine MCP',
            'keine Parallelität', 'keine Migration', 'externe Speicher',
            'test_dag_scheduler_v4_cli.py', 'dag_scheduler_v4_tests.cpp',
            'verify_examples.py führt V4 nicht aus',
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, section)

    def test_dag_scheduler_v4_manual_distinguishes_inspect_and_replay(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        section = manual.partition('## `dag-scheduler-v4`')[2].split('\n## ', 1)[0]
        for contract in (
            'Header, Digest, Dekodierung und Kanonizität', 'keine Callbacks',
            'keine erneute Ausführung', 'vollständige Gleichheit',
            'Spec, Receipts und Trace', 'byteidentisches erneutes Packen',
            'lokale Reproduzierbarkeit', 'kein mathematischer Beweis',
            'keine Authentifizierung', 'DagSchedulerV4ReplayMismatch',
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, section)

    def test_dag_scheduler_v4_bundle_boundary_is_cross_referenced(self):
        for filename in (
            'docs/cli/README.md', 'docs/cli/formats.md',
            'docs/architecture/03-blueprint-consolidation.md',
        ):
            text = (ROOT / filename).read_text()
            for contract in (
                'IMPLEMENTIERT UND BELEGT',
                'bruce dag-scheduler-v4 execute SPEC --output BUNDLE',
                'bruce dag-scheduler-v4 inspect BUNDLE',
                'bruce dag-scheduler-v4 replay BUNDLE',
                'BRUCE_DAG_SCHEDULER_RUN_V4', '5 MiB',
                'Request- und Payload-Rohbytes', 'vertrauliche',
                'inspect', 'replay', 'Kanonizität', 'Reproduzierbarkeit',
            ):
                with self.subTest(reference=filename, contract=contract):
                    self.assertIn(contract, text)

    def test_dag_scheduler_v3_manual_matches_implemented_scope(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        heading = '## `dag-scheduler-v3`'
        self.assertTrue(heading in manual, 'V3 manual section is missing')
        spec_section = manual.partition(heading)[2].split('\n## ', 1)[0]
        required_contracts = (
            'IMPLEMENTIERT UND BELEGT',
            'bruce dag-scheduler-v3 execute SPEC',
            '{"error":"Usage: bruce dag-scheduler-v3 execute|inspect SPEC"}',
            'Parser-/CLI-Adapter über V2', 'kein neuer Scheduler',
            'BRUCE_DAG_SCHEDULER_SPEC_V3',
            'dag-scheduler-v3-cli-v1', 'dag-scheduler-spec-v3',
            'dag-scheduler-v2', 'spec_digest', 'unveränderten Dateibytes',
            'literal-v1', 'append-v1', 'join-v1',
            'LITERAL_READY', 'APPENDED_READY', 'JOINED_READY',
            'ASCII', 'LF-only', 'CRLF', '65.536 Bytes',
            'direkten READY-Vorgänger', 'nach `producer_node_id` sortiert',
            'keine rohen Request- oder Payload-Bytes',
            'keine Hex-Kodierungen dieser Daten',
            'Exit 0', 'leeres stderr', 'Exit 2', 'leeres stdout',
            'keine freien Callback-Namen', 'keine Persistenz',
            'keine Bundle-/Replay-Schnittstelle',
            'keine Threads', 'keine Parallelität',
            'keine GPU-Ausführung', 'keine Netzwerkausführung',
            'keine allgemeine Runtime-Orchestrierung',
            'Phase-DAG V3', 'test_dag_scheduler_v3_cli.py',
            'verify_examples.py führt V3 nicht aus',
            'scripts/with-host-deps.sh python3 docs/cli/verify_examples.py',
            'DOKUMENTIERT / KONZEPT',
        )
        for contract in required_contracts:
            with self.subTest(contract=contract):
                self.assertIn(contract, spec_section)

    def test_dag_scheduler_v3_inspector_manual_matches_read_only_contract(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        section = manual.partition('## `dag-scheduler-v3`')[2].split('\n## ', 1)[0]
        required_contracts = (
            'bruce dag-scheduler-v3 execute SPEC',
            'bruce dag-scheduler-v3 inspect SPEC',
            '{"error":"Usage: bruce dag-scheduler-v3 execute|inspect SPEC"}',
            'dag-scheduler-v3-inspect-v1', 'read-only', 'parse_spec()',
            'statische', 'Kahn-Reihenfolge',
            'lexikographisch kleinsten bereiten Knoten',
            'a-source', 'b-transform', 'c-independent', 'd-join',
            'spec_version', 'spec_digest', 'scheduler_version', 'dag_id',
            'max_started_nodes', 'max_total_receipt_bytes', 'nodes',
            'node_id', 'callback_id', 'dependencies', 'request_size',
            'request_digest', 'planned_start_order', 'kanonischen Spec-Reihenfolge',
            'keine Callbacks', 'execute_dag_scheduler_v2',
            'keine Receipts', 'keinen Trace', 'keine rohen Request- oder Payload-Bytes',
            'request-hex', 'kein Teilbericht', '65.537',
            'Callbackfehler', 'Payloadgrößen', 'Receipt-Budgets',
            'Startbudget-Cancellation', 'keine Persistenz',
            'keine Bundle-/Replay-Schnittstelle', 'keine Parallelität',
            'keine GPU-Ausführung', 'keine Netzwerkausführung',
            'keine MCP-Anbindung', 'IMPLEMENTIERT UND BELEGT',
            'verify_examples.py führt V3 nicht aus',
        )
        for contract in required_contracts:
            with self.subTest(contract=contract):
                self.assertIn(contract, section)

        references = {
            'docs/cli/README.md': (
                'bruce dag-scheduler-v3 inspect SPEC', 'dag-scheduler-v3-inspect-v1',
                'read-only', 'planned_start_order', 'callbacks',
            ),
            'docs/cli/formats.md': (
                'bruce dag-scheduler-v3 inspect SPEC', 'parse_spec()',
                'dag-scheduler-v3-inspect-v1', 'planned_start_order', 'request-hex',
            ),
            'docs/architecture/03-blueprint-consolidation.md': (
                'bruce dag-scheduler-v3 inspect SPEC', 'dag-scheduler-v3-inspect-v1',
                'read-only', 'parse_spec()', 'planned_start_order',
                'execute_dag_scheduler_v2', 'no callbacks',
            ),
        }
        for filename, contracts in references.items():
            text = (ROOT / filename).read_text()
            for contract in contracts:
                with self.subTest(reference=filename, contract=contract):
                    self.assertIn(contract, text)

    def test_dag_scheduler_v2_flow_manual_matches_implemented_scope(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        heading = '## `dag-scheduler-v2`'
        self.assertTrue(heading in manual, 'V2 manual section is missing')
        flow_section = manual.partition(heading)[2].split('\n## ', 1)[0]
        required_contracts = (
            'IMPLEMENTIERT UND BELEGT',
            'bruce dag-scheduler-v2 demo flow',
            'dag-scheduler-v2-cli-v1',
            'a-source', 'b-transform', 'c-independent', 'd-join',
            'serielle Kahn-Auswahl',
            'lexikographisch kleinsten bereiten Knoten',
            'genau ein Callback',
            'direkten READY-Vorgänger',
            'nach `producer_node_id` sortiert',
            '65.536 Bytes', 'SHA-256',
            'max_total_receipt_bytes',
            'Input-Kopien',
            'keine rohen Request- oder Payload-Bytes',
            'Exit 0', 'leeres stderr', 'Exit 2', 'leeres stdout',
            '{"error":"Usage: bruce dag-scheduler-v2 demo flow"}',
            'keine freien DAG-Dateien', 'keine freien Callback-Namen',
            'keine freien Budgets', 'keine freien Payloads',
            'keine weiteren Parameter',
            'keine Bundle-/Replay-Schnittstelle', 'keine Persistenz',
            'externe Payloadspeicher', 'Phase-DAG-V3-Adapter',
            'keine Threads', 'Parallelität',
            'keine GPU-Ausführung', 'keine Netzwerkausführung',
            'keine allgemeine Runtime-Orchestrierung',
            'gültige, aber unterschiedliche Callback-Rückgaben',
            'keine erneute Ausführung zur Erkennung',
            'DOKUMENTIERT / KONZEPT',
            'test_dag_scheduler_v2_cli.py',
            'verify_examples.py führt diese V2-Demo nicht aus',
            'scripts/with-host-deps.sh python3 docs/cli/verify_examples.py',
        )
        for contract in required_contracts:
            with self.subTest(contract=contract):
                self.assertIn(contract, flow_section)

    def test_dag_scheduler_demo_manual_matches_implemented_scope(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        demo_section = manual.partition('## `dag-scheduler-v1`')[2].split('\n## ', 1)[0]
        required_contracts = (
            'IMPLEMENTIERT UND BELEGT',
            'bruce dag-scheduler-v1 demo mixed',
            'bruce dag-scheduler-v1 demo budget',
            'bruce dag-scheduler-v1 demo cancelled',
            'dag-scheduler-v1-cli-v1',
            'drei feste In-Memory-Demos',
            'serielle Kahn-Auswahl',
            'lexikographisch kleinsten bereiten Knoten',
            'genau ein Callback',
            'direkten READY-Vorgänger',
            'producer_node_id',
            'receipts',
            'nach `node_id` sortiert',
            'trace',
            'chronologische logische Reihenfolge',
            'COMPLETED',
            'leere `node_id`',
            'a-root',
            'b-fail',
            'c-independent',
            'd-blocked',
            'e-finish',
            'ROOT_READY',
            'EXPECTED_FAILURE',
            'INDEPENDENT_READY',
            'DEPENDENCY_NOT_READY',
            'FINISHED',
            'START_BUDGET_EXHAUSTED',
            'CONTROLLED_CANCELLATION',
            'keine weiteren Callbacks',
            'Exit 0',
            'Exit 2',
            'leeres stderr',
            'leeres stdout',
            'Usage: bruce dag-scheduler-v1 demo mixed|budget|cancelled',
            'keine freien DAG-Dateien',
            'keine freien Callback-Namen',
            'keine freien Budgets',
            'keine weiteren Parameter',
            'keine Erweiterung von Phase-DAG V2/V3',
            'keine Threads',
            'keine GPU-Ausführung',
            'keine Netzwerkausführung',
            'keine Bundle-/Replay-Schnittstelle',
            'keine allgemeine Runtime-Orchestrierung',
            'test_dag_scheduler_v1_cli.py',
            'verify_examples.py führt diese Demos nicht aus',
            'scripts/with-host-deps.sh python3 docs/cli/verify_examples.py',
            'DOKUMENTIERT / KONZEPT',
        )
        for contract in required_contracts:
            with self.subTest(contract=contract):
                self.assertIn(contract, demo_section)

    def test_signed_affine_relation_demo_manual_matches_implemented_scope(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        demo_section = manual.partition('## `signed-affine-relation-v1`')[2].split('\n## ', 1)[0]
        required_contracts = (
            'IMPLEMENTIERT UND BELEGT',
            'bruce signed-affine-relation-v1 demo merge',
            'signed-affine-relation-v1-cli-v1',
            'm × s + c',
            'Quelle `2`',
            'G=1',
            'Q=2',
            'n=101',
            'a-add',
            'z-double',
            'a=c mod n',
            'b=m mod n',
            'Q=s mod n',
            'kanonische Kollision',
            'coefficient_delta=100',
            'value_delta=99',
            'candidate_k=2',
            'VERIFIED',
            'unabhängige Rückverifikation',
            'ein festes In-Memory-Demo',
            'Exit 0',
            'Exit 2',
            'keine Suche',
            'kein Geheimnis',
            'kein DLP-Löser',
            'keine reale Kryptographie',
            'keine elliptische Kurve',
            'kein secp256k1',
            'keine freien Formeln',
            'keine Gruppenparameter',
            'keine Dateien',
            'keine Persistenz',
            'keine Erweiterung von Phase-DAG V2/V3',
            'Signed-Affine-DAG V1 und Toy-Relation V1 bleiben unverändert',
            'verify_examples.py führt dieses Demo nicht aus',
            'scripts/with-host-deps.sh python3 docs/cli/verify_examples.py',
            'test_signed_affine_relation_v1_cli.py',
            'DOKUMENTIERT / KONZEPT',
        )
        for contract in required_contracts:
            with self.subTest(contract=contract):
                self.assertIn(contract, demo_section)

    def test_toy_relation_demo_manual_matches_implemented_scope(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        demo_section = manual.partition('## `toy-relation-v1`')[2].split('\n## ', 1)[0]
        required_contracts = (
            'IMPLEMENTIERT UND BELEGT',
            'bruce toy-relation-v1 demo collision',
            'bruce toy-relation-v1 demo noninvertible',
            'toy-relation-v1-cli-v1',
            'zwei feste In-Memory-Demos',
            'n=101',
            'k=17',
            'canonical_point=53',
            'coefficient_delta=2',
            'value_delta=34',
            'unabhängige Rückverifikation',
            'provenance_id',
            'VERIFIED',
            'n=15',
            'canonical_point=5',
            'NO_UNIQUE_RELATION',
            'NonInvertibleToyRelationCoefficient',
            'keinen Relation Record',
            'kein `candidate_k`',
            'Exit 0',
            'Exit 2',
            'keine freien Gruppenparameter',
            'keine Dateien',
            'keine Suche',
            'kein Geheimnis',
            'kein diskreter-Logarithmus-Löser',
            'keine elliptische Kurve',
            'kein secp256k1',
            'keine Erweiterung von Phase-DAG V2/V3',
            'verify_examples.py',
            'test_toy_relation_v1_cli.py',
        )
        for contract in required_contracts:
            with self.subTest(contract=contract):
                self.assertIn(contract, demo_section)

    def test_signed_affine_demo_manual_matches_implemented_scope(self):
        manual = (ROOT / 'docs/cli/commands.md').read_text()
        demo_section = manual.partition('## `signed-affine-dag-v1`')[2].split('\n## ', 1)[0]
        required_contracts = (
            'bruce signed-affine-dag-v1 demo merge',
            'bruce signed-affine-dag-v1 demo cancellation',
            'signed-affine-dag-v1-cli-v1',
            'zwei feste In-Memory-Demos',
            'keine Graphdateien',
            'keine freien Formeln',
            'keine Persistenz',
            'keine Erweiterung von Phase-DAG V2/V3',
        )
        for contract in required_contracts:
            with self.subTest(contract=contract):
                self.assertIn(contract, demo_section)

    def test_snapshot_excludes_local_signing_material(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'fixture'
            source.mkdir()
            (source / '.env').write_text('dummy-private-signing-material\n')
            (source / '.env.local').write_text('dummy-local-credentials\n')
            (source / 'README.md').write_text('public source\n')
            snapshot = Path(directory) / 'snapshot'
            shutil.copytree(source, snapshot, ignore=IGNORE)
            self.assertFalse((snapshot / '.env').exists())
            self.assertFalse((snapshot / '.env.local').exists())
            self.assertEqual((snapshot / 'README.md').read_text(), 'public source\n')

    def test_examples_snapshot_orders_inputs_canonically(self):
        with tempfile.TemporaryDirectory() as directory:
            snapshot = Path(directory) / 'source'
            shutil.copytree(ROOT, snapshot, ignore=IGNORE)
            result = subprocess.run(
                [sys.executable, snapshot / 'docs/cli/verify_examples.py', BIN],
                cwd=snapshot,
                capture_output=True,
                text=True,
                timeout=120,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            inputs = json.loads((snapshot / 'docs/cli/examples.json').read_text())['inputs']
            self.assertEqual(list(inputs), sorted(inputs))


if __name__ == '__main__':
    unittest.main()
