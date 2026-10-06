#!/usr/bin/env python3
"""Verify a relocated CLI installation and optionally a locally built CPack archive."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    with path.open('rb') as stream:
        value = hashlib.sha256()
        for block in iter(lambda: stream.read(65536), b''):
            value.update(block)
        return value.hexdigest()


def command(args, cwd):
    result = subprocess.run(list(map(str, args)), cwd=cwd, capture_output=True, text=True)
    require(result.returncode == 0, f'{args[0]} failed: {result.stderr or result.stdout}')
    return result.stdout


def call(binary, args, cwd, code=0):
    result = subprocess.run([str(binary), *map(str, args)], cwd=cwd, capture_output=True, text=True)
    require(result.returncode == code, f'CLI exit {result.returncode}, expected {code}: {result.stderr}')
    return json.loads(result.stdout if code == 0 else result.stderr)


def verify_tree(prefix, baseline, fixtures, workspace, version):
    binary = prefix / 'bin/bruce'
    build_info = json.loads((prefix / 'share/bruce/build-info.json').read_text())
    require(digest(prefix / 'share/bruce/source-files.sha256') == build_info['source_index_sha256'],
            'Source index digest mismatch')
    require(build_info['version'] == version, 'Installed build metadata version mismatch')
    require(build_info['build_type'], 'Missing build configuration')
    require((prefix / 'share/bruce/DEPENDENCIES.md').is_file(), 'Missing dependency notices')
    require((prefix / 'share/bruce/THIRD_PARTY_NOTICES.md').is_file(), 'Missing third-party notices')
    require((prefix / 'share/bruce/LICENSE').is_file(), 'Missing project license')
    require((prefix / 'share/bruce/NOTICE').is_file(), 'Missing project notice')
    require(call(binary, ['--version'], workspace) == {'contract': 'bruce-version-v1', 'version': version},
            'Installed CLI version mismatch')
    train, hold, result = (workspace / name for name in ('train.csv', 'hold.csv', 'result.bruce'))
    train.write_text('x,y\n0,3\n1,5\n4,11\n')
    hold.write_text('x,y\n2,7\n10,23\n')
    args = ['research', '--train', train, '--holdout', hold, '--min', '-3', '--max', '3',
            '--verification', 'replay', '--max-session-checkpoints', '100000']
    checked = call(binary, [*args, '--output', result], workspace)
    require(checked['execution']['replay_verified'] and checked['execution']['result_complete'],
            'Research session not complete and verified')
    require(checked['result']['hypotheses'], 'No affine hypothesis found')
    require(call(binary, ['replay', result], workspace) == checked['result'], 'Installed replay differs')
    failed_target = workspace / 'must-not-exist.bruce'
    failure_args = ['grid-batch', '--paths', '2,6;4,3', '--verification', 'replay',
                    '--max-session-checkpoints', '0', '--output', failed_target]
    failure = call(binary, failure_args, workspace, code=2)
    require(failure['execution']['failed_at'] == 'EXECUTING' and
            failure['execution']['cause'].startswith('SessionCheckpointBudgetExceeded:'), 'Wrong budget failure')
    require(not failed_target.exists(), 'Budget failure exported a result')
    failed_target.write_text('keep')
    call(binary, failure_args, workspace, code=2)
    require(failed_target.read_text() == 'keep', 'Budget failure overwrote existing file')
    restored = []
    for fixture in fixtures:
        copied = workspace / fixture.name
        shutil.copyfile(fixture, copied)
        require(call(binary, ['replay', copied], workspace) == call(baseline, ['replay', copied], workspace),
                f'Historical replay differs: {fixture.name}')
        restored.append({'name': fixture.name, 'sha256': digest(copied)})
    return {'version': version, 'build_info': build_info, 'binary_sha256': digest(binary),
            'research_export_replay': True, 'budget_failure_no_export': True,
            'historical_fixtures': restored}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, help='Actual build CLI (e.g. a multi-config target)')
    parser.add_argument('--archive-out', type=Path, help='Keep the tested archive at a new path; requires --package')
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--config', default='Release')
    parser.add_argument('--package', action='store_true')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    baseline = args.baseline.resolve() if args.baseline else build / args.config / 'bruce'
    if not args.baseline and not baseline.is_file():
        baseline = build / 'bruce'
    fixtures = sorted(args.fixtures.resolve().glob('*.bruce'))
    require(baseline.is_file(), 'Build CLI missing')
    require(fixtures, 'No historical fixtures found')
    if args.report:
        require(not args.report.exists(), 'Report already exists')
    if args.archive_out:
        require(args.package, '--archive-out requires --package')
        require(not args.archive_out.exists(), 'Archive output already exists')
    with tempfile.TemporaryDirectory(prefix='bruce-install-audit-') as directory:
        root = Path(directory)
        prefix = root / 'stage'
        command(['cmake', '--install', build, '--config', args.config, '--prefix', prefix], root)
        relocated = root / 'relocated'
        prefix.rename(relocated)
        runtime = root / 'independent-runtime'
        runtime.mkdir()
        report = {'contract': 'bruce-install-audit-v1',
                  'installation': verify_tree(relocated, baseline, fixtures, runtime, args.version)}
        if args.package:
            packages = root / 'packages'
            command(['cpack', '--config', build / 'CPackConfig.cmake', '-G', 'TGZ', '-B', packages,
                     '-C', args.config], root)
            archives = list(packages.glob('*.tar.gz'))
            require(len(archives) == 1, 'Expected one CPack archive')
            unpacked = root / 'unpacked'
            unpacked.mkdir()
            with tarfile.open(archives[0]) as archive:
                # Accept only ordinary files/directories beneath this fresh extraction root.
                for member in archive.getmembers():
                    relative = Path(member.name)
                    require(not relative.is_absolute() and '..' not in relative.parts,
                            'Unsafe package member path')
                    destination = unpacked / relative
                    if member.isdir():
                        destination.mkdir(parents=True, exist_ok=True)
                    else:
                        require(member.isfile(), 'Package contains a non-regular file')
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        with archive.extractfile(member) as source, destination.open('xb') as target:
                            shutil.copyfileobj(source, target)
                        destination.chmod(member.mode & 0o777)
            candidates = list(unpacked.glob('*/bin/bruce'))
            require(len(candidates) == 1, 'Expected one packaged CLI')
            package_runtime = root / 'package-runtime'
            package_runtime.mkdir()
            report['package'] = verify_tree(candidates[0].parent.parent, baseline, fixtures, package_runtime, args.version)
            report['package']['archive_sha256'] = digest(archives[0])
            report['package']['archive_name'] = archives[0].name
            if args.archive_out:
                with archives[0].open('rb') as source, args.archive_out.open('xb') as target:
                    shutil.copyfileobj(source, target)
    output = json.dumps(report, ensure_ascii=False, indent=2) + '\n'
    if args.report:
        with args.report.open('x') as stream:
            stream.write(output)
    print(output, end='')


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, ValueError, tarfile.TarError) as error:
        raise SystemExit(str(error))
