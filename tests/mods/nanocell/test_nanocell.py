import base64
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())
MOD_HEADER = b"BRUCE_MOD_RUN_V1\n"
LEGACY_V1_HEADER = b"BRUCE_NANOCELL_RUN_V1\n"
LEGACY_V1_FIXTURE = (Path(__file__).resolve().parents[2] / "fixtures" /
                     "nanocell-v1-masked-match.nanocell.b64")


def read_field(body, offset):
    colon = body.index(b":", offset)
    count = int(body[offset:colon])
    start = colon + 1
    return body[start:start + count], start + count


def unpack_mod_run(data):
    header, digest, body = data.split(b"\n", 2)
    assert header + b"\n" == MOD_HEADER
    assert digest == hashlib.sha256(body).hexdigest().encode()
    offset = 0
    fields = []
    for _ in range(4):
        value, offset = read_field(body, offset)
        fields.append(value)
    for _ in range(int(fields[3])):
        value, offset = read_field(body, offset)
        fields.append(value)
    while offset < len(body):
        value, offset = read_field(body, offset)
        fields.append(value)
    assert offset == len(body)
    return fields


def pack_mod_run(fields):
    body = b"".join(str(len(field)).encode() + b":" + field for field in fields)
    return MOD_HEADER + hashlib.sha256(body).hexdigest().encode() + b"\n" + body


class NanoCellCliTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.context = self.root / "context.txt"
        self.context.write_bytes(b"nanocell-reference-context-v1")

    def call(self, *args):
        return subprocess.run([BIN, "nanocell", *map(str, args)],
                              capture_output=True, text=True, timeout=10)

    def run_args(self, core="1020a54050607080", technique="masked-match-v1"):
        return ("run", "--core", core, "--active-index", "2",
                "--context-id", "reference.context_1", "--context-file", self.context,
                "--technique", technique)

    def assert_error(self, *args):
        process = self.call(*args)
        self.assertEqual(process.returncode, 2, process.stderr)
        self.assertFalse(process.stdout)
        self.assertIn("error", json.loads(process.stderr))
        return process

    def test_run_export_replay(self):
        bundle = self.root / "run.nanocell"
        run = self.call(*self.run_args(), "--output", bundle)
        self.assertEqual(run.returncode, 0, run.stderr)
        payload = json.loads(run.stdout)
        self.assertEqual(payload["contract"], "nanocell-cli-v1")
        self.assertEqual(payload["status"], "MATCHED")
        self.assertEqual(payload["final_state"], 160)
        self.assertEqual(payload["context_digest"],
                         hashlib.sha256(b"nanocell-reference-context-v1").hexdigest())
        self.assertEqual(payload["operations_used"], 1)
        self.assertEqual(payload["trace"], [
            {"kind": "START", "operations_used": 0, "state": 165},
            {"kind": "OPERATION", "operations_used": 1, "state": 160},
            {"kind": "TERMINAL", "operations_used": 1, "state": 160},
        ])
        self.assertNotIn("nanocell-reference-context-v1", run.stdout)
        self.assertTrue(bundle.is_file())
        stored = bundle.read_bytes()
        self.assertTrue(stored.startswith(MOD_HEADER))
        fields = unpack_mod_run(stored)
        self.assertEqual(fields[0:5], [b"nanocell", b"nanocell-mod-v1", b"bruce-mod-api-v1",
                                       b"1", b"4"])
        self.assertEqual(fields[5], b"bruce-nanocell-request-v1")
        self.assertEqual(fields[7], hashlib.sha256(fields[6]).hexdigest().encode())
        self.assertEqual(fields[8], b"bruce-nanocell-run-v1")
        self.assertEqual(fields[10], hashlib.sha256(fields[9]).hexdigest().encode())
        replay = self.call("replay", bundle)
        self.assertEqual(replay.returncode, 0, replay.stderr)
        self.assertEqual(json.loads(replay.stdout), payload)
        self.assertEqual(bundle.read_bytes(), stored)

    def test_historical_direct_v1_bundle_still_replays_read_only(self):
        legacy = self.root / "historical.nanocell"
        legacy.write_bytes(base64.b64decode(LEGACY_V1_FIXTURE.read_text().strip()))
        self.assertTrue(legacy.read_bytes().startswith(LEGACY_V1_HEADER))
        replay = self.call("replay", legacy)
        self.assertEqual(replay.returncode, 0, replay.stderr)
        payload = json.loads(replay.stdout)
        self.assertEqual(payload["contract"], "nanocell-cli-v1")
        self.assertEqual(payload["status"], "MATCHED")
        self.assertEqual(payload["final_state"], 160)

    def test_other_terminal_statuses(self):
        cases = [
            (self.run_args(core="1020b54050607080"), "NO_MATCH", 176, 1),
            (self.run_args(technique="context-conflict-v1"), "CONTEXT_REJECTED", None, 0),
            ((*self.run_args(technique="rotate-match-v1"), "--max-operations", "1"),
             "INCOMPLETE", 75, 1),
        ]
        for args, status, final_state, operations in cases:
            with self.subTest(status=status):
                process = self.call(*args)
                self.assertEqual(process.returncode, 0, process.stderr)
                payload = json.loads(process.stdout)
                self.assertEqual(payload["status"], status)
                self.assertEqual(payload["final_state"], final_state)
                self.assertEqual(payload["operations_used"], operations)
                self.assertEqual(payload["trace"][0]["kind"], "START")
                self.assertEqual(payload["trace"][-1]["kind"], "TERMINAL")

    def test_invalid_inputs_leave_no_export(self):
        missing = self.root / "missing-context"
        cases = [
            self.run_args(core="1020a5405060708"),
            self.run_args(core="1020a5405060708g"),
            self.run_args(technique="invented-v1"),
            ("run", "--core", "1020a54050607080", "--active-index", "2",
             "--context-id", "reference.context_1", "--context-file", missing,
             "--technique", "masked-match-v1"),
        ]
        for index, args in enumerate(cases):
            with self.subTest(index=index):
                output = self.root / f"invalid-{index}.run"
                self.assert_error(*args, "--output", output)
                self.assertFalse(output.exists())

    def test_corrupt_bundle_and_existing_output_are_rejected(self):
        bundle = self.root / "run.nanocell"
        run = self.call(*self.run_args(), "--output", bundle)
        self.assertEqual(run.returncode, 0, run.stderr)
        original = bundle.read_bytes()
        self.assert_error(*self.run_args(), "--output", bundle)
        self.assertEqual(bundle.read_bytes(), original)
        corrupt = self.root / "corrupt.nanocell"
        corrupt.write_bytes(original + b"x")
        self.assert_error("replay", corrupt)
        self.assertEqual(corrupt.read_bytes(), original + b"x")
        fields = unpack_mod_run(original)
        other = self.root / "other.nanocell"
        other_run = self.call(*self.run_args(core="1020b54050607080"), "--output", other)
        self.assertEqual(other_run.returncode, 0, other_run.stderr)
        other_fields = unpack_mod_run(other.read_bytes())
        altered = self.root / "altered.nanocell"
        changed_payload = list(fields)
        changed_payload[9] = other_fields[9]
        changed_payload[10] = hashlib.sha256(changed_payload[9]).hexdigest().encode()
        altered.write_bytes(pack_mod_run(changed_payload))
        self.assert_error("replay", altered)
        changed_input = list(fields)
        changed_input[6] = other_fields[6]
        changed_input[7] = hashlib.sha256(changed_input[6]).hexdigest().encode()
        altered.write_bytes(pack_mod_run(changed_input))
        self.assert_error("replay", altered)


if __name__ == "__main__":
    unittest.main()
