from fractions import Fraction
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
BIN=str(Path(sys.argv.pop(1)).resolve())
ROOT=Path(__file__).resolve().parents[2]
class ExpressionPortfolioTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=40)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def pairs(self,name,rows):
        p=self.root/name;p.write_text('x,y\n'+''.join(f'{x},{y}\n' for x,y in rows));return p
    def run_task(self,f,*opts):
        return self.call('research','--strategy','portfolio-expression','--min',0,'--max',1,
            '--max-power',2,'--max-degree',2,'--max-nodes',5,
            '--train',self.pairs('train',[(x,f(x)) for x in range(1,6)]),
            '--holdout',self.pairs('hold',[(0,f(0)),(6,f(6))]),*opts)
    def test_five_families_merge_and_replay(self):
        path=self.root/'run';out=self.run_task(lambda x:Fraction(1,x+1),'--output',path)
        self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
        self.assertEqual([r['strategy'] for r in out['strategy_runs']],
            ['power-affine','polynomial','rational-linear','rational-quadratic','expression'])
        good=[h for h in out['hypotheses'] if h['status']=='VERIFIED_ON_SAMPLES']
        self.assertEqual(len(good),1)
        self.assertEqual({o['strategy'] for o in good[0]['origins']},{'rational-linear','expression'})
        self.assertEqual(good[0]['normal_form']['excluded_polynomial'],['1','1'])
        self.assertEqual(out['operations_used'],sum(r['operations_used'] for r in out['strategy_runs'])+out['union_operations'])
        self.assertGreater(out['union_operations'],0)
        self.assertEqual(out['stored_source_hypotheses'],sum(r.get('stored_source_hypotheses',len(r['hypotheses'])) for r in out['strategy_runs']))
        self.assertEqual(self.call('replay',path),out)
        self.call('experience','add','--from-run',path,'--output',self.root/'store')
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot five\n')
        selected=self.root/'selected'
        selection=self.call('compatibility','--rules',rules,'--from-run',path,'--at',0,'--output',selected)
        self.assertEqual(selection['status'],'SELECTED');self.assertEqual(selection['candidates'][0]['value'],'1')
        self.call('compatibility','--rules',rules,'--from-run',path,'--at',-1,ok=False)
        for p in self.root.iterdir():
            if p!=selected:p.unlink()
        self.assertEqual(self.call('replay',selected),selection)
    def test_expression_extends_portfolio(self):
        path=self.root/'run'
        out=self.call('research','--strategy','portfolio-expression','--min',1,'--max',1,
            '--max-power',2,'--max-degree',2,'--max-nodes',7,
            '--train',ROOT/'examples/expression-training.csv','--holdout',ROOT/'examples/expression-holdout.csv','--output',path)
        self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
        good=[h for h in out['hypotheses'] if h['status']=='VERIFIED_ON_SAMPLES']
        self.assertEqual({o['strategy'] for o in good[0]['origins']},{'expression'})
        self.assertEqual(len(out['strategy_runs'][-1]['hypotheses'][0]['expression_origins']),3)
        self.assertEqual(self.call('replay',path),out)
    def test_holes_survive_cross_family_cancellation(self):
        out=self.run_task(lambda x:Fraction(1))
        self.assertEqual(out['status'],'AMBIGUOUS')
        groups={tuple(h['normal_form']['excluded_polynomial']):h for h in out['hypotheses'] if h['normal_form']}
        self.assertIn(('1',),groups);self.assertIn(('0','1'),groups);self.assertIn(('1','1'),groups)
        nohole=groups[('1',)]
        self.assertTrue({'power-affine','polynomial','expression'}.issubset({o['strategy'] for o in nohole['origins']}))
        self.assertEqual(groups[('0','1')]['status'],'REFUTED')
        self.assertEqual(groups[('1','1')]['status'],'VERIFIED_ON_SAMPLES')
    def test_limits_and_versions(self):
        for flag,n in [('--max-candidates',1),('--max-operations',1),('--max-trace',2),('--max-trace-bytes',256),('--max-results',1)]:
            p=self.root/flag[2:];out=self.run_task(lambda x:Fraction(1,x+1),flag,n,'--output',p)
            self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',p),out)
            rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot partial\n')
            error=self.call('compatibility','--rules',rules,'--from-run',p,'--at',0,ok=False)
            self.assertEqual(error['error'],'IncompleteResearchCannotBeRanked')
        for opts in [('--planner','experience-order'),('--equivalence','syntax'),('--max-nodes',2),('--max',64),('--domain','bigint')]:
            self.call('research','--strategy','portfolio-expression','--train',self.root/'train',*opts,ok=False)
        p=self.root/'run';self.run_task(lambda x:Fraction(1,x+1),'--output',p)
        header,_,body=p.read_text().split('\n',2)
        for changed in [body.replace('normal-form-union-v1','unknown-union'),body.replace('"excluded_polynomial":["1","1"]','"excluded_polynomial":["1"]',1)]:
            self.assertNotEqual(changed,body);p.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
            self.call('replay',p,ok=False)
        fixture=ROOT/'tests/fixtures/expression-v2.bruce'
        self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().split('RESULT\n',1)[1]))
if __name__=='__main__':unittest.main()
