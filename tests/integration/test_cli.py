import hashlib
import json
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest

BIN = str(Path(sys.argv.pop(1)).resolve())


class CliTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def call(self, *args, ok=True):
        p = subprocess.run([BIN, *map(str, args)], capture_output=True, text=True, timeout=10)
        if ok:
            self.assertEqual(p.returncode, 0, p.stderr)
            return json.loads(p.stdout)
        self.assertNotEqual(p.returncode, 0)
        self.assertIn('error', json.loads(p.stderr))
        return p

    def pairs(self, name, pairs):
        path = self.root / name
        path.write_text('x,y\n' + ''.join(f'{x},{y}\n' for x, y in pairs))
        return path

    def test_dynamic_unseen_inputs(self):
        rng = random.Random(173)
        for i in range(12):
            a, b = rng.randint(-7, 7), rng.randint(-7, 7)
            train = self.pairs(f'train{i}.csv', [(x, a*x+b) for x in (-13, 0, 17)])
            hold = self.pairs(f'hold{i}.csv', [(x, a*x+b) for x in (21, 33)])
            result = self.call('research', '--train', train, '--holdout', hold)
            self.assertEqual(result['status'], 'VERIFIED_ON_SAMPLES')
            self.assertEqual([(h['a'], h['b']) for h in result['hypotheses']], [(a, b)])
            self.assertFalse(result['general_proof'])
        self.assertEqual(len(list(self.root.iterdir())), 24, 'no implicit database')

    def test_replay_new_process_and_no_overwrite(self):
        train = self.pairs('train.csv', [(0, 3), (1, 5), (4, 11)])
        hold = self.pairs('hold.csv', [(2, 7), (10, 23)])
        out = self.root / 'run.bruce'
        result = self.call('research', '--train', train, '--holdout', hold, '--output', out)
        original = out.read_bytes()
        train.unlink()
        hold.unlink()
        self.assertEqual(self.call('replay', out), result)
        train = self.pairs('train.csv', [(0, 3), (1, 5)])
        self.call('research', '--train', train, '--output', out, ok=False)
        self.assertEqual(out.read_bytes(), original)

    def test_rehash_does_not_hide_wrong_result_or_version(self):
        train = self.pairs('train.csv', [(0, 3), (1, 5)])
        out = self.root / 'run.bruce'
        self.call('research', '--train', train, '--output', out)
        header, digest, payload = out.read_text().split('\n', 2)
        for mutated in [payload.replace('"general_proof":false', '"general_proof":true'),
                        payload.replace('bruce-core-v1', 'bruce-core-v9')]:
            out.write_text(header+'\n'+hashlib.sha256(mutated.encode()).hexdigest()+'\n'+mutated)
            self.call('replay', out, ok=False)

    def test_invalid_input(self):
        for content in ['Startwert,Zielwert\n1,2\n', 'x,y\n1,2junk\n',
                        'x,y\n9223372036854775808,2\n', 'x,y\n', 'x,y\n1,2\n1,3\n']:
            file = self.root / 'bad.csv'
            file.write_text(content)
            self.call('research', '--train', file, ok=False)
        self.call('research', '--train', self.root/'missing.csv', ok=False)
        self.call('coordinate', '--x', '-1', '--y', '0', ok=False)
        self.call('coordinate', '--x', '0', '--x', '1', '--y', '0', ok=False)
        self.call('research', '--unknown', '1', ok=False)

    def test_malformed_bytes_still_produce_json_errors(self):
        file = self.root / 'bad-bytes.csv'
        for malformed in [b'\xff', b'\xc0\x80', b'\xed\xa0\x80', b'\xf4\x90\x80\x80', b'1'*79+b'\xc3\xa4']:
            file.write_bytes(b'x,y\n1,'+malformed+b'\n')
            process = subprocess.run([BIN,'research','--train',str(file)],capture_output=True,timeout=10)
            self.assertEqual(process.returncode,2)
            self.assertIn('error',json.loads(process.stderr))
        path = self.root/'fehlende-Äpfel.csv'
        process = self.call('research','--train',path,ok=False)
        self.assertIn('Äpfel',json.loads(process.stderr)['error'])

    def test_holdout_can_refute_training_alternatives(self):
        train = self.pairs('one.csv',[(0,0)])
        hold = self.pairs('hold.csv',[(1,1)])
        result = self.call('research','--train',train,'--holdout',hold)
        self.assertEqual(result['status'],'VERIFIED_ON_SAMPLES')
        self.assertEqual(len(result['hypotheses']),17)
        survivors = [h for h in result['hypotheses'] if h['status']=='VERIFIED_ON_SAMPLES']
        self.assertEqual([(h['a'],h['b']) for h in survivors],[(1,0)])
        self.assertFalse(result['general_proof'])

    def test_grid_modes_and_sentinel(self):
        path = '/'.join(['0,0']*11+['2,6'])
        exact = self.call('grid', '--path', path)
        self.assertEqual(exact['depth'], 12)
        self.assertEqual(exact['value'], '64')
        self.assertEqual(exact['evaluations'], 1)
        self.assertEqual(self.call('grid', '--path', '2,6', '--mode', 'modular', '--modulus', 17)['value'], '13')
        self.assertEqual(self.call('grid', '--path', '2,6', '--mode', 'metric')['bit_length'], 7)
        stopped = self.call('grid', '--path', '0,0', '--zero-zero', 'stop')
        self.assertEqual(stopped['stop_scope'], 'BRANCH')
        self.call('grid', '--path', '2,6', '--max-bits', '6', ok=False)
        self.call('grid', '--path', '2,6', '--mode', 'modular', '--modulus', '0', ok=False)

    def test_grid_selected_rule_and_legacy_default(self):
        legacy = self.call('grid', '--path', '2,3')
        self.assertEqual((legacy['rule'], legacy['value']), ('LEAF_LOCAL_POWER_V1', '8'))
        explicit = self.call('grid', '--path', '2,3', '--rule', 'leaf-local-power')
        self.assertEqual(explicit, legacy)
        out = self.root / 'scaled-grid.bruce'
        scaled = self.call('grid', '--path', '2,3', '--rule', 'scaled-exponential-v1', '--output', out)
        self.assertEqual((scaled['rule'], scaled['stage'], scaled['value']), ('SCALED_EXPONENTIAL_V1', 3, '700'))
        self.assertEqual(self.call('replay', out), scaled)

    def test_grid_stage_verify_exact_metric_and_replay(self):
        # Catches missing command, wrong Metric comparison, lost source or generic scope.
        for mode, endpoint in [('exact', '14411518807585587100'), ('metric', '64')]:
            with self.subTest(mode=mode):
                source = self.root / (mode + '-sweep.bruce')
                output = self.root / (mode + '-verify.bruce')
                self.call('grid-stage-sweep', '--path', '0,0', '--from-stage', 0, '--to-stage', 57,
                          '--mode', mode, '--output', source)
                original = source.read_bytes()
                result = self.call('grid-stage-verify', '--from-sweep', source, '--output', output)
                self.assertEqual(result['status'], 'VERIFIED_ON_DOMAIN')
                self.assertEqual((result['verified_domain'], result['checked_entries']), ('0..57', 58))
                self.assertEqual(result['formula'], '100*(2^stage-1)')
                self.assertEqual(result['mode'], mode)
                self.assertEqual(result['sweep_digest'], hashlib.sha256(original).hexdigest())
                self.assertEqual(result['claim_scope'], 'finite-uint64-scaled-exponential-stage-domain-0..57')
                self.assertEqual(result['records'][-1]['expected'], endpoint)
                self.assertEqual(result['records'][-1]['observed'], endpoint)
                self.assertTrue(all(r['matches'] for r in result['records']))
                self.assertIsNone(result['first_mismatch'])
                self.assertEqual(source.read_bytes(), original)
                saved = output.read_bytes()
                self.assertTrue(saved.startswith(b'BRUCE_GRID_STAGE_VERIFY_BUNDLE_V1\n'))
                self.assertIn(original, saved)
                self.assertEqual(self.call('grid-stage-verify', '--from-sweep', source), result)
                self.call('grid-stage-verify', '--from-sweep', source, '--output', output, ok=False)
                self.assertEqual(output.read_bytes(), saved)
                source.unlink()
                self.assertEqual(self.call('replay', output), result)
                for command in ['assess-run', 'inspect-run']:
                    summary = self.call(command, output)
                    self.assertEqual(summary['kind'], 'grid-stage-verify')
                    self.assertEqual(summary['status'], 'VERIFIED_ON_DOMAIN')
                    self.assertEqual(summary['claim_scope'], result['claim_scope'])
                    self.assertTrue(summary['replay_checked'])
                self.assertFalse(self.call('assess-run', output)['proof_established'])

    def test_grid_stage_verify_invalid_sources_and_closed_options(self):
        # Catches accepting partial/modular/foreign/rehashed inputs or writing partial evidence.
        source = self.root / 'source.bruce'
        output = self.root / 'invalid-verify.bruce'
        for extra in [['--mode', 'modular', '--modulus', 128], ['--max-bits', 63],
                      ['--from-stage', 1], ['--to-stage', 56]]:
            args = {'--from-stage': 0, '--to-stage': 57}
            if extra[0] in args:
                args[extra[0]] = extra[1]
                extra = []
            self.call('grid-stage-sweep', '--path', '0,0', *[v for pair in args.items() for v in pair],
                      *extra, '--output', source)
            process = self.call('grid-stage-verify', '--from-sweep', source, '--output', output, ok=False)
            self.assertEqual(json.loads(process.stderr)['error'], 'InvalidSweepVerificationSource')
            self.assertEqual(process.stdout, '')
            self.assertFalse(output.exists())
            source.unlink()
        self.call('grid-stage-sweep', '--path', '0,0', '--from-stage', 0, '--to-stage', 57, '--output', source)
        header, digest, payload = source.read_text().split('\n', 2)
        changed = payload.replace('"value":"100"', '"value":"101"', 1)
        self.assertNotEqual(changed, payload)
        invalid = [b'foreign', source.read_bytes()[:-1] + b'x',
                   (header + '\n' + hashlib.sha256(changed.encode()).hexdigest() + '\n' + changed).encode()]
        for content in invalid:
            source.write_bytes(content)
            process = self.call('grid-stage-verify', '--from-sweep', source, '--output', output, ok=False)
            self.assertEqual(json.loads(process.stderr)['error'], 'InvalidSweepVerificationSource')
            self.assertFalse(output.exists())
        for extra in [['--formula', 'free'], ['--rule', 'free'], ['--mode', 'metric'], ['--stage', 1],
                      ['--verification', 'replay'], ['--from-run', source]]:
            process = self.call('grid-stage-verify', '--from-sweep', source, *extra, ok=False)
            self.assertEqual(json.loads(process.stderr)['error'], 'UnknownOption: ' + extra[0])
        for args, error in [([], 'MissingOption: --from-sweep'),
                            (['--from-sweep'], 'MissingOptionValue: --from-sweep'),
                            (['--from-sweep', source, '--from-sweep', source], 'DuplicateOption: --from-sweep')]:
            process = self.call('grid-stage-verify', *args, ok=False)
            self.assertEqual(json.loads(process.stderr)['error'], error)

    def test_grid_stage_verify_help(self):
        process = subprocess.run([BIN, '--help'], capture_output=True, text=True, timeout=10)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertIn('bruce grid-stage-verify --from-sweep SWEEP.bruce [--output VERIFY.bruce]', process.stdout)
        syntax = process.stdout.split('bruce grid-stage-verify ', 1)[1].split('\nbruce ', 1)[0]
        self.assertIn('0..57', syntax)
        self.assertIn('finite', syntax)

    def test_grid_stage_sweep(self):
        # Catches missing CLI wiring, implicit leaf stages and debug changing results.
        args = ['grid-stage-sweep', '--path', '2,6', '--from-stage', 0, '--to-stage', 7]
        compact = self.call(*args)
        debug = self.call(*args, '--debug')
        self.assertEqual(compact['status'], 'COMPLETED')
        self.assertEqual([r['stage'] for r in debug['records']], list(range(8)))
        self.assertEqual([r['result']['value'] for r in debug['records']],
                         ['0', '100', '300', '700', '1500', '3100', '6300', '12700'])
        self.assertEqual(len({r['cache_key_digest'] for r in debug['records']}), 8)
        for plain, verbose in zip(compact['records'], debug['records']):
            self.assertNotIn('trace', plain)
            self.assertTrue(verbose['trace'])
            self.assertEqual(plain, {k: v for k, v in verbose.items() if k != 'trace'})
        zero = self.call('grid-stage-sweep', '--path', '0,0', '--from-stage', 0, '--to-stage', 7)
        self.assertEqual([r['result']['value'] for r in zero['records']],
                         ['0', '100', '300', '700', '1500', '3100', '6300', '12700'])

    def test_grid_stage_sweep_modes_and_partial(self):
        # Catches using Exact limits in Metric and losing the successful Exact prefix.
        args = ['grid-stage-sweep', '--path', '0,0', '--from-stage', 0, '--to-stage', 57]
        partial = self.call(*args, '--max-bits', 63)
        self.assertEqual((partial['status'], partial['stop_reason'], partial['stopped_stage']),
                         ('INCOMPLETE', 'IntegerBitLimit', 57))
        self.assertEqual(partial['processed_entries'], 57)
        self.assertEqual(partial['records'][-1]['stage'], 56)
        metric = self.call(*args, '--mode', 'metric', '--max-bits', 1)
        self.assertEqual(metric['processed_entries'], 58)
        self.assertEqual(metric['records'][-1]['result']['bit_length'], 64)
        modular = self.call(*args, '--mode', 'modular', '--modulus', 128)
        self.assertEqual(modular['processed_entries'], 58)
        self.assertEqual([r['result']['value'] for r in modular['records'][-2:]], ['28', '28'])
        bypass = self.call(*args, '--mode', 'metric', '--cache-bytes', 0)
        self.assertEqual(bypass['cache']['bypasses'], 58)

    def test_grid_stage_sweep_invalid_closed_options(self):
        # Catches accidental free-rule/list/resume inputs and unchecked stage parsing.
        base = ['grid-stage-sweep', '--path', '2,6']
        for start, end in [(8, 7), (0, 58), (-1, 7), ('bad', 7), (0, '18446744073709551616')]:
            with self.subTest(start=start, end=end):
                out = self.root / 'invalid.bruce'
                process = self.call(*base, '--from-stage', start, '--to-stage', end, '--output', out, ok=False)
                self.assertIn('InvalidStageRange', json.loads(process.stderr)['error'])
                self.assertFalse(out.exists())
        args = base + ['--from-stage', 0, '--to-stage', 7]
        for extra in [ ['--rule', 'scaled-exponential-v1'], ['--stage', 3], ['--stages', '0,1'],
                       ['--repeat', 2], ['--from-run', 'old'], ['--zero-zero', 'stop'],
                       ['--debug', '--debug'], ['--debug', 'true'] ]:
            self.call(*args, *extra, ok=False)
        for extra, error in [(['--mode', 'bad'], 'InvalidEvaluationMode'),
                             (['--max-bits', 0], 'InvalidBitLimit'),
                             (['--mode', 'modular', '--modulus', 0], 'InvalidModulus'),
                             (['--modulus', 128], 'ModulusRequiresModularMode'),
                             (['--cache-bytes', 1048577], 'GridCacheByteLimit')]:
            process = self.call(*args, *extra, ok=False)
            self.assertIn(error, json.loads(process.stderr)['error'])
        self.call(*base, '--to-stage', 7, ok=False)
        self.call(*base, '--from-stage', 0, ok=False)

    def test_grid_stage_sweep_export_replay_and_rehashed_rejection(self):
        # Catches missing generic dispatch or accepting rehashed diagnostic changes.
        args = ['grid-stage-sweep', '--path', '2,6', '--from-stage', 0, '--to-stage', 7, '--debug']
        out = self.root / 'sweep.bruce'
        result = self.call(*args, '--output', out)
        original = out.read_text()
        self.assertTrue(original.startswith('BRUCE_GRID_STAGE_SWEEP_BUNDLE_V1\n'))
        self.assertEqual(self.call('replay', out), result)
        self.call(*args, '--output', out, ok=False)
        self.assertEqual(out.read_text(), original)
        header, digest, payload = original.split('\n', 2)
        for before, after in [('"stage":1,', '"stage":2,'), ('"value":"100"', '"value":"101"'),
                              ('MISS_INSERT', 'HIT'), ('"misses":8', '"misses":9'),
                              ('PATH_VALIDATED depth=1', 'PATH_VALIDATED depth=2'),
                              (result['records'][0]['cache_key_digest'], '0' * 64)]:
            self.assertIn(before, payload)
            mutated = payload.replace(before, after, 1)
            out.write_text(header + '\n' + hashlib.sha256(mutated.encode()).hexdigest() + '\n' + mutated)
            process = self.call('replay', out, ok=False)
            self.assertIn('ReplayMismatch', json.loads(process.stderr)['error'])
        partial_out = self.root / 'partial.bruce'
        partial = self.call('grid-stage-sweep', '--path', '0,0', '--from-stage', 0, '--to-stage', 57,
                            '--max-bits', 63, '--output', partial_out)
        self.assertEqual(self.call('replay', partial_out), partial)
        checked_out = self.root / 'checked.bruce'
        checked = self.call(*args, '--verification', 'replay', '--output', checked_out)
        self.assertEqual(checked['result'], result)
        self.assertEqual(self.call('replay', checked_out), result)

    def test_grid_stage_sweep_help(self):
        process = subprocess.run([BIN, '--help'], capture_output=True, text=True, timeout=10)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertIn('bruce grid-stage-sweep --path X,Y --from-stage A --to-stage B', process.stdout)
        syntax = process.stdout.split('bruce grid-stage-sweep ', 1)[1].split('\nbruce ', 1)[0]
        for option in ['--debug', '--mode exact|metric|modular', '--max-bits 1..64', '--modulus', '--cache-bytes', '--output']:
            self.assertIn(option, syntax)
        self.assertNotIn('--rule', syntax)

    def test_scaled_grid_batch_and_resume_inherit_rule(self):
        parent = self.root / 'scaled-parent.bruce'
        batch = self.call('grid-batch', '--paths', '2,6;02,06;4,3', '--rule', 'scaled-exponential-v1',
                          '--max-query-steps', 1, '--output', parent)
        self.assertEqual(batch['spec']['value_rule'], 'SCALED_EXPONENTIAL_V1')
        self.assertEqual(batch['queries'][0]['result']['value'], '6300')
        self.assertIn(b'value_rule SCALED_EXPONENTIAL_V1\n', parent.read_bytes())
        self.assertEqual(self.call('replay', parent), batch)
        child = self.root / 'scaled-child.bruce'
        resumed = self.call('grid-resume', '--from-run', parent, '--additional-queries', 2, '--output', child)
        self.assertEqual(resumed['batch']['spec']['value_rule'], 'SCALED_EXPONENTIAL_V1')
        self.assertEqual(resumed['batch']['queries'][1]['cache_event'], 'HIT')
        self.assertEqual(resumed['batch']['queries'][2]['result']['value'], '700')
        self.assertEqual(self.call('replay', child), resumed)

    def test_grid_invalid_rule_is_controlled_error(self):
        for command, flag in [('grid', '--path'), ('grid-batch', '--paths')]:
            for rule in ['unknown', 'SCALED_EXPONENTIAL_V1', 'LEAF_LOCAL_POWER_V1', 'scaled-exponential']:
                with self.subTest(command=command, rule=rule):
                    out = self.root / 'invalid-rule.bruce'
                    process = self.call(command, flag, '2,3', '--rule', rule, '--output', out, ok=False)
                    self.assertEqual(process.returncode, 2)
                    self.assertIn('InvalidGridRule', json.loads(process.stderr)['error'])
                    self.assertFalse(out.exists())

    def test_grid_help_exposes_explicit_stage_for_grid_and_batch(self):
        process = subprocess.run([BIN, '--help'], capture_output=True, text=True, timeout=10)
        self.assertEqual(process.returncode, 0, process.stderr)
        for command in ['grid --path', 'grid-batch --paths']:
            with self.subTest(command=command):
                syntax = process.stdout.split('bruce ' + command, 1)[1].split('\nbruce ', 1)[0]
                self.assertIn('--stage 0..57', syntax)
                self.assertIn('scaled-exponential-v1 only', syntax)

    def test_grid_explicit_stage_57_and_legacy_headers(self):
        out = self.root / 'stage-grid.bruce'
        explicit = self.call('grid', '--path', '2,6', '--rule', 'scaled-exponential-v1', '--stage', 57, '--output', out)
        self.assertEqual((explicit['stage_source'], explicit['stage'], explicit['value']),
                         ('explicit', 57, '14411518807585587100'))
        self.assertTrue(out.read_bytes().startswith(b'BRUCE_GRID_BUNDLE_V2\n'))
        self.assertEqual(self.call('replay', out), explicit)
        legacy_out = self.root / 'legacy-grid.bruce'
        legacy = self.call('grid', '--path', '2,6', '--rule', 'scaled-exponential-v1', '--output', legacy_out)
        self.assertEqual((legacy['stage'], legacy['value']), (6, '6300'))
        self.assertNotIn('stage_source', legacy)
        self.assertTrue(legacy_out.read_bytes().startswith(b'BRUCE_GRID_BUNDLE_V1\n'))

    def test_grid_explicit_batch_resume_stage_and_headers(self):
        complete_out = self.root / 'stage-complete.bruce'
        complete = self.call('grid-batch', '--paths', '2,6;4,3', '--rule', 'scaled-exponential-v1',
                             '--stage', 57, '--output', complete_out)
        self.assertTrue(complete_out.read_bytes().startswith(b'BRUCE_GRID_BATCH_BUNDLE_V3\n'))
        self.assertEqual(complete['spec']['version'], 'grid-spec-v2')
        self.assertEqual([q['result']['value'] for q in complete['queries']], ['14411518807585587100'] * 2)
        self.assertEqual(self.call('replay', complete_out), complete)
        parent = self.root / 'stage-partial.bruce'
        partial = self.call('grid-batch', '--paths', '2,6;02,06;4,3', '--rule', 'scaled-exponential-v1',
                            '--stage', 57, '--max-query-steps', 1, '--output', parent)
        self.assertTrue(parent.read_bytes().startswith(b'BRUCE_GRID_BATCH_BUNDLE_V4\n'))
        self.assertEqual(self.call('replay', parent), partial)
        child = self.root / 'stage-resume.bruce'
        resumed = self.call('grid-resume', '--from-run', parent, '--additional-queries', 2, '--output', child)
        self.assertEqual((resumed['batch']['spec']['stage_source'], resumed['batch']['spec']['stage']), ('explicit', 57))
        self.assertEqual(resumed['batch']['queries'][1]['cache_event'], 'HIT')
        self.assertEqual(resumed['batch']['queries'][2]['result']['value'], '14411518807585587100')
        self.assertTrue(child.read_bytes().startswith(b'BRUCE_GRID_RESUME_BUNDLE_V2\n'))
        self.assertEqual(self.call('replay', child), resumed)
        self.call('grid-resume', '--from-run', parent, '--additional-queries', 2, '--stage', 6, ok=False)
        # An omitted stage keeps the historical complete/partial/resume headers.
        legacy_complete = self.root / 'legacy-complete.bruce'
        legacy_parent = self.root / 'legacy-partial.bruce'
        legacy_child = self.root / 'legacy-resume.bruce'
        self.call('grid-batch', '--paths', '2,6;4,3', '--rule', 'scaled-exponential-v1', '--output', legacy_complete)
        self.call('grid-batch', '--paths', '2,6;4,3', '--rule', 'scaled-exponential-v1',
                  '--max-query-steps', 1, '--output', legacy_parent)
        self.call('grid-resume', '--from-run', legacy_parent, '--additional-queries', 1, '--output', legacy_child)
        for path, header in [(legacy_complete, b'BRUCE_GRID_BATCH_BUNDLE_V1\n'),
                             (legacy_parent, b'BRUCE_GRID_BATCH_BUNDLE_V2\n'),
                             (legacy_child, b'BRUCE_GRID_RESUME_BUNDLE_V1\n')]:
            self.assertTrue(path.read_bytes().startswith(header))
            self.assertNotIn(b'stage_source ', path.read_bytes())
            self.call('replay', path)

    def test_grid_invalid_stage_is_controlled_error(self):
        for command, flag in [('grid', '--path'), ('grid-batch', '--paths')]:
            for stage in ['58', '-1', 'invalid', '18446744073709551616', '3.5', '', '+1']:
                with self.subTest(command=command, stage=stage):
                    out = self.root / 'invalid-stage.bruce'
                    process = self.call(command, flag, '2,6', '--rule', 'scaled-exponential-v1',
                                        '--stage', stage, '--output', out, ok=False)
                    self.assertEqual(process.returncode, 2)
                    self.assertIn('InvalidGridStage', json.loads(process.stderr)['error'])
                    self.assertFalse(out.exists())
            out = self.root / 'leaf-stage.bruce'
            process = self.call(command, flag, '2,6', '--rule', 'leaf-local-power', '--stage', 3, '--output', out, ok=False)
            self.assertIn('StageRequiresScaledExponentialRule', json.loads(process.stderr)['error'])
            self.assertFalse(out.exists())

    def test_budgets(self):
        train = self.pairs('train.csv', [(0,3)])
        for flag, value, reason in [('--max-candidates',1,'CandidateBudgetExceeded'),
                                    ('--max-operations',1,'OperationBudgetExceeded'),
                                    ('--max-trace',2,'TraceBudgetExceeded'),
                                    ('--max-results',1,'ResultBudgetExceeded')]:
            out = self.root / (flag+'.bruce')
            result = self.call('research','--train',train,flag,value,'--output',out)
            self.assertEqual(result['status'],'INCOMPLETE')
            self.assertEqual(result['stop_reason'],reason)
            self.assertEqual(self.call('replay',out),result)


if __name__ == '__main__':
    unittest.main()
