from fractions import Fraction
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
BIN=str(Path(sys.argv.pop(1)).resolve())
class RationalPortfolioTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=20)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def pairs(self,name,rows):
        p=self.root/name;p.write_text('x,y\n'+''.join(f'{x},{y}\n' for x,y in rows));return p
    def run_family(self,rows,*opts):
        return self.call('research','--strategy','portfolio-rational','--train',self.pairs('train.csv',rows),*opts)
    def test_reciprocal_three_strategies_replay_and_projection(self):
        path=self.root/'run';hold=self.pairs('hold',[(3,'1/4')])
        out=self.run_family([(0,1),(1,'1/2'),(2,'1/3')],'--max-degree',2,'--holdout',hold,'--output',path)
        self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
        self.assertEqual([s['strategy'] for s in out['strategy_runs']],['power-affine','polynomial','rational-linear'])
        h=next(h for h in out['hypotheses'] if h['kind']=='rational-linear')
        self.assertEqual((h['a'],h['b'],h['c'],h['excluded_x']),('0','1','1','-1'))
        self.assertEqual(h['origins'][0]['strategy'],'rational-linear')
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot rational-portfolio\n')
        selection=self.root/'selected'
        selected=self.call('compatibility','--rules',rules,'--from-run',path,'--at',0,'--output',selection)
        self.assertEqual(selected['candidates'][0]['value'],'1')
        self.assertIn('Pole',self.call('compatibility','--rules',rules,'--from-run',path,'--at',-1,ok=False)['error'])
        self.call('compatibility','--rules',rules,'--from-run',path,'--at',1,ok=False)
        hold.unlink();(self.root/'train.csv').unlink()
        self.assertEqual(self.call('replay',path),out)
        path.unlink();rules.unlink();self.assertEqual(self.call('replay',selection),selected)
    def test_constant_holes_are_not_merged_with_total_function(self):
        options=['--min',0,'--max',1,'--max-denominator',1,'--max-degree',1,'--max-power',1]
        out=self.run_family([(2,1),(3,1)],*options)
        self.assertEqual(out['status'],'AMBIGUOUS');self.assertEqual(len(out['hypotheses']),3)
        poly=next(h for h in out['hypotheses'] if h['kind']=='polynomial')
        self.assertEqual(len(poly['origins']),2)
        self.assertEqual({h['excluded_x'] for h in out['hypotheses'] if h['kind']=='rational-linear'},{'0','-1'})
        hold=self.pairs('hold',[(0,1)])
        out=self.run_family([(2,1),(3,1)],*options,'--holdout',hold)
        self.assertEqual(out['status'],'AMBIGUOUS')
        self.assertEqual({h['c']:h['status'] for h in out['hypotheses'] if h['kind']=='rational-linear'},
                         {'0':'REFUTED','1':'VERIFIED_ON_SAMPLES'})
    def test_shared_budgets_and_missing_polynomial_anchors(self):
        rows=[(0,1),(1,'1/2'),(2,'1/3')]
        full=self.run_family(rows,'--max-degree',2)
        for key in ['operations_used','candidates_executed','planning_operations','plans_completed']:
            self.assertEqual(full[key],sum(r[key] for r in full['strategy_runs']))
        self.assertEqual(full['trace_bytes'],sum(r['trace_bytes'] for r in full['strategy_runs'])+sum(len(e.encode()) for e in full['trace']))
        first_two=sum(r['operations_used'] for r in full['strategy_runs'][:2])
        limited=self.run_family(rows,'--max-degree',2,'--max-operations',first_two)
        self.assertEqual((limited['status'],len(limited['strategy_runs'])),('INCOMPLETE',2))
        self.assertEqual(self.run_family(rows)['status'],'INCOMPLETE')
        for flag,n in [('--max-operations',1),('--max-candidates',1),('--max-results',1),('--max-trace',3),('--max-trace-bytes',256)]:
            path=self.root/flag[2:]
            out=self.run_family(rows,'--max-degree',2,flag,n,'--output',path)
            self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',path),out)
    def test_polynomial_merge_and_refutation(self):
        out=self.run_family([(x,2*x*x+3) for x in range(4)],'--max-denominator',1)
        self.assertEqual(out['status'],'CANDIDATE');self.assertEqual(len(out['hypotheses']),1)
        self.assertEqual(len(out['hypotheses'][0]['origins']),2)
        hold=self.pairs('hold',[(5,99)])
        self.assertEqual(self.run_family([(x,2*x*x+3) for x in range(4)],'--max-denominator',1,'--holdout',hold)['status'],'REFUTED')
    def test_large_fraction_id_projects_safely(self):
        a=b=c=Fraction(2047)
        f=lambda x:(a*x+b)/(x+c)
        path=self.root/'large'
        out=self.run_family([(x,f(x)) for x in (0,1,2)],'--max-degree',2,'--max-power',1,
                            '--min',-2048,'--max',2047,'--max-denominator',16,'--output',path)
        h=next(h for h in out['hypotheses'] if h['kind']=='rational-linear')
        self.assertGreater(h['id'],2**48)
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot large-id\n')
        selection=self.root/'selection'
        selected=self.call('compatibility','--rules',rules,'--from-run',path,'--at',0,'--output',selection)
        self.assertEqual(Fraction(selected['candidates'][0]['value']),f(0))
        self.assertEqual(self.call('replay',selection),selected)

    def test_versions_and_old_formats(self):
        path=self.root/'run';out=self.run_family([(0,1),(1,'1/2'),(2,'1/3')],'--max-degree',2,'--output',path)
        self.assertEqual(out['engine'],'bruce-portfolio-v2')
        head,_,body=path.read_text().split('\n',2)
        for changed in [body.replace('power-polynomial-rational-v1','unknown-v1'),body.replace('"excluded_x":"-1"','"excluded_x":"0"')]:
            self.assertNotEqual(changed,body)
            path.write_text(head+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
            self.call('replay',path,ok=False)
        for name in ['rational-linear-v1.bruce','portfolio-v1.bruce']:
            fixture=Path(__file__).resolve().parents[1]/'fixtures'/name
            self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().split('RESULT\n',1)[1]))
        train=self.pairs('t',[(0,1)])
        for opts in [('--domain','bigint'),('--planner','solve'),('--max-degree',9),('--max-power',9)]:
            self.call('research','--strategy','portfolio-rational','--train',train,*opts,ok=False)
if __name__=='__main__':unittest.main()
