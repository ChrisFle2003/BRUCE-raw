import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
BIN=str(Path(sys.argv.pop(1)).resolve())
class AdaptivePortfolioTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=20)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def pairs(self,name,rows):
        p=self.root/name;p.write_text('x,y\n'+''.join(f'{x},{y}\n' for x,y in rows));return p
    def research(self,rows,*opts,adaptive=True):
        return self.call('research','--strategy','portfolio-rational','--planner','adaptive-order' if adaptive else 'portfolio',
                         '--train',self.pairs('train',rows),*opts)
    @staticmethod
    def facts(out):
        return sorted((h['kind'],tuple(h.get('coefficients',h.get('numerator',[]))),tuple(h.get('denominator',[])),h['status'],
                       tuple(sorted((o['strategy'],o['id'],o['status']) for o in h['origins']))) for h in out['hypotheses'])
    def test_patterns_choose_order_without_changing_complete_findings(self):
        cases=[([(0,1),(1,2),(2,3),(3,4)],'power-affine'),
               ([(0,1),(1,'1/2'),(2,'1/3'),(3,'1/4')],'rational-linear'),
               ([(0,1),(1,3),(2,7),(3,13)],'polynomial')]
        for rows,first in cases:
            out=self.research(rows,'--max-degree',2)
            self.assertEqual(out['strategy_order'][0],first)
            self.assertEqual(out['strategy_runs'][0]['strategy'],first)
            fixed=self.research(rows,'--max-degree',2,adaptive=False)
            self.assertNotEqual(out['status'],'INCOMPLETE')
            self.assertEqual((out['status'],self.facts(out)),(fixed['status'],self.facts(fixed)))
            self.assertGreater(out['ordering_operations'],0)
            self.assertEqual(out['operations_used'],out['ordering_operations']+sum(r['operations_used'] for r in out['strategy_runs']))
            self.assertEqual(out['trace_events_used'],len(out['trace'])+sum(len(r['trace']) for r in out['strategy_runs']))
    def test_merged_ids_do_not_depend_on_execution_order(self):
        for rows in [[(0,1),(1,1),(2,1),(3,1)],[(0,1),(1,2),(2,5),(3,10)]]:
            opts=['--min',0,'--max',1,'--max-denominator',1,'--max-degree',2]
            adaptive=self.research(rows,*opts)
            fixed=self.research(rows,*opts,adaptive=False)
            self.assertNotEqual(adaptive['status'],'INCOMPLETE')
            self.assertEqual(sorted(adaptive['hypotheses'],key=lambda h:h['id']),sorted(fixed['hypotheses'],key=lambda h:h['id']))

    def test_holdout_does_not_leak_and_partial_hint_is_not_proof(self):
        rows=[(0,1),(1,'1/2'),(2,'1/3')]
        good=self.pairs('good',[(3,'1/4')]);bad=self.pairs('bad',[(3,99)])
        yes=self.research(rows,'--max-degree',2,'--holdout',good)
        no=self.research(rows,'--max-degree',2,'--holdout',bad)
        self.assertEqual((yes['strategy_order'],yes['ordering_reason'],yes['ordering_operations']),
                         (no['strategy_order'],no['ordering_reason'],no['ordering_operations']))
        self.assertEqual(yes['status'],'VERIFIED_ON_SAMPLES');self.assertEqual(no['status'],'REFUTED')
        partial=self.research(rows,'--max-degree',2,'--holdout',good,'--max-candidates',1)
        self.assertEqual(partial['status'],'INCOMPLETE')
        self.assertEqual(partial['hypotheses'][0]['status'],'VERIFIED_ON_SAMPLES')
        self.assertEqual(len(partial['strategy_runs']),1)
        misleading=self.research(rows+[(3,9)],'--max-degree',2)
        self.assertEqual(misleading['strategy_order'][0],'rational-linear')
        self.assertEqual(misleading['strategy_runs'][0]['status'],'NO_CANDIDATE')
    def test_zeros_short_input_limits_and_replay(self):
        for rows in [[(0,0)],[(0,0),(1,1)],[(0,0),(1,1),(2,4),(3,9)]]:
            out=self.research(rows,'--max-degree',min(len(rows)-1,2),'--min',0,'--max',1,'--max-denominator',1)
            self.assertEqual(set(out['strategy_order']),{'power-affine','polynomial','rational-linear'})
        for flag,n,reason in [('--max-operations',1,'OperationBudgetExceeded'),('--max-trace',2,'TraceBudgetExceeded'),
                              ('--max-trace-bytes',256,'TraceByteBudgetExceeded')]:
            path=self.root/flag[2:]
            out=self.research([(0,1),(1,'1/2'),(2,'1/3')],flag,n,'--output',path)
            self.assertEqual((out['status'],out['stop_reason']),('INCOMPLETE',reason))
            self.assertEqual(self.call('replay',path),out)
        out=self.research([(-15,-15),(15,15),(0,0)],'--min',0,'--max',1,'--max-bits',4)
        self.assertEqual((out['status'],out['stop_reason']),('INCOMPLETE','IntegerBitLimit'))
    def test_projection_versions_and_historical_replay(self):
        path=self.root/'run';hold=self.pairs('hold',[(3,'1/4')])
        out=self.research([(0,1),(1,'1/2'),(2,'1/3')],'--holdout',hold,'--max-degree',2,'--output',path)
        self.assertEqual(out['engine'],'bruce-portfolio-v3')
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot ordered\n')
        selection=self.root/'selection'
        selected=self.call('compatibility','--rules',rules,'--from-run',path,'--at',0,'--output',selection)
        self.assertEqual(selected['candidates'][0]['value'],'1')
        self.assertIn('Pole',self.call('compatibility','--rules',rules,'--from-run',path,'--at',-1,ok=False)['error'])
        head,_,body=path.read_text().split('\n',2)
        for changed in [body.replace('sample-shape-v1','unknown-v1'),body.replace('"ordering_operations":','"ordering_operations":9')]:
            self.assertNotEqual(body,changed)
            path.write_text(head+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
            self.call('replay',path,ok=False)
        path.unlink();rules.unlink();hold.unlink();(self.root/'train').unlink()
        self.assertEqual(self.call('replay',selection),selected)
        fixture=Path(__file__).resolve().parents[1]/'fixtures/portfolio-v2.bruce'
        self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().split('RESULT\n',1)[1]))
        train=self.pairs('t',[(0,1)])
        for strategy in ['affine','portfolio','power-affine','rational-linear','polynomial']:
            self.call('research','--strategy',strategy,'--planner','adaptive-order','--train',train,ok=False)
if __name__=='__main__':unittest.main()
