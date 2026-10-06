from fractions import Fraction
import hashlib
import json
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest

BIN=str(Path(sys.argv.pop(1)).resolve())


class PolynomialTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(); self.root=Path(self.tmp.name)
    def tearDown(self): self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=20)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def pairs(self,name,rows):
        path=self.root/name
        path.write_text('x,y\n'+''.join(f'{x},{y}\n' for x,y in rows)); return path
    def research(self,rows,*options):
        return self.call('research','--strategy','polynomial','--train',self.pairs('train.csv',rows),*options)
    @staticmethod
    def value(cs,x): return sum(c*x**i for i,c in enumerate(cs))

    def test_multiterm_quadratic_replay_and_projection(self):
        hold=self.pairs('hold.csv',[(-3,7),(5,31)])
        archive=self.root/'poly.bruce'
        out=self.research([(x,x*x+x+1) for x in (0,1,2,3)],'--holdout',hold,'--output',archive)
        self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
        self.assertEqual(out['hypotheses'][0]['coefficients'],['1','1','1'])
        self.assertEqual(out['hypotheses'][0]['degree'],2)
        self.assertEqual(len(out['hypotheses']),1)
        self.assertFalse(out['general_proof'])
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot polynomial\n')
        selection=self.root/'selection.bruce'
        selected=self.call('compatibility','--rules',rules,'--from-run',archive,'--at',-3,'--output',selection)
        self.assertEqual(selected['candidates'][0]['value'],'7')
        self.assertEqual(selected['candidates'][0]['domain'],'polynomial-bigint-v1')
        hold.unlink();rules.unlink();(self.root/'train.csv').unlink()
        self.assertEqual(self.call('replay',archive),out)
        archive.unlink();self.assertEqual(self.call('replay',selection),selected)

    def test_rational_cubic_and_independent_random_reference(self):
        rng=random.Random(606)
        for degree in range(1,6):
            cs=[Fraction(rng.randint(-2,2),rng.randint(1,3)) for _ in range(degree)] + [Fraction(1,2)]
            xs=[Fraction(i,2) for i in range(-degree,2)]
            hold=self.pairs('hold.csv',[(Fraction(7,3),self.value(cs,Fraction(7,3)))])
            out=self.research([(x,self.value(cs,x)) for x in xs],'--domain','rational',
                              '--max-degree',degree,'--max-denominator',3,'--holdout',hold)
            self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
            self.assertEqual(out['hypotheses'][0]['coefficients'],list(map(str,cs)))

    def test_insufficient_samples_never_assert_uniqueness(self):
        out=self.research([(0,1),(1,3)],'--max-degree',3)
        self.assertEqual((out['status'],out['stop_reason']),('INCOMPLETE','InsufficientTrainingSamples'))
        self.assertEqual(out['hypotheses'][0]['coefficients'],['1','2'])
        constant=self.research([(0,1),(1,1),(2,1),(3,1)])
        self.assertEqual(constant['status'],'CANDIDATE')
        self.assertEqual([h['degree'] for h in constant['hypotheses']],[0])
        degreezero=self.research([(99,1)],'--max-degree',0)
        self.assertEqual(degreezero['status'],'CANDIDATE')

    def test_training_and_holdout_contradictions_and_family_bounds(self):
        rows=[(x,x*x+x+1) for x in (0,1,2,3)]
        hold=self.pairs('bad.csv',[(4,99)])
        out=self.research(rows,'--holdout',hold)
        self.assertEqual(out['status'],'REFUTED')
        self.assertEqual(self.research(rows,'--max-degree',1)['status'],'NO_CANDIDATE')
        self.assertEqual(self.research(rows,'--min',2,'--max',3)['status'],'NO_CANDIDATE')
        rational=[(0,0),(2,1),(4,2),(6,3)]
        self.assertEqual(self.research(rational)['status'],'NO_CANDIDATE')
        out=self.research(rational,'--domain','rational','--max-denominator',2)
        self.assertEqual(out['hypotheses'][0]['coefficients'],['0','1/2'])

    def test_limits_and_rehashed_version_or_result_tampering(self):
        rows=[(x,x*x+x+1) for x in (0,1,2,3)]
        for flag,value,reason in [('--max-operations',1,'OperationBudgetExceeded'),
                                 ('--max-candidates',1,'CandidateBudgetExceeded'),
                                 ('--max-trace',2,'TraceBudgetExceeded'),
                                 ('--max-trace-bytes',256,'TraceByteBudgetExceeded')]:
            path=self.root/(flag[2:]+'.bruce')
            out=self.research(rows,flag,value,'--output',path)
            self.assertEqual((out['status'],out['stop_reason']),('INCOMPLETE',reason))
            self.assertEqual(self.call('replay',path),out)
        # A separate case forces a bounded interpolation difference beyond 4 bits.
        limit=self.research([(-15,-15),(15,15)],'--min',0,'--max',1,'--max-degree',1,'--max-bits',4)
        self.assertEqual((limit['status'],limit['stop_reason']),('INCOMPLETE','IntegerBitLimit'))
        path=self.root/'tamper.bruce';self.research(rows,'--output',path)
        header,_,body=path.read_text().split('\n',2)
        for changed in [body.replace('newton-v1','newton-v9'),body.replace('"coefficients":["1","1","1"]','"coefficients":["2","1","1"]')]:
            self.assertNotEqual(changed,body)
            path.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
            self.call('replay',path,ok=False)

    def test_degree_eight_large_values_and_invalid_options(self):
        cs=[1,1,0,0,0,0,0,0,1]
        n=10**12
        out=self.research([(n+i,self.value(cs,n+i)) for i in range(9)],'--max-degree',8)
        self.assertEqual(out['status'],'CANDIDATE')
        self.assertEqual(out['hypotheses'][0]['coefficients'],list(map(str,cs)))
        train=self.pairs('train.csv',[(0,0)])
        for args in [('--strategy','polynomial','--max-degree',9),('--strategy','polynomial','--planner','adaptive'),
                     ('--strategy','polynomial','--max-power',3),('--max-degree',3),('--strategy','polynomial','--domain','int64')]:
            self.call('research','--train',train,*args,ok=False)
        fixture=Path(__file__).resolve().parents[1]/'fixtures/adaptive-v1.bruce'
        self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().split('RESULT\n',1)[1]))


if __name__=='__main__': unittest.main()
