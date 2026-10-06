import hashlib
import json
from pathlib import Path
from fractions import Fraction
import random
import subprocess
import sys
import tempfile
import unittest

BIN = str(Path(sys.argv.pop(1)).resolve())

class ModularCompareTests(unittest.TestCase):
    def call(self, *args, ok=True):
        p = subprocess.run([BIN, *map(str, args)], capture_output=True, text=True, timeout=15)
        self.assertEqual(p.returncode, 0 if ok else 2, p.stderr)
        return json.loads(p.stdout if ok else p.stderr)

    def compare(self, poly, *extra, policy='modular', right='1', ok=True):
        return self.call('compare', '--left-numerator', poly, '--left-denominator', poly,
                         '--right-numerator', right, '--right-denominator', right,
                         '--domain-policy', policy, *extra, ok=ok)

    def test_new_proof_and_independent_certificate(self):
        out = self.compare('-2,0,0,1')
        self.assertEqual(out['status'], 'PROVEN_EQUIVALENT')
        cert = out['domain_certificate']
        self.assertEqual(cert['left_modular_prime'], 7)
        self.assertEqual(cert['left_modular_values'], [5, 6, 6, 4, 6, 4, 4])
        self.assertEqual(self.compare('-2,0,0,1', policy='quadratic')['status'], 'UNDECIDED')
        reverse = self.compare('1', right='-2,0,0,1')
        self.assertEqual(reverse['status'], 'PROVEN_EQUIVALENT')
        self.assertEqual(reverse['domain_certificate']['right_modular_prime'], 7)
        self.assertEqual(reverse['domain_certificate']['right_modular_values'], [5, 6, 6, 4, 6, 4, 4])
        both = self.compare('-2,0,0,1', right='-3,0,0,1')
        self.assertEqual(both['status'], 'PROVEN_EQUIVALENT')
        self.assertEqual(both['domain_certificate']['left_modular_prime'], 7)
        self.assertEqual(both['domain_certificate']['right_modular_prime'], 7)
        rng = random.Random(2909)
        proved = 0
        for _ in range(80):
            poly = [Fraction(rng.randrange(-5, 6), rng.randrange(1, 8)) for _ in range(rng.randrange(4, 8))]
            poly[-1] = Fraction(1)
            out = self.compare(','.join(map(str, poly)))
            cert = out['domain_certificate']
            p = cert['left_modular_prime']
            if p:
                proved += 1
                coefficients = list(map(Fraction, cert['left_remainder']))
                # Direct powers, separate from production Horner evaluation.
                expected = [sum(c.numerator * pow(c.denominator, -1, p) * pow(x, i, p)
                                for i, c in enumerate(coefficients)) % p for x in range(p)]
                self.assertEqual(cert['left_modular_values'], expected)
                self.assertNotIn(0, expected)
                self.assertEqual(out['status'], 'PROVEN_EQUIVALENT')
        self.assertGreater(proved, 10)

    def test_rational_holes_and_inconclusive_primes(self):
        # (2x-1)(x^2+1): must not lose the hole at 1/2 through reduction mod 2.
        for poly in ['-1,2,-1,2', '0,1,0,1', '-1,0,0,1']:
            self.assertNotEqual(self.compare(poly)['status'], 'PROVEN_EQUIVALENT')
        # Rootless over Q, but has a root modulo every prime: bounded method is incomplete.
        # (x^2-2)(x^2-3)(x^2-6)
        self.assertEqual(self.compare('-36,0,36,0,-11,0,1')['status'], 'UNDECIDED')
        self.assertEqual(self.compare('0')['status'], 'PROVEN_DIFFERENT')
        self.assertEqual(self.compare('0', right='0')['status'], 'PROVEN_EQUIVALENT')
        self.assertEqual(self.compare('0,-2,0,0,1', right='0,1')['status'], 'PROVEN_EQUIVALENT')
        self.assertEqual(self.compare('-2,0,0,1', right='0,1')['status'], 'PROVEN_DIFFERENT')
        self.compare('1', policy='unknown', ok=False)

    def test_high_degree_scaling_and_known_rational_roots(self):
        degree64 = ','.join(['1'] + ['0']*63 + ['1'])
        self.assertEqual(self.compare(degree64)['status'], 'PROVEN_EQUIVALENT')
        for poly in ['-1,0,0,1/2', '6/5,0,0,-3/5', '-2,0,0,1,0,0']:
            out = self.compare(poly)
            self.assertEqual(out['status'], 'PROVEN_EQUIVALENT')
            self.assertEqual(out['domain_certificate']['left_modular_prime'], 7)
        # Every configured prime divides this coefficient denominator.
        # The real hole at 1/D must survive every skipped prime.
        D = 200560490130
        poly = f'-1/{D},1,-1/{D},1'
        out = self.compare(poly)
        self.assertEqual(out['status'], 'UNDECIDED')
        self.assertEqual(out['domain_certificate']['left_modular_prime'], 0)
        # Construct rational roots independently by multiplying (b*x-a)*q(x).
        rng = random.Random(290929)
        for _ in range(80):
            a, b = rng.randrange(-7, 8), rng.randrange(1, 12)
            q = [rng.randrange(-6, 7) for _ in range(4)] + [1]
            poly = [0]*(len(q)+1)
            for i, c in enumerate(q):
                poly[i] -= a*c
                poly[i+1] += b*c
            out = self.compare(','.join(map(str, poly)))
            self.assertIn(out['status'], ['UNDECIDED', 'PROVEN_DIFFERENT'])
            self.assertEqual(out['domain_certificate']['left_modular_prime'], 0)
        low = self.compare('1,1,1', '--max-bits', 1)
        self.assertEqual((low['status'], low['stop_reason']), ('INCOMPLETE', 'IntegerBitLimit'))

    def test_replay_and_tamper(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/'run.bruce'
            out = self.compare('-2,0,0,1', '--output', path)
            self.assertEqual(self.call('replay', path), out)
            original = path.read_text()
            self.assertTrue(original.startswith('BRUCE_FRACTION_COMPARISON_V2\n'))
            for old, new in [('gcd-modular-v1', 'gcd-modular-v9'),
                             ('"left_modular_prime":7', '"left_modular_prime":5'),
                             ('[5,6,6,4,6,4,4]', '[5,6,6,4,6,4,0]')]:
                header, _, payload = original.split('\n', 2)
                self.assertIn(old, payload)
                payload = payload.replace(old, new, 1)
                path.write_text(header+'\n'+hashlib.sha256(payload.encode()).hexdigest()+'\n'+payload)
                self.call('replay', path, ok=False)
            # Header-only downgrade/upgrade cannot select another proof policy.
            path.write_text(original.replace('BRUCE_FRACTION_COMPARISON_V2', 'BRUCE_FRACTION_COMPARISON_V1', 1))
            self.call('replay', path, ok=False)
            path.write_text(original.replace('BRUCE_FRACTION_COMPARISON_V2', 'BRUCE_FRACTION_COMPARISON_V9', 1))
            self.call('replay', path, ok=False)
            path.unlink()
            short = self.compare('-2,0,0,1', '--max-operations', out['operations']-1, '--output', path)
            self.assertEqual(short['status'], 'INCOMPLETE')
            self.assertEqual(self.call('replay', path), short)

if __name__ == '__main__':
    unittest.main()
