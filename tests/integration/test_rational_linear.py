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


class RationalLinearTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=20)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def pairs(self,name,rows):
        p=self.root/name;p.write_text('x,y\n'+''.join(f'{x},{y}\n' for x,y in rows));return p
    def research(self,rows,*options):
        return self.call('research','--strategy','rational-linear','--train',self.pairs('train.csv',rows),*options)
    @staticmethod
    def triples(out):return {(Fraction(h['a']),Fraction(h['b']),Fraction(h['c'])) for h in out['hypotheses']}

    def test_reciprocal_derivation_holdout_and_replay(self):
        hold=self.pairs('hold.csv',[(-2,-1),(3,'1/4')]);path=self.root/'run.bruce'
        out=self.research([(0,1),(1,'1/2'),(2,'1/3')],'--holdout',hold,'--output',path)
        self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
        self.assertEqual(self.triples(out),{(Fraction(0),Fraction(1),Fraction(1))})
        self.assertEqual(out['hypotheses'][0]['excluded_x'],'-1')
        self.assertEqual(out['candidates_executed'],1)
        self.assertFalse(out['general_proof'])
        hold.unlink();(self.root/'train.csv').unlink();self.assertEqual(self.call('replay',path),out)

    def test_zero_constant_denominator_and_independent_fraction_cases(self):
        out=self.research([(1,1),(2,'1/2'),(3,'1/3')])
        self.assertEqual(self.triples(out),{(Fraction(0),Fraction(1),Fraction(0))})
        rng=random.Random(808)
        for _ in range(15):
            a=Fraction(rng.randint(-2,2),rng.randint(1,3));b=Fraction(rng.randint(-2,2),rng.randint(1,3))
            c=Fraction(rng.randint(-2,2),rng.randint(1,3))
            if b==a*c:continue
            xs=[Fraction(x) for x in (-4,-3,3,4)]
            hold=self.pairs('hold.csv',[(Fraction(9,2),(a*Fraction(9,2)+b)/(Fraction(9,2)+c))])
            out=self.research([(x,(a*x+b)/(x+c)) for x in xs],'--holdout',hold,'--min',-2,'--max',2,'--max-denominator',3)
            self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
            self.assertEqual(self.triples(out),{(a,b,c)})

    def test_singular_fallback_matches_complete_small_family(self):
        for rows in [[(2,1)],[(2,1),(3,1)],[(2,0),(3,0)],[(2,1),(3,2),(4,1)]]:
            expected={(Fraction(a),Fraction(b),Fraction(c)) for a in range(-1,2) for b in range(-1,2) for c in range(-1,2)
                      if all(x+c!=0 and Fraction(a*x+b,x+c)==y for x,y in rows)}
            out=self.research(rows,'--min',-1,'--max',1,'--max-denominator',1)
            self.assertNotEqual(out['status'],'INCOMPLETE')
            self.assertEqual(self.triples(out),expected)
        out=self.research([(2,1),(3,1)],'--min',0,'--max',1,'--max-denominator',1)
        self.assertEqual(out['status'],'AMBIGUOUS')
        self.assertEqual({h['excluded_x'] for h in out['hypotheses']},{'0','-1'})

    def test_poles_refute_even_removable_singularities(self):
        pole=self.pairs('pole.csv',[(-1,0)])
        out=self.research([(0,1),(1,'1/2'),(2,'1/3')],'--holdout',pole)
        self.assertEqual(out['status'],'REFUTED')
        self.assertTrue(any('pole' in e for e in out['trace']))
        removable=self.pairs('removable.csv',[(0,1)])
        out=self.research([(2,1),(3,1)],'--min',0,'--max',1,'--max-denominator',1,'--holdout',removable)
        self.assertEqual({h['c']:h['status'] for h in out['hypotheses']},{'0':'REFUTED','1':'VERIFIED_ON_SAMPLES'})
        self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')

    def test_projection_pole_fractional_rejection_and_version_tamper(self):
        path=self.root/'run.bruce';out=self.research([(0,1),(1,'1/2'),(2,'1/3')],'--output',path)
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot fraction\n')
        selection=self.root/'selection.bruce'
        selected=self.call('compatibility','--rules',rules,'--from-run',path,'--at',0,'--output',selection)
        self.assertEqual(selected['candidates'][0]['value'],'1')
        self.assertIn('Pole',self.call('compatibility','--rules',rules,'--from-run',path,'--at',-1,ok=False)['error'])
        self.call('compatibility','--rules',rules,'--from-run',path,'--at',1,ok=False)
        head,_,body=path.read_text().split('\n',2)
        for changed in [body.replace('linear-system-v1','linear-system-v9'),body.replace('"excluded_x":"-1"','"excluded_x":"0"')]:
            self.assertNotEqual(changed,body)
            path.write_text(head+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
            self.call('replay',path,ok=False)
        path.unlink();rules.unlink();self.assertEqual(self.call('replay',selection),selected)

    def test_limits_invalid_options_and_historical_portfolio(self):
        rows=[(0,1),(1,'1/2'),(2,'1/3')]
        for flag,n,reason in [('--max-operations',1,'OperationBudgetExceeded'),('--max-trace',2,'TraceBudgetExceeded'),
                              ('--max-trace-bytes',256,'TraceByteBudgetExceeded')]:
            path=self.root/(flag[2:]+'.bruce')
            out=self.research(rows,flag,n,'--output',path)
            self.assertEqual((out['status'],out['stop_reason']),('INCOMPLETE',reason))
            self.assertEqual(self.call('replay',path),out)
        out=self.research([(2,1),(3,1)],'--min',0,'--max',1,'--max-denominator',1,'--max-candidates',1)
        self.assertEqual((out['status'],out['stop_reason']),('INCOMPLETE','CandidateBudgetExceeded'))
        out=self.research([(15,15)],'--min',0,'--max',1,'--max-bits',4)
        self.assertEqual((out['status'],out['stop_reason']),('INCOMPLETE','IntegerBitLimit'))
        train=self.pairs('train.csv',rows)
        for args in [('--domain','bigint'),('--planner','adaptive'),('--max-power',2),('--max-degree',2)]:
            self.call('research','--strategy','rational-linear','--train',train,*args,ok=False)
        fixture=Path(__file__).resolve().parents[1]/'fixtures/portfolio-v1.bruce'
        self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().split('RESULT\n',1)[1]))


if __name__=='__main__':unittest.main()
