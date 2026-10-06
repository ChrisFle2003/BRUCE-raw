#!/usr/bin/env python3
"""Reproducible Linux CLI benchmarks. No third-party Python dependencies."""
import argparse
import csv
from datetime import datetime, timezone
from fractions import Fraction
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import resource
import signal
import statistics
import subprocess
import sys
import time


def check_result(result, expected):
    for path, value in expected.items():
        node = result
        try:
            for key in path.split('.'):
                node = node[key]
        except (KeyError, TypeError):
            return False
        if node != value:
            return False
    return True


def prepare_output(path):
    path.mkdir(parents=True, exist_ok=False)


def measure(command, root, name, timeout, memory_mib):
    """Linux wait4 measures each child separately. Timeout kills the process group."""
    stdout = root / (name + '.stdout')
    stderr = root / (name + '.stderr')
    def limits():
        maximum = memory_mib * 1024 * 1024
        resource.setrlimit(resource.RLIMIT_AS, (maximum, maximum))
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    started = time.perf_counter()
    timed_out = False
    with stdout.open('xb') as out, stderr.open('xb') as err:
        process = subprocess.Popen(list(map(str, command)), cwd=root, stdout=out, stderr=err,
                                   start_new_session=True, preexec_fn=limits)
        try:
            while True:
                pid, status, usage = os.wait4(process.pid, os.WNOHANG)
                if pid:
                    break
                if time.perf_counter()-started >= timeout:
                    timed_out = True
                    os.killpg(process.pid, signal.SIGKILL)
                    _, status, usage = os.wait4(process.pid, 0)
                    break
                time.sleep(.001)
            code = os.waitstatus_to_exitcode(status)
            process.returncode = code
        except BaseException:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            raise
    elapsed = time.perf_counter() - started
    cpu = usage.ru_utime + usage.ru_stime
    rss = usage.ru_maxrss
    return {'exit_code': code, 'timed_out': timed_out, 'wall_seconds': elapsed,
            'cpu_seconds': cpu, 'peak_rss_kib': rss,
            'stdout_bytes': stdout.stat().st_size, 'stderr_bytes': stderr.stat().st_size,
            'stdout_file': stdout.name, 'stderr_file': stderr.name}


