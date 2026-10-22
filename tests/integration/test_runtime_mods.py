"""Real CLI lifecycle; catches missing activation, stale replay and unsafe errors."""
import fcntl
import hashlib
import json
import os
import select
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BIN = str(Path(sys.argv.pop(1)).resolve())
WORKER = Path(BIN).with_name('bruce_logic_mod_worker_fixture')
FIXTURES = Path(__file__).resolve().parents[1] / 'fixtures/runtime_mods'
PACKAGE = FIXTURES / 'math-rational-power-v1.brucemod'
TRUST = FIXTURES / 'test-truststore-v1.pub'
ROUTE_V1 = FIXTURES / 'route-report-v1-math-rational-power-active.json'
MOD = 'user.math-rational-power'


class RuntimeModTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='bruce-runtime-cli-')
        self.addCleanup(self.directory.cleanup)
        self.work = Path(self.directory.name)
        self.root = self.work / 'mods'

    def call(self, *args, code=0, timeout=30):
        # Failure is part of the public mutation contract. Snapshot every
        # failed command against an existing root. Kernel access-time updates
        # from reads are excluded; bytes, entries, modes and write times are not.
        before = self.mutation_image(self.root) if code != 0 and self.root.exists() else None
        result = subprocess.run([BIN, *map(str, args)], capture_output=True,
                                text=True, timeout=timeout, cwd=self.work)
        if before is not None:
            self.assertEqual(self.mutation_image(self.root), before)
        self.assertEqual(result.returncode, code, result.stderr)
        self.assertEqual(result.stderr if code == 0 else result.stdout, '')
        reply = json.loads(result.stdout if code == 0 else result.stderr)
        if args[:2] == ('mods', 'trust'):
            self.assertEqual(result.stdout if code == 0 else result.stderr,
                             json.dumps(reply, separators=(',', ':')) + '\n')
        return reply

    def install(self):
        return self.call('mods', 'install', PACKAGE, '--mod-root', self.root,
                         '--trust-store', TRUST)

    def trust(self, action, *extra, code=0):
        return self.call('mods', 'trust', action, *extra, '--mod-root', self.root, code=code)

    def test_trust_import_prepare_activate_rollback_canonical_and_read_only(self):
        self.install()
        before = self.mutation_image(self.root)
        self.assertEqual(self.trust('status', code=2), {'error': 'RuntimeTrustNotImported'})
        self.assertEqual(self.mutation_image(self.root), before)
        imported = self.trust('import-v1')
        self.assertEqual(imported, {'contract': 'runtime-mod-trust-v2', 'action': 'import-v1', 'result': 'ok'})
        status = self.trust('status')
        state = status['state']
        self.assertEqual(state, {'active': {
            'generation': '00000000000000000001',
            'trust_sha256': hashlib.sha256(TRUST.read_bytes()).hexdigest()},
            'prepared': None, 'previous': None})
        self.assertEqual(set(status), {'contract', 'action', 'state'})
        self.age_access_times(self.root)
        before = self.mutation_image(self.root)
        self.assertEqual(self.trust('status'), status)
        self.assertEqual(self.mutation_image(self.root), before)
        next_trust = self.work / 'next.pub'
        next_trust.write_bytes(TRUST.read_bytes())
        next_trust.chmod(0o600)
        self.assertEqual(self.trust('prepare', next_trust),
                         {'contract': 'runtime-mod-trust-v2', 'action': 'prepare', 'result': 'ok'})
        prepared = self.trust('status')
        self.assertEqual(prepared['state']['active'], state['active'])
        self.assertEqual(prepared['state']['prepared']['generation'], '00000000000000000002')
        verified = self.trust('verify')
        self.assertEqual(verified, dict(prepared, action='verify',
                                       active_incompatible=[], prepared_incompatible=[]))
        activated = self.trust('activate')
        self.assertEqual(activated['deactivated'], [])
        self.assertEqual(activated['state']['active'], prepared['state']['prepared'])
        self.assertEqual(activated['state']['previous'], state['active'])
        rolled = self.trust('rollback')
        self.assertEqual(rolled['state']['active'], state['active'])
        self.assertEqual(self.trust('status')['state'], rolled['state'])

    def test_trust_status_absent_and_empty_roots_are_read_only(self):
        self.assertIn('error', self.trust('status', code=2))
        self.assertFalse(self.root.exists())
        self.root.mkdir(mode=0o700)
        before = self.root_image(self.root)
        self.assertIn('error', self.trust('status', code=2))
        self.assertEqual(self.root_image(self.root), before)

    def test_trust_imports_actual_legacy_data_only_root(self):
        self.install()
        self.root = self.legacy_data_only_root('legacy-import')
        self.assertTrue(all(not (self.root / name).exists() for name in (
            'logic-packages', 'logic-workers', 'logic-journal.v1', 'logic-registry.v1')))
        self.trust('import-v1')
        self.assertEqual(self.trust('status')['state']['active']['generation'], '00000000000000000001')
        self.assertEqual(self.trust('verify')['active_incompatible'], [])
        self.call('mods', 'activate', MOD, '1.0.0', '--mod-root', self.root)
        self.assertEqual(self.route()['status'], 'MATCHED')

    def test_trust_pre_fence_import_retry_preserves_legacy_host_lifecycle(self):
        for boundary in ('after_generation', 'after_prepare', 'after_snapshot'):
            with self.subTest(boundary=boundary):
                self.root = self.work / boundary
                self.install()
                self.call('mods', 'activate', MOD, '1.0.0', '--mod-root', self.root)
                names = ('truststore.pub', 'journal.v1', 'registry.v1',
                         'logic-journal.v1', 'logic-registry.v1')
                legacy = {name: (self.root / name).read_bytes() for name in names}
                self.trust('import-v1')
                journal = self.root / 'trust/journal.v1'
                data = journal.read_bytes()
                # The complete first Prepare ends before the first Commit.
                prepare = data[:data.index(b'6:Commit')]
                self.assertTrue(prepare.startswith(b'BRUCE_RUNTIME_TRUST_JOURNAL_V2\n7:Prepare'))
                for name, value in legacy.items(): (self.root / name).write_bytes(value)
                (self.root / 'trust/state.json').unlink()
                if boundary == 'after_generation':
                    journal.unlink()
                    (self.root / 'trust/pending-state.json').unlink()
                else:
                    journal.write_bytes(prepare)
                    if boundary == 'after_prepare': (self.root / 'trust/pending-state.json').unlink()
                self.assertEqual(self.route()['status'], 'MATCHED')
                self.call('mods', 'disable', MOD, '--mod-root', self.root)
                self.trust('import-v1')
                self.assertEqual(self.trust('status')['state']['active']['generation'],
                                 '00000000000000000001')
                self.assertEqual(self.trust('verify')['active_incompatible'], [])
                self.call('mods', 'activate', MOD, '1.0.0', '--mod-root', self.root)
                self.assertEqual(self.route()['status'], 'MATCHED')

    def test_trust_import_rejects_unsafe_installed_root_directories_without_migration(self):
        for layout in ('modern', 'legacy-data-only'):
            for name in ('packages', 'logic-packages', 'logic-workers'):
                for defect in ('missing', 'symlink', 'fifo'):
                    # Total absence of Logic sidecars is the valid legacy case
                    # covered above; a stray sidecar must never inherit it.
                    if layout == 'legacy-data-only' and name != 'packages' and defect == 'missing':
                        continue
                    with self.subTest(layout=layout, name=name, defect=defect):
                        tag = layout + '-' + name + '-' + defect
                        self.root = self.work / ('modern-source-' + tag)
                        self.install()
                        if layout == 'legacy-data-only':
                            self.root = self.legacy_data_only_root('legacy-root-' + tag)
                        target = self.root / name
                        saved = self.work / ('saved-directory-' + tag)
                        if target.exists(): target.rename(saved)
                        else: saved.mkdir(mode=0o700)
                        if defect == 'symlink': target.symlink_to(saved, target_is_directory=True)
                        elif defect == 'fifo': os.mkfifo(target, mode=0o600)
                        before = self.mutation_image(self.root)
                        self.assertIn(self.trust('import-v1', code=2), (
                            {'error': 'RuntimeModFileOpenFailed'}, {'error': 'LogicModFileOpenFailed'}))
                        self.assertEqual(self.mutation_image(self.root), before)
                        self.assertFalse((self.root / 'trust').exists())
                        self.assertEqual((self.root / 'truststore.pub').read_bytes(), TRUST.read_bytes())

    def test_trust_status_preserves_torn_tail_and_next_process_recovers(self):
        self.install()
        self.trust('import-v1')
        imported = self.trust('status')
        journal = self.root / 'trust/journal.v1'
        committed = journal.read_bytes()
        journal.write_bytes(committed + b'12:incomplete')
        self.assertEqual(self.trust('status', code=2), {'error': 'RuntimeTrustRecoveryRequired'})
        verified = self.trust('verify')
        self.assertEqual(verified['state'], imported['state'])
        self.assertEqual(journal.read_bytes(), committed)
        self.assertEqual(self.trust('status')['state'], imported['state'])

    def test_trust_parser_errors_are_stable_redacted_and_nonmutating(self):
        self.install()
        for tail, expected in (((), 'MissingRuntimeTrustAction'),
                (('bad/private/action',), 'UnknownRuntimeTrustAction'),
                (('status',), 'MissingOption'),
                (('prepare',), 'MissingRuntimeTrustStore'),
                (('verify', '--mod-root'), 'MissingOptionValue'),
                (('status', '--mod-root', self.root, '--mod-root', self.root), 'DuplicateOption'),
                (('activate', '--mod-root', self.root, '--deactivate-untrusted', '--no-deactivate-untrusted'), 'UnknownOption'),
                (('rollback', '--mod-root', self.root, '--deactivate-untrusted', 'false'), 'UnknownOption'),
                (('import-v1', '--mod-root', self.root, '--trust-store', '/private/key'), 'UnknownOption')):
            with self.subTest(tail=tail):
                self.assertEqual(self.call('mods', 'trust', *tail, code=2), {'error': expected})

    def test_trust_mutation_reply_needs_no_post_commit_lock(self):
        barrier = self.work / 'trust-response-barrier.so'
        source = Path(__file__).resolve().parents[1] / 'fixtures/mod_lifecycle_response_barrier.cpp'
        compiled = subprocess.run(['c++', '-std=c++20', '-shared', '-fPIC', str(source), '-ldl', '-o', str(barrier)],
                                  capture_output=True, text=True, timeout=30)
        self.assertEqual(compiled.returncode, 0, compiled.stderr)
        external = self.work / 'next.pub'
        external.write_bytes(TRUST.read_bytes()); external.chmod(0o600)
        self.install()
        for action in ('import-v1', 'prepare'):
            with self.subTest(action=action):
                journal = self.root / 'trust/journal.v1'
                lock_info = (self.root / 'registry.lock').stat()
                ready_read, ready_write = os.pipe()
                resume_read, resume_write = os.pipe()
                env = dict(os.environ, LD_PRELOAD=str(barrier), BRUCE_TEST_JOURNAL=str(journal),
                    BRUCE_TEST_JOURNAL_SIZE=str(journal.stat().st_size if journal.exists() else 0),
                    BRUCE_TEST_LOCK_INODE=str(lock_info.st_ino), BRUCE_TEST_LOCK_DEVICE=str(lock_info.st_dev),
                    BRUCE_TEST_READY_FD=str(ready_write), BRUCE_TEST_RESUME_FD=str(resume_read))
                args = [BIN, 'mods', 'trust', action]
                if action == 'prepare': args.append(str(external))
                process = subprocess.Popen([*args, '--mod-root', str(self.root)], cwd=self.work, env=env,
                    pass_fds=(ready_write, resume_read), stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                os.close(ready_write); os.close(resume_read)
                try:
                    self.assertTrue(select.select([ready_read], [], [], 10)[0], 'no committed trust barrier')
                    self.assertEqual(os.read(ready_read, 1), b'R')
                    with (self.root / 'registry.lock').open('rb') as peer:
                        fcntl.flock(peer, fcntl.LOCK_EX | fcntl.LOCK_NB)
                        os.write(resume_write, b'G')
                        try:
                            stdout, stderr = process.communicate(timeout=3)
                        except subprocess.TimeoutExpired:
                            self.fail('committed trust success waited for a post-commit lock')
                    self.assertEqual((process.returncode, stderr), (0, ''))
                    self.assertEqual(json.loads(stdout), {'contract': 'runtime-mod-trust-v2',
                        'action': action, 'result': 'ok'})
                finally:
                    os.close(ready_read); os.close(resume_write)
                    if process.poll() is None: process.kill()
                    process.communicate()

    def test_trust_help_lists_exact_supported_commands(self):
        result = subprocess.run([BIN, '--help'], capture_output=True, text=True, timeout=5)
        self.assertEqual((result.returncode, result.stderr), (0, ''))
        self.assertEqual([line for line in result.stdout.splitlines() if line.startswith('bruce mods trust ')], [
            'bruce mods trust status --mod-root ROOT',
            'bruce mods trust import-v1 --mod-root ROOT',
            'bruce mods trust prepare NEXT_TRUST.pub --mod-root ROOT',
            'bruce mods trust verify --mod-root ROOT',
            'bruce mods trust activate --mod-root ROOT [--deactivate-untrusted]',
            'bruce mods trust rollback --mod-root ROOT [--deactivate-untrusted]'])

    def test_trust_failed_transitions_and_unsafe_inputs_preserve_root(self):
        self.install()
        self.trust('import-v1')
        self.assertEqual(self.trust('import-v1', code=2), {'error': 'RuntimeTrustAlreadyImported'})
        for action in ('activate', 'rollback'):
            self.assertEqual(self.trust(action, code=2), {'error': 'RuntimeTrustTransitionMissing'})
        external = self.work / 'external.pub'
        alias = self.work / 'external-parent'
        alias.symlink_to(self.work, target_is_directory=True)
        for defect in ('absent', 'directory', 'fifo', 'symlink', 'permissions', 'malformed', 'traversal'):
            with self.subTest(defect=defect):
                if external.exists() or external.is_symlink():
                    if external.is_dir(): external.rmdir()
                    else: external.unlink()
                path = external
                if defect == 'directory': external.mkdir(mode=0o700)
                elif defect == 'fifo': os.mkfifo(external, mode=0o600)
                elif defect == 'symlink': external.symlink_to(TRUST)
                elif defect in ('permissions', 'malformed', 'traversal'):
                    external.write_bytes(b'private key bytes must never escape' if defect == 'malformed' else TRUST.read_bytes())
                    external.chmod(0o644 if defect == 'permissions' else 0o600)
                    if defect == 'traversal': path = str(alias) + '/../' + self.work.name + '/external.pub'
                reply = self.trust('prepare', path, code=2)
                self.assertEqual(set(reply), {'error'})
                self.assertRegex(reply['error'], r'^[A-Za-z0-9]+$')
                self.assertNotIn(str(self.work), json.dumps(reply))
                self.assertNotIn('private key', json.dumps(reply))

    def test_trust_status_busy_corrupt_pending_and_unsafe_are_read_only(self):
        for defect in ('busy', 'corrupt', 'pending', 'symlink', 'fifo', 'missing-lock'):
            with self.subTest(defect=defect):
                self.root = self.work / defect
                self.install()
                self.trust('import-v1')
                target = self.root / 'trust/state.json'
                if defect == 'corrupt': target.write_bytes(b'private internal journal data')
                elif defect == 'pending':
                    journal = self.root / 'trust/journal.v1'
                    journal.write_bytes(journal.read_bytes() + b'12:incomplete')
                elif defect == 'symlink':
                    target.unlink(); target.symlink_to(self.work / 'private-target')
                elif defect == 'fifo':
                    target.unlink(); os.mkfifo(target, mode=0o600)
                elif defect == 'missing-lock': (self.root / 'registry.lock').unlink()
                self.age_access_times(self.root)
                before = self.mutation_image(self.root)
                if defect == 'busy':
                    with (self.root / 'registry.lock').open('rb') as lock:
                        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                        self.call('mods', 'trust', 'status', '--mod-root', self.root, code=2, timeout=3)
                else:
                    self.trust('status', code=2)
                self.assertEqual(self.mutation_image(self.root), before)

    def test_trust_status_smallest_root_and_symlink_components(self):
        self.install()
        self.trust('import-v1')
        # Run inside the root: a one-character existing root is sufficient.
        result = subprocess.run([BIN, 'mods', 'trust', 'status', '--mod-root', '.'],
                                cwd=self.root, capture_output=True, text=True, timeout=5)
        self.assertEqual((result.returncode, result.stderr), (0, ''))
        self.assertEqual(json.loads(result.stdout)['state']['active']['generation'], '00000000000000000001')
        alias = self.work / 'alias'
        alias.symlink_to(self.root, target_is_directory=True)
        for path in (str(alias), str(alias) + '/.', str(alias) + '/../mods'):
            self.assertEqual(self.call('mods', 'trust', 'status', '--mod-root', path, code=2),
                             {'error': 'RuntimeModFileOpenFailed'})
        for kind in ('regular', 'fifo'):
            target = self.work / ('root-' + kind)
            if kind == 'fifo': os.mkfifo(target, mode=0o600)
            else:
                target.write_bytes(b'not a directory'); target.chmod(0o600)
            before = self.mutation_image(self.work)
            self.assertEqual(self.call('mods', 'trust', 'status', '--mod-root', target, code=2),
                             {'error': 'RuntimeModFileOpenFailed'})
            self.assertEqual(self.mutation_image(self.work), before)

    def test_trust_transitions_reject_ancestor_aliases_before_any_mutation(self):
        external = self.work / 'next.pub'
        external.write_bytes(TRUST.read_bytes()); external.chmod(0o600)
        alias = self.work / 'ancestor-alias'
        alias.symlink_to(self.work, target_is_directory=True)
        for action in ('activate', 'rollback'):
            for emergency in (False, True):
                for suffix in ('/mods', '/./mods', '/../' + self.work.name + '/mods'):
                    with self.subTest(action=action, emergency=emergency, suffix=suffix):
                        self.root = self.work / 'mods'
                        if not self.root.exists(): self.install()
                        if not (self.root / 'trust').exists(): self.trust('import-v1')
                        self.trust('prepare', external)
                        if action == 'rollback': self.trust('activate')
                        args = ['mods', 'trust', action, '--mod-root', str(alias) + suffix]
                        if emergency: args.append('--deactivate-untrusted')
                        self.assertEqual(self.call(*args, code=2), {'error': 'RuntimeModFileOpenFailed'})

    def test_trust_failed_admission_never_creates_missing_lock(self):
        external = self.work / 'next.pub'
        external.write_bytes(TRUST.read_bytes()); external.chmod(0o600)
        for fixture in ('empty', 'missing-lock', 'symlink-lock', 'fifo-lock'):
            for action in ('import-v1', 'prepare', 'verify', 'activate', 'rollback'):
                with self.subTest(fixture=fixture, action=action):
                    self.root = self.work / (fixture + '-' + action)
                    if fixture == 'empty': self.root.mkdir(mode=0o700)
                    else:
                        self.install(); self.trust('import-v1')
                        (self.root / 'registry.lock').unlink()
                        if fixture == 'symlink-lock':
                            outside = self.work / ('outside-lock-' + action)
                            outside.write_bytes(b''); outside.chmod(0o600)
                            (self.root / 'registry.lock').symlink_to(outside)
                        elif fixture == 'fifo-lock': os.mkfifo(self.root / 'registry.lock', mode=0o600)
                    extra = [external] if action == 'prepare' else []
                    expected = 'UnsafeRuntimeModFilePermissions' if fixture == 'fifo-lock' else 'RuntimeModFileOpenFailed'
                    self.assertEqual(self.trust(action, *extra, code=2), {'error': expected})
                    if fixture in ('empty', 'missing-lock'):
                        self.assertFalse((self.root / 'registry.lock').exists())

    def test_trust_rejections_preserve_pending_recovery_bytes(self):
        for defect, action in (('duplicate', 'import-v1'), ('absent-file', 'prepare'),
                ('malformed-file', 'prepare'), ('missing-target', 'activate'),
                ('missing-target', 'rollback'), ('untrusted-active', 'activate'),
                ('bad-package', 'verify')):
            with self.subTest(defect=defect, action=action):
                self.root = self.work / (defect + '-' + action)
                self.install(); self.trust('import-v1')
                external = self.work / (defect + '-' + action + '.pub')
                extra = []
                if defect == 'untrusted-active':
                    self.call('mods', 'activate', MOD, '1.0.0', '--mod-root', self.root)
                    external.write_bytes(b'BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n'); external.chmod(0o600)
                    self.trust('prepare', external)
                if action == 'prepare':
                    extra = [external]
                    if defect == 'malformed-file':
                        external.write_bytes(b'private invalid trust bytes'); external.chmod(0o600)
                if defect == 'bad-package':
                    next((self.root / 'packages').iterdir()).write_bytes(b'corrupt installed package')
                journal = self.root / 'trust/journal.v1'
                journal.write_bytes(journal.read_bytes() + b'12:incomplete')
                expected = ('RuntimeTrustAlreadyImported' if defect == 'duplicate' else
                    'RuntimeTrustInstalledPackageIntegrity' if defect == 'bad-package' else 'RuntimeTrustRecoveryRequired')
                self.assertEqual(self.trust(action, *extra, code=2), {'error': expected})
                self.assertTrue(journal.read_bytes().endswith(b'12:incomplete'))

    def test_trust_verify_declines_unprojected_commit_without_mutation(self):
        self.install(); self.trust('import-v1')
        journal = self.root / 'trust/journal.v1'
        prefix, projected_record = journal.read_bytes().rstrip(b'\n').rsplit(b'\n', 1)
        self.assertTrue(projected_record.startswith(b'9:Projected'))
        # Simulate a commit whose projections exist but whose final Projected
        # acknowledgment was not appended. Trust admission is conservative.
        journal.write_bytes(prefix + b'\n')
        self.assertEqual(self.trust('verify', code=2), {'error': 'RuntimeTrustRecoveryRequired'})

    def build_rotation_package(self, version, *, logic=False):
        # RFC 8032 vector 2: deterministic public test material, independent of
        # the original fixture's signing key. Build real signed packages.
        tag = ('logic-' if logic else 'data-') + version
        source = self.work / ('source-' + tag)
        source.mkdir()
        signing = self.work / ('signing-' + tag + '.env')
        signing.write_text('BRUCE_MOD_ED25519_SEED_HEX='
                           '4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb\n')
        signing.chmod(0o600)
        identifier = 'user.rotation-logic' if logic else 'user.rotation-data'
        (source / 'manifest.json').write_text(json.dumps({
            'mod_id': identifier, 'signing_key_id': 'next-key', 'version': version},
            sort_keys=True, separators=(',', ':')))
        alias = 'hoch' if logic else 'datarotate'
        template = 'math.rational.power.v1' if logic else identifier + '.template'
        route_input = '3/4 hoch 2' if logic else alias
        pattern = ['<rational>', 'hoch', '<integer>'] if logic else [alias]
        (source / 'tree.json').write_text(json.dumps({'mount': [1, 1], 'nodes': [{
            'aliases': [alias], 'claim_scope': 'candidate_only', 'id': [1, 1, 8 if logic else 7],
            'key': alias, 'route_pattern': pattern, 'template_id': template}]},
            sort_keys=True, separators=(',', ':')))
        (source / 'golden-cases.json').write_text(json.dumps([
            {'input': route_input, 'status': 'MATCHED', 'template_id': template}],
            sort_keys=True, separators=(',', ':')))
        package, trust = self.work / (tag + '.brucemod'), self.work / (tag + '.pub')
        builder = Path(BIN).with_name('bruce-mod-builder')
        result = subprocess.run([builder, 'data', source, '--output', package,
            '--trust-store-output', trust, '--signing-env', signing], capture_output=True, text=True, timeout=30)
        self.assertEqual((result.returncode, result.stderr), (0, ''))
        if logic:
            logic_source = self.work / ('logic-source-' + tag)
            logic_source.mkdir()
            (logic_source / 'data.brucemod').write_bytes(package.read_bytes())
            worker = WORKER.read_bytes()
            (logic_source / 'worker.bin').write_bytes(worker)
            (logic_source / 'logic.json').write_text(json.dumps({
                'capabilities': ['worker.execute'], 'template_ids': [template], 'worker_path': 'worker.bin',
                'worker_sha256': hashlib.sha256(worker).hexdigest()}, sort_keys=True, separators=(',', ':')))
            package, trust = self.work / (tag + '.logic.brucemod'), self.work / (tag + '.logic.pub')
            result = subprocess.run([builder, 'logic', logic_source, '--output', package,
                '--trust-store-output', trust, '--signing-env', signing], capture_output=True, text=True, timeout=30)
            self.assertEqual((result.returncode, result.stderr), (0, ''))
        return package, trust, identifier, template

    def test_trust_rotation_enforces_active_keys_on_data_logic_and_restarts(self):
        data, next_trust, data_id, _ = self.build_rotation_package('1.0.0')
        logic, logic_trust, logic_id, template = self.build_rotation_package('1.0.0', logic=True)
        self.assertEqual(logic_trust.read_bytes(), next_trust.read_bytes())
        self.install()
        self.call('mods', 'activate', MOD, '1.0.0', '--mod-root', self.root)
        self.trust('import-v1')
        self.trust('prepare', next_trust)
        self.assertEqual(self.route()['status'], 'MATCHED', 'prepare activated the next key')
        verified = self.trust('verify')
        affected = [{'kind': 'data', 'mod_id': MOD, 'version': '1.0.0', 'signing_key_id': 'test-rfc8032'}]
        self.assertEqual(verified['prepared_incompatible'], affected)
        for package in (data, logic):
            self.assertEqual(self.call('mods', 'install', package, '--trust-store', next_trust,
                '--mod-root', self.root, code=2), {'error': 'RuntimeModTrustStoreChanged'})
        self.assertEqual(self.trust('activate', code=2), {
            'error': 'RuntimeTrustActiveModsIncompatible', 'affected': affected})
        changed = self.trust('activate', '--deactivate-untrusted')
        self.assertEqual(changed['deactivated'], affected)
        self.assertEqual(self.route()['status'], 'UNKNOWN')
        # Each CLI call is a fresh process: status and route prove restart state.
        self.assertEqual(self.trust('status')['state'], changed['state'])
        for action in ('install', 'update'):
            self.assertEqual(self.call('mods', action, PACKAGE, '--trust-store', TRUST,
                '--mod-root', self.root, code=2), {'error': 'RuntimeModTrustStoreChanged'})
            self.assertEqual(self.call('mods', action, PACKAGE, '--trust-store', next_trust,
                '--mod-root', self.root, code=2), {'error': 'RuntimeModSigningKeyUntrusted'})
        self.assertEqual(self.call('mods', 'activate', MOD, '1.0.0', '--mod-root', self.root, code=2),
                         {'error': 'RuntimeModSigningKeyUntrusted'})
        for package, identifier in ((data, data_id), (logic, logic_id)):
            self.call('mods', 'install', package, '--trust-store', next_trust, '--mod-root', self.root)
            self.call('mods', 'activate', identifier, '1.0.0', '--mod-root', self.root)
        routed = self.call('route', '--text', '3/4 hoch 2', '--mod-details', '--mod-root', self.root)
        self.assertEqual(routed['selected_owner']['id'], logic_id)
        run = self.call('mods', 'run', logic_id, '--template', template, '--input', '3/4 hoch 2', '--mod-root', self.root)
        self.assertEqual((run['accepted'], run['result']), (True, '9/16'))
        for is_logic in (False, True):
            package, trust, _, _ = self.build_rotation_package('2.0.0', logic=is_logic)
            self.call('mods', 'update', package, '--trust-store', trust, '--mod-root', self.root)
        blocked = self.trust('rollback', code=2)
        self.assertEqual(blocked, {'error': 'RuntimeTrustActiveModsIncompatible', 'affected': [
            {'kind': 'data', 'mod_id': data_id, 'version': '2.0.0', 'signing_key_id': 'next-key'},
            {'kind': 'logic', 'mod_id': logic_id, 'version': '2.0.0', 'signing_key_id': 'next-key'}]})
        self.assertNotIn(str(self.root), json.dumps(blocked))
        self.assertNotIn('brucemod', json.dumps(blocked))
        self.assertNotIn(next_trust.read_text().split(':', 1)[1].strip(), json.dumps(blocked))
        rolled = self.trust('rollback', '--deactivate-untrusted')
        self.assertEqual({r['mod_id'] for r in rolled['deactivated']}, {data_id, logic_id})
        self.assertEqual(self.trust('status')['state'], rolled['state'])
        self.assertEqual(self.call('route', '--text', '3/4 hoch 2', '--mod-details', '--mod-root', self.root)['status'], 'UNKNOWN')
        for identifier in (data_id, logic_id):
            self.assertEqual(self.call('mods', 'activate', identifier, '2.0.0', '--mod-root', self.root, code=2),
                             {'error': 'RuntimeModSigningKeyUntrusted'})
        self.assertEqual(self.call('mods', 'run', logic_id, '--template', template, '--input', 'x',
            '--mod-root', self.root, code=2), {'error': 'LogicModNotActive'})
        for package in (data, logic):
            for action in ('install', 'update'):
                self.assertEqual(self.call('mods', action, package, '--trust-store', next_trust,
                    '--mod-root', self.root, code=2), {'error': 'RuntimeModTrustStoreChanged'})
                self.assertEqual(self.call('mods', action, package, '--trust-store', TRUST,
                    '--mod-root', self.root, code=2), {'error': 'RuntimeModSigningKeyUntrusted'})
        incompatible = self.trust('verify')['active_incompatible']
        self.assertEqual({r['kind'] for r in incompatible}, {'data', 'logic'})
        self.assertTrue(all(r['signing_key_id'] == 'next-key' for r in incompatible))

    def legacy_data_only_root(self, name):
        legacy = self.work / name
        legacy.mkdir(mode=0o700)
        (legacy / 'packages').mkdir(mode=0o700)
        for filename in ('truststore.pub', 'registry.v1', 'journal.v1', 'registry.lock'):
            (legacy / filename).write_bytes((self.root / filename).read_bytes())
            (legacy / filename).chmod(0o600)
        for source in (self.root / 'packages').iterdir():
            target = legacy / 'packages' / source.name
            target.write_bytes(source.read_bytes())
            target.chmod(0o600)
        return legacy

    def route(self, *extra):
        return self.call('route', '--text', '3/4 hoch 2', '--mod-root', self.root, *extra)

    def test_signed_builtin_packages_never_create_install_or_update_roots(self):
        # Public RFC 8032 test-vector seed, never a production signing key.
        signing = self.work / 'signing.env'
        signing.write_text('BRUCE_MOD_ED25519_SEED_HEX='
                           '9d61b19deffd5a60ba844af492ec2cc4'
                           '4449c5697b326919703bac031cae7f60\n')
        signing.chmod(0o600)
        sources = Path(__file__).resolve().parents[2] / 'examples/local-data-mods/math-rational-power'
        for identifier in ('language', 'nanocell', 'mcp'):
            source = self.work / identifier
            source.mkdir()
            (source / 'manifest.json').write_text(json.dumps({
                'mod_id': identifier, 'signing_key_id': 'test-rfc8032', 'version': '1.0.0'},
                sort_keys=True, separators=(',', ':')))
            for name in ('tree.json', 'golden-cases.json'):
                (source / name).write_bytes((sources / name).read_bytes())
            package, trust = self.work / (identifier + '.brucemod'), self.work / (identifier + '.pub')
            built = subprocess.run([str(Path(BIN).with_name('bruce-mod-builder')), 'data', str(source),
                '--output', str(package), '--trust-store-output', str(trust),
                '--signing-env', str(signing)], capture_output=True, text=True, timeout=30)
            self.assertEqual(built.returncode, 0, built.stderr)
            self.assertEqual(built.stderr, '')
            self.assertEqual(self.call('mods', 'verify', package, '--trust-store', trust)['mod_id'],
                             identifier)
            for action in ('install', 'update'):
                with self.subTest(identifier=identifier, action=action):
                    root = self.work / (identifier + '-' + action + '-missing')
                    self.assertFalse(root.exists())
                    self.assertEqual(self.call('mods', action, package, '--mod-root', root,
                        '--trust-store', trust, code=2), {'error': 'BuiltinModImmutable'})
                    self.assertFalse(root.exists(), 'rejected Built-in package created its root')

    def test_core_manifest_schema_rejection_never_creates_roots(self):
        # Core is already forbidden by the V1 manifest schema, before signature
        # verification. Preserve that earlier defense instead of expanding it.
        header, envelope = PACKAGE.read_bytes().split(b'\n', 1)
        name_prefix = b'13:manifest.json'
        self.assertTrue(envelope.startswith(name_prefix))
        length, content = envelope[len(name_prefix):].split(b':', 1)
        manifest = content[:int(length)]
        core_manifest = manifest.replace(b'"mod_id":"user.math-rational-power"', b'"mod_id":"core"')
        self.assertNotEqual(core_manifest, manifest)
        package = self.work / 'core-invalid-schema.brucemod'
        package.write_bytes(header + b'\n' + name_prefix + str(len(core_manifest)).encode() + b':'
                            + core_manifest + content[int(length):])
        for action in ('install', 'update'):
            with self.subTest(action=action):
                root = self.work / ('core-' + action + '-missing')
                self.assertFalse(root.exists())
                self.assertEqual(self.call('mods', action, package, '--mod-root', root,
                    '--trust-store', TRUST, code=2), {'error': 'InvalidRuntimeModSchema'})
                self.assertFalse(root.exists())

    def test_full_lifecycle_and_snapshot_replay(self):
        verified = self.call('mods', 'verify', PACKAGE, '--trust-store', TRUST)
        self.assertEqual(verified['mod_id'], MOD)
        self.assertEqual(verified['package_digest'], hashlib.sha256(PACKAGE.read_bytes()).hexdigest())
        self.install()
        listed = self.call('mods', 'list', '--mod-root', self.root)
        self.assertEqual(listed['contract'], 'bruce-mod-catalog-v2')
        records = [entry for entry in listed['entries'] if entry['origin'] == 'installed']
        self.assertEqual(records[0]['lifecycle_state'], 'installed_inactive')
        status = self.call('mods', 'status', MOD, '1.0.0', '--mod-root', self.root)
        self.assertEqual(status['entries'], records)
        self.assertEqual(self.route()['status'], 'UNKNOWN')
        self.call('mods', 'activate', MOD, '1.0.0', '--mod-root', self.root)
        report = self.work / 'route.report'
        routed = self.route('--output', report)
        self.assertEqual(routed['contract'], 'route-report-v1')
        self.assertEqual(routed['status'], 'MATCHED')
        self.assertEqual(routed['candidate_addresses'], [[1, 1, 5]])
        self.assertEqual(routed['selected_template'], 'math.rational.power.v1')
        self.assertEqual(routed['active_packages'], [{
            'mod_id': MOD, 'version': '1.0.0',
            'package_digest': hashlib.sha256(PACKAGE.read_bytes()).hexdigest()}])
        self.assertEqual(json.loads(report.read_text()), routed)
        self.assertEqual(report.read_text(), json.dumps(routed, separators=(',', ':')) + '\n')
        # Historical bytes, not regenerated from the current serializer: fresh
        # registry, only the signed v1 rational-power package installed/active.
        # Pin field order, registry/package digests and the final LF as well.
        expected_v1 = ROUTE_V1.read_bytes()
        self.assertEqual(report.read_bytes(), expected_v1)
        stdout_v1 = subprocess.run(
            [BIN, 'route', '--text', '3/4 hoch 2', '--mod-root', str(self.root)],
            capture_output=True, timeout=30, cwd=self.work)
        self.assertEqual(stdout_v1.returncode, 0, stdout_v1.stderr)
        self.assertEqual(stdout_v1.stderr, b'')
        self.assertEqual(stdout_v1.stdout, expected_v1)
        self.assertEqual(self.call('route', 'replay', ROUTE_V1, '--mod-root', self.root), routed)
        replay = self.call('route', 'replay', report, '--mod-root', self.root)
        self.assertEqual(replay, routed)
        details_report = self.work / 'route-details.report'
        details = self.call('route', '--text', '3/4 hoch 2', '--mod-root', self.root,
                            '--mod-details', '--output', details_report)
        self.assertEqual(details['contract'], 'route-report-v2')
        self.assertEqual(details['selected_owner'], {
            'id': MOD, 'origin': 'installed', 'kind': 'data', 'version': '1.0.0',
            'package_digest': hashlib.sha256(PACKAGE.read_bytes()).hexdigest()})
        self.assertEqual(details['candidates'][0]['owner'], details['selected_owner'])
        self.assertEqual(self.call('route', 'replay', details_report, '--mod-root', self.root,
                                   '--mod-details'), details)
        core = self.call('route', '--text', '3/4 plus 5/6', '--mod-root', self.root,
                         '--mod-details')
        self.assertEqual(core['selected_owner'], {
            'id': 'core', 'origin': 'builtin', 'kind': 'builtin', 'version': None,
            'package_digest': None})
        unknown = self.call('route', '--text', 'not a route', '--mod-root', self.root,
                            '--mod-details')
        self.assertEqual((unknown['candidates'], unknown['selected_owner']), ([], None))
        self.call('mods', 'disable', MOD, '--mod-root', self.root)
        self.assertEqual(self.route()['status'], 'UNKNOWN')
        mismatch = self.call('route', 'replay', report, '--mod-root', self.root, code=2)
        self.assertEqual(mismatch['error'], 'RouteRegistryDigestMismatch')

    def test_rootless_catalog_is_read_only_and_reports_all_builtins(self):
        before = sorted(path.name for path in self.work.iterdir())
        listed = self.call('mods', 'list')
        self.assertEqual(listed['contract'], 'bruce-mod-catalog-v2')
        self.assertEqual([entry['id'] for entry in listed['entries']],
                         ['language', 'mcp', 'nanocell'])
        self.assertTrue(all(entry['origin'] == 'builtin' for entry in listed['entries']))
        self.assertTrue(all(entry['build_state'] == 'available' for entry in listed['entries']))
        self.assertEqual(sorted(path.name for path in self.work.iterdir()), before)

        missing = self.work / 'missing-root'
        self.assertIn('error', self.call('mods', 'list', '--mod-root', missing, code=2))
        self.assertFalse(missing.exists())

        empty = self.work / 'empty-root'
        empty.mkdir(mode=0o700)
        for action in (('list',), ('status', 'mcp')):
            before = self.root_image(empty)
            self.assertIn('error', self.call('mods', *action, '--mod-root', empty, code=2))
            self.assertEqual(self.root_image(empty), before)

        status = self.call('mods', 'status', 'mcp')
        self.assertEqual([entry['id'] for entry in status['entries']], ['mcp'])
        self.assertEqual(self.call('mods', 'status', 'unknown', code=2)['error'], 'ModNotFound')

    def mutation_image(self, root):
        return {name: (metadata[:6] + metadata[7:], content)
                for name, (metadata, content) in self.root_image(root).items()}

    def root_image(self, root):
        """Capture bytes/entries/metadata without changing access times ourselves."""
        image = {}

        def visit(path):
            content = None
            if path.is_symlink():
                # readlink itself can update symlink atime; lstat identity,
                # size and change time detect replacement without touching it.
                content = '<symlink>'
            elif path.is_dir():
                fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOATIME)
                try:
                    names = sorted(os.listdir(fd))
                finally:
                    os.close(fd)
                content = tuple(names)
                for name in names:
                    visit(path / name)
            elif path.is_file():
                fd = os.open(path, os.O_RDONLY | os.O_NOATIME)
                with os.fdopen(fd, 'rb') as stream:
                    content = stream.read()
            info = path.lstat()
            metadata = (info.st_mode, info.st_uid, info.st_gid, info.st_nlink,
                        info.st_size, info.st_ino, info.st_atime_ns,
                        info.st_mtime_ns, info.st_ctime_ns)
            image[str(path.relative_to(root))] = (metadata, content)

        visit(root)
        return image

    def age_access_times(self, root):
        for path in [*root.rglob('*'), root]:
            info = path.lstat()
            if not path.is_symlink():
                os.utime(path, ns=(1_000_000_000, info.st_mtime_ns))

    def assert_catalog_unchanged(self, root, *, code=0):
        self.age_access_times(root)
        before = self.root_image(root)
        for action in (('list',), ('status', MOD, '1.0.0')):
            try:
                result = self.call('mods', *action, '--mod-root', root, code=code)
                if code == 0:
                    records = [entry for entry in result['entries']
                               if entry['origin'] == 'installed']
                    self.assertEqual([(entry['id'], entry['version'], entry['kind'])
                                      for entry in records], [(MOD, '1.0.0', 'data')])
                else:
                    self.assertIn('error', result)
            finally:
                self.assertEqual(self.root_image(root), before)

    def test_catalog_reads_modern_root_without_metadata_changes(self):
        self.install()
        self.assert_catalog_unchanged(self.root)

    def test_catalog_reads_legacy_data_only_root_without_logic_initialization(self):
        self.install()
        legacy = self.legacy_data_only_root('legacy')
        self.assert_catalog_unchanged(legacy)

    def test_legacy_data_only_root_keeps_data_lifecycle_without_logic_initialization(self):
        self.install()
        legacy = self.legacy_data_only_root('legacy-lifecycle')

        logic_sidecars = ('logic-packages', 'logic-workers', 'logic-journal.v1',
                          'logic-registry.v1')
        self.call('mods', 'activate', MOD, '1.0.0', '--mod-root', legacy)
        self.assertTrue(all(not (legacy / name).exists() for name in logic_sidecars))
        active = self.call('mods', 'status', MOD, '1.0.0', '--mod-root', legacy)
        self.assertEqual(active['entries'][0]['lifecycle_state'], 'active')

        self.call('mods', 'disable', MOD, '--mod-root', legacy)
        self.assertTrue(all(not (legacy / name).exists() for name in logic_sidecars))
        disabled = self.call('mods', 'status', MOD, '1.0.0', '--mod-root', legacy)
        self.assertEqual(disabled['entries'][0]['lifecycle_state'], 'disabled')

        self.call('mods', 'activate', MOD, '1.0.0', '--mod-root', legacy)
        self.assertTrue(all(not (legacy / name).exists() for name in logic_sidecars))
        before_update = self.root_image(legacy)
        self.call('mods', 'update', PACKAGE, '--trust-store', TRUST, '--mod-root', legacy)
        self.assertEqual(self.root_image(legacy), before_update)
        self.call('mods', 'update', PACKAGE, '--trust-store', TRUST, '--mod-root', legacy)
        self.assertEqual(self.root_image(legacy), before_update)

    def test_update_rejects_partial_logic_sidecars_before_mutating_legacy_root(self):
        self.install()
        legacy = self.legacy_data_only_root('legacy-partial-logic')
        (legacy / 'logic-packages').mkdir(mode=0o700)
        before = self.root_image(legacy)
        first = self.call('mods', 'update', PACKAGE, '--trust-store', TRUST,
                          '--mod-root', legacy, code=2)
        self.assertEqual(first, {'error': 'RuntimeModFileOpenFailed'})
        self.assertEqual(self.root_image(legacy), before)
        second = self.call('mods', 'update', PACKAGE, '--trust-store', TRUST,
                           '--mod-root', legacy, code=2)
        self.assertEqual(second, first)
        self.assertEqual(self.root_image(legacy), before)

    def test_catalog_rejects_recovery_needed_state_without_repair(self):
        for name in ('journal.v1', 'logic-journal.v1', 'registry.v1', 'logic-registry.v1'):
            with self.subTest(name=name):
                self.root = self.work / name.replace('.', '-')
                self.install()
                target = self.root / name
                if 'journal' in name:
                    target.write_bytes(target.read_bytes() + b'12:incomplete')
                else:
                    target.write_bytes(b'BRUCE_' + (b'LOGIC_MOD' if name.startswith('logic')
                                       else b'RUNTIME_MOD') + b'_REGISTRY_V1\n1:01:0\n')
                    # A canonical old generation is recoverable for Data; missing
                    # snapshot is recoverable for either registry.
                    if name.startswith('logic'):
                        target.unlink()
                self.assert_catalog_unchanged(self.root, code=2)

    def test_catalog_rejects_unsafe_or_malformed_state_without_mutation(self):
        for defect in ('malformed', 'partial-logic', 'missing-lock', 'permissions', 'symlink', 'fifo'):
            with self.subTest(defect=defect):
                self.root = self.work / defect
                self.install()
                if defect == 'malformed':
                    (self.root / 'journal.v1').write_bytes(b'not a journal\n')
                elif defect == 'partial-logic':
                    (self.root / 'logic-journal.v1').unlink()
                elif defect == 'missing-lock':
                    (self.root / 'registry.lock').unlink()
                elif defect == 'permissions':
                    next((self.root / 'packages').iterdir()).chmod(0o644)
                elif defect == 'symlink':
                    target = self.root / 'registry.v1'
                    outside = self.work / 'outside'
                    outside.write_bytes(target.read_bytes())
                    target.unlink()
                    target.symlink_to(outside)
                else:
                    target = self.root / 'journal.v1'
                    target.unlink()
                    os.mkfifo(target, mode=0o600)
                self.assert_catalog_unchanged(self.root, code=2)

    def test_catalog_rejects_busy_root_without_waiting_or_mutation(self):
        self.install()
        with (self.root / 'registry.lock').open('rb') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.age_access_times(self.root)
            before = self.root_image(self.root)
            for action in (('list',), ('status', MOD)):
                try:
                    result = self.call('mods', *action, '--mod-root', self.root,
                                       code=2, timeout=3)
                    self.assertIn('error', result)
                finally:
                    self.assertEqual(self.root_image(self.root), before)

    def test_catalog_rejects_symlink_root_components_without_metadata_changes(self):
        self.install()
        alias = self.work / 'alias'
        ancestor = self.work / 'alias-parent'
        alias.symlink_to(self.root, target_is_directory=True)
        ancestor.symlink_to(self.work, target_is_directory=True)
        for path, link in ((str(alias), alias), (str(alias) + '/', alias),
                           (str(alias) + '/.', alias), (str(ancestor / 'mods'), ancestor)):
            for action in (('list',), ('status', MOD)):
                with self.subTest(path=path, action=action):
                    self.age_access_times(self.root)
                    info = link.lstat()
                    os.utime(link, ns=(1_000_000_000, info.st_mtime_ns), follow_symlinks=False)
                    before_link = link.lstat()
                    before_root = self.root_image(self.root)
                    try:
                        self.assertIn('error', self.call('mods', *action, '--mod-root', path, code=2))
                    finally:
                        self.assertEqual(link.lstat(), before_link)
                        self.assertEqual(self.root_image(self.root), before_root)

    def test_lifecycle_reply_survives_peer_lock_after_activate_and_update_commit(self):
        barrier = self.work / 'lifecycle-response-barrier.so'
        source = Path(__file__).resolve().parents[1] / 'fixtures/mod_lifecycle_response_barrier.cpp'
        compiled = subprocess.run(['c++', '-std=c++20', '-shared', '-fPIC', str(source), '-ldl', '-o', str(barrier)],
                                  capture_output=True, text=True, timeout=30)
        self.assertEqual(compiled.returncode, 0, compiled.stderr)
        for action in ('activate', 'update'):
            with self.subTest(action=action):
                self.root = self.work / action
                self.install()
                journal = self.root / 'journal.v1'
                old_size = journal.stat().st_size
                lock_info = (self.root / 'registry.lock').stat()
                ready_read, ready_write = os.pipe()
                resume_read, resume_write = os.pipe()
                env = dict(os.environ, LD_PRELOAD=str(barrier),
                           BRUCE_TEST_JOURNAL=str(journal), BRUCE_TEST_JOURNAL_SIZE=str(old_size),
                           BRUCE_TEST_LOCK_INODE=str(lock_info.st_ino),
                           BRUCE_TEST_LOCK_DEVICE=str(lock_info.st_dev),
                           BRUCE_TEST_READY_FD=str(ready_write),
                           BRUCE_TEST_RESUME_FD=str(resume_read))
                args = ['mods', 'activate', MOD, '1.0.0'] if action == 'activate' else [
                    'mods', 'update', str(PACKAGE), '--trust-store', str(TRUST)]
                process = subprocess.Popen([BIN, *args, '--mod-root', str(self.root)],
                                           cwd=self.work, env=env, pass_fds=(ready_write, resume_read),
                                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                os.close(ready_write)
                os.close(resume_read)
                try:
                    self.assertTrue(select.select([ready_read], [], [], 10)[0],
                                    'CLI did not reach the committed-mutation barrier')
                    self.assertEqual(os.read(ready_read, 1), b'R')
                    self.assertGreater(journal.stat().st_size, old_size)
                    with (self.root / 'registry.lock').open('rb') as peer:
                        fcntl.flock(peer, fcntl.LOCK_EX | fcntl.LOCK_NB)
                        os.write(resume_write, b'G')
                        events = select.select([ready_read, process.stdout], [], [], 10)[0]
                        self.assertTrue(events, 'CLI neither attempted a reply lock nor returned a reply')
                        if ready_read in events:
                            operation = os.read(ready_read, 1)
                            self.assertIn(operation, (b'B', b'N'))
                            if operation == b'B':
                                fcntl.flock(peer, fcntl.LOCK_UN)
                        stdout, stderr = process.communicate(timeout=10)
                    self.assertEqual(process.returncode, 0, stderr)
                    self.assertEqual(stderr, '')
                    records = [entry for entry in json.loads(stdout)['entries'] if entry['id'] == MOD]
                    self.assertEqual([(entry['version'], entry['lifecycle_state']) for entry in records],
                                     [('1.0.0', 'active')])
                    if action == 'update':
                        committed = journal.read_bytes()
                        self.call('mods', 'update', PACKAGE, '--trust-store', TRUST,
                                  '--mod-root', self.root)
                        self.assertEqual(journal.read_bytes(), committed)
                finally:
                    os.close(ready_read)
                    os.close(resume_write)
                    if process.poll() is None:
                        process.kill()
                    process.communicate()

    def test_untrusted_package_never_creates_installation(self):
        empty = self.work / 'empty.pub'
        empty.write_text('BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n')
        for action in ('verify', 'install'):
            args = ['mods', action, PACKAGE, '--trust-store', empty]
            if action == 'install':
                args += ['--mod-root', self.root]
            self.assertEqual(self.call(*args, code=2)['error'], 'RuntimeModSigningKeyUntrusted')
        self.assertFalse(self.root.exists())

    def test_trust_store_boundary_rejects_special_and_symlinked_inputs_without_mutation(self):
        regular = self.work / 'trust.pub'
        regular.write_bytes(TRUST.read_bytes())
        final_symlink = self.work / 'trust-link.pub'
        final_symlink.symlink_to(regular)
        directory = self.work / 'trust-directory'
        directory.mkdir()
        fifo = self.work / 'trust-fifo'
        os.mkfifo(fifo)
        parent = self.work / 'real-parent'
        parent.mkdir()
        (parent / 'trust.pub').write_bytes(TRUST.read_bytes())
        parent_symlink = self.work / 'parent-link'
        parent_symlink.symlink_to(parent, target_is_directory=True)
        missing = self.work / 'missing.pub'

        for action in ('verify', 'install', 'update'):
            for label, trust in (
                    ('final-symlink', final_symlink),
                    ('parent-symlink', parent_symlink / 'trust.pub'),
                    ('directory', directory),
                    ('fifo', fifo),
                    ('missing', missing)):
                with self.subTest(action=action, input=label):
                    root = self.work / f'{action}-{label}-root'
                    args = ['mods', action, PACKAGE, '--trust-store', trust]
                    if action != 'verify':
                        args += ['--mod-root', root]
                    result = self.call(*args, code=2, timeout=2)
                    self.assertEqual(result, {'error': 'ModTrustStoreFileRejected'})
                    self.assertFalse(root.exists())

    def test_trust_store_boundary_accepts_regular_relative_and_absolute_paths(self):
        relative = Path('relative-trust.pub')
        (self.work / relative).write_bytes(TRUST.read_bytes())
        for path in (relative, (self.work / relative).resolve()):
            with self.subTest(path=path):
                verified = self.call('mods', 'verify', PACKAGE, '--trust-store', path)
                self.assertEqual(verified['mod_id'], MOD)

    def test_trust_store_file_size_boundary_accepts_65536_and_rejects_65537(self):
        exact = bytearray(TRUST.read_bytes())
        index = 0
        while 65536 - len(exact) > 194:
            exact += f'zz{index:05d}:'.encode() + b'0' * 64 + b'\n'
            index += 1
        remaining = 65536 - len(exact)
        final_id = 'zz99999' + 'z' * (remaining - 66 - len('zz99999'))
        exact += final_id.encode() + b':' + b'0' * 64 + b'\n'
        self.assertEqual(len(exact), 65536)

        accepted = self.work / 'trust-65536.pub'
        accepted.write_bytes(exact)
        verified = self.call('mods', 'verify', PACKAGE, '--trust-store', accepted)
        self.assertEqual(verified['mod_id'], MOD)

        oversized = self.work / 'trust-65537.pub'
        oversized.write_bytes(exact + b'x')
        root = self.work / 'oversized-trust-root'
        self.assertEqual(self.call('mods', 'update', PACKAGE, '--trust-store', oversized,
                                   '--mod-root', root, code=2),
                         {'error': 'ModTrustStoreFileRejected'})
        self.assertFalse(root.exists())

    def test_trust_store_boundary_rejects_metadata_change_during_read(self):
        barrier = self.work / 'trust-store-read-barrier.so'
        source = Path(__file__).resolve().parents[1] / 'fixtures/mod_trust_store_read_barrier.cpp'
        compiled = subprocess.run(['c++', '-std=c++20', '-shared', '-fPIC', str(source), '-ldl',
                                   '-o', str(barrier)], capture_output=True, text=True, timeout=30)
        self.assertEqual(compiled.returncode, 0, compiled.stderr)
        trust = self.work / 'large-trust.pub'
        trust.write_text(TRUST.read_text() + ''.join(
            f'zz{index:04d}:{index:064x}\n' for index in range(700)))
        self.assertLessEqual(trust.stat().st_size, 65536)
        info = trust.stat()
        ready_read, ready_write = os.pipe()
        resume_read, resume_write = os.pipe()
        env = dict(os.environ, LD_PRELOAD=str(barrier),
                   BRUCE_TEST_TRUST_INODE=str(info.st_ino),
                   BRUCE_TEST_TRUST_DEVICE=str(info.st_dev),
                   BRUCE_TEST_READY_FD=str(ready_write),
                   BRUCE_TEST_RESUME_FD=str(resume_read))
        process = subprocess.Popen(
            [BIN, 'mods', 'verify', str(PACKAGE), '--trust-store', str(trust)],
            cwd=self.work, env=env, pass_fds=(ready_write, resume_read),
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        os.close(ready_write)
        os.close(resume_read)
        try:
            self.assertTrue(select.select([ready_read], [], [], 10)[0],
                            'CLI did not begin reading the target trust store')
            self.assertEqual(os.read(ready_read, 1), b'R')
            current = trust.stat()
            os.utime(trust, ns=(current.st_atime_ns, current.st_mtime_ns + 1_000_000_000))
            os.write(resume_write, b'G')
            stdout, stderr = process.communicate(timeout=10)
            self.assertEqual(process.returncode, 2, stderr)
            self.assertEqual(stdout, '')
            self.assertEqual(json.loads(stderr), {'error': 'ModTrustStoreFileRejected'})
        finally:
            os.close(ready_read)
            os.close(resume_write)
            if process.poll() is None:
                process.kill()
            process.communicate()

    def test_invalid_options_cannot_mutate_registry(self):
        for args in (
            ['mods', 'install', PACKAGE, '--trust-store', TRUST],
            ['mods', 'install', PACKAGE, '--trust-store', TRUST, '--mod-root', self.root, '--bogus', 'x'],
            ['mods', 'install', PACKAGE, '--trust-store', TRUST, '--trust-store', TRUST, '--mod-root', self.root],
            ['mods', 'list', '--mod-root'],
            ['route', '--text', 'x'],
        ):
            self.assertIn('error', self.call(*args, code=2))
        self.assertFalse(self.root.exists())

    def test_replay_rejects_modified_result_and_existing_output(self):
        self.install()
        self.call('mods', 'activate', MOD, '1.0.0', '--mod-root', self.root)
        report = self.work / 'route.report'
        self.route('--output', report)
        original = report.read_bytes()
        self.call('route', '--text', '3/4 hoch 2', '--mod-root', self.root,
                  '--output', report, code=2)
        self.assertEqual(report.read_bytes(), original)
        for old, new in ((b'MATCHED', b'UNKNOWN'), (b'1.0.0', b'2.0.0'),
                         (b'math.rational.power.v1', b'fake.template')):
            report.write_bytes(original.replace(old, new))
            self.assertEqual(self.call('route', 'replay', report, '--mod-root', self.root,
                                      code=2)['error'], 'RouteReportMismatch')
        report.write_bytes(original[:-2])
        self.assertIn('error', self.call('route', 'replay', report, '--mod-root', self.root, code=2))

    def test_ambiguous_route_never_selects_a_template(self):
        package = FIXTURES / 'math-rational-power-ambiguous-v1.brucemod'
        self.call('mods', 'install', package, '--mod-root', self.root, '--trust-store', TRUST)
        self.call('mods', 'activate', 'user.math-rational-power-ambiguous', '1.0.0', '--mod-root', self.root)
        report = self.work / 'ambiguous.report'
        routed = self.route('--output', report)
        self.assertEqual(routed['status'], 'AMBIGUOUS')
        self.assertEqual(routed['candidate_addresses'], [[1, 1, 5], [1, 1, 6]])
        self.assertIsNone(routed['selected_template'])
        detailed = self.call('route', '--text', '3/4 hoch 2', '--mod-root', self.root,
                             '--mod-details')
        self.assertEqual(detailed['status'], 'AMBIGUOUS')
        self.assertIsNone(detailed['selected_owner'])
        self.assertEqual([candidate['owner']['id'] for candidate in detailed['candidates']],
                         ['user.math-rational-power-ambiguous'] * 2)
        self.assertEqual(self.call('route', 'replay', report, '--mod-root', self.root), routed)

    def test_unknown_utf8_and_control_characters_roundtrip(self):
        self.install()
        for text in ('3/0 hoch 2', 'Höhe "π" \\ prüfen\n', '3/4\tPLUS\n5/6'):
            report = self.work / ('roundtrip-' + str(len(text)))
            routed = self.call('route', '--text', text, '--mod-root', self.root, '--output', report)
            self.assertEqual(routed['text'], text)
            self.assertEqual(routed['status'], 'MATCHED' if text == '3/4\tPLUS\n5/6' else 'UNKNOWN')
            self.assertEqual(self.call('route', 'replay', report, '--mod-root', self.root), routed)
        self.assertEqual(self.call('route', '--text', 'x' * 4097, '--mod-root', self.root,
                                  code=2)['error'], 'RouteInputTooLarge')

    def test_malformed_replay_reports_are_controlled_errors(self):
        self.install()
        report = self.work / 'malformed.report'
        for content in ('', '{}\n', '{"contract":"route-report-v2"}\n',
                        '{"contract":"route-report-v1","text":"\\q"}\n',
                        '{"contract":"route-report-v1","text":"' + 'x' * 4097 + '"}\n'):
            report.write_text(content)
            self.assertEqual(self.call('route', 'replay', report, '--mod-root', self.root,
                                      code=2)['error'], 'InvalidRouteReport')


if __name__ == '__main__':
    unittest.main()
