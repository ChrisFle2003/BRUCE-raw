from fractions import Fraction
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
BIN=str(Path(sys.argv.pop(1)).resolve());ROOT=Path(__file__).resolve().parents[2]
class FiveExperienceTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=50)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def pairs(self,name,rows):
        p=self.root/name;p.write_text('x,y\n'+''.join(f'{x},{y}\n' for x,y in rows));return p
    def expression(self,start=1,*opts):
        f=lambda x:Fraction(x*x,x+1)
        return self.call('research','--strategy','portfolio-expression','--min',1,'--max',1,
            '--max-nodes',7,'--max-power',2,'--max-degree',2,
            '--train',self.pairs('train',[(x,f(x)) for x in range(start,start+4)]),
            '--holdout',self.pairs('hold',[(0,0),(start+5,f(start+5))]),*opts)
    def store(self):
        source=self.root/'source';self.expression(1,'--output',source)
        store=self.root/'store';report=self.call('experience','add','--from-run',source,'--output',store)
        self.assertEqual(report['records'][0]['votes'],[0,0,0,0,1]);return store,report
    def test_ranking_revoke_and_embedded_replay(self):
        store,report=self.store();self.assertTrue(store.read_text().startswith('BRUCE_EXPERIENCE_V3\n'))
        baseline=self.expression(2,'--planner','adaptive-order');learned_path=self.root/'learned'
        learned=self.expression(2,'--planner','experience-order','--experience',store,'--output',learned_path)
        self.assertEqual(baseline['strategy_order'][0],'power-affine')
        self.assertEqual(learned['strategy_order'][0],'expression')
        self.assertEqual(learned['hypotheses'],baseline['hypotheses'])
        self.assertEqual(learned['experience_votes'],[0,0,0,0,1]);self.assertEqual(learned['experience_records_used'],1)
        self.assertEqual(len(learned['strategy_runs']),5)
        # Ranking consumes training and context only, even when hold-out contradicts every candidate.
        bad=self.pairs('bad',[(0,99),(7,99)])
        changed=self.call('research','--strategy','portfolio-expression','--min',1,'--max',1,
            '--max-nodes',7,'--max-power',2,'--max-degree',2,'--train',self.root/'train','--holdout',bad,
            '--planner','experience-order','--experience',store)
        self.assertEqual(changed['strategy_order'],learned['strategy_order'])
        self.assertEqual(changed['experience_votes'],learned['experience_votes'])
        self.assertEqual(changed['status'],'REFUTED')
        self.call('experience','add','--from-run',learned_path,'--output',self.root/'recursive',ok=False)
        revoked=self.root/'revoked';self.call('experience','revoke','--store',store,'--id',report['records'][0]['id'],
            '--reason','Quelle zurückgezogen','--output',revoked)
        fallback=self.expression(2,'--planner','experience-order','--experience',revoked)
        self.assertEqual(fallback['strategy_order'],baseline['strategy_order']);self.assertEqual(fallback['experience_records_used'],0)
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot learned\n')
        selected=self.root/'selected';selection=self.call('compatibility','--rules',rules,'--from-run',learned_path,'--at',0,'--output',selected)
        self.assertEqual(selection['status'],'SELECTED')
        self.call('compatibility','--rules',rules,'--from-run',learned_path,'--at',-1,ok=False)
        for p in self.root.iterdir():
            if p not in [learned_path,selected]:p.unlink()
        self.assertEqual(self.call('replay',learned_path),learned);learned_path.unlink()
        self.assertEqual(self.call('replay',selected),selection)
    def test_shape_order_identity_and_holdout_independence(self):
        for f,first in [(lambda x:Fraction(1,x+1),'rational-linear'),(lambda x:Fraction(1,x*x+1),'rational-quadratic'),(lambda x:Fraction(x*x),'polynomial'),(lambda x:Fraction(x*x+1),'rational-quadratic')]:
            train=self.pairs('shape',[(x,f(x)) for x in range(5)]);hold=self.pairs('check',[(6,f(6))])
            args=['research','--strategy','portfolio-expression','--min',0,'--max',1,'--max-nodes',7,'--max-power',2,'--max-degree',2,'--train',train,'--holdout',hold]
            fixed=self.call(*args);out=self.call(*args,'--planner','adaptive-order')
            self.assertEqual(out['strategy_order'][0],first);self.assertEqual(out['hypotheses'],fixed['hypotheses'])
            self.assertEqual(len(set(out['strategy_order'])),5)
            hold.write_text('x,y\n6,99\n');changed=self.call(*args,'--planner','adaptive-order')
            self.assertEqual(changed['strategy_order'],out['strategy_order']);self.assertEqual(changed['ordering_operations'],out['ordering_operations'])
    def test_context_self_exclusion_and_limits(self):
        store,_=self.store()
        same=self.expression(1,'--planner','experience-order','--experience',store)
        self.assertEqual(same['experience_records_used'],0)
        for flag,n in [('--max-nodes',5),('--max-degree',1),('--max-power',1)]:
            # Use a direct call to avoid duplicate options.
            args=['research','--strategy','portfolio-expression','--min',1,'--max',1,'--max-nodes',7,'--max-power',2,'--max-degree',2,
                '--train',self.pairs('other',[(x,Fraction(x*x,x+1)) for x in range(2,6)]),'--planner','experience-order','--experience',store]
            args[args.index(flag)+1]=n
            self.assertEqual(self.call(*args)['experience_records_used'],0)
        for flag,n in [('--max-operations',1),('--max-candidates',1),('--max-trace',2),('--max-trace-bytes',256),('--max-results',1)]:
            p=self.root/flag[2:];out=self.expression(2,'--planner','experience-order','--experience',store,flag,n,'--output',p)
            self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',p),out)
        partial=self.expression(2,'--planner','experience-order','--experience',store,'--max-candidates',2000)
        self.assertEqual(partial['status'],'INCOMPLETE');self.assertTrue(any(h['status']=='VERIFIED_ON_SAMPLES' for h in partial['hypotheses']))
        path=self.root/'adaptive';self.expression(2,'--planner','adaptive-order','--output',path)
        self.assertEqual(self.call('experience','add','--from-run',path,'--output',self.root/'adaptive-store')['records'][0]['votes'],[0,0,0,0,1])
        header,_,body=path.read_text().split('\n',2);changed=body.replace('sample-shape-v3','bad-shape-v3',1)
        path.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed);self.call('replay',path,ok=False)
    def test_mixed_stores_and_historical_fixture(self):
        fixture=ROOT/'tests/fixtures/portfolio-v8.bruce'
        self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().split('RESULT\n',1)[1]))
        four=ROOT/'tests/fixtures/portfolio-v6.bruce';old=self.root/'old'
        old_report=self.call('experience','add','--from-run',four,'--output',old);before=old.read_bytes()
        mixed=self.root/'mixed';report=self.call('experience','add','--store',old,'--from-run',fixture,'--output',mixed)
        self.assertEqual(old.read_bytes(),before);self.assertEqual(report['records'][0]['id'],old_report['records'][0]['id'])
        self.assertEqual([len(r['votes']) for r in report['records']],[4,5])
        target=self.expression(2,'--planner','experience-order','--experience',mixed)
        self.assertEqual(target['experience_records_used'],1)
        self.assertEqual(self.expression(2,'--planner','experience-order','--experience',old)['experience_records_used'],0)
        older=self.call('research','--strategy','portfolio-quadratic','--max-degree',2,
            '--train',self.pairs('four-target',[(x,Fraction(1,x*x+1)) for x in range(5,9)]),
            '--holdout',self.pairs('four-hold',[(9,Fraction(1,82))]),
            '--planner','experience-order','--experience',mixed)
        self.assertEqual(older['experience_records_used'],1)
        self.assertEqual(older['experience_votes'],[0,0,0,1])
        header,_,body=mixed.read_text().split('\n',2);changed=body.replace('"status":"VERIFIED_ON_SAMPLES"','"status":"REFUTED"',1)
        mixed.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
        self.call('experience','inspect',mixed,ok=False)
if __name__=='__main__':unittest.main()
