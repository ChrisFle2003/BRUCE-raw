from fractions import Fraction
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
BIN=str(Path(sys.argv.pop(1)).resolve());ROOT=Path(__file__).resolve().parents[2]
class IndexedPortfolioTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=50)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def pairs(self,name,rows):
        p=self.root/name;p.write_text('x,y\n'+''.join(f'{x},{y}\n' for x,y in rows));return p
    def task(self,start=1,expression_planner='indexed-grammar',*opts):
        f=lambda x:Fraction(x*x,x+1)
        return self.call('research','--strategy','portfolio-expression','--expression-planner',expression_planner,
            '--min',1,'--max',1,'--max-nodes',7,'--max-power',2,'--max-degree',2,
            '--train',self.pairs('train',[(x,f(x)) for x in range(start,start+4)]),
            '--holdout',self.pairs('hold',[(0,0),(start+5,f(start+5))]),*opts)
    def test_fixed_adaptive_evidence_cost_and_replay(self):
        for planner in ['portfolio','adaptive-order']:
            old=self.task(1,'grammar','--planner',planner);path=self.root/planner
            new=self.task(1,'indexed-grammar','--planner',planner,'--output',path)
            self.assertEqual(new['hypotheses'],old['hypotheses']);self.assertEqual(new['status'],old['status'])
            self.assertEqual(new['strategy_order'],old['strategy_order']);self.assertEqual(len(new['strategy_runs']),5)
            child=next(c for c in new['strategy_runs'] if c['strategy']=='expression')
            self.assertEqual(child['planner'],'block-index-v1');self.assertEqual(child['index_entries'],20)
            self.assertEqual(new['expression_planner'],'block-index-v1')
            self.assertEqual(old['operations_used']-new['operations_used'],12221)
            self.assertEqual(self.call('replay',path),new)
            self.assertEqual(self.task(1,'grammar','--planner',planner,'--max-operations',new['operations_used'])['status'],'INCOMPLETE')
            self.assertEqual(self.task(1,'indexed-grammar','--planner',planner,'--max-operations',new['operations_used'])['status'],new['status'])
    def test_experience_context_mixed_revoke_and_projection(self):
        old_source=self.root/'old-source';self.task(1,'grammar','--output',old_source)
        old_store=self.root/'old-store';old_report=self.call('experience','add','--from-run',old_source,'--output',old_store)
        source=self.root/'source';self.task(2,'indexed-grammar','--planner','adaptive-order','--output',source)
        mixed=self.root/'mixed';report=self.call('experience','add','--store',old_store,'--from-run',source,'--output',mixed)
        self.assertTrue(mixed.read_text().startswith('BRUCE_EXPERIENCE_V4\n'))
        self.assertEqual(report['records'][0]['id'],old_report['records'][0]['id'])
        learned_path=self.root/'learned'
        learned=self.task(3,'indexed-grammar','--planner','experience-order','--experience',mixed,'--output',learned_path)
        old=self.task(3,'grammar','--planner','experience-order','--experience',mixed)
        self.assertEqual(learned['hypotheses'],old['hypotheses'])
        self.assertEqual(learned['experience_records_used'],1);self.assertEqual(old['experience_records_used'],1)
        self.assertEqual(learned['experience_votes'],[0,0,0,0,1]);self.assertEqual(learned['strategy_order'][0],'expression')
        mismatch=self.task(3,'indexed-grammar','--planner','experience-order','--experience',old_store)
        self.assertEqual(mismatch['experience_records_used'],0)
        own=self.task(2,'indexed-grammar','--planner','experience-order','--experience',mixed)
        self.assertEqual(own['experience_records_used'],0)
        self.call('experience','add','--from-run',learned_path,'--output',self.root/'recursive',ok=False)
        revoked=self.root/'revoked';self.call('experience','revoke','--store',mixed,'--id',report['records'][1]['id'],
            '--reason','Indexquelle zurückgezogen','--output',revoked)
        fallback=self.task(3,'indexed-grammar','--planner','experience-order','--experience',revoked)
        self.assertEqual(fallback['experience_records_used'],0)
        self.assertEqual(fallback['strategy_order'],self.task(3,'indexed-grammar','--planner','adaptive-order')['strategy_order'])
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot indexed-five\n')
        selection_path=self.root/'selection';selection=self.call('compatibility','--rules',rules,'--from-run',learned_path,'--at',0,'--output',selection_path)
        self.assertEqual(selection['status'],'SELECTED');self.assertEqual(selection['candidates'][0]['value'],'0')
        self.call('compatibility','--rules',rules,'--from-run',learned_path,'--at',-1,ok=False)
        for p in self.root.iterdir():
            if p not in [learned_path,selection_path]:p.unlink()
        self.assertEqual(self.call('replay',learned_path),learned);learned_path.unlink()
        self.assertEqual(self.call('replay',selection_path),selection)
    def test_limits_versions_and_validation(self):
        source=self.root/'source';self.task(1,'indexed-grammar','--output',source)
        store=self.root/'store';self.call('experience','add','--from-run',source,'--output',store)
        for planner in ['portfolio','adaptive-order','experience-order']:
            extra=['--experience',store] if planner=='experience-order' else []
            for flag,n in [('--max-operations',1),('--max-candidates',1),('--max-trace',2),('--max-trace-bytes',256),('--max-results',1)]:
                path=self.root/(planner+flag);out=self.task(2,'indexed-grammar','--planner',planner,*extra,flag,n,'--output',path)
                self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',path),out)
        for strategy in ['expression','affine','portfolio-quadratic']:
            self.call('research','--strategy',strategy,'--expression-planner','indexed-grammar','--train',self.root/'train',ok=False)
        self.call('research','--strategy','portfolio-expression','--expression-planner','bad','--train',self.root/'train',ok=False)
        header,_,body=source.read_text().split('\n',2)
        for changed in [body.replace('block-index-v1','bad-index-v1',1),body.replace('"index_entries":20','"index_entries":19')]:
            self.assertNotEqual(changed,body);source.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
            self.call('replay',source,ok=False)
        for name in ['expression-v3.bruce','portfolio-v8.bruce','portfolio-v10.bruce']:
            fixture=ROOT/'tests/fixtures'/name
            self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().rsplit('RESULT\n',1)[1]))
if __name__=='__main__':unittest.main()
