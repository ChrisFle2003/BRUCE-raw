"""Real CLI lifecycle; catches missing activation, stale replay and unsafe errors."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BIN = str(Path(sys.argv.pop(1)).resolve())
FIXTURES = Path(__file__).resolve().parents[1] / 'fixtures/runtime_mods'
PACKAGE = FIXTURES / 'math-rational-power-v1.brucemod'
TRUST = FIXTURES / 'test-truststore-v1.pub'
MOD = 'user.math-rational-power'


class RuntimeModTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='bruce-runtime-cli-')
        self.addCleanup(self.directory.cleanup)
        self.work = Path(self.directory.name)
        self.root = self.work / 'mods'

    def call(self, *args, code=0):
        result = subprocess.run([BIN, *map(str, args)], capture_output=True,
                                text=True, timeout=30)
        self.assertEqual(result.returncode, code, result.stderr)
        self.assertEqual(result.stderr if code == 0 else result.stdout, '')
        return json.loads(result.stdout if code == 0 else result.stderr)

    def install(self):
        return self.call('mods', 'install', PACKAGE, '--mod-root', self.root,
                         '--trust-store', TRUST)

    def route(self, *extra):
        return self.call('route', '--text', '3/4 hoch 2', '--mod-root', self.root, *extra)

    def test_full_lifecycle_and_snapshot_replay(self):
        verified = self.call('mods', 'verify', PACKAGE, '--trust-store', TRUST)
        self.assertEqual(verified['mod_id'], MOD)
        self.assertEqual(verified['package_digest'], hashlib.sha256(PACKAGE.read_bytes()).hexdigest())
        self.install()
        records = self.call('mods', 'list', '--mod-root', self.root)['records']
        self.assertEqual(records[0]['state'], 'installed_inactive')
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
        replay = self.call('route', 'replay', report, '--mod-root', self.root)
        self.assertEqual(replay, routed)
        self.call('mods', 'disable', MOD, '--mod-root', self.root)
        self.assertEqual(self.route()['status'], 'UNKNOWN')
        mismatch = self.call('route', 'replay', report, '--mod-root', self.root, code=2)
        self.assertEqual(mismatch['error'], 'RouteRegistryDigestMismatch')

    def test_untrusted_package_never_creates_installation(self):
        empty = self.work / 'empty.pub'
        empty.write_text('BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n')
        for action in ('verify', 'install'):
            args = ['mods', action, PACKAGE, '--trust-store', empty]
            if action == 'install':
                args += ['--mod-root', self.root]
            self.assertEqual(self.call(*args, code=2)['error'], 'RuntimeModSigningKeyUntrusted')
        self.assertFalse(self.root.exists())

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
