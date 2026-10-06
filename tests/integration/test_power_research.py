from fractions import Fraction
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BIN = str(Path(sys.argv.pop(1)).resolve())


class PowerResearchTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def call(self, *args, ok=True):
        p = subprocess.run([BIN, *map(str, args)], capture_output=True, timeout=20)
        self.assertEqual(p.returncode, 0 if ok else 2, p.stderr)
        return json.loads(p.stdout if ok else p.stderr)

    def pairs(self, name, rows):
        path = self.root / name
        path.write_text('x,y\n' + ''.join(f'{x},{y}\n' for x, y in rows))
        return path

    def research(self, rows, *options):
        train = self.pairs('train.csv', rows)
        return self.call('research', '--strategy', 'power-affine', '--train', train, *options)

    @staticmethod
    def surviving(out):
        return {(Fraction(h['a']), h['power'], Fraction(h['b']))
                for h in out['hypotheses'] if h['status'] != 'REFUTED'}

    def test_quadratic_discovery_holdout_and_replay(self):
        hold = self.pairs('hold.csv', [(-3, 21), (5, 53)])
        archive = self.root / 'quadratic.bruce'
        out = self.research([(0, 3), (1, 5), (2, 11)], '--holdout', hold,
                            '--output', archive)
        self.assertEqual(out['status'], 'VERIFIED_ON_SAMPLES')
        self.assertEqual(self.surviving(out), {(Fraction(2), 2, Fraction(3))})
        self.assertFalse(out['general_proof'])
        self.assertEqual(out['strategy'], 'power-affine')
        hold.unlink(); (self.root / 'train.csv').unlink()
        self.assertEqual(self.call('replay', archive), out)

    def test_rational_cube_large_integer_and_negative_inputs(self):
        a, b = Fraction(1, 2), Fraction(-1, 3)
        xs = [Fraction(-2), Fraction(0), Fraction(3, 2)]
        hold = self.pairs('hold.csv', [(Fraction(-1, 7), a*Fraction(-1, 7)**3+b)])
        out = self.research([(x, a*x**3+b) for x in xs], '--domain', 'rational',
                            '--min', -2, '--max', 2, '--max-denominator', 3, '--holdout', hold)
        self.assertEqual(out['status'], 'VERIFIED_ON_SAMPLES')
        self.assertEqual(self.surviving(out), {(a, 3, b)})
        n = 10**80
        out = self.research([(n, 2*n**3+1), (n+1, 2*(n+1)**3+1)],
                            '--min', 1, '--max', 2, '--max-power', 3)
        self.assertEqual(self.surviving(out), {(Fraction(2), 3, Fraction(1))})

    def test_ambiguity_constant_dedup_and_counterexample(self):
        out = self.research([(0, 0), (1, 1)], '--min', 0, '--max', 1, '--max-power', 4)
        self.assertEqual(out['status'], 'AMBIGUOUS')
        self.assertEqual({h['power'] for h in out['hypotheses']}, {1, 2, 3, 4})
        constant = self.research([(-2, 1), (0, 1), (3, 1)], '--min', 0, '--max', 1)
        self.assertEqual(constant['status'], 'CANDIDATE')
        self.assertEqual(self.surviving(constant), {(Fraction(0), 1, Fraction(1))})
        hold = self.pairs('bad.csv', [(3, 99)])
        bad = self.research([(0, 3), (1, 5), (2, 11)], '--holdout', hold)
        self.assertEqual(bad['status'], 'REFUTED')
        none = self.research([(0, 3), (1, 5), (2, 11)], '--max-power', 1)
        self.assertEqual(none['status'], 'NO_CANDIDATE')

    def test_full_candidate_sets_against_independent_fraction_oracle(self):
        coefficients = sorted({Fraction(n, d) for n in range(-2, 3) for d in range(1, 4)})
        for xs, target in [([0, 1], lambda x: x), ([-1, 0, 2], lambda x: Fraction(1, 2)*x**2-1),
                           ([Fraction(-1, 2), Fraction(2, 3)], lambda x: Fraction(-2, 3)*x**3+1)]:
            rows = [(Fraction(x), target(Fraction(x))) for x in xs]
            expected = {(a, p, b) for p in range(1, 5) for a in coefficients for b in coefficients
                        if (a != 0 or p == 1) and all(a*x**p+b == y for x, y in rows)}
            out = self.research(rows, '--domain', 'rational', '--min', -2, '--max', 2,
                                '--max-denominator', 3)
            self.assertEqual(self.surviving(out), expected)

    def test_limits_and_invalid_strategy_options(self):
        archive = self.root / 'limit.bruce'
        limited = self.research([(7, 8)], '--min', 1, '--max', 1,
                                '--max-bits', 4, '--output', archive)
        self.assertEqual((limited['status'], limited['stop_reason']), ('INCOMPLETE', 'IntegerBitLimit'))
        self.assertEqual(limited['hypotheses'][0]['power'], 1)
        self.assertEqual(self.call('replay', archive), limited)
        for flag, value, reason in [('--max-candidates', 1, 'CandidateBudgetExceeded'),
                                    ('--max-operations', 1, 'OperationBudgetExceeded'),
                                    ('--max-trace', 2, 'TraceBudgetExceeded'),
                                    ('--max-trace-bytes', 256, 'TraceByteBudgetExceeded')]:
            out = self.research([(0, 0)], flag, value)
            self.assertEqual((out['status'], out['stop_reason']), ('INCOMPLETE', reason))
        train = self.pairs('train.csv', [(0, 0)])
        for args in [('--strategy', 'unknown'), ('--strategy', 'power-affine', '--max-power', 0),
                     ('--strategy', 'power-affine', '--max-power', 9), ('--max-power', 2),
                     ('--strategy', 'power-affine', '--domain', 'int64')]:
            self.call('research', '--train', train, *args, ok=False)

    def test_projection_evaluates_power_and_replays_nested_origin(self):
        source = self.root / 'power.bruce'
        self.research([(0, 3), (1, 5), (2, 11)], '--output', source)
        rules = self.root / 'rules'
        rules.write_text('BRUCE_RULES_V1\nsnapshot power\nrule p positive value eq 21 0 5 manual -\n')
        dest = self.root / 'selection.bruce'
        out = self.call('compatibility', '--rules', rules, '--from-run', source, '--at', -3, '--output', dest)
        self.assertEqual(out['status'], 'SELECTED')
        self.assertEqual(out['candidates'][0]['value'], '21')
        self.assertEqual(out['candidates'][0]['domain'], 'power-bigint-v1')
        source.unlink(); rules.unlink()
        self.assertEqual(self.call('replay', dest), out)

    def test_rehashed_power_tamper_and_old_exact_fixture(self):
        archive = self.root / 'power.bruce'
        self.research([(0, 3), (1, 5), (2, 11)], '--output', archive)
        header, _, original = archive.read_text().split('\n', 2)
        for payload in [original.replace('"power":2', '"power":3'),
                        original.replace('max_power 4', 'max_power 3'),
                        original.replace('power-affine-v1', 'power-affine-v9')]:
            self.assertNotEqual(payload, original)
            archive.write_text(header+'\n'+hashlib.sha256(payload.encode()).hexdigest()+'\n'+payload)
            self.call('replay', archive, ok=False)
        fixture = Path(__file__).resolve().parents[1] / 'fixtures/exact-v1.bruce'
        self.assertEqual(self.call('replay', fixture), json.loads(fixture.read_text().split('RESULT\n', 1)[1]))

    def test_maximum_exponent_and_constant_without_irrelevant_expansion(self):
        out = self.research([(x, 2*x**8+1) for x in (-2, 0, 3)], '--min', 1, '--max', 2,
                            '--max-power', 8)
        self.assertEqual(out['status'], 'CANDIDATE')
        self.assertEqual(self.surviving(out), {(Fraction(2), 8, Fraction(1))})
        constant = self.research([(15, 0)], '--min', 0, '--max', 0,
                                 '--max-bits', 4, '--max-power', 8)
        self.assertEqual(constant['status'], 'CANDIDATE')
        self.assertEqual(constant['candidates_executed'], 1)


if __name__ == '__main__':
    unittest.main()
