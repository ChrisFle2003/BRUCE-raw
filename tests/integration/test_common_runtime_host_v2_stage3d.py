import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest

BIN = str(Path(sys.argv.pop(1)).resolve())
REAL_BIN = str(Path(sys.argv.pop(1)).resolve()) if len(sys.argv) > 1 else str(Path(BIN).with_name('bruce'))


def spec_bytes(nodes, receipt_bytes=4096, max_started=None):
    lines = ['BRUCE_DAG_SCHEDULER_SPEC_V3', 'dag-id=stage3d',
             f'max-started-nodes={max_started if max_started is not None else len(nodes)}',
             f'max-total-receipt-bytes={receipt_bytes}', '']
    for name, dependencies, callback, request in nodes:
        lines += [f'node={name}', f'depends={dependencies}', f'callback={callback}',
                  f'request-hex={request.hex()}', '']
    return '\n'.join(lines).encode()


def events_for(bundle):
    return [json.loads(line) for line in Path(str(bundle) + '.run/control/run-log.jsonl').read_text().splitlines()]


class PayloadHostTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fault_directory = tempfile.TemporaryDirectory(prefix='bruce-stage3d-fault-')
        cls.addClassCleanup(cls.fault_directory.cleanup)
        cls.fault_library = Path(cls.fault_directory.name) / 'fault.so'
        subprocess.run(['cc', '-shared', '-fPIC', '-Wall', '-Wextra', '-Werror',
                        str(Path(__file__).with_name('common_runtime_host_v2_stage3d_fault.c')),
                        '-o', str(cls.fault_library), '-ldl'], check=True, capture_output=True)

    def fault_env(self, bundle, mode, wait=False):
        marker = bundle.parent / 'fault-marker'
        release = bundle.parent / 'fault-release'
        env = dict(os.environ, LD_PRELOAD=str(self.fault_library),
                   BRUCE_TEST_STAGE3D_MODE=mode, BRUCE_TEST_STAGE3D_MARKER=str(marker))
        if wait:
            env['BRUCE_TEST_STAGE3D_RELEASE'] = str(release)
        return env, marker, release

    def await_marker(self, marker, process):
        deadline = time.monotonic() + 10
        while not marker.exists():
            self.assertIsNone(process.poll(), 'host ended before synchronization point')
            self.assertLess(time.monotonic(), deadline, 'synchronization timeout')
            time.sleep(0.001)

    def prepare(self, nodes, retry=0, workers=1, receipt_bytes=4096, extra=(), max_started=None):
        directory = tempfile.TemporaryDirectory(prefix='bruce-stage3d-')
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        source = root / 'seed'
        source.write_bytes(b'seed')
        source.chmod(0o600)
        store = root / 'store'
        initialized = subprocess.run([REAL_BIN, 'artifact-store-v1', 'import', str(source), '--store', str(store)],
                                     capture_output=True, timeout=15)
        self.assertEqual(initialized.returncode, 0, initialized.stderr)
        spec, bundle = root / 'workflow.spec', root / 'bundle'
        spec.write_bytes(spec_bytes(nodes, receipt_bytes, max_started))
        command = [BIN, 'dag-scheduler-v5', 'payload', str(spec), '--output', str(bundle),
                   '--store', str(store), '--retry', str(retry), '--workers', str(workers), *extra]
        return command, bundle, store

    def run_payload(self, nodes, **kwargs):
        command, bundle, store = self.prepare(nodes, **kwargs)
        process = subprocess.run(command, capture_output=True, timeout=20)
        self.assertTrue(bundle.exists(), process.stderr)
        report = json.loads(bundle.read_text())
        events = events_for(bundle)
        self.assert_inspector(bundle, report, events)
        return process, report, events, bundle, store

    def assert_inspector(self, bundle, report, events):
        self.assertEqual([e['seq'] for e in events], list(range(1, len(events) + 1)))
        self.assertEqual(events[-1]['event'], 'RUN_FINISHED')
        self.assertEqual(events[-1]['status'], report['status'])
        self.assertEqual(sum(e['event'] == 'RUN_FINISHED' for e in events), 1)
        histories = [attempt for receipt in report.get('receipts', [report]) for attempt in receipt['attempts']]
        self.assertEqual(sum(e['event'] == 'ATTEMPT_STARTED' for e in events), len(histories))
        # This is the observable shared Host lifecycle, unlike the legacy payload loop.
        self.assertTrue(all(e['status'] == 'RUNNING' for e in events if e['event'] == 'ATTEMPT_STARTED'))
        for command in ([BIN, 'fixture-inspect', str(bundle) + '.run'],
                        [BIN, 'dag-scheduler-v5', 'payload-inspect', str(bundle)]):
            inspected = subprocess.run(command, capture_output=True, timeout=15)
            self.assertEqual(inspected.returncode, 0, (command, inspected.stderr, report, events))
            metadata = json.loads(inspected.stdout)
            self.assertEqual(metadata['status'], 'VALID')
            self.assertEqual(metadata['run_status'], report['status'])

    def test_chain_and_fanin_bytes_provenance_and_canonical_receipts(self):
        nodes = [('a', '', 'literal-v1', b'A'), ('b', 'a', 'append-v1', b'B'),
                 ('c', '', 'literal-v1', b'C'), ('d', 'b,c', 'join-v1', b'-')]
        for workers in (1, 2):
            with self.subTest(workers=workers):
                process, report, events, bundle, store = self.run_payload(nodes, workers=workers)
                self.assertEqual(process.returncode, 0, process.stderr)
                receipts = report['receipts']
                self.assertEqual([r['node_id'] for r in receipts], ['a', 'b', 'c', 'd'])
                for receipt, expected in zip(receipts, (b'A', b'AB', b'C', b'AB-C')):
                    output = Path(str(bundle) + f".run/attempts/{receipt['attempt_id']}/output/result.bin")
                    self.assertEqual(output.read_bytes(), expected)
                    self.assertEqual(receipt['payload_digest'], hashlib.sha256(expected).hexdigest())
                manifests = [dict(line.split('=', 1) for line in p.read_text().splitlines() if '=' in line)
                             for p in (store / 'manifests').glob('*.manifest')]
                self.assertEqual(len(manifests), 4)
                for receipt in receipts:
                    self.assertTrue(any(m['node_id'] == receipt['node_id'] and
                                        m['attempt_id'] == receipt['attempt_id'] for m in manifests))
                accepted = [e['attempt_id'] for e in events if e['event'] == 'ARTIFACT_ACCEPTED']
                self.assertEqual(accepted, [r['attempt_id'] for r in receipts])

    def test_retry_stages_fresh_inputs_and_accepts_only_success(self):
        nodes = [('a', '', 'literal-v1', b'A'), ('retry_consumer', 'a', 'append-v1', b'B'),
                 ('z', 'retry_consumer', 'append-v1', b'C')]
        for workers in (1, 2):
            process, report, events, bundle, _ = self.run_payload(nodes, workers=workers, retry=2)
            self.assertEqual(process.returncode, 0, process.stderr)
            receipt = report['receipts'][1]
            self.assertEqual([a['failure_code'] for a in receipt['attempts']], ['PROCESS_CRASHED', 'READY'])
            staged = Path(str(bundle) + '.run/payload-inputs')
            self.assertEqual((staged / 'n1-a-2-1-0.bin').read_bytes(), b'A')
            self.assertEqual((staged / 'n1-a-2-2-0.bin').read_bytes(), b'A')
            self.assertEqual(Path(str(bundle) + '.run/attempts/a-3-1/output/result.bin').read_bytes(), b'ABC')
            self.assertNotIn('a-2-1', [e.get('attempt_id') for e in events if e['event'] == 'ARTIFACT_ACCEPTED'])

    def test_exhaustion_blocks_successor(self):
        process, report, _, _, _ = self.run_payload(
            [('always_crash', '', 'literal-v1', b'A'), ('z', 'always_crash', 'append-v1', b'B')], retry=2)
        self.assertEqual(process.returncode, 1)
        self.assertEqual([r['status'] for r in report['receipts']], ['FAILED', 'BLOCKED'])
        self.assertEqual(len(report['receipts'][0]['attempts']), 3)

    def test_input_preflight_failure_has_no_invented_attempt_and_blocks(self):
        process, report, events, _, _ = self.run_payload(
            [('a', '', 'literal-v1', b'A'), ('b', 'a', 'append-v1', b'B'),
             ('c', 'b', 'append-v1', b'C')], retry=3, extra=('--max-input-bytes', '1'))
        self.assertEqual(process.returncode, 1)
        self.assertEqual(report['receipts'][1]['result_code'], 'INPUT_LIMIT_EXCEEDED')
        self.assertEqual(report['receipts'][1]['attempts'], [])
        self.assertEqual(report['receipts'][2]['status'], 'BLOCKED')
        self.assertEqual(sum(e['event'] == 'ATTEMPT_STARTED' for e in events), 1)

    def test_receipt_budget_typed_rejection_is_final_and_inspectable(self):
        process, report, events, _, store = self.run_payload(
            [('a', '', 'literal-v1', b'AB'), ('z', 'a', 'append-v1', b'C')], retry=3, receipt_bytes=1)
        self.assertEqual(process.returncode, 1)
        self.assertEqual(report['receipts'][0]['failure_code'], 'OUTPUT_LIMIT_EXCEEDED')
        self.assertEqual([(e['attempt_id'], e['status']) for e in events if e['event'] == 'RESULT_REJECTED'],
                         [('a-1-1', 'OUTPUT_LIMIT_EXCEEDED')])
        self.assertFalse(list((store / 'manifests').glob('*.manifest')))

    def test_worker_deadline_retries_are_bounded(self):
        process, report, events, _, _ = self.run_payload(
            [('a_slow', '', 'literal-v1', b'A')], retry=1, extra=('--max-wall-ms', '30'))
        self.assertEqual(process.returncode, 1)
        self.assertEqual([a['failure_code'] for a in report['attempts']], ['TIMEOUT', 'TIMEOUT'])
        self.assertFalse(any(e['event'] == 'ARTIFACT_ACCEPTED' for e in events))

    def test_typed_publication_digest_and_store_rejection(self):
        for mode, code in (('digest', 'ARTIFACT_DIGEST_MISMATCH'), ('store', 'ARTIFACT_STORE_VIOLATION')):
            with self.subTest(mode=mode):
                command, bundle, _ = self.prepare(
                    [('a', '', 'literal-v1', b'AB'), ('z', 'a', 'append-v1', b'C')], retry=3)
                env, marker, _ = self.fault_env(bundle, mode)
                process = subprocess.run(command, env=env, capture_output=True, timeout=20)
                self.assertTrue(marker.exists(), 'fault was never exercised')
                self.assertEqual(process.returncode, 1, process.stderr)
                report, events = json.loads(bundle.read_text()), events_for(bundle)
                self.assertEqual(report['receipts'][0]['result_code'], code)
                self.assertEqual(report['receipts'][1]['status'], 'BLOCKED')
                self.assertEqual(len(report['receipts'][0]['attempts']), 1)
                self.assertEqual([e['status'] for e in events if e['event'] == 'RESULT_REJECTED'], [code])
                self.assertIsNone(report['receipts'][0]['payload_digest'])
                self.assert_inspector(bundle, report, events)

    def test_absolute_deadline_covers_materialization_and_publication(self):
        for mode in ('deadline_materialize', 'deadline_publish', 'deadline_ready'):
            with self.subTest(mode=mode):
                nodes = [('a', '', 'literal-v1', b'A'), ('b', 'a', 'append-v1', b'B')]
                command, bundle, _ = self.prepare(nodes)
                env, marker, _ = self.fault_env(bundle, mode)
                process = subprocess.run(command, env=env, capture_output=True, timeout=20)
                self.assertTrue(marker.exists(), 'deadline boundary was never exercised')
                self.assertEqual(process.returncode, 1, process.stderr)
                report, events = json.loads(bundle.read_text()), events_for(bundle)
                failed = report['receipts'][1 if mode == 'deadline_materialize' else 0]
                self.assertEqual(failed['result_code'], 'TIMEOUT')
                if mode == 'deadline_materialize':
                    self.assertEqual(failed['attempts'], [])
                    self.assertEqual(sum(e['event'] == 'ATTEMPT_STARTED' for e in events), 1)
                else:
                    self.assertEqual([e['status'] for e in events if e['event'] == 'RESULT_REJECTED'], ['TIMEOUT'])
                    self.assertEqual(failed['failure_code'], 'TIMEOUT')
                self.assert_inspector(bundle, report, events)

    def test_post_ready_timeout_can_retry_and_publish_success(self):
        command, bundle, store = self.prepare([('a', '', 'literal-v1', b'A')], retry=1)
        env, marker, _ = self.fault_env(bundle, 'deadline_publish')
        process = subprocess.run(command, env=env, capture_output=True, timeout=20)
        self.assertTrue(marker.exists())
        self.assertEqual(process.returncode, 0, process.stderr)
        report, events = json.loads(bundle.read_text()), events_for(bundle)
        self.assertEqual([a['failure_code'] for a in report['attempts']], ['TIMEOUT', 'READY'])
        self.assertEqual([e['attempt_id'] for e in events if e['event'] == 'ARTIFACT_ACCEPTED'], ['a-1-2'])
        self.assertEqual(len(list((store / 'manifests').glob('*.manifest'))), 2)
        self.assert_inspector(bundle, report, events)

    def test_cancel_synchronized_before_dispatch_materialization_retry_ready_and_publish(self):
        for mode in ('before_dispatch', 'materialize', 'between_retries', 'ready', 'publication', 'acceptance', 'materialize_retry'):
            with self.subTest(mode=mode):
                if mode == 'materialize_retry':
                    nodes = [('a', '', 'literal-v1', b'A'), ('retry_consumer', 'a', 'append-v1', b'B'),
                             ('z', 'retry_consumer', 'append-v1', b'C')]
                elif mode == 'materialize':
                    nodes = [('a', '', 'literal-v1', b'A'), ('b', 'a', 'append-v1', b'B'),
                             ('z', 'b', 'append-v1', b'C')]
                elif mode == 'between_retries':
                    nodes = [('crash_once', '', 'literal-v1', b'A'), ('z', 'crash_once', 'append-v1', b'B')]
                else:
                    nodes = [('a', '', 'literal-v1', b'A'), ('z', 'a', 'append-v1', b'B')]
                command, bundle, _ = self.prepare(nodes, retry=3)
                env, marker, release = self.fault_env(bundle, mode, wait=True)
                process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                try:
                    self.await_marker(marker, process)
                    cancel = subprocess.run([BIN, 'dag-scheduler-v5', 'cancel', str(bundle)],
                                            capture_output=True, timeout=10)
                    self.assertEqual(cancel.returncode, 0, cancel.stderr)
                    release.write_text('1')
                    _, stderr = process.communicate(timeout=20)
                    self.assertEqual(process.returncode, 1, stderr)
                    report, events = json.loads(bundle.read_text()), events_for(bundle)
                    self.assertEqual(report['status'], 'CANCELLED')
                    starts = [e for e in events if e['event'] == 'ATTEMPT_STARTED']
                    self.assertEqual(len(starts), 0 if mode == 'before_dispatch' else (2 if mode == 'materialize_retry' else 1))
                    accepted = [e for e in events if e['event'] == 'ARTIFACT_ACCEPTED']
                    self.assertEqual(len(accepted), 1 if mode in ('materialize', 'materialize_retry') else 0)
                    self.assert_inspector(bundle, report, events)
                finally:
                    release.touch()
                    if process.poll() is None:
                        process.kill()
                        process.communicate(timeout=5)

    def test_cancel_active_retry_and_no_late_acceptance(self):
        for late in (False, True):
            command, bundle, _ = self.prepare(
                [('slow_retry', '', 'literal-v1', b'A'), ('z', 'slow_retry', 'append-v1', b'B')], retry=3)
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            stopped = False
            try:
                self.await_marker(Path(str(bundle) + '.run/attempts/a-1-2/output/fixture-started'), process)
                if late:
                    os.kill(process.pid, signal.SIGSTOP)
                    stopped = True
                    deadline = time.monotonic() + 10
                    while 'State:\tT' not in Path(f'/proc/{process.pid}/status').read_text():
                        self.assertLess(time.monotonic(), deadline)
                        time.sleep(0.001)
                    pids = [int(v) for v in Path(f'/proc/{process.pid}/task/{process.pid}/children').read_text().split()]
                    while any(Path(f'/proc/{p}/status').exists() and 'State:\tZ' not in
                              Path(f'/proc/{p}/status').read_text() for p in pids):
                        self.assertLess(time.monotonic(), deadline)
                        time.sleep(0.001)
                cancelled = subprocess.run([BIN, 'dag-scheduler-v5', 'cancel', str(bundle)], capture_output=True, timeout=10)
                self.assertEqual(cancelled.returncode, 0, cancelled.stderr)
                if stopped:
                    os.kill(process.pid, signal.SIGCONT)
                    stopped = False
                _, stderr = process.communicate(timeout=20)
                self.assertEqual(process.returncode, 1, stderr)
                report, events = json.loads(bundle.read_text()), events_for(bundle)
                self.assertEqual(report['status'], 'CANCELLED')
                self.assertEqual(len(report['receipts'][0]['attempts']), 2)
                self.assertFalse(any(e['event'] == 'ARTIFACT_ACCEPTED' for e in events))
                self.assert_inspector(bundle, report, events)
            finally:
                if stopped and process.poll() is None: os.kill(process.pid, signal.SIGCONT)
                if process.poll() is None:
                    process.kill()
                    process.communicate(timeout=5)

    def test_invalid_cancel_marker_is_failed_with_consistent_attempt_projection(self):
        for phase, multi in ((phase, multi) for phase in ('before_dispatch', 'ready', 'active') for multi in (False, True)):
            with self.subTest(phase=phase, multi=multi):
                nodes = [('a_slow' if phase == 'active' else 'a', '', 'literal-v1', b'A'),
                         ('z', 'a_slow' if phase == 'active' else 'a', 'append-v1', b'B')]
                if not multi: nodes = nodes[:1]
                command, bundle, _ = self.prepare(nodes, retry=3)
                env, marker, release = self.fault_env(bundle, phase, wait=phase != 'active')
                process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                try:
                    if phase == 'active':
                        self.await_marker(Path(str(bundle) + ('.run/attempts/a-1-1/output/fixture-started' if multi else '.run/attempts/a-1/output/fixture-started')), process)
                    else:
                        self.await_marker(marker, process)
                    invalid = Path(str(bundle) + '.run/control/cancel.request')
                    invalid.write_text('malformed cancel marker\n')
                    invalid.chmod(0o600)
                    release.touch()
                    _, stderr = process.communicate(timeout=20)
                    self.assertEqual(process.returncode, 1, stderr)
                    report, events = json.loads(bundle.read_text()), events_for(bundle)
                    self.assertEqual(report['status'], 'FAILED')
                    self.assertFalse(any(e['event'] == 'ARTIFACT_ACCEPTED' for e in events))
                    self.assertEqual(sum(e['event'] == 'ATTEMPT_STARTED' for e in events),
                                     0 if phase == 'before_dispatch' else 1)
                    if phase == 'active':
                        self.assertEqual(report.get('receipts', [report])[0]['failure_code'], 'PROTOCOL_ERROR')
                    self.assert_inspector(bundle, report, events)
                finally:
                    release.touch()
                    if process.poll() is None:
                        process.kill()
                        process.communicate(timeout=5)

    def test_invalid_control_during_materialization_and_retry_preserves_dispatched_identity(self):
        for phase in ('materialize', 'between_retries', 'materialize_retry'):
            with self.subTest(phase=phase):
                if phase == 'between_retries':
                    nodes = [('crash_once', '', 'literal-v1', b'A'), ('z', 'crash_once', 'append-v1', b'B')]
                else:
                    consumer = 'retry_consumer' if phase == 'materialize_retry' else 'b'
                    nodes = [('a', '', 'literal-v1', b'A'), (consumer, 'a', 'append-v1', b'B'),
                             ('z', consumer, 'append-v1', b'C')]
                command, bundle, _ = self.prepare(nodes, retry=3)
                env, marker, release = self.fault_env(bundle, phase, wait=True)
                process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                try:
                    self.await_marker(marker, process)
                    invalid = Path(str(bundle) + '.run/control/cancel.request')
                    invalid.write_text('invalid owner marker\n')
                    invalid.chmod(0o600)
                    release.touch()
                    _, stderr = process.communicate(timeout=20)
                    self.assertEqual(process.returncode, 1, stderr)
                    report, events = json.loads(bundle.read_text()), events_for(bundle)
                    self.assertEqual(report['status'], 'FAILED')
                    self.assertEqual(sum(e['event'] == 'ATTEMPT_STARTED' for e in events),
                                     2 if phase == 'materialize_retry' else 1)
                    for receipt in report['receipts']:
                        if receipt['attempts']:
                            self.assertEqual(receipt['attempt_id'], receipt['attempts'][-1]['attempt_id'])
                        else:
                            self.assertIsNone(receipt['attempt_id'])
                            self.assertEqual(receipt['status'], 'BLOCKED')
                    self.assert_inspector(bundle, report, events)
                finally:
                    release.touch()
                    if process.poll() is None:
                        process.kill()
                        process.communicate(timeout=5)

    def test_pending_ready_expires_at_canonical_receipt_position(self):
        command, bundle, _ = self.prepare(
            [('a_slow', '', 'literal-v1', b'A'), ('z', '', 'literal-v1', b'Z')],
            workers=2, extra=('--max-wall-ms', '40'))
        process = subprocess.run(command, capture_output=True, timeout=20)
        self.assertEqual(process.returncode, 1, process.stderr)
        report, events = json.loads(bundle.read_text()), events_for(bundle)
        projected = [e for e in events if e['event'] == 'RESULT_RECEIVED']
        self.assertEqual(projected[0]['attempt_id'], 'a-2-1')
        self.assertEqual(projected[0]['status'], 'READY')
        self.assertEqual([r['failure_code'] for r in report['receipts']], ['TIMEOUT', 'TIMEOUT'])
        self.assertEqual([e['status'] for e in events if e['event'] == 'RESULT_REJECTED'], ['TIMEOUT'])
        self.assert_inspector(bundle, report, events)

    def test_external_preflight_provenance_digest_and_budget_are_final_without_attempts(self):
        for fault, expected in (('missing', 'EXTERNAL_MANIFEST_INVALID'),
                                ('provenance', 'EXTERNAL_MANIFEST_INVALID'),
                                ('digest', 'EXTERNAL_OBJECT_INVALID'),
                                ('limit', 'EXTERNAL_INPUT_LIMIT_EXCEEDED')):
            with self.subTest(fault=fault):
                nodes = [('a', '', 'append-v1', b'A')]
                command, bundle, store = self.prepare(nodes, retry=3,
                    extra=('--max-input-bytes', '1') if fault == 'limit' else ())
                seed_spec, seed_bundle = bundle.parent / 'seed.spec', bundle.parent / 'seed-bundle'
                seed_spec.write_bytes(spec_bytes([('seed', '', 'literal-v1', b'B')]))
                seeded = subprocess.run([REAL_BIN, 'dag-scheduler-v5', 'payload', str(seed_spec),
                                         '--output', str(seed_bundle), '--store', str(store)],
                                        capture_output=True, timeout=20)
                self.assertEqual(seeded.returncode, 0, seeded.stderr)
                manifest_path = next((store / 'manifests').glob('*.manifest'))
                manifest = dict(line.split('=', 1) for line in manifest_path.read_text().splitlines() if '=' in line)
                entry = {'manifest_id': manifest_path.stem, 'object_index': 0,
                         'sha256': manifest['object_0_sha256'], 'size_bytes': 1,
                         'source_run_id': manifest['run_id'], 'source_node_id': manifest['node_id'],
                         'source_attempt_id': manifest['attempt_id']}
                if fault == 'missing': entry['manifest_id'] = 'absent-manifest'
                if fault == 'provenance': entry['source_node_id'] = 'wrong-node'
                if fault == 'digest':
                    digest = entry['sha256']
                    (store / 'objects' / digest[:2] / (digest + '.blob')).write_bytes(b'X')
                sidecar = bundle.parent / 'external.json'
                sidecar.write_text(json.dumps({'contract': 'bruce-dag-scheduler-v5-external-inputs-v14',
                                              'bindings': [{'node_id': 'a', 'inputs': [entry]}]}, separators=(',', ':')))
                sidecar.chmod(0o600)
                process = subprocess.run([*command, '--external-inputs', str(sidecar)],
                                         capture_output=True, timeout=20)
                self.assertEqual(process.returncode, 1, process.stderr)
                report, events = json.loads(bundle.read_text()), events_for(bundle)
                self.assertEqual(report['contract'], 'dag-scheduler-v5-payload-v14')
                self.assertEqual(report['result_code'], expected)
                self.assertEqual(report['attempts'], [])
                self.assertEqual([e['event'] for e in events], ['RUN_CREATED', 'RUN_FINISHED'])
                self.assert_inspector(bundle, report, events)

    def test_payload_inspector_rejects_unknown_duplicate_and_metadata_decisions(self):
        process, report, events, bundle, _ = self.run_payload(
            [('a', '', 'literal-v1', b'AB')], retry=3, receipt_bytes=1)
        self.assertEqual(process.returncode, 1)
        logfile = Path(str(bundle) + '.run/control/run-log.jsonl')
        for change in ('unknown', 'duplicate', 'metadata', 'accept_after_reject', 'start_after_cancel'):
            with self.subTest(change=change):
                changed = json.loads(json.dumps(events))
                rejection = next(e for e in changed if e['event'] == 'RESULT_REJECTED')
                if change == 'unknown': rejection['status'] = 'UNRECOGNIZED_REASON'
                if change == 'duplicate': changed.insert(-1, dict(rejection))
                if change == 'metadata': rejection['digest'] = '0' * 64
                if change == 'accept_after_reject':
                    acceptance = dict(next(e for e in changed if e['event'] == 'RESULT_RECEIVED'))
                    acceptance['event'] = 'ARTIFACT_ACCEPTED'
                    changed.insert(-1, acceptance)
                if change == 'start_after_cancel':
                    rejection['event'], rejection['status'] = 'CANCEL_REQUESTED', 'CANCELLED'
                    restart = dict(next(e for e in changed if e['event'] == 'ATTEMPT_STARTED'))
                    restart['attempt_id'] = 'a-1-2'
                    changed.insert(-1, restart)
                for seq, e in enumerate(changed, 1): e['seq'] = seq
                logfile.write_text(''.join(json.dumps(e, separators=(',', ':')) + '\n' for e in changed))
                inspected = subprocess.run([BIN, 'dag-scheduler-v5', 'payload-inspect', str(bundle)],
                                           capture_output=True, timeout=15)
                self.assertEqual(inspected.returncode, 2, inspected.stdout)
                self.assertEqual(inspected.stderr, b'{"error":"PROTOCOL_ERROR"}\n')
        logfile.write_text(''.join(json.dumps(e, separators=(',', ':')) + '\n' for e in events))
        self.assert_inspector(bundle, report, events)


if __name__ == '__main__':
    unittest.main()