def summarize(rows):
    good = [row for row in rows if row['passed']]
    result = {'runs': len(rows), 'passed': len(good), 'failed': len(rows) - len(good)}
    for key, prefix in [('wall_seconds', 'wall'), ('cpu_seconds', 'cpu'), ('peak_rss_kib', 'rss_kib')]:
        values = [row[key] for row in good if row[key] is not None]
        result[prefix + '_median'] = statistics.median(values) if values else None
        result[prefix + '_min'] = min(values) if values else None
        result[prefix + '_max'] = max(values) if values else None
    return result


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def make_cases(root, profile):
    if hasattr(sys, 'set_int_max_str_digits'):
        sys.set_int_max_str_digits(20000)
    def pairs(name, xs, fn):
        (root / name).write_text('x,y\n' + ''.join(f'{x},{fn(x)}\n' for x in xs))
    pairs('train.csv', range(5), lambda x: x*x)
    pairs('hold.csv', [7, 11], lambda x: x*x)
    pairs('expression.csv', range(4), lambda x: x+1)
    pairs('expression-hold.csv', [7, 11], lambda x: x+1)
    pairs('fraction.csv', range(4), lambda x: Fraction(x+2, x+1))
    pairs('fraction-hold.csv', [5], lambda x: Fraction(x+2, x+1))
    pairs('target.csv', range(4), lambda x: Fraction(2*x+3, x+1))
    pairs('target-hold.csv', [5], lambda x: Fraction(2*x+3, x+1))
    cases = []
    def add(name, args, expected, code=0, formula=None, replay=False):
        cases.append({'name': name, 'args': list(map(str, args)), 'expected': expected,
                      'code': code, 'formula': formula, 'replay': replay})
    for bits in ([64, 512] if profile == 'quick' else [64, 512, 2048, 8192]):
        a, b = (1 << (bits-2)) + 13, (1 << (bits-3)) + 7
        add(f'bigint-mul-{bits}', ['calculate', '--op', 'mul', '--left', a, '--right', b,
                                 '--max-bits', min(16384, bits*2)], {'value': str(a*b)})
        add(f'rational-add-{bits}', ['calculate', '--domain', 'rational', '--op', 'add',
                                    '--left', f'{a}/{b}', '--right', f'{b}/{a}',
                                    '--max-bits', min(16384, bits*2)], {'value': str(Fraction(a,b)+Fraction(b,a))})
    base = ['research', '--train', 'train.csv', '--holdout', 'hold.csv', '--min', -2, '--max', 2]
    for planner in ['enumerate', 'adaptive']:
        add('power-' + planner, base + ['--strategy', 'power-affine', '--max-power', 2, '--planner', planner],
            {'status': 'VERIFIED_ON_SAMPLES'}, formula='square')
    add('polynomial', base + ['--strategy', 'polynomial', '--max-degree', 2],
        {'status': 'VERIFIED_ON_SAMPLES'}, formula='square')
    for planner in ['grammar', 'indexed-grammar', 'cached-grammar']:
        add('expression-' + planner, ['research', '--strategy', 'expression', '--planner', planner,
            '--train', 'expression.csv', '--holdout', 'expression-hold.csv', '--max-nodes',
            3 if profile == 'quick' else 5, '--operators', '+*', '--equivalence', 'rational',
            '--max-candidates', 100000, '--max-operations', 1000000],
            {'status': 'VERIFIED_ON_SAMPLES'}, formula='linear')
    for planner in ['portfolio', 'adaptive-order', 'experience-order']:
        argv = ['research', '--strategy', 'portfolio-rational', '--planner', planner,
                '--train', 'target.csv', '--holdout', 'target-hold.csv', '--min', -3, '--max', 3,
                '--max-degree', 2, '--max-power', 2, '--max-denominator', 1]
        if planner == 'experience-order':
            argv += ['--experience', 'routing.experience']
        add('portfolio-' + planner, argv, {'status': 'VERIFIED_ON_SAMPLES'}, formula='fraction')
    add('rational-linear', ['research', '--strategy', 'rational-linear', '--train', 'target.csv',
        '--holdout', 'target-hold.csv', '--min', -3, '--max', 3, '--max-denominator', 1],
        {'status': 'VERIFIED_ON_SAMPLES'}, formula='fraction')
    count = {'quick': 64, 'standard': 512, 'stress': 1024}[profile]
    repeated = ';'.join(['2,6', '4,3'] * (count//2))
    for cache in [0, 65536]:
        add(f'grid-repeat-cache-{cache}', ['grid-batch', '--paths', repeated, '--cache-bytes', cache],
            {'status': 'COMPLETED', 'evaluations': count if cache == 0 else 2})
    unique = ';'.join(f'{i//64%8},{i//8%8}/{i%8},2' for i in range(count))
    add('grid-unique', ['grid-batch', '--paths', unique], {'status': 'COMPLETED', 'evaluations': count})
    add('grid-resume', ['grid-resume', '--from-run', 'partial.bruce', '--additional-queries', count], {'status': 'COMPLETED'})
    add('replay-grid', ['replay', 'grid.bruce'], {'status': 'COMPLETED', 'evaluations': 2})
    add('replay-corrupt', ['replay', 'corrupt.bruce'], {'error': 'InvalidEvidence: digest mismatch'}, 2)
    rules = 8 if profile == 'quick' else 128
    candidates = {'quick': 64, 'standard': 1024, 'stress': 4096}[profile]
    (root/'rules.txt').write_text('BRUCE_RULES_V1\nsnapshot benchmark\n' + ''.join(
        f'rule r{i} positive value ge 0 0 1 manual -\n' for i in range(rules)))
    (root/'candidates.csv').write_text('id,domain,layer,x,y,value\n' + ''.join(
        f'{i},bench,0,{i},0,1\n' for i in range(candidates)))
    add('compatibility', ['compatibility', '--rules', 'rules.txt', '--candidates', 'candidates.csv',
        '--max-trace', 65536], {'status': 'AMBIGUOUS'} if rules*candidates<=65536 else
        {'status': 'INCOMPLETE', 'stop_reason': 'TraceBudgetExceeded'})
    for policy in ['quadratic', 'modular', 'modular-witness']:
        add('compare-' + policy, ['compare', '--left-numerator', '-1,0,1', '--left-denominator', '-1,1',
            '--right-numerator', '1,1', '--right-denominator', 1, '--domain-policy', policy], {'status': 'PROVEN_DIFFERENT'})
    add('export-verified', base + ['--strategy', 'polynomial', '--max-degree', 2, '--verification', 'replay',
        '--output', '@EXPORT@'], {'execution.replay_verified': True, 'result.status': 'VERIFIED_ON_SAMPLES'}, formula='square', replay=True)
    for option, phase in [('--max-session-checkpoints', 'EXECUTING'), ('--max-serialized-bytes', 'SERIALIZING'), ('--max-replay-bytes', 'REPLAYING')]:
        add('budget-' + phase.lower(), ['grid-batch', '--paths', '2,6', '--verification', 'replay',
            option, 0, '--output', '@EXPORT@'], {'execution.state': 'FAILED', 'execution.failed_at': phase}, 2)
    return cases


def check_formula(result, family):
    """Independent Python Fraction substitution at unseen points; never eval()."""
    result = result.get('result', result)
    hypotheses = result.get('hypotheses', [])
    if not hypotheses:
        return False
    for hypothesis in hypotheses:
        if hypothesis.get('status') != 'VERIFIED_ON_SAMPLES':
            return False
        for x in [-3, 6, 13]:
            expected = x*x if family == 'square' else x+1
            if 'coefficients' in hypothesis:
                value = sum(Fraction(c)*x**i for i,c in enumerate(hypothesis['coefficients']))
                if hypothesis.get('denominator'):
                    denominator = sum(Fraction(c)*x**i for i,c in enumerate(hypothesis['denominator']))
                    if not denominator:
                        return False
                    value /= denominator
            elif 'a' in hypothesis and 'b' in hypothesis:
                value = Fraction(hypothesis['a']) * x**hypothesis.get('power', 1) + Fraction(hypothesis['b'])
                if hypothesis.get('denominator'):
                    value /= sum(Fraction(c)*x**i for i,c in enumerate(hypothesis['denominator']))
            elif 'normal_form' in hypothesis:
                normal = hypothesis['normal_form']
                numerator = sum(Fraction(c)*x**i for i,c in enumerate(normal['numerator']))
                denominator = sum(Fraction(c)*x**i for i,c in enumerate(normal['denominator']))
                if not denominator:
                    return False
                value = numerator/denominator
            else:
                return False
            if family == 'fraction':
                expected = Fraction(2*x+3, x+1)
            if value != expected:
                return False
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='New directory; never overwritten')
    parser.add_argument('--profile', choices=['quick','standard','stress'], default='quick')
    parser.add_argument('--repeats', type=int)
    parser.add_argument('--timeout', type=float, default=30)
    parser.add_argument('--memory-mib', type=int, default=1024)
    parser.add_argument('--seed', type=int, default=20260925)
    args = parser.parse_args()
    repeats = args.repeats if args.repeats is not None else {'quick':3,'standard':10,'stress':30}[args.profile]
    if not 1<=repeats<=1000 or args.timeout<=0 or args.memory_mib<64:
        parser.error('repeats 1..1000, timeout >0, memory >=64 MiB required')
    binary = args.binary.resolve(strict=True)
    root = args.output.resolve()
    prepare_output(root)
    cases = make_cases(root, args.profile)
    # Preparatory evidence generation is excluded from timing comparisons but retained.
    setup_commands = [
        ['research','--strategy','portfolio-rational','--train','fraction.csv','--holdout','fraction-hold.csv',
         '--min',-3,'--max',3,'--max-degree',2,'--max-power',2,'--max-denominator',1,'--output','source.bruce'],
        ['experience','add','--from-run','source.bruce','--output','routing.experience'],
        ['grid-batch','--paths','2,6;4,3;2,6','--output','grid.bruce'],
        ['grid-batch','--paths','2,6;4,3;2,6','--max-query-steps',1,'--output','partial.bruce']]
    for index, command in enumerate(setup_commands):
        setup = measure([binary,*command],root,f'setup-{index}',args.timeout,args.memory_mib)
        if setup['exit_code']!=0:
            raise RuntimeError(f'Setup failed: {command}: see setup-{index}.stderr')
    corrupt = bytearray((root/'grid.bruce').read_bytes())
    corrupt[-2] ^= 1
    (root/'corrupt.bruce').write_bytes(corrupt)
    metadata = {'contract':'bruce-benchmark-v1', 'profile':args.profile, 'repeats':repeats,
        'seed':args.seed, 'utc':datetime.now(timezone.utc).isoformat(), 'binary':str(binary),
        'binary_sha256':sha(binary), 'platform':platform.platform(), 'python':platform.python_version(),
        'cpu_count':os.cpu_count(), 'cpu_affinity':sorted(os.sched_getaffinity(0)),
        'cpuinfo':Path('/proc/cpuinfo').read_text(), 'memory':Path('/proc/meminfo').read_text(),
        'timeout_seconds':args.timeout, 'address_space_limit_mib':args.memory_mib,
        'measurement':'sequential fresh CLI processes; OS caches uncontrolled; no excluded warmup',
        'setup_commands':setup_commands,
        'inputs':{p.name:sha(p) for p in root.iterdir() if p.is_file()}}
    project = Path(__file__).resolve().parent.parent
    for label, command in [('compiler',['c++','--version']),('libraries',['ldd',str(binary)])]:
        try:
            metadata[label] = subprocess.run(command,capture_output=True,text=True,timeout=5).stdout
        except (OSError, subprocess.TimeoutExpired):
            metadata[label] = None
    metadata['runner_sha256'] = sha(Path(__file__))
    metadata['source_hashes'] = {str(path.relative_to(project)):sha(path) for pattern in
        ['src/**/*.cpp','include/**/*.hpp','apps/*.cpp','CMakeLists.txt'] for path in sorted(project.glob(pattern))}
    version = measure([binary, '--version'], root, 'version', args.timeout, args.memory_mib)
    if version['exit_code'] != 0:
        raise RuntimeError('Cannot execute benchmark binary')
    metadata['version'] = json.loads((root/'version.stdout').read_text())
    (root/'manifest.json').write_text(json.dumps({'metadata':metadata,'cases':cases}, indent=2)+'\n')
    schedule = [(case, repeat) for repeat in range(repeats) for case in cases]
    random.Random(args.seed).shuffle(schedule)
    rows = []
    with (root/'measurements.jsonl').open('x') as log:
        for case, repeat in schedule:
            name = f'{case["name"]}-{repeat:03d}'
            export = root/(name+'.bruce')
            argv = [str(export) if arg=='@EXPORT@' else arg for arg in case['args']]
            row = measure([binary,*argv], root, name, args.timeout, args.memory_mib)
            result = None
            try:
                result = json.loads((root/row['stdout_file' if case['code']==0 else 'stderr_file']).read_text())
                passed = row['exit_code']==case['code'] and not row['timed_out'] and check_result(result,case['expected'])
                if passed and case['formula']:
                    passed = check_formula(result, case['formula'])
                if passed and case['name']=='portfolio-experience-order':
                    passed = result.get('experience_records_used') == 1
            except (ValueError, TypeError, KeyError, ZeroDivisionError):
                passed = False
            if case['code']!=0 and export.exists():
                passed = False
            if passed and case['replay']:
                replay = measure([binary,'replay',export],root,name+'-replay',args.timeout,args.memory_mib)
                try:
                    passed = replay['exit_code']==0 and json.loads((root/replay['stdout_file']).read_text())==result['result']
                except (ValueError, KeyError):
                    passed = False
                row['replay_measurement'] = replay
            native = result.get('result',result) if isinstance(result,dict) else {}
            row.update({'case':case['name'],'repeat':repeat,'passed':passed,
                'expected_exit_code':case['code'],'status':native.get('status'),
                'stop_reason':native.get('stop_reason'),'operations':native.get('operations_used',native.get('operations')),
                'candidates':native.get('candidates_executed'),'cache':native.get('cache'),
                'trace_bytes_reported':native.get('trace_bytes'),
                'bundle_bytes':export.stat().st_size if export.exists() else 0})
            rows.append(row)
            log.write(json.dumps(row)+'\n');log.flush()
            print(f'{name}: {"PASS" if passed else "FAIL"} {row["wall_seconds"]:.4f}s',flush=True)
    summary = {case['name']:summarize([row for row in rows if row['case']==case['name']]) for case in cases}
    (root/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    with (root/'summary.csv').open('x',newline='') as stream:
        writer = csv.DictWriter(stream,fieldnames=['case',*next(iter(summary.values())).keys()])
        writer.writeheader()
        for case, values in summary.items():
            writer.writerow({'case':case,**values})
    report = ['# BRUCE Benchmark V1', '',f'Profil: {args.profile}; Wiederholungen: {repeats}.', '',
        'Messung vollständiger CLI-Prozesse inklusive Start und Ausgabe. Linux wait4: CPU-Sekunden und maximaler RSS in KiB. '
        'Zeitlimit pro Prozess, RLIMIT_AS begrenzt virtuellen Adressraum, nicht RSS. '
        'Betriebssystemcache nicht kontrolliert; Grid-Caches starten je Prozess leer. '
        'Erwartete Fehler und INCOMPLETE-Fälle sind Funktionsprüfungen, keine erfolgreichen Problemlösungen. '
        'Keine Rangliste über unterschiedliche Fälle oder Ergebnisqualitäten. Fehlgeschlagene Messungen sind aus Medianen ausgeschlossen.', '',
        '| Fall | Bestanden | Median s | Min–Max s | Median RSS KiB |', '| --- | --- | --- | --- | --- |']
    for name, stats in summary.items():
        report.append(f'| {name} | {stats["passed"]}/{stats["runs"]} | {stats["wall_median"]} | {stats["wall_min"]}–{stats["wall_max"]} | {stats["rss_kib_median"]} |')
    report += ['', 'Rohdaten: measurements.jsonl; vollständige Antworten: *.stdout/*.stderr; '
        'Aufgaben, Eingabehashes und Hardware: manifest.json. '
        'Zusätzliche Python-Gegenprüfungen und Replay laufen außerhalb der primären Messung. '
        'Kurze Prozesse sind durch Startkosten, Timerauflösung und Warteverwaltung dominiert. '
        'Diese Suite ist kein Mikrobenchmark des C++-Kerns und kein allgemeiner Mathematikbeweis.']
    (root/'report.md').write_text('\n'.join(report)+'\n')
    return 0 if all(row['passed'] for row in rows) else 1


if __name__=='__main__':
    raise SystemExit(main())
