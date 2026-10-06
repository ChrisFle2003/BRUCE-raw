import importlib.util
from pathlib import Path
import subprocess
import unittest


SCRIPT = Path(__file__).parents[2] / "scripts" / "verify_control_contract_matrix.py"
SPEC = importlib.util.spec_from_file_location("control_contract_matrix", SCRIPT)
MATRIX = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MATRIX)


class FailureEnvelope(unittest.TestCase):
    def test_rejects_stdout_on_failure(self):
        process = subprocess.CompletedProcess(
            ["bruce"], 2, '{"result":{}}\n', '{"error":"GateInspectionCancelled"}\n')
        with self.assertRaisesRegex(RuntimeError, "unexpected stdout"):
            MATRIX.failure_reply(process, "gate", {"error"})

    def test_rejects_extra_member_on_simple_failure(self):
        process = subprocess.CompletedProcess(
            ["bruce"], 2, "", '{"error":"TransferCancelled","execution":{}}\n')
        with self.assertRaisesRegex(RuntimeError, "unexpected keys"):
            MATRIX.failure_reply(process, "transfer", {"error"})

    def test_rejects_missing_session_member(self):
        process = subprocess.CompletedProcess(
            ["bruce"], 2, "", '{"error":"ExecutionSessionFailed"}\n')
        with self.assertRaisesRegex(RuntimeError, "unexpected keys"):
            MATRIX.failure_reply(process, "session", {"error", "execution"})


if __name__ == "__main__":
    unittest.main()
