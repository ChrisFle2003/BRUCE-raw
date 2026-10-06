import hashlib
import json
from fractions import Fraction
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BIN = str(Path(sys.argv.pop(1)).resolve())

class RationalWitnessTests(unittest.TestCase):
    def call(self, *args, ok=True):
        p = subprocess.run([BIN, *map(str, args)], capture_output=True, text=True, timeout=20)
        self.assertEqual(p.returncode, 0 if ok else 2, p.stderr)
        return json.loads(p.stdout if ok else p.stderr)

    def compare(self, left, right='1', *args):
        return self.call('compare', '--left-numerator', left, '--left-denominator', left,
                         '--right-numerator', right, '--right-denominator', right,
                         '--domain-policy', 'modular-witness', *args)

    def test_higher_degree_holes_have_exact_counterexamples(self):
        # Reject losing rational holes merely because numerator/denominator cancel.
        for poly, expected in [('-8,0,0,1', '2'), ('8,0,0,1', '-2'),
                               ('-1,0,0,8', '1/2'), ('1,0,0,8', '-1/2'),
                               ('0,1,0,1', '0'), ('-4096,0,0,1', '16'),
                               ('-1,0,0,4096', '1/16')]:
            for reverse in (False, True):
                with self.subTest(poly=poly, reverse=reverse):
                    out = self.compare('1', poly) if reverse else self.compare(poly)
                    self.assertEqual(out['status'], 'PROVEN_DIFFERENT')
                    cert = out['domain_certificate']
                    self.assertEqual(cert['witness'], expected)
                    self.assertEqual(cert['excluded_side'], 'right' if reverse else 'left')
                    x = Fraction(cert['witness'])
                    self.assertEqual(sum(Fraction(c)*x**i for i,c in enumerate(poly.split(','))), 0)
                    self.assertGreater(cert['right_witness_candidates' if reverse else 'left_witness_candidates'], 0)

    def test_negative_search_does_not_prove_absence(self):
        # A root outside either bound must not become a spurious equality proof.
        for poly in ('-4913,0,0,1', '-1,0,0,4913'):
            out = self.compare(poly)
            self.assertEqual(out['status'], 'UNDECIDED')
            self.assertEqual(out['domain_certificate']['left_roots'], 'UNKNOWN')
            self.assertIsNone(out['domain_certificate']['witness'])
        out = self.compare('-2,0,0,1')
        self.assertEqual(out['status'], 'PROVEN_EQUIVALENT')
        self.assertEqual(out['domain_certificate']['left_witness_candidates'], 0)
        self.assertGreater(out['domain_certificate']['left_modular_prime'], 0)

    def test_common_holes_are_removed_before_witness_search(self):
        # x(x^3-8) and x share zero; only 2 distinguishes their domains.
        out = self.compare('0,-8,0,0,1', '0,1')
        self.assertEqual(out['status'], 'PROVEN_DIFFERENT')
        self.assertEqual(out['domain_certificate']['witness'], '2')
        out = self.compare('-8,0,0,1', '16,0,0,-2')
        self.assertEqual(out['status'], 'PROVEN_EQUIVALENT')
        self.assertEqual(out['domain_certificate']['left_witness_candidates'], 0)

    def test_independent_constructed_rational_roots(self):
        # (b*x-a)*(x^2+1) has precisely the rational root a/b.
        # Independent Fraction substitution checks the unreduced input polynomial.
        for a,b in ((3,7),(-5,9),(16,15),(-16,1),(1,16)):
            coefficients = [Fraction(-a,3),Fraction(b,3),Fraction(-a,3),Fraction(b,3)]
            poly = ','.join(map(str,coefficients))
            out = self.compare(poly)
            self.assertEqual(out['status'], 'PROVEN_DIFFERENT')
            witness = Fraction(out['domain_certificate']['witness'])
            self.assertEqual(witness, Fraction(a,b))
            self.assertEqual(sum(c*witness**i for i,c in enumerate(coefficients)), 0)
        # Every small modular field has a root of at least one factor, but none
        # of (x^2-2)(x^2-3)(x^2-6) has a rational root. Remain conservative.
        out = self.compare('-36,0,36,0,-11,0,1')
        self.assertEqual(out['status'], 'UNDECIDED')
        self.assertGreater(out['domain_certificate']['left_witness_candidates'], 100)
        degree64 = ','.join(['-1']+['0']*63+['1'])
        out = self.compare(degree64)
        self.assertEqual(out['status'], 'PROVEN_DIFFERENT')
        self.assertEqual(out['domain_certificate']['witness'], '1')

    def test_export_replay_and_rehashed_tampering(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'proof.bruce'
            out = self.compare('-1,0,0,8', '1', '--output', path)
            original = path.read_text()
            self.assertTrue(original.startswith('BRUCE_FRACTION_COMPARISON_V3\n'))
            self.assertEqual(self.call('replay', path), out)
            for old, new in [('"witness":"1/2"', '"witness":"1/3"'),
                             ('"left_witness_candidates":', '"left_witness_candidates":9'),
                             ('rational-domain-gcd-modular-witness16-v1', 'rational-domain-gcd-modular-v1')]:
                header, _, payload = original.split('\n', 2)
                self.assertIn(old, payload)
                payload = payload.replace(old, new, 1)
                path.write_text(header+'\n'+hashlib.sha256(payload.encode()).hexdigest()+'\n'+payload)
                self.call('replay', path, ok=False)
            path.write_text(original.replace('BRUCE_FRACTION_COMPARISON_V3', 'BRUCE_FRACTION_COMPARISON_V2', 1))
            self.call('replay', path, ok=False)
            path.unlink()
            part = self.compare('-1,0,0,8', '1', '--max-operations', out['operations']-1, '--output', path)
            self.assertEqual(part['status'], 'INCOMPLETE')
            self.assertEqual(self.call('replay', path), part)

if __name__ == '__main__':
    unittest.main()
