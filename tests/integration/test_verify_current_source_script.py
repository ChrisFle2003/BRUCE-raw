#!/usr/bin/env python3
"""Behavioral checks for the current-source verification runner."""

from __future__ import annotations

import pathlib
import subprocess
import unittest


REPOSITORY = pathlib.Path(__file__).resolve().parents[2]
RUNNER = REPOSITORY / "scripts/verify_current_source.sh"


class VerifyCurrentSourceScriptTests(unittest.TestCase):
    def test_rejects_unsafe_requested_build_path(self) -> None:
        """Removing runner path validation would start CMake with a corrupt path."""
        self.assertTrue(RUNNER.is_file(), "current-source verification runner is missing")
        result = subprocess.run(
            [str(RUNNER), "/tmp/build path with space"],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("unsafe", result.stderr.lower())


if __name__ == "__main__":
    unittest.main()
