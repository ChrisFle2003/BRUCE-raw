import argparse
import json
from pathlib import Path
import subprocess


def run(*command):
    return subprocess.run(command, capture_output=True, text=True, timeout=120)


parser = argparse.ArgumentParser()
parser.add_argument('--cmake', required=True)
parser.add_argument('--source', required=True)
parser.add_argument('--build', required=True)
args = parser.parse_args()

source = Path(args.source).resolve()
build = Path(args.build).resolve()

configured = run(args.cmake, '-S', str(source), '-B', str(build), '-G', 'Ninja',
                 '-DBUILD_TESTING=OFF', '-DBRUCE_BUILD_MOD_LANGUAGE=OFF',
                 '-DBRUCE_BUILD_MOD_NANOCELL=OFF')
assert configured.returncode == 0, configured.stderr

built = run(args.cmake, '--build', str(build), '--target', 'bruce')
assert built.returncode == 0, built.stderr

binary = build / 'bruce'
version = run(str(binary), '--version')
assert version.returncode == 0, version.stderr
assert json.loads(version.stdout)['contract'] == 'bruce-version-v1'

grid = run(str(binary), 'grid', '--path', '2,6')
assert grid.returncode == 0, grid.stderr
assert json.loads(grid.stdout)['value'] == '64'

language = run(str(binary), 'language')
assert language.returncode == 2, language.stderr
assert 'UnknownCommand: language' in json.loads(language.stderr)['error']

nanocell = run(str(binary), 'nanocell')
assert nanocell.returncode == 2, nanocell.stderr
assert 'UnknownCommand: nanocell' in json.loads(nanocell.stderr)['error']

help_output = run(str(binary), '--help')
assert help_output.returncode == 0, help_output.stderr
assert 'bruce nanocell ' not in help_output.stdout
