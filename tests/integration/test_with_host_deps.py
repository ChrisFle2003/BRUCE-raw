from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


WRAPPER = Path(sys.argv.pop(1)).resolve()


class WithHostDepsTests(unittest.TestCase):
    def test_rejects_dependency_root_with_path_separator(self):
        with tempfile.TemporaryDirectory(prefix='bruce:gvfs-') as directory:
            root = Path(directory)
            wrapper = root / 'scripts' / 'with-host-deps.sh'
            wrapper.parent.mkdir()
            shutil.copy2(WRAPPER, wrapper)

            bundled_pkgconf = root / 'build-host-deps' / 'root' / 'usr' / 'bin' / 'pkgconf'
            bundled_pkgconf.parent.mkdir(parents=True)
            bundled_pkgconf.write_text('#!/bin/sh\nexit 0\n')
            bundled_pkgconf.chmod(0o755)

            marker = root / 'command-ran'
            result = subprocess.run(
                [str(wrapper), 'sh', '-c', f'touch "{marker}"'],
                capture_output=True,
                text=True,
                timeout=10,
            )

            self.assertEqual(result.returncode, 2, result.stderr)
            self.assertIn('Unsupported dependency path', result.stderr)
            self.assertFalse(marker.exists(), 'wrapper executed a command with a split dependency path')


if __name__ == '__main__':
    unittest.main()
