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
