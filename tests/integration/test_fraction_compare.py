import hashlib
import json
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest
from fractions import Fraction

BIN = str(Path(sys.argv.pop(1)).resolve())

class FractionCompareTests(unittest.TestCase):
    def call(self, *args, ok=True):
        p = subprocess.run([BIN, *map(str, args)], capture_output=True, text=True, timeout=15)
        self.assertEqual(p.returncode, 0 if ok else 2, p.stderr)
        return json.loads(p.stdout if ok else p.stderr)

    def compare(self, ln, ld, rn='1', rd='1', *extra, **kw):
        return self.call('compare', '--left-numerator', ln, '--left-denominator', ld,
                         '--right-numerator', rn, '--right-denominator', rd, *extra, **kw)

    def test_domain_and_function_certificates(self):
        cases = [
            ('1,0,1', '1,0,1', '1', '1', 'PROVEN_EQUIVALENT', 'EQUAL'),
            ('-2,0,1', '-2,0,1', '1', '1', 'PROVEN_EQUIVALENT', 'EQUAL'),
            ('0,1', '0,1', '1', '1', 'PROVEN_DIFFERENT', 'DIFFERENT'),
            ('-1,0,1', '-1,0,1', '1', '1', 'PROVEN_DIFFERENT', 'DIFFERENT'),
            ('1,2,1', '1,2,1', '1', '1', 'PROVEN_DIFFERENT', 'DIFFERENT'),
            ('0,1,0,1', '0,1,0,1', '0,1', '0,1', 'PROVEN_EQUIVALENT', 'EQUAL'),
            ('0,-2,0,1', '0,-2,0,1', '0,1', '0,1', 'PROVEN_EQUIVALENT', 'EQUAL'),
            ('-2,0,0,1', '-2,0,0,1', '1', '1', 'UNDECIDED', 'UNKNOWN'),
            ('-2,0,0,1', '-2,0,0,1', '0,1', '0,1', 'PROVEN_DIFFERENT', 'DIFFERENT'),
            ('1', '-2,0,0,1', '0', '1', 'PROVEN_DIFFERENT', 'UNKNOWN'),
            ('99', '0', '1', '0', 'PROVEN_EQUIVALENT', 'EQUAL'),
            ('99', '0', '1', '1', 'PROVEN_DIFFERENT', 'DIFFERENT'),
            ('0', '0,1', '0', '1', 'PROVEN_DIFFERENT', 'DIFFERENT'),
            ('0', '1,0,1', '0', '1', 'PROVEN_EQUIVALENT', 'EQUAL'),
            ('1,1', '1', '2,1', '1', 'PROVEN_DIFFERENT', 'EQUAL'),
            ('-1,0,1', '-1,1', '1,1', '1', 'PROVEN_DIFFERENT', 'DIFFERENT'),
            ('1/2,0,1/2,0', '1/2,0,1/2,0', '1', '1', 'PROVEN_EQUIVALENT', 'EQUAL'),
        ]
        for ln, ld, rn, rd, status, relation in cases:
            with self.subTest(denominators=(ld, rd)):
                out = self.compare(ln, ld, rn, rd)
                self.assertEqual((out['status'], out['domain_relation']), (status, relation))
                self.assertEqual(out['input_domain'], 'rational')
                self.assertEqual(out['proof_scope'], 'supplied-polynomial-fractions-over-Q')
                self.assertEqual(out['stop_reason'], 'Completed')
                self.assertLessEqual(len(out['trace']), 5)
                reverse = self.compare(rn, rd, ln, ld)
                self.assertEqual((reverse['status'], reverse['domain_relation']), (status, relation))

    def test_independent_quadratic_oracle(self):
        rng = random.Random(2609)
        import math
        for _ in range(80):
            a, b, c = rng.choice([-5, -3, 1, 2, 7]), rng.randrange(-9, 10), rng.randrange(-9, 10)
            disc = b*b - 4*a*c
            has_root = disc >= 0 and math.isqrt(disc)**2 == disc
            denominator = f'{c},{b},{a}'
            result = self.compare(denominator, denominator)
            self.assertEqual(result['status'], 'PROVEN_DIFFERENT' if has_root else 'PROVEN_EQUIVALENT')
            if has_root:
                witness = Fraction(result['domain_certificate']['witness'])
                self.assertEqual(a*witness*witness+b*witness+c, 0)

    def test_independent_function_and_domain_oracle(self):
        import math
        rng = random.Random(260926)
        def trim(p):
            while len(p) > 1 and p[-1] == 0:
                p.pop()
            return p
        def mul(a, b):
            out = [0]*(len(a)+len(b)-1)
            for i, x in enumerate(a):
                for j, y in enumerate(b):
                    out[i+j] += x*y
            return trim(out)
        def roots(p):
            p = trim(p[:])
            if p == [0]:
                return None  # All Q excluded.
            if len(p) == 1:
                return set()
            if len(p) == 2:
                return {Fraction(-p[0], p[1])}
            c, b, a = p
            disc = b*b-4*a*c
            if disc < 0 or math.isqrt(disc)**2 != disc:
                return set()
            root = math.isqrt(disc)
            return {Fraction(-b+root, 2*a), Fraction(-b-root, 2*a)}
        for index in range(100):
            ln, ld, rn, rd = [trim([rng.randrange(-3, 4) for _ in range(3)]) for _ in range(4)]
            if index % 3 == 0:
                rn, rd = [-2*c for c in ln], [-2*c for c in ld]
            domain_equal = roots(ld) == roots(rd)
            expected = domain_equal and (roots(ld) is None or mul(ln, rd) == mul(rn, ld))
            result = self.compare(*(','.join(map(str, p)) for p in (ln, ld, rn, rd)))
            self.assertEqual(result['status'], 'PROVEN_EQUIVALENT' if expected else 'PROVEN_DIFFERENT')
            self.assertEqual(result['domain_relation'], 'EQUAL' if domain_equal else 'DIFFERENT')
            witness = result['domain_certificate']['witness']
            if witness is not None:
                def evaluate(p, x):
                    value = Fraction(0)
                    for c in reversed(p):
                        value = value*x+c
                    return value
                w = Fraction(witness)
                self.assertNotEqual(evaluate(ld, w) == 0, evaluate(rd, w) == 0)

    def test_replay_limits_and_tampering(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/'proof.bruce'
            out = self.compare('0,1,0,1', '0,1,0,1', '0,1', '0,1', '--output', path)
            self.assertEqual(self.call('replay', path), out)
            original = path.read_text()
            self.assertTrue(original.startswith('BRUCE_FRACTION_COMPARISON_V1\n'))
            self.compare('1', '1', '1', '1', '--output', path, ok=False)
            self.assertEqual(path.read_text(), original)
            exact = self.compare('0,1,0,1', '0,1,0,1', '0,1', '0,1', '--max-operations', out['operations'])
            self.assertEqual(exact['status'], out['status'])
            short = Path(tmp)/'short.bruce'
            result = self.compare('0,1,0,1', '0,1,0,1', '0,1', '0,1', '--max-operations', out['operations']-1, '--output', short)
            self.assertEqual((result['status'], result['stop_reason']), ('INCOMPLETE', 'OperationBudgetExceeded'))
            self.assertEqual(self.call('replay', short), result)
            for old, new in [('rational-domain-gcd-quadratic-v1', 'rational-domain-gcd-quadratic-v9'),
                             ('"status":"PROVEN_EQUIVALENT"', '"status":"PROVEN_DIFFERENT"'),
                             ('"operations":', '"operations":9')]:
                header, _, payload = original.split('\n', 2)
                self.assertIn(old, payload)
                payload = payload.replace(old, new, 1)
                path.write_text(header+'\n'+hashlib.sha256(payload.encode()).hexdigest()+'\n'+payload)
                self.call('replay', path, ok=False)
            path.write_text(original+'garbage')
            self.call('replay', path, ok=False)
            bit = Path(tmp)/'bit.bruce'
            result = self.compare('1,1,1', '1,1,1', '1', '1', '--max-bits', 1, '--output', bit)
            self.assertEqual((result['status'], result['stop_reason']), ('INCOMPLETE', 'IntegerBitLimit'))
            self.assertEqual(self.call('replay', bit), result)

    def test_strict_input_boundaries(self):
        for text in ['', ',', '1,', ',1', '1,,2', '1.5', '1/0', '1 2', '+1', '1\n2', ','.join(['0']*66)]:
            self.compare('1', text, ok=False)
        for flag, value in [('--max-bits', 0), ('--max-bits', 16385), ('--max-operations', 0), ('--max-operations', 1000001)]:
            self.compare('1', '1', '1', '1', flag, value, ok=False)
        self.call('compare', '--left-numerator', '1', ok=False)
        self.compare('1', '1', '1', '1', '--domain', 'real', ok=False)
        self.compare('1', '1', '1', '1', '--max-bits', 8, '--max-bits', 16, ok=False)
        self.compare('256', '1', '1', '1', '--max-bits', 8, ok=False)
        degree64 = ','.join(['1']+['0']*63+['1'])
        out = self.compare('1', degree64, '1', degree64)
        self.assertEqual(out['status'], 'PROVEN_EQUIVALENT')

if __name__ == '__main__':
    unittest.main()
