from fractions import Fraction
import hashlib
import json
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest

BIN = str(Path(sys.argv.pop(1)).resolve())


class AdaptiveResearchTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)

    def tearDown(self): self.tmp.cleanup()

    def call(self, *args, ok=True):
        out = subprocess.run([BIN, *map(str, args)], capture_output=True, timeout=20)
        self.assertEqual(out.returncode, 0 if ok else 2, out.stderr)
        return json.loads(out.stdout if ok else out.stderr)

    def pairs(self, name, rows):
        path = self.root/name
        path.write_text('x,y\n'+''.join(f'{x},{y}\n' for x,y in rows))
        return path

    def run_search(self, rows, *options, planner='adaptive'):
        train = self.pairs('train.csv', rows)
        return self.call('research', '--strategy', 'power-affine', '--planner', planner,
                         '--train', train, *options)

    def test_quadratic_work_reduction_and_nested_replay(self):
        rows = [(0,3),(1,5),(2,11)]
        hold = self.pairs('hold.csv', [(-3,21),(5,53)])
        source = self.root/'adaptive.bruce'
        new = self.run_search(rows, '--holdout', hold, '--output', source)
        old = self.run_search(rows, '--holdout', hold, planner='enumerate')
        self.assertEqual(new['status'], 'VERIFIED_ON_SAMPLES')
        self.assertEqual(new['hypotheses'], old['hypotheses'])
        self.assertEqual(new['candidates_executed'], 4)
        self.assertLess(new['operations_used'], old['operations_used'])
        self.assertEqual(new['planner'], 'two-point-v1')
        self.assertEqual(new['plans_completed'], 4)
        self.assertEqual(self.call('replay', source), new)
        rules=self.root/'rules'; rules.write_text('BRUCE_RULES_V1\nsnapshot adaptive\n')
        dest=self.root/'selection.bruce'
        selected=self.call('compatibility','--rules',rules,'--from-run',source,'--at',-3,'--output',dest)
        self.assertEqual(selected['candidates'][0]['value'], '21')
        source.unlink(); rules.unlink(); hold.unlink(); (self.root/'train.csv').unlink()
        self.assertEqual(self.call('replay', dest), selected)

    def test_random_candidates_match_enumeration_and_fraction_reference(self):
        rng=random.Random(1922)
        coefficients={Fraction(n,d) for n in range(-2,3) for d in range(1,4)}
        xs=[Fraction(-2),Fraction(0),Fraction(3,2)]
        for _ in range(15):
            a=Fraction(rng.randint(-2,2),rng.randint(1,3)); b=Fraction(rng.randint(-2,2),rng.randint(1,3))
            degree=rng.randint(1,4)
            rows=[(x,a*x**degree+b) for x in xs]
            options=('--domain','rational','--min',-2,'--max',2,'--max-denominator',3)
            adaptive=self.run_search(rows,*options)
            baseline=self.run_search(rows,*options,planner='enumerate')
            expected={(ca,p,cb) for p in range(1,5) for ca in coefficients for cb in coefficients
                      if (ca!=0 or p==1) and all(ca*x**p+cb==y for x,y in rows)}
            self.assertEqual(adaptive['hypotheses'],baseline['hypotheses'])
            self.assertEqual({(Fraction(h['a']),h['power'],Fraction(h['b'])) for h in adaptive['hypotheses']},expected)

    def test_singular_fallback_and_inconsistent_projection(self):
        for rows in [[(0,1)], [(-2,4),(2,4)], [(-2,1),(2,2)], [(0,0),(1,1)]]:
            new=self.run_search(rows,'--min',-2,'--max',2,'--max-power',3)
            old=self.run_search(rows,'--min',-2,'--max',2,'--max-power',3,planner='enumerate')
            self.assertEqual((new['status'],new['hypotheses']),(old['status'],old['hypotheses']))
        fallback=self.run_search([(0,1)],'--min',0,'--max',1)
        self.assertTrue(any('fallback' in event for event in fallback['trace']))
        conflict=self.run_search([(-2,1),(2,2)],'--min',-2,'--max',2)
        self.assertTrue(any('inconsistent_projection' in event for event in conflict['trace']))

    def test_family_limits_and_holdout_not_used_for_generation(self):
        rows=[(0,0),(2,1)]
        integer=self.run_search(rows,'--max-power',1)
        self.assertEqual(integer['status'],'NO_CANDIDATE')
        rational=self.run_search(rows,'--domain','rational','--max-power',1,'--max-denominator',2)
        self.assertEqual(rational['hypotheses'][0]['a'],'1/2')
        hold=self.pairs('bad.csv',[(3,99)])
        bad=self.run_search([(0,3),(1,5),(2,11)],'--holdout',hold)
        self.assertEqual(bad['status'],'REFUTED')
        nohold=self.run_search([(0,3),(1,5),(2,11)])
        self.assertEqual(bad['candidates_executed'],nohold['candidates_executed'])
        self.assertEqual([e for e in bad['trace'] if e.startswith('PLAN')],
                         [e for e in nohold['trace'] if e.startswith('PLAN')])

    def test_planning_budgets_and_bitlimit_remain_incomplete(self):
        for option,value,reason in [('--max-operations',1,'OperationBudgetExceeded'),
                                    ('--max-trace',2,'TraceBudgetExceeded'),
                                    ('--max-trace-bytes',256,'TraceByteBudgetExceeded'),
                                    ('--max-candidates',1,'CandidateBudgetExceeded')]:
            path=self.root/(option[2:]+'.bruce')
            out=self.run_search([(0,3),(1,5),(2,11)],option,value,'--output',path)
            self.assertEqual((out['status'],out['stop_reason']),('INCOMPLETE',reason))
            self.assertLessEqual(out['trace_bytes'],out.get('max_trace_bytes',4194304))
            self.assertEqual(self.call('replay',path),out)
        out=self.run_search([(0,1),(15,1)],'--min',0,'--max',1,'--max-bits',4)
        self.assertEqual((out['status'],out['stop_reason']),('INCOMPLETE','IntegerBitLimit'))

    def test_version_tamper_and_historical_power_fixture(self):
        path=self.root/'run.bruce'
        self.run_search([(0,3),(1,5),(2,11)],'--output',path)
        header,_,body=path.read_text().split('\n',2)
        changed=body.replace('two-point-v1','two-point-v9')
        self.assertNotEqual(changed,body)
        path.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
        self.call('replay',path,ok=False)
        fixture=Path(__file__).resolve().parents[1]/'fixtures/power-v1.bruce'
        self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().split('RESULT\n',1)[1]))
        train=self.pairs('train.csv',[(0,0)])
        for args in [('--planner','unknown'),('--planner','adaptive'),('--strategy','affine','--planner','adaptive')]:
            self.call('research','--train',train,*args,ok=False)


if __name__=='__main__': unittest.main()
