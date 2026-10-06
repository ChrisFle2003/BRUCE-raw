from fractions import Fraction
import hashlib
import itertools
import json
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest
BIN=str(Path(sys.argv.pop(1)).resolve())
class RationalQuadraticTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=20)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def pairs(self,name,rows):
        p=self.root/name;p.write_text('x,y\n'+''.join(f'{x},{y}\n' for x,y in rows));return p
    def research(self,rows,*opts):
        return self.call('research','--strategy','rational-quadratic','--train',self.pairs('train',rows),*opts)
    def test_quadratic_reciprocal_holdout_replay_and_projection(self):
        rows=[(x,Fraction(1,x*x+1)) for x in [0,1,2,3]]
        hold=self.pairs('hold',[(-2,'1/5'),(4,'1/17')]);origin=self.root/'run'
        out=self.research(rows,'--holdout',hold,'--output',origin)
        self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES');self.assertEqual(out['candidates_executed'],1)
        h=out['hypotheses'][0]
        self.assertEqual([h[k] for k in ['a','b','c','d']],['0','1','0','1'])
        self.assertEqual(h['denominator'],['1','0','1'])
        self.assertEqual(h['domain_condition'],'x^2+(0)*x+(1) != 0')
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot quadratic\n');selected=self.root/'selection'
        selection=self.call('compatibility','--rules',rules,'--from-run',origin,'--at',0,'--output',selected)
        self.assertEqual(selection['candidates'][0]['value'],'1')
        self.call('compatibility','--rules',rules,'--from-run',origin,'--at',1,ok=False)
        (self.root/'train').unlink();hold.unlink();self.assertEqual(self.call('replay',origin),out)
        origin.unlink();rules.unlink();self.assertEqual(self.call('replay',selected),selection)
    def test_fraction_reference_and_rank_deficient_family(self):
        rng=random.Random(1212)
        for _ in range(12):
            a,b,c,d=[Fraction(rng.randint(-2,2),rng.randint(1,2)) for _ in range(4)]
            f=lambda x:(a*x+b)/(x*x+c*x+d)
            xs=[Fraction(x) for x in [-4,-3,2,3,4] if x*x+c*x+d]
            out=self.research([(x,f(x)) for x in xs],'--min',-2,'--max',2,'--max-denominator',2)
            # Cancellable functions may not have a unique coefficient representation.
            self.assertNotEqual(out['status'],'INCOMPLETE')
            self.assertIn((a,b,c,d),{tuple(Fraction(h[k]) for k in ['a','b','c','d']) for h in out['hypotheses']})
        for rows in [[(2,0)],[(2,1)],[(2,1),(3,1)],[(2,1),(3,0),(4,1)]]:
            expected={tuple(map(Fraction,cs)) for cs in itertools.product(range(-1,2),repeat=4)
                      if all(x*x+cs[2]*x+cs[3]!=0 and Fraction(cs[0]*x+cs[1],x*x+cs[2]*x+cs[3])==y for x,y in rows)}
            out=self.research(rows,'--min',-1,'--max',1,'--max-denominator',1)
            self.assertNotEqual(out['status'],'INCOMPLETE')
            self.assertEqual({tuple(Fraction(h[k]) for k in ['a','b','c','d']) for h in out['hypotheses']},expected)
    def test_simple_double_irrational_and_removable_poles(self):
        for c,d,poles in [(0,-1,[-1,1]),(-2,1,[1])]:
            rows=[(x,Fraction(1,x*x+c*x+d)) for x in [2,3,4,5]]
            for pole in poles:
                out=self.research(rows,'--holdout',self.pairs('hold',[(pole,0)]))
                self.assertEqual(out['status'],'REFUTED');self.assertTrue(any('pole' in e for e in out['trace']))
        # Irrational roots are kept exactly as a polynomial condition, not rounded.
        out=self.research([(x,Fraction(1,x*x-2)) for x in [0,1,2,3]])
        self.assertEqual(out['hypotheses'][0]['domain_condition'],'x^2+(0)*x+(-2) != 0')
        # x/(x*x) keeps the hole at zero, despite cancellation to 1/x.
        out=self.research([(x,Fraction(1,x)) for x in [1,2,3,4]],'--min',0,'--max',1,'--max-denominator',1,
                          '--holdout',self.pairs('hole',[(0,0)]))
        target=next(h for h in out['hypotheses'] if [h[k] for k in ['a','b','c','d']]==['1','0','0','0'])
        self.assertEqual(target['status'],'REFUTED')
    def test_limits_versions_and_historical_experience(self):
        rows=[(x,Fraction(1,x*x+1)) for x in range(4)]
        for flag,n in [('--max-operations',1),('--max-candidates',1),('--max-trace',2),('--max-trace-bytes',256),('--max-results',1)]:
            p=self.root/flag[2:]
            # Singular all-zero data requires many candidates/results.
            out=self.research([(2,0)],'--min',-1,'--max',1,'--max-denominator',1,flag,n,'--output',p)
            self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',p),out)
        out=self.research([(15,1)],'--max-bits',4,'--min',0,'--max',1)
        self.assertEqual((out['status'],out['stop_reason']),('INCOMPLETE','IntegerBitLimit'))
        p=self.root/'run';self.research(rows,'--output',p);head,_,body=p.read_text().split('\n',2)
        for changed in [body.replace('quadratic-system-v1','unknown-v1'),body.replace('"d":"1"','"d":"0"')]:
            self.assertNotEqual(changed,body);p.write_text(head+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
            self.call('replay',p,ok=False)
        train=self.root/'train'
        for opts in [('--domain','bigint'),('--planner','adaptive'),('--max-power',2),('--max-degree',2),('--min',-2048,'--max',2047,'--max-denominator',16)]:
            self.call('research','--strategy','rational-quadratic','--train',train,*opts,ok=False)
        fixture=Path(__file__).resolve().parents[1]/'fixtures/portfolio-v4.bruce'
        self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().rsplit('RESULT\n',1)[1]))
    def test_large_id_projection_and_pole_rejection(self):
        rows=[(x,Fraction(2047*x+2047,x*x+2047*x+2047)) for x in range(5)]
        origin=self.root/'large';selected=self.root/'selected'
        out=self.research(rows,'--min',-2048,'--max',2047,'--max-denominator',8,'--output',origin)
        self.assertEqual(out['status'],'CANDIDATE')
        self.assertEqual(out['raw_parameter_combinations'],2**60)
        self.assertGreater(out['hypotheses'][0]['id'],2**53)
        self.assertEqual(self.call('replay',origin),out)
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot large\n')
        selection=self.call('compatibility','--rules',rules,'--from-run',origin,'--at',0,'--output',selected)
        self.assertEqual(selection['candidates'][0]['value'],'1')
        self.assertEqual(self.call('replay',selected),selection)
        pole_origin=self.root/'pole'
        self.research([(x,Fraction(1,x*x-1)) for x in [2,3,4,5]],'--output',pole_origin)
        error=self.call('compatibility','--rules',rules,'--from-run',pole_origin,'--at',1,ok=False)
        self.assertEqual(error['error'],'RationalFunctionPole')
    def test_outside_family_and_inconsistent_rows(self):
        rows=[(x,Fraction(1,x*x+1)) for x in range(4)]
        for name,data,opts,reason in [
            ('outside',rows,['--min',0,'--max',0],'outside_family'),
            ('inconsistent',rows+[(4,7)],[],'inconsistent')]:
            origin=self.root/name
            out=self.research(data,*opts,'--output',origin)
            self.assertEqual(out['status'],'NO_CANDIDATE')
            self.assertEqual(out['candidates_executed'],0)
            self.assertTrue(any(reason in e for e in out['trace']))
            self.assertEqual(self.call('replay',origin),out)
if __name__=='__main__':unittest.main()
