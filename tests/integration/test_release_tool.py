#!/usr/bin/env python3
"""Offline release boundaries; RFC 8032 key is NOT a production key."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import resource
import shutil
import signal
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(sys.argv.pop(1)).resolve()
parser = argparse.ArgumentParser(add_help=False)
parser.add_argument('--build-dir', type=Path)
parser.add_argument('--baseline', type=Path)
parser.add_argument('--fixtures', type=Path)
parser.add_argument('--version')
parser.add_argument('--config', default='Release')
PACKAGE, remaining = parser.parse_known_args()
sys.argv[1:] = remaining
SEED = '9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60'
PUBLIC = 'd75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a'
# RFC 8032 test vector 2, also NOT a production key.
ATTACKER_SEED = '4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb'
ATTACKER_PUBLIC = '3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c'


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':')) + '\n'


class ReleaseFixture(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='bruce-release-test-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.archive = self.root / 'bruce-0.102.0-Linux.tar.gz'
        self.archive.write_bytes(b'precisely audited archive fixture\x00\xff')
        self.key = self.root / 'private-secret-key'
        self.key.write_text('BRUCE_RELEASE_PRIVATE_KEY_V1\n' + SEED + '\n')
        self.key.chmod(0o600)
        self.pin = self.root / 'pin.json'
        self.pin.write_text(canonical({'contract': 'bruce-release-key-v1',
            'key_id': hashlib.sha256(bytes.fromhex(PUBLIC)).hexdigest(),
            'public_key_ed25519': PUBLIC}))
        tree = {'version': '0.102.0', 'build_info': {'contract': 'bruce-build-info-v1',
            'version': '0.102.0', 'build_type': 'Release'}, 'binary_sha256': 'a' * 64,
            'research_export_replay': True, 'budget_failure_no_export': True,
            'historical_fixtures': [{'name': 'historical.bruce', 'sha256': 'b' * 64}]}
        self.report = {'contract': 'bruce-install-audit-v1', 'installation': copy.deepcopy(tree),
            'package': dict(tree, archive_name=self.archive.name,
                            archive_sha256=hashlib.sha256(self.archive.read_bytes()).hexdigest())}
        self.audit = self.root / 'audit.json'
        self.write_audit()
        self.output = self.root / 'release'

    def write_audit(self):
        self.audit.write_text(json.dumps(self.report, indent=2) + '\n')

    def run_tool(self, args, ok=True, cwd=None):
        self.assertTrue(TOOL.is_file(), 'missing executable: bruce-release-tool')
        result = subprocess.run([str(TOOL), *map(str, args)], capture_output=True,
            text=True, timeout=10, cwd=cwd or self.root,
            env={'PATH': '/usr/bin:/bin', 'LC_ALL': 'C'})
        captured = result.stdout + result.stderr
        for forbidden in (SEED, SEED[:16], SEED[-16:], ATTACKER_SEED,
                          ATTACKER_SEED[:16], ATTACKER_SEED[-16:], str(self.root), str(self.key)):
            self.assertFalse(forbidden in captured, 'release tool leaked fixture key material or a private path')
        self.assertEqual(result.returncode, 0 if ok else 2, captured)
        self.assertEqual(result.stderr if ok else result.stdout, '')
        text = result.stdout if ok else result.stderr
        self.assertLess(len(text), 4096)
        self.assertEqual(canonical(json.loads(text)), text)
        reply = json.loads(text)
        if not ok:
            self.assertEqual(set(reply), {'contract', 'error', 'ok'})
            self.assertEqual(reply['contract'], 'bruce-release-tool-report-v1')
            self.assertIs(reply['ok'], False)
            self.assertIn(reply['error'], {'ReleaseToolInvalidArguments', 'ReleaseToolInvalidContract',
                'ReleaseToolVerificationFailed', 'ReleaseToolUnsafeFile', 'ReleaseToolFailed'})
        return reply

    def sign(self, ok=True, **overrides):
        options = {'archive': self.archive, 'audit-report': self.audit,
            'private-key': self.key, 'trusted-public-key': self.pin, 'output-dir': self.output}
        options.update(overrides)
        return self.run_tool(['sign', *[part for key, value in options.items()
                               for part in ('--' + key, value)]], ok)

    def verify(self, ok=True, release_dir=None, pin=None, cwd=None):
        return self.run_tool(['verify', '--release-dir', release_dir or self.output,
                             '--trusted-public-key', pin or self.pin], ok, cwd)


class ReleaseTool(ReleaseFixture):

    def assert_error(self, reply, code):
        self.assertEqual(reply, {'contract': 'bruce-release-tool-report-v1',
                                'error': code, 'ok': False})

    def test_public_argument_error_code(self):
        self.assert_error(self.run_tool(['sign', '--version', 'private-input'], False),
                          'ReleaseToolInvalidArguments')

    def test_public_contract_error_code(self):
        self.audit.write_text('{"private-field":"private-input"}')
        self.assert_error(self.sign(False), 'ReleaseToolInvalidContract')
        self.write_audit()
        self.pin.write_text('{}\n')
        self.assert_error(self.sign(False), 'ReleaseToolInvalidContract')

    def test_public_crypto_and_trust_error_code(self):
        self.sign()
        signature = self.output / (self.archive.name + '.sig.json')
        value = json.loads(signature.read_text())
        value['signature_ed25519'] = '0' * 128
        signature.write_text(canonical(value))
        self.assert_error(self.verify(False), 'ReleaseToolVerificationFailed')
        self.key.write_text('BRUCE_RELEASE_PRIVATE_KEY_V1\n' + ATTACKER_SEED + '\n')
        self.assert_error(self.sign(False, **{'output-dir': self.root / 'other'}),
                          'ReleaseToolVerificationFailed')

    def test_public_file_safety_error_code(self):
        for defect in ('mode', 'missing', 'symlink', 'fifo'):
            with self.subTest(defect=defect):
                if defect == 'mode': self.key.chmod(0o644)
                elif defect == 'missing': self.key.unlink()
                elif defect == 'symlink': self.key.symlink_to(self.pin)
                else:
                    self.key.unlink(); os.mkfifo(self.key, 0o600)
                self.assert_error(self.sign(False), 'ReleaseToolUnsafeFile')
        self.assertFalse(self.output.exists())

    def test_help_and_argument_errors(self):
        self.run_tool(['--help'])
        for args in ([], ['unknown'], ['sign'], ['verify'], ['sign', '--version', '0.102.0'],
                     ['verify', '--release-dir', self.output, '--release-dir', self.output],
                     ['verify', '--release-dir', '--trusted-public-key', self.pin],
                     ['verify', '--release-dir', self.output, '--trusted-public-key', self.pin, '--x', 'y']):
            with self.subTest(args=args):
                self.run_tool(args, False)

    def test_sign_verify_readonly_and_output_collision(self):
        self.sign()
        self.assertEqual({p.name for p in self.output.iterdir()}, {
            self.archive.name, self.archive.name + '.sha256', self.archive.name + '.sig.json',
            'release-key-v1.json', 'release-set-v1.json'})
        self.assertEqual((self.output / self.archive.name).read_bytes(), self.archive.read_bytes())
        self.assertEqual((self.output / (self.archive.name + '.sha256')).read_text(),
            hashlib.sha256(self.archive.read_bytes()).hexdigest() + '  ' + self.archive.name + '\n')
        before = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.output.iterdir()}
        self.verify()
        self.assertEqual(before, {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.output.iterdir()})
        self.sign(False)
        self.assertEqual(before, {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.output.iterdir()})
        self.assertEqual(sorted(p.name for p in self.root.iterdir()),
            sorted([self.archive.name, self.key.name, self.pin.name, self.audit.name, 'release']))

    def test_real_cpack_basename_and_semantic_version(self):
        renamed = self.root / 'bruce-core-0.102.0-Linux-x86_64.tar.gz'
        self.archive.rename(renamed)
        self.archive = renamed
        self.report['package']['archive_name'] = renamed.name
        self.write_audit()
        self.sign()
        self.verify()
        self.output = self.root / 'wrong-version-release'
        self.report['package']['version'] = '0.103.0'
        self.report['package']['build_info']['version'] = '0.103.0'
        self.report['installation']['version'] = '0.103.0'
        self.report['installation']['build_info']['version'] = '0.103.0'
        self.write_audit()
        self.sign(False)

    def test_each_release_component_is_required_and_bound(self):
        self.sign()
        for target in list(self.output.iterdir()):
            original = target.read_bytes()
            for operation in ('corrupt', 'missing', 'symlink', 'fifo'):
                with self.subTest(file=target.name, operation=operation):
                    target.unlink()
                    if operation == 'corrupt': target.write_bytes(original + b'bad')
                    if operation == 'symlink': target.symlink_to(self.archive)
                    if operation == 'fifo': os.mkfifo(target)
                    self.verify(False)
                    if target.exists() or target.is_symlink(): target.unlink()
                    target.write_bytes(original)

    def test_independent_anchor_and_derived_key(self):
        alternate = {'contract': 'bruce-release-key-v1',
            'key_id': '39f713d0a644253f04529421b9f51b9b08979d08295959c4f3990ee617f5139f',
            'public_key_ed25519': '3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c'}
        self.sign()
        self.pin.write_text(canonical(alternate))
        self.verify(False)
        self.output = self.root / 'other-release'
        self.sign(False)
        self.assertFalse(self.output.exists())

    def test_audit_success_types_and_version_binding(self):
        original = copy.deepcopy(self.report)
        mutations = [('contract', 'wrong'), ('package', None), ('installation', None)]
        for field, value in mutations:
            self.report = copy.deepcopy(original)
            self.report[field] = value
            self.write_audit()
            self.sign(False)
        for section in ('installation', 'package'):
            for field, value in [('version', '0.103.0'), ('version', 102),
                    ('research_export_replay', False), ('budget_failure_no_export', 'true'),
                    ('historical_fixtures', []), ('build_info', {'version': '0.103.0'})]:
                with self.subTest(section=section, field=field):
                    self.report = copy.deepcopy(original)
                    self.report[section][field] = value
                    self.write_audit()
                    self.sign(False)
        for field, value in [('archive_name', '../unsafe.tar.gz'), ('archive_sha256', '0' * 64),
                             ('version', '0.103.0')]:
            self.report = copy.deepcopy(original)
            self.report['package'][field] = value
            self.write_audit()
            self.sign(False)
        self.assertFalse(self.output.exists())

    def test_missing_unsafe_and_corrupt_inputs(self):
        for option in ('archive', 'audit-report', 'private-key', 'trusted-public-key'):
            with self.subTest(option=option):
                self.sign(False, **{option: self.root / 'absent'})
                self.sign(False, **{option: self.root})
        self.archive.write_bytes(b'changed')
        self.sign(False)
        self.audit.write_text('{"contract":"bruce-install-audit-v1","contract":"bruce-install-audit-v1"}')
        self.sign(False)
        self.audit.write_bytes(b'x' * 1048577)
        self.sign(False)
        self.assertFalse(self.output.exists())

    def test_private_key_format_and_metadata(self):
        original = self.key.read_bytes()
        for value in (b'', original[:-1], original + b'\n', original.upper(), original + b'x' * 4096,
                      b'BRUCE_RELEASE_PRIVATE_KEY_V1\n' + b'0' * 64 + b'\n'):
            self.key.write_bytes(value)
            self.sign(False)
        self.key.write_bytes(original)
        for mode in (0o400, 0o640, 0o644, 0o660, 0o700, 0o1600):
            self.key.chmod(mode)
            self.sign(False)
        self.key.chmod(0o600)
        link = self.root / 'hardlink'
        os.link(self.key, link)
        self.sign(False)
        link.unlink()
        self.key.rename(link)
        self.key.symlink_to(link)
        self.sign(False)
        self.key.unlink()
        os.mkfifo(self.key, 0o600)
        self.sign(False)
        self.key.unlink()
        shutil.copyfile(link, self.key)
        self.key.chmod(0o600)
        self.assertFalse(self.output.exists())

    @unittest.skipUnless(os.geteuid() == 0, 'wrong-owner fixture requires root chown capability')
    def test_wrong_owner_private_key(self):
        os.chown(self.key, 1, 1)
        self.sign(False)
        self.assertFalse(self.output.exists())

    def test_existing_file_symlink_and_dangling_output_are_untouched(self):
        self.output.write_text('keep')
        self.sign(False)
        self.assertEqual(self.output.read_text(), 'keep')
        self.output.unlink()
        self.output.symlink_to(self.root / 'absent')
        self.sign(False)
        self.assertTrue(self.output.is_symlink())

    def test_failed_staging_cleans_only_its_owned_files(self):
        foreign = self.root / '.bruce-release-foreign'
        foreign.mkdir()
        sentinel = foreign / 'keep'
        sentinel.write_text('untouched')
        # The archive itself fits NAME_MAX; its sidecar does not. This causes
        # a real exclusive file-creation failure after the staged archive copy.
        long_name = 'bruce-0.102.0-' + 'a' * 234 + '.tar.gz'
        self.assertEqual(len(long_name), 255)
        renamed = self.root / long_name
        self.archive.rename(renamed)
        self.archive = renamed
        self.report['package']['archive_name'] = renamed.name
        self.write_audit()
        before = {p.name for p in self.root.iterdir()}
        self.sign(False)
        self.assertEqual(before, {p.name for p in self.root.iterdir()})
        self.assertEqual(sentinel.read_text(), 'untouched')
        self.assertFalse(self.output.exists())

    def test_canonical_semantic_tampering_is_rejected(self):
        self.sign()
        signature = self.output / (self.archive.name + '.sig.json')
        original = signature.read_bytes()
        for field, value in [('version', '0.103.0'), ('archive_sha256', '0' * 64),
                ('key_id', '0' * 64), ('signature_ed25519', '0' * 128),
                ('archive', 'bruce-0.102.0-other.tar.gz')]:
            with self.subTest(field=field):
                changed = json.loads(original)
                changed[field] = value
                signature.write_text(canonical(changed))
                self.verify(False)
        signature.write_bytes(original)
        self.verify()

    def test_symlink_fifo_and_bounded_input_reads(self):
        for option, path in [('archive', self.archive), ('audit-report', self.audit),
                             ('trusted-public-key', self.pin)]:
            renamed = self.root / ('saved-' + path.name)
            path.rename(renamed)
            path.symlink_to(renamed)
            self.sign(False)
            path.unlink()
            os.mkfifo(path)
            self.sign(False)
            path.unlink()
            renamed.rename(path)
        with self.archive.open('wb') as stream:
            stream.truncate(512 * 1024 * 1024 + 1)
        self.sign(False)
        self.assertFalse(self.output.exists())

    def test_unsafe_shared_parent_cannot_swap_staging_entry(self):
        shared = self.root / 'shared'
        shared.mkdir(mode=0o700)
        shared.chmod(0o777)
        self.output = shared / 'release'
        self.sign(False)
        self.assertEqual(list(shared.iterdir()), [])
        shared.chmod(0o1777)
        self.sign()
        self.verify()

    def test_invalid_utf8_audit_is_rejected(self):
        self.audit.write_bytes(self.audit.read_bytes().replace(b'"Release"', b'"Release\xff"'))
        self.sign(False)
        self.assertFalse(self.output.exists())


class ReleasePackageWorkflow(ReleaseFixture):
    def test_exact_audited_cpack_bytes_and_isolated_tampering(self):
        # Missing CTest wiring must fail, rather than silently skip the real package.
        self.assertIsNotNone(PACKAGE.build_dir, 'CPack workflow requires --build-dir from CTest')
        self.assertIsNotNone(PACKAGE.baseline, 'CPack workflow requires --baseline from CTest')
        self.assertIsNotNone(PACKAGE.fixtures, 'CPack workflow requires --fixtures from CTest')
        self.assertIsNotNone(PACKAGE.version, 'CPack workflow requires --version from CTest')
        self.archive.unlink()
        self.audit.unlink()
        handed_off = self.root / 'tested-archive.tgz'
        audit = subprocess.run([sys.executable,
            str(Path(__file__).resolve().parents[2] / 'scripts/verify_install.py'),
            '--build-dir', str(PACKAGE.build_dir), '--baseline', str(PACKAGE.baseline),
            '--fixtures', str(PACKAGE.fixtures), '--version', PACKAGE.version,
            '--config', PACKAGE.config, '--package', '--archive-out', str(handed_off),
            '--report', str(self.audit)], capture_output=True, text=True, timeout=180)
        # Do not echo captured audit diagnostics: paths may be private.
        self.assertEqual(audit.returncode, 0, 'CPack installation/package audit failed')
        self.report = json.loads(self.audit.read_text())
        self.assertEqual(json.loads(audit.stdout), self.report)
        self.assertEqual(self.report['contract'], 'bruce-install-audit-v1')
        for section in ('installation', 'package'):
            tree = self.report[section]
            self.assertEqual(tree['version'], PACKAGE.version)
            self.assertEqual(tree['build_info']['version'], PACKAGE.version)
            self.assertEqual(tree['build_info']['build_type'], PACKAGE.config)
            self.assertIs(tree['research_export_replay'], True)
            self.assertIs(tree['budget_failure_no_export'], True)
            self.assertTrue(tree['historical_fixtures'])
        archive_name = self.report['package']['archive_name']
        self.assertEqual(Path(archive_name).name, archive_name)
        self.archive = self.root / archive_name
        # Restore CPack's basename after the handoff; never regenerate the archive.
        handed_off.rename(self.archive)
        audited_bytes = self.archive.read_bytes()
        archive_digest = hashlib.sha256(audited_bytes).hexdigest()
        audit_bytes = self.audit.read_bytes()
        self.assertEqual(archive_digest, self.report['package']['archive_sha256'])

        trust = self.root / 'independent-trust'
        trust.mkdir()
        self.pin.rename(trust / 'pinned-rfc8032-test-key.json')
        self.pin = trust / 'pinned-rfc8032-test-key.json'
        pinned_bytes = self.pin.read_bytes()
        self.key.rename(self.root / 'NOT-A-PRODUCTION-private-key')
        self.key = self.root / 'NOT-A-PRODUCTION-private-key'
        expected = {'action': 'verify', 'archive': archive_name, 'archive_sha256': archive_digest,
            'contract': 'bruce-release-tool-report-v1', 'key_id': hashlib.sha256(bytes.fromhex(PUBLIC)).hexdigest(),
            'ok': True, 'version': PACKAGE.version}
        self.assertEqual(self.sign(), dict(expected, action='sign'))
        self.assertEqual((self.output / archive_name).read_bytes(), audited_bytes)
        fresh = self.root / 'fresh-verification-directory'
        fresh.mkdir()
        original = fresh / 'release'
        shutil.copytree(self.output, original)
        before = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in original.iterdir()}

        def original_verifies():
            self.assertEqual(self.verify(release_dir=original, cwd=fresh), expected)
            self.assertEqual(before, {p.name: (p.read_bytes(), p.stat().st_mtime_ns)
                                      for p in original.iterdir()})
            self.assertEqual(self.archive.read_bytes(), audited_bytes)
            self.assertEqual(self.audit.read_bytes(), audit_bytes)
            self.assertEqual(self.pin.read_bytes(), pinned_bytes)

        original_verifies()
        cases = self.root / 'isolated-negatives'
        cases.mkdir(mode=0o700)
        for name in ('one-byte-archive', 'renamed-archive', 'checksum', 'signature',
                     'missing-manifest', 'incomplete-manifest'):
            with self.subTest(case=name):
                altered = cases / name
                shutil.copytree(original, altered)
                if name == 'one-byte-archive':
                    data = bytearray(audited_bytes)
                    data[len(data) // 2] ^= 1
                    (altered / archive_name).write_bytes(data)
                elif name == 'renamed-archive':
                    renamed = archive_name.replace('.tar.gz', '-renamed.tar.gz')
                    for suffix in ('', '.sha256', '.sig.json'):
                        (altered / (archive_name + suffix)).rename(altered / (renamed + suffix))
                    for filename in ('release-set-v1.json', renamed + '.sig.json'):
                        value = json.loads((altered / filename).read_text())
                        value['archive'] = renamed
                        (altered / filename).write_text(canonical(value))
                    (altered / (renamed + '.sha256')).write_text(archive_digest + '  ' + renamed + '\n')
                elif name == 'checksum':
                    (altered / (archive_name + '.sha256')).write_text('0' * 64 + '  ' + archive_name + '\n')
                elif name == 'signature':
                    target = altered / (archive_name + '.sig.json')
                    envelope = json.loads(target.read_text())
                    signature = envelope['signature_ed25519']
                    envelope['signature_ed25519'] = ('1' if signature[0] == '0' else '0') + signature[1:]
                    target.write_text(canonical(envelope))
                elif name == 'missing-manifest':
                    (altered / 'release-set-v1.json').unlink()
                else:
                    (altered / 'release-set-v1.json').write_text('{"contract":')
                self.verify(False, release_dir=altered, cwd=fresh)
                original_verifies()

        with self.subTest(case='changed-audit-version'):
            changed = copy.deepcopy(self.report)
            for section in ('installation', 'package'):
                changed[section]['version'] = '999.0.0'
                changed[section]['build_info']['version'] = '999.0.0'
            changed_audit = cases / 'changed-audit.json'
            changed_audit.write_text(canonical(changed))
            rejected = cases / 'wrong-version-release'
            self.sign(False, **{'audit-report': changed_audit, 'output-dir': rejected})
            self.assertFalse(rejected.exists())
            original_verifies()

        attacker_key = cases / 'NOT-A-PRODUCTION-attacker-key'
        attacker_key.write_text('BRUCE_RELEASE_PRIVATE_KEY_V1\n' + ATTACKER_SEED + '\n')
        attacker_key.chmod(0o600)
        attacker_pin = trust / 'NOT-A-PRODUCTION-attacker-pin.json'
        attacker_pin.write_text(canonical({'contract': 'bruce-release-key-v1',
            'key_id': hashlib.sha256(bytes.fromhex(ATTACKER_PUBLIC)).hexdigest(),
            'public_key_ed25519': ATTACKER_PUBLIC}))
        with self.subTest(case='wrong-pinned-key'):
            self.verify(False, release_dir=original, pin=attacker_pin, cwd=fresh)
            original_verifies()
        with self.subTest(case='attacker-replacement-set'):
            attacker_release = cases / 'attacker-release'
            replacement = self.sign(**{'private-key': attacker_key, 'trusted-public-key': attacker_pin,
                                       'output-dir': attacker_release})
            attacker_expected = dict(expected, key_id=hashlib.sha256(bytes.fromhex(ATTACKER_PUBLIC)).hexdigest())
            self.assertEqual(replacement, dict(attacker_expected, action='sign'))
            self.assertEqual(self.verify(release_dir=attacker_release, pin=attacker_pin, cwd=fresh), attacker_expected)
            self.verify(False, release_dir=attacker_release, cwd=fresh)
            original_verifies()

        with self.subTest(case='second-sign-cannot-overwrite'):
            self.sign(False, **{'output-dir': original})
            original_verifies()

        with self.subTest(case='crash-before-publication'):
            crash_parent = cases / 'crash'
            crash_parent.mkdir(mode=0o700)
            crash_output = crash_parent / 'release'

            def crash_during_archive_copy():
                resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
                resource.setrlimit(resource.RLIMIT_FSIZE, (len(audited_bytes) // 2,) * 2)
                signal.signal(signal.SIGXFSZ, signal.SIG_DFL)

            # Real OS-enforced process death during staged archive output;
            # no timing race, production hook, or regenerated package.
            killed = subprocess.run([str(TOOL), 'sign', '--archive', str(self.archive),
                '--audit-report', str(self.audit), '--private-key', str(self.key),
                '--trusted-public-key', str(self.pin), '--output-dir', str(crash_output)],
                capture_output=True, timeout=10, cwd=fresh, preexec_fn=crash_during_archive_copy,
                env={'PATH': '/usr/bin:/bin', 'LC_ALL': 'C'})
            self.assertEqual(killed.returncode, -signal.SIGXFSZ)
            self.assertEqual(killed.stdout + killed.stderr, b'')
            self.assertFalse(crash_output.exists())
            stages = list(crash_parent.iterdir())
            self.assertEqual(len(stages), 1)
            stage = stages[0]
            self.assertTrue(stage.name.startswith('.bruce-release-'))
            self.assertTrue(0 < (stage / archive_name).stat().st_size < len(audited_bytes))
            self.assertFalse((stage / 'release-set-v1.json').exists())
            self.verify(False, release_dir=stage, cwd=fresh)
            self.verify(False, release_dir=crash_output, cwd=fresh)
            original_verifies()


if __name__ == '__main__':
    unittest.main()
