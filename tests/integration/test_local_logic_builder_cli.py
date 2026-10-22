"""Logic builder CLI: signed package, deterministic bytes, and no tool execution."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

BUILDER = Path(sys.argv.pop(1)).resolve()
FAULT_BUILDER = Path(sys.argv.pop(1)).resolve()
BRUCE = Path(sys.argv.pop(1)).resolve()
WORKER = Path(sys.argv.pop(1)).resolve()
FIXTURES = Path(__file__).resolve().parents[1] / "fixtures/runtime_mods"
SEED = "9d61b19deffd5a60ba844af492ec2cc4" "4449c5697b326919703bac031cae7f60"


class LogicBuilderCLI(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bruce-logic-builder-cli-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        self.source.mkdir()
        shutil.copyfile(FIXTURES / "math-rational-power-v1.brucemod",
                        self.source / "data.brucemod")
        worker = WORKER.read_bytes()
        (self.source / "worker.bin").write_bytes(worker)
        (self.source / "logic.json").write_text(json.dumps({
            "capabilities": ["worker.execute"],
            "template_ids": ["math.rational.power.v1"],
            "worker_path": "worker.bin",
            "worker_sha256": hashlib.sha256(worker).hexdigest(),
        }, sort_keys=True, separators=(",", ":")))
        self.signing = self.root / "signing.env"
        self.signing.write_text("BRUCE_MOD_ED25519_SEED_HEX=" + SEED + "\n")
        self.signing.chmod(0o600)
        self.package = self.root / "logic.brucemod"
        self.trust = self.root / "logic.pub"

    def build(self, output=None, trust=None, env=None):
        return subprocess.run([
            BUILDER, "logic", self.source,
            "--output", output or self.package,
            "--trust-store-output", trust or self.trust,
            "--signing-env", self.signing,
        ], cwd=self.root, capture_output=True, text=True, timeout=30, env=env)

    def build_args(self):
        return ["logic", self.source, "--output", self.package,
                "--trust-store-output", self.trust, "--signing-env", self.signing]

    def assert_failure(self, args):
        result = subprocess.run([BUILDER, *args], cwd=self.root,
                                capture_output=True, timeout=30)
        for secret in (SEED, "private-sentinel", str(self.root), str(self.source),
                       "signing.env", "data.brucemod", "logic.json", "worker.bin"):
            self.assertNotIn(secret.encode(), result.stdout + result.stderr)
        self.assertEqual(result.stdout, b"")
        self.assertEqual(result.stderr, b'{"error":"LocalModBuildFailed"}\n')
        self.assertEqual(result.returncode, 2)

    def test_argument_errors_are_bounded_json_exit_two(self):
        base = self.build_args()
        cases = {
            "missing source": ["logic"],
            "empty source": ["logic", "", *base[2:]],
            "missing signing option": base[:-2],
            "missing option value": base[:-1],
            "empty option value": [*base[:-1], ""],
            "duplicate option": [*base, "--output", "private-sentinel"],
            "unexpected option": [*base, "--private-sentinel", SEED],
            "unexpected positional": [*base, "private-sentinel"],
        }
        for name, args in cases.items():
            with self.subTest(name=name):
                self.assert_failure(args)
        self.assertFalse(self.package.exists())
        self.assertFalse(self.trust.exists())

    def test_missing_unsafe_and_malformed_signing_are_redacted(self):
        link = self.root / "private-sentinel-key-link"
        link.symlink_to(self.signing)
        for name, path in (("missing", self.root / "private-sentinel-missing-key"),
                           ("symlink", link)):
            with self.subTest(name=name):
                self.assert_failure([*self.build_args()[:-1], path])
        self.signing.chmod(0o644)
        self.assert_failure(self.build_args())
        self.signing.chmod(0o600)
        self.signing.write_text("BRUCE_MOD_ED25519_SEED_HEX=" + SEED + "private-sentinel\n")
        self.assert_failure(self.build_args())
        # A well-formed different key must fail embedded Data signature validation.
        self.signing.write_text("BRUCE_MOD_ED25519_SEED_HEX=" + "0" * 64 + "\n")
        self.assert_failure(self.build_args())
        self.assertFalse(self.package.exists())
        self.assertFalse(self.trust.exists())

    def test_missing_unsafe_and_malformed_sources_are_redacted(self):
        base = self.build_args()
        link = self.root / "private-sentinel-source-link"
        link.symlink_to(self.source, target_is_directory=True)
        for source in (self.root / "private-sentinel-missing-source", link):
            with self.subTest(source=source.name):
                self.assert_failure(["logic", source, *base[2:]])
        for name in ("data.brucemod", "logic.json", "worker.bin"):
            path = self.source / name
            original = path.read_bytes()
            for kind in ("missing", "symlink", "malformed"):
                with self.subTest(source=name, kind=kind):
                    path.unlink()
                    if kind == "symlink":
                        path.symlink_to(WORKER)
                    elif kind == "malformed":
                        path.write_bytes(b"private-sentinel-malformed-input")
                    try:
                        self.assert_failure(base)
                    finally:
                        if path.is_symlink() or path.exists():
                            path.unlink()
                        path.write_bytes(original)
        self.assertFalse(self.package.exists())
        self.assertFalse(self.trust.exists())

    def test_output_collisions_are_redacted_and_preserve_existing_bytes(self):
        for path, other in ((self.package, self.trust), (self.trust, self.package)):
            path.write_bytes(b"private-sentinel-existing-output")
            try:
                with self.subTest(output=path.name):
                    self.assert_failure(self.build_args())
                    self.assertEqual(path.read_bytes(), b"private-sentinel-existing-output")
                    self.assertFalse(other.exists())
            finally:
                path.unlink()
        base = self.build_args()
        self.assert_failure([*base[:5], self.package, *base[6:]])
        self.assertFalse(self.package.exists())

    def test_logic_report_serialization_failure_leaves_no_outputs_and_retry_succeeds(self):
        result = subprocess.run([FAULT_BUILDER, *self.build_args()], cwd=self.root,
                                capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stderr, b'{"error":"LocalModBuildFailed"}\n')
        self.assertEqual(result.stdout, b"")
        self.assertFalse(self.package.exists())
        self.assertFalse(self.trust.exists())
        retry = subprocess.run([BUILDER, *self.build_args()], cwd=self.root,
                               capture_output=True, timeout=30)
        self.assertEqual(retry.returncode, 0, retry.stderr)
        self.assertTrue(self.package.exists())
        self.assertTrue(self.trust.exists())

    def test_unsafe_output_paths_are_redacted(self):
        unsafe = self.root / "private-sentinel-unsafe-directory"
        unsafe.mkdir(mode=0o700)
        unsafe.chmod(0o777)
        link = self.root / "private-sentinel-output-link"
        link.symlink_to(self.source, target_is_directory=True)
        for directory in (unsafe, link, self.root / "private-sentinel-missing-directory"):
            with self.subTest(directory=directory.name):
                base = self.build_args()
                self.assert_failure([*base[:3], directory / "output.brucemod", *base[4:]])
                self.assertFalse((directory / "output.brucemod").exists())
                self.assertFalse(self.trust.exists())

    def test_builds_and_verifies_without_invoking_external_tools(self):
        traps = self.root / "traps"
        traps.mkdir()
        marker = self.root / "tool-was-run"
        for name in ("cc", "c++", "gcc", "g++", "sh", "bash", "cmake", "ninja"):
            trap = traps / name
            trap.write_text("#!/bin/sh\nprintf invoked > '" + str(marker) + "'\nexit 99\n")
            trap.chmod(0o700)
        environment = os.environ.copy()
        environment["PATH"] = str(traps)
        result = self.build(env=environment)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(marker.exists())
        report = json.loads(result.stdout)
        self.assertEqual(report["contract"], "local-logic-mod-build-v1")
        self.assertEqual(report["kind"], "logic")
        self.assertEqual(report["signing_key_id"], "test-rfc8032")
        expected = ('{"contract":"local-logic-mod-build-v1","kind":"logic",'
                    '"mod_id":"user.math-rational-power","version":"1.0.0",'
                    '"signing_key_id":"test-rfc8032","package_digest":"'
                    + report["package_digest"] + '"}\n')
        self.assertEqual(result.stdout, expected)
        verified = subprocess.run([BRUCE, "mods", "verify", self.package,
                                   "--trust-store", self.trust],
                                  capture_output=True, text=True, timeout=30)
        self.assertEqual(verified.returncode, 0, verified.stderr)
        self.assertEqual(json.loads(verified.stdout)["kind"], "logic")
        self.assertNotIn(SEED, result.stdout + result.stderr + verified.stdout + verified.stderr)

    def test_is_deterministic_and_never_replaces_outputs(self):
        first = self.build()
        self.assertEqual(first.returncode, 0, first.stderr)
        package_bytes, trust_bytes = self.package.read_bytes(), self.trust.read_bytes()
        again = self.build()
        self.assertNotEqual(again.returncode, 0)
        self.assertEqual(self.package.read_bytes(), package_bytes)
        self.assertEqual(self.trust.read_bytes(), trust_bytes)
        second_package, second_trust = self.root / "second.brucemod", self.root / "second.pub"
        second = self.build(second_package, second_trust)
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(second_package.read_bytes(), package_bytes)
        self.assertEqual(second_trust.read_bytes(), trust_bytes)


if __name__ == "__main__":
    unittest.main()
