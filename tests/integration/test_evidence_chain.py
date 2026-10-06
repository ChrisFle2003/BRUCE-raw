import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())


class EvidenceChainWorkflows(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def call(self, *args, ok=True):
        process = subprocess.run([BIN, *map(str, args)], capture_output=True, timeout=30)
        self.assertEqual(process.returncode, 0 if ok else 2,
                         f"stdout={process.stdout!r} stderr={process.stderr!r}")
        self.assertEqual(process.stderr if ok else process.stdout, b"")
        return json.loads(process.stdout if ok else process.stderr)

    def samples(self, name, rows):
        path = self.root / name
        path.write_text("x,y\n" + "".join(f"{x},{y}\n" for x, y in rows))
        return path

    def create(self, train_rows=((0, "1/2"), (1, "3/2")),
               holdout_rows=((2, "5/2"),), *extra):
        train = self.samples("train.csv", train_rows)
        holdout = self.samples("holdout.csv", holdout_rows)
        artifact = self.root / "chain.bruce"
        result = self.call("evidence", "create", "--train", train,
                           "--holdout", holdout, "--output", artifact,
                           "--domain", "rational", "--min", 0, "--max", 1,
                           "--max-denominator", 2, *extra)
        return result, artifact, train, holdout

    @staticmethod
    def rehash(bundle):
        prefix, _, payload = bundle.split(b"\n", 2)
        return prefix + b"\n" + hashlib.sha256(payload).hexdigest().encode() + b"\n" + payload

    @staticmethod
    def rehash_mutation(bundle, old, new):
        assert len(old) == len(new)
        prefix, digest, payload = bundle.split(b"\n", 2)
        assert digest and old in payload
        changed = payload.replace(old, new, 1)
        return prefix + b"\n" + hashlib.sha256(changed).hexdigest().encode() + b"\n" + changed

    def test_rational_literals_proof_and_source_independent_replay(self):
        result, artifact, train, holdout = self.create()
        self.assertEqual(result["evidence_contract"], "affine-evidence-chain-v1")
        self.assertEqual(result["independence"], "declared-disjoint-v1")
        self.assertEqual(result["countercheck_policy"], "affine-inverse-v1")
        self.assertEqual(result["countercheck_scope"],
                         "short-circuit-training-then-holdout-v1")
        self.assertEqual((result["training_samples"], result["holdout_samples"]), (2, 1))
        self.assertEqual(result["result"]["status"], "VERIFIED_ON_SAMPLES")
        self.assertFalse(result["result"]["general_proof"])
        bundle = artifact.read_bytes()
        self.assertTrue(bundle.startswith(b"BRUCE_EVIDENCE_CHAIN_V1\n"))
        for literal in (b"contract affine-evidence-chain-v1\n",
                        b"independence declared-disjoint-v1\n",
                        b"countercheck_policy affine-inverse-v1\n",
                        b"countercheck_scope short-circuit-training-then-holdout-v1\n",
                        b"BRUCE_EXACT_BUNDLE_V1\n"):
            self.assertIn(literal, bundle)
        train.unlink()
        holdout.unlink()
        self.assertEqual(self.call("evidence", "replay", artifact), result)
        self.assertIn("error", self.call("evidence", "replay", artifact,
                                         "extra", ok=False))

    def test_holdout_counterexample_is_refuted_and_replays(self):
        result, artifact, _, _ = self.create(holdout_rows=((2, "99/2"),))
        self.assertEqual(result["result"]["status"], "REFUTED")
        self.assertEqual(self.call("evidence", "replay", artifact), result)

    def test_overlap_rejects_without_export(self):
        train = self.samples("train.csv", ((0, "1/2"), (1, "3/2")))
        holdout = self.samples("holdout.csv", ((1, "3/2"),))
        artifact = self.root / "overlap.bruce"
        error = self.call("evidence", "create", "--train", train,
                          "--holdout", holdout, "--output", artifact,
                          "--domain", "rational", ok=False)
        self.assertEqual(error["error"], "DuplicateOrOverlappingSampleInput")
        self.assertFalse(artifact.exists())

    def test_unsupported_option_preserves_existing_output(self):
        train = self.samples("train.csv", ((0, 0),))
        holdout = self.samples("holdout.csv", ((1, 1),))
        artifact = self.root / "existing.bruce"
        artifact.write_bytes(b"keep this existing output")
        error = self.call("evidence", "create", "--train", train,
                          "--holdout", holdout, "--output", artifact,
                          "--strategy", "power-affine", ok=False)
        self.assertIn("UnknownOption", error["error"])
        self.assertEqual(artifact.read_bytes(), b"keep this existing output")

    def test_rehashed_policy_scope_independence_csv_native_and_result_mutations_reject(self):
        _, artifact, _, _ = self.create()
        original = artifact.read_bytes()
        mutations = (
            (b"countercheck_policy affine-inverse-v1", b"countercheck_policy affine-inversf-v1"),
            (b"countercheck_scope short-circuit-training-then-holdout-v1",
             b"countercheck_scope short-circuit-training-then-holdout-v2"),
            (b"independence declared-disjoint-v1", b"independence declared-disjoinu-v1"),
            (b"0,1/2\n", b"0,3/2\n"),
            (b"BRUCE_EXACT_BUNDLE_V1\n", b"BRUCE_EXACT_BUNDLE_V2\n"),
            (b'"general_proof":false', b'"general_proof":true '),
        )
        for index, (old, new) in enumerate(mutations):
            with self.subTest(mutation=index):
                changed = self.root / f"tampered-{index}.bruce"
                changed.write_bytes(self.rehash_mutation(original, old, new))
                error = self.call("evidence", "replay", changed, ok=False)
                self.assertIn("error", error)

        # The native bundle also has a RESULT block. Mutate only the chain's
        # final RESULT, preserving native bytes and their embedded digest.
        prefix, digest, payload = original.split(b"\n", 2)
        before_result, marker, outer_result = payload.rpartition(b"RESULT\n")
        self.assertTrue(digest and marker)
        self.assertIn(b'"general_proof":false', outer_result)
        changed_payload = before_result + marker + outer_result.replace(
            b'"general_proof":false', b'"general_proof":true ', 1)
        outer_tampered = self.root / "tampered-outer-result.bruce"
        outer_tampered.write_bytes(prefix + b"\n" +
                                   hashlib.sha256(changed_payload).hexdigest().encode() +
                                   b"\n" + changed_payload)
        error = self.call("evidence", "replay", outer_tampered, ok=False)
        self.assertEqual(error["error"], "ReplayMismatch: evidence chain differs")

    def test_rehashed_trailing_payload_and_duplicate_field_reach_chain_parser(self):
        _, artifact, _, _ = self.create()
        original = artifact.read_bytes()
        cases = (
            (original + b"trailing", "ReplayMismatch: evidence chain differs"),
            (original.replace(b"independence declared-disjoint-v1\n",
                              b"independence declared-disjoint-v1\n" * 2, 1),
             "InvalidEvidenceChainField"),
        )
        for index, (mutation, diagnostic) in enumerate(cases):
            with self.subTest(diagnostic=diagnostic):
                changed = self.root / f"framing-{index}.bruce"
                changed.write_bytes(self.rehash(mutation))
                error = self.call("evidence", "replay", changed, ok=False)
                self.assertEqual(error["error"], diagnostic)

    def test_operation_budget_incomplete_and_replay(self):
        train = self.samples("train.csv", ((0, "1/2"), (1, "3/2")))
        holdout = self.samples("holdout.csv", ((2, "5/2"),))
        artifact = self.root / "incomplete.bruce"
        result = self.call("evidence", "create", "--train", train,
                           "--holdout", holdout, "--output", artifact,
                           "--domain", "rational", "--min", 0, "--max", 1,
                           "--max-denominator", 2, "--max-operations", 1)
        self.assertEqual(result["result"]["status"], "INCOMPLETE")
        self.assertEqual(result["result"]["stop_reason"], "OperationBudgetExceeded")
        self.assertEqual(self.call("evidence", "replay", artifact), result)


if __name__ == "__main__":
    unittest.main()
