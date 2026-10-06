import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())
ROOT = Path(__file__).resolve().parents[2]
IGNORE = shutil.ignore_patterns('.git', '.agents', '.codex', '.worktrees', 'build*', 'runs',
                              '__pycache__', '*.pyc')


class CliDocumentationTests(unittest.TestCase):
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
