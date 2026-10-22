"""Exercise the local Markdown checker against actual temporary documents."""
from pathlib import Path
import subprocess
import tempfile
import unittest


class LocalDocLinkTests(unittest.TestCase):
    checker = Path(__file__).resolve().parents[2] / 'scripts/check_local_doc_links.py'

    def check(self, files):
        with tempfile.TemporaryDirectory(prefix='bruce-doc-links-') as directory:
            root = Path(directory)
            for name, text in files.items():
                target = root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(text)
            return subprocess.run(['python3', str(self.checker), '--root', str(root)],
                                  capture_output=True, text=True, timeout=10)

    def test_valid_relative_files_encoded_spaces_and_external_links(self):
        result = self.check({'README.md': '[local](docs/a%20b.md#title) [remote](https://example.invalid/a) [anchor](#title)\n',
                             'docs/a b.md': '[up](../README.md)\n'})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('links=2 broken=0', result.stdout)

    def test_missing_target_reports_document_and_returns_failure(self):
        result = self.check({'README.md': '[missing](docs/absent.md)\n'})
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn('README.md:1: docs/absent.md', result.stdout)
        self.assertIn('broken=1', result.stdout)

    def test_multiline_inline_missing_target_is_reported(self):
        result = self.check({'README.md': '# Links\n[missing](\n  docs/absent.md\n)\n'})
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn('README.md:2: docs/absent.md', result.stdout)
        self.assertIn('links=1 broken=1', result.stdout)

    def test_multiline_reference_missing_target_is_reported(self):
        result = self.check({'README.md': '[label]:\n  docs/absent.md\n\n[missing][label]\n'})
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn('README.md:1: docs/absent.md', result.stdout)
        self.assertIn('links=1 broken=1', result.stdout)

    def test_balanced_parentheses_in_existing_filenames(self):
        result = self.check({'README.md': '[chapter](docs/chapter(1).md) [nested](docs/a(b(c)).md)\n',
                             'docs/chapter(1).md': '# Chapter\n',
                             'docs/a(b(c)).md': '# Nested\n'})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('links=2 broken=0', result.stdout)

    def test_escaped_parentheses_in_inline_and_reference_destinations(self):
        result = self.check({'README.md': '[chapter](docs/chapter\\(1\\).md)\n[label]:\n  docs/chapter\\(1\\).md\n[again][label]\n',
                             'docs/chapter(1).md': '# Chapter\n'})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('links=2 broken=0', result.stdout)

    def test_multiline_angle_destination_with_spaces_and_title(self):
        result = self.check({'README.md': '[chapter](\n <docs/chapter (1).md>\n "Chapter"\n)\n[label]:\n <docs/chapter (1).md>\n[again][label]\n',
                             'docs/chapter (1).md': '# Chapter\n'})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('links=2 broken=0', result.stdout)

    def test_multiline_links_in_fenced_examples_are_ignored(self):
        result = self.check({'README.md': '~~~markdown\n[example](\n missing.md\n)\n[label]:\n missing-reference.md\n~~~\n[real](docs/real.md)\n',
                             'docs/real.md': '# Real\n'})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('links=1 broken=0', result.stdout)

    def test_uses_existing_ci_documentation_scope(self):
        result = self.check({'README.md': '# Root\n',
                             'docs/guide.md': '# Guide\n',
                             'benchmarks/README.md': '# Benchmark\n',
                             'website/content/en/copy.md': '[site copy](missing.md)\n',
                             'runs/evidence.md': '[local artifact](missing.md)\n'})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('documents=3 links=0 broken=0', result.stdout)

    def test_fenced_examples_are_not_real_document_links(self):
        result = self.check({'README.md': '```markdown\n[example](absent.md)\n```\n[real](docs/real.md)\n',
                             'docs/real.md': '# Real\n'})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('links=1 broken=0', result.stdout)


if __name__ == '__main__':
    unittest.main()
