#!/usr/bin/env python3
"""Behavioral checks for the host-dependency environment wrapper."""

from __future__ import annotations

import pathlib
import shutil
import subprocess
import tempfile
import unittest


REPOSITORY = pathlib.Path(__file__).resolve().parents[2]


class WithHostDepsWrapperTests(unittest.TestCase):
    def make_project(self, parent: pathlib.Path, name: str) -> pathlib.Path:
        project = parent / name
        (project / "scripts").mkdir(parents=True)
        shutil.copy2(REPOSITORY / "scripts" / "with-host-deps.sh", project / "scripts")
        pkgconf = project / "build-host-deps/root/usr/bin/pkgconf"
        pkgconf.parent.mkdir(parents=True)
        pkgconf.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
        pkgconf.chmod(0o755)
        return project

    def run_wrapper(self, project: pathlib.Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(project / "scripts/with-host-deps.sh"), "/usr/bin/true"],
            capture_output=True,
            text=True,
            check=False,
        )

    def test_rejects_whitespace_in_dependency_path_before_running_command(self) -> None:
        """Removing the whitespace guard would execute true instead of stopping."""
        with tempfile.TemporaryDirectory() as temporary:
            project = self.make_project(pathlib.Path(temporary), "project with space")
            result = self.run_wrapper(project)
        self.assertEqual(result.returncode, 2)
        self.assertIn("Unsupported dependency path", result.stderr)
        self.assertEqual(result.stdout, "")

    def test_rejects_colon_in_dependency_path(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            project = self.make_project(pathlib.Path(temporary), "project:colon")
            result = self.run_wrapper(project)
        self.assertEqual(result.returncode, 2)
        self.assertIn("Unsupported dependency path", result.stderr)

    def test_rejects_other_whitespace_in_dependency_path(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            project = self.make_project(pathlib.Path(temporary), "project\rreturn")
            result = self.run_wrapper(project)
        self.assertEqual(result.returncode, 2)
        self.assertIn("Unsupported dependency path", result.stderr)

    def test_executes_command_from_safe_path(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            project = self.make_project(pathlib.Path(temporary), "safe-project")
            result = self.run_wrapper(project)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stderr, "")


if __name__ == "__main__":
    unittest.main()
