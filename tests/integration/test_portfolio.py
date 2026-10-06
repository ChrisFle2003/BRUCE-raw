from fractions import Fraction
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BIN=str(Path(sys.argv.pop(1)).resolve())


class PortfolioTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=20)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def pairs(self,name,rows):
        path=self.root/name;path.write_text('x,y\n'+''.join(f'{x},{y}\n' for x,y in rows));return path
    def research(self,rows,*options):
        return self.call('research','--strategy','portfolio','--train',self.pairs('train.csv',rows),*options)

    def test_same_formula_merges_with_provenance_and_replay(self):
        hold=self.pairs('hold.csv',[(-3,21),(5,53)])
        path=self.root/'run.bruce'
        out=self.research([(x,2*x*x+3) for x in range(4)],'--holdout',hold,'--output',path)
        self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
        self.assertEqual(len(out['hypotheses']),1)
        h=out['hypotheses'][0]
        self.assertEqual(h['coefficients'],['3','0','2'])
        self.assertEqual({s['strategy'] for s in h['origins']},{'power-affine','polynomial'})
        self.assertEqual(len(out['strategy_runs']),2)
        self.assertFalse(out['general_proof'])
        hold.unlink();(self.root/'train.csv').unlink()
        self.assertEqual(self.call('replay',path),out)

    def test_multiterm_and_rational_family_union(self):
        out=self.research([(x,x*x+x+1) for x in range(4)])
        self.assertEqual(out['status'],'CANDIDATE')
        self.assertEqual(out['strategy_runs'][0]['status'],'NO_CANDIDATE')
        self.assertEqual(out['hypotheses'][0]['coefficients'],['1','1','1'])
        rows=[(Fraction(i,2),Fraction(1,2)*Fraction(i,2)**2+Fraction(1,3)) for i in range(4)]
        out=self.research(rows,'--domain','rational','--max-denominator',3)
        self.assertEqual(out['status'],'CANDIDATE')
        self.assertEqual(out['hypotheses'][0]['coefficients'],['1/3','0','1/2'])
        self.assertEqual(len(out['hypotheses'][0]['origins']),2)

    def test_shared_accounting_and_incomplete_child(self):
        rows=[(x,2*x*x+3) for x in range(4)]
        full=self.research(rows)
        self.assertEqual(full['operations_used'],sum(r['operations_used'] for r in full['strategy_runs']))
        self.assertEqual(full['candidates_executed'],sum(r['candidates_executed'] for r in full['strategy_runs']))
        self.assertEqual(full['trace_events_used'],sum(len(r['trace']) for r in full['strategy_runs'])+len(full['trace']))
        self.assertEqual(full['trace_bytes'],sum(r['trace_bytes'] for r in full['strategy_runs'])+sum(len(s.encode()) for s in full['trace']))
        firstops=full['strategy_runs'][0]['operations_used']
        limited=self.research(rows,'--max-operations',firstops)
        self.assertEqual(limited['status'],'INCOMPLETE')
        self.assertEqual(len(limited['strategy_runs']),1)
        self.assertLessEqual(limited['operations_used'],firstops)
        short=self.research([(0,0),(1,1)])
        self.assertEqual(short['status'],'INCOMPLETE')
        self.assertEqual(short['strategy_runs'][0]['status'],'AMBIGUOUS')
        self.assertEqual(short['strategy_runs'][1]['stop_reason'],'InsufficientTrainingSamples')
        for flag,n in [('--max-candidates',1),('--max-operations',1),('--max-trace',3),('--max-trace-bytes',256),('--max-results',1)]:
            archive=self.root/(flag[2:]+'.bruce')
            r=self.research(rows,flag,n,'--output',archive)
            self.assertEqual(r['status'],'INCOMPLETE')
            self.assertEqual(self.call('replay',archive),r)

    def test_refutation_and_projection(self):
        rows=[(x,2*x*x+3) for x in range(4)]
        hold=self.pairs('bad.csv',[(5,99)])
        bad=self.research(rows,'--holdout',hold)
        self.assertEqual(bad['status'],'REFUTED')
        self.assertEqual(len(bad['hypotheses']),1)
        archive=self.root/'origin.bruce';self.research(rows,'--output',archive)
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot portfolio\n')
        selected=self.root/'selection.bruce'
        r=self.call('compatibility','--rules',rules,'--from-run',archive,'--at',-3,'--output',selected)
        self.assertEqual(r['status'],'SELECTED')
        self.assertEqual(r['candidates'][0]['value'],'21')
        self.assertEqual(r['candidates'][0]['domain'],'portfolio-bigint-v1')
        archive.unlink();rules.unlink();self.assertEqual(self.call('replay',selected),r)

    def test_distinct_formulas_remain_ambiguous_and_sparse_union_is_kept(self):
        out=self.research([(0,0),(1,1)],'--max-degree',1)
        self.assertEqual(out['status'],'AMBIGUOUS')
        self.assertEqual(len(out['hypotheses']),4)
        line=next(h for h in out['hypotheses'] if h['coefficients']==['0','1'])
        self.assertEqual(len(line['origins']),2)
        sparse=self.research([(x,x*x+1) for x in range(4)],'--min',1,'--max',2)
        self.assertEqual(sparse['status'],'CANDIDATE')
        self.assertEqual(sparse['hypotheses'][0]['coefficients'],['1','0','1'])
        self.assertEqual(sparse['strategy_runs'][1]['status'],'NO_CANDIDATE')
        self.assertEqual(len(sparse['hypotheses'][0]['origins']),1)

    def test_historical_fixture_invalid_configuration_and_tamper(self):
        path=self.root/'run.bruce';self.research([(x,x*x+x+1) for x in range(4)],'--output',path)
        head,_,body=path.read_text().split('\n',2)
        for changed in [body.replace('power-then-polynomial-v1','unknown-v1'),body.replace('"status":"NO_CANDIDATE"','"status":"CANDIDATE"')]:
            self.assertNotEqual(changed,body)
            path.write_text(head+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
            self.call('replay',path,ok=False)
        fixture=Path(__file__).resolve().parents[1]/'fixtures/polynomial-v1.bruce'
        self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().split('RESULT\n',1)[1]))
        train=self.pairs('train.csv',[(0,1)])
        for args in [('--planner','adaptive'),('--planner','enumerate'),('--domain','int64'),('--max-degree',9),('--max-power',9)]:
            self.call('research','--strategy','portfolio','--train',train,*args,ok=False)


if __name__=='__main__':unittest.main()
