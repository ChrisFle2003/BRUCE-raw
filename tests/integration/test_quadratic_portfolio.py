from fractions import Fraction
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
BIN=str(Path(sys.argv.pop(1)).resolve())
class QuadraticPortfolioTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=30)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def pairs(self,name,rows):
        p=self.root/name;p.write_text('x,y\n'+''.join(f'{x},{y}\n' for x,y in rows));return p
    def run_family(self,k=0,*opts):
        f=lambda x:Fraction(k*x+k+1,x*x+1)
        defaults=[] if '--max-degree' in opts else ['--max-degree',2]
        return self.call('research','--strategy','portfolio-quadratic',*defaults,
            '--train',self.pairs('train',[(x,f(x)) for x in range(5)]),
            '--holdout',self.pairs('hold',[(6,f(6))]),*opts)
    def test_four_families_order_and_replay(self):
        baseline=self.run_family()
        self.assertEqual(baseline['status'],'VERIFIED_ON_SAMPLES')
        self.assertEqual(len(baseline['strategy_runs']),4)
        self.assertEqual(baseline['hypotheses'][0]['kind'],'rational-quadratic')
        self.assertEqual(baseline['hypotheses'][0]['denominator'],['1','0','1'])
        origin=self.root/'run'
        ordered=self.run_family(0,'--planner','adaptive-order','--output',origin)
        self.assertEqual(ordered['strategy_order'][0],'rational-quadratic')
        self.assertEqual(ordered['hypotheses'],baseline['hypotheses'])
        self.assertEqual(self.call('replay',origin),ordered)
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot quadratic\n')
        selected=self.root/'selected'
        selection=self.call('compatibility','--rules',rules,'--from-run',origin,'--at',0,'--output',selected)
        self.assertEqual(selection['candidates'][0]['value'],'1')
        for p in self.root.iterdir():
            if p!=selected:p.unlink()
        self.assertEqual(self.call('replay',selected),selection)
    def test_experience_ranking_revoke_and_isolation(self):
        source=self.root/'source';self.run_family(1,'--output',source)
        store=self.root/'store';report=self.call('experience','add','--from-run',source,'--output',store)
        self.assertEqual(report['records'][0]['votes'],[0,0,0,1])
        self.assertTrue(store.read_text().startswith('BRUCE_EXPERIENCE_V2\n'))
        baseline=self.run_family(2,'--planner','adaptive-order')
        learned_path=self.root/'learned'
        learned=self.run_family(2,'--planner','experience-order','--experience',store,'--output',learned_path)
        self.assertEqual(baseline['strategy_order'][0],'power-affine')
        self.assertEqual(learned['strategy_order'][0],'rational-quadratic')
        self.assertEqual(learned['hypotheses'],baseline['hypotheses'])
        self.assertEqual(learned['experience_votes'],[0,0,0,1])
        self.assertEqual(learned['experience_records_used'],1)
        partial=self.run_family(2,'--planner','experience-order','--experience',store,'--max-candidates',1)
        self.assertEqual(partial['status'],'INCOMPLETE')
        self.assertEqual(partial['hypotheses'][0]['status'],'VERIFIED_ON_SAMPLES')
        bad_hold=self.pairs('bad-hold',[(6,99)])
        bad=self.call('research','--strategy','portfolio-quadratic','--max-degree',2,
            '--train',self.root/'train','--holdout',bad_hold,'--planner','experience-order','--experience',store)
        self.assertEqual((bad['strategy_order'],bad['experience_votes']),(learned['strategy_order'],learned['experience_votes']))
        self.call('experience','add','--from-run',learned_path,'--output',self.root/'recursive',ok=False)
        revoked=self.root/'revoked';self.call('experience','revoke','--store',store,'--id',report['records'][0]['id'],
            '--reason','countercheck','--output',revoked)
        fallback=self.run_family(2,'--planner','experience-order','--experience',revoked)
        self.assertEqual(fallback['strategy_order'],baseline['strategy_order'])
        self.assertEqual(fallback['experience_records_used'],0)
        for p in self.root.iterdir():
            if p!=learned_path:p.unlink()
        self.assertEqual(self.call('replay',learned_path),learned)
    def test_limits_and_integrity(self):
        for flag,n in [('--max-candidates',1),('--max-operations',1),('--max-trace',2),('--max-trace-bytes',256)]:
            p=self.root/flag[2:]
            out=self.run_family(0,'--planner','adaptive-order',flag,n,'--output',p)
            self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',p),out)
        p=self.root/'run';self.run_family(0,'--output',p)
        head,_,body=p.read_text().split('\n',2)
        changed=body.replace('domain-union-v2','unknown-domain-v1')
        self.assertNotEqual(changed,body)
        p.write_text(head+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
        self.call('replay',p,ok=False)
        fixture=Path(__file__).resolve().parents[1]/'fixtures/rational-quadratic-v1.bruce'
        self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().split('RESULT\n',1)[1]))
    def test_domains_remain_distinct(self):
        out=self.call('research','--strategy','portfolio-quadratic','--min',0,'--max',1,'--max-denominator',1,
            '--max-power',2,'--max-degree',2,'--train',self.pairs('zero',[(x,0) for x in [1,2,3,4,5]]))
        self.assertEqual(out['status'],'AMBIGUOUS')
        self.assertEqual({h['kind'] for h in out['hypotheses']},{'polynomial','rational-linear','rational-quadratic'})
        quad=[h for h in out['hypotheses'] if h['kind']=='rational-quadratic']
        self.assertEqual(len(quad),4)
        self.assertEqual(len({tuple(h['denominator']) for h in quad}),4)
    def test_mixed_stores_keep_ids_and_separate_contexts(self):
        def old(k,*opts):
            f=lambda x:Fraction(k*x+k+1,x+1)
            return self.call('research','--strategy','portfolio-rational','--max-degree',2,
                '--train',self.pairs('old-train',[(x,f(x)) for x in range(5)]),
                '--holdout',self.pairs('old-hold',[(6,f(6))]),*opts)
        old_source=self.root/'old-source';old(1,'--output',old_source)
        old_store=self.root/'old-store'
        report=self.call('experience','add','--from-run',old_source,'--output',old_store)
        before=old_store.read_bytes()
        mismatch=self.run_family(2,'--planner','experience-order','--experience',old_store)
        self.assertEqual(mismatch['experience_records_used'],0)
        new_source=self.root/'new-source';self.run_family(1,'--planner','adaptive-order','--output',new_source)
        mixed=self.root/'mixed'
        mixed_report=self.call('experience','add','--store',old_store,'--from-run',new_source,'--output',mixed)
        self.assertEqual(old_store.read_bytes(),before)
        self.assertEqual(mixed_report['records'][0]['id'],report['records'][0]['id'])
        self.assertEqual([len(r['votes']) for r in mixed_report['records']],[3,4])
        a=old(2,'--planner','experience-order','--experience',mixed)
        b=self.run_family(2,'--planner','experience-order','--experience',mixed)
        self.assertEqual((a['experience_records_used'],b['experience_records_used']),(1,1))
        self.assertEqual(a['strategy_order'][0],'rational-linear')
        self.assertEqual(b['strategy_order'][0],'rational-quadratic')
        own=self.run_family(1,'--planner','experience-order','--experience',mixed)
        self.assertEqual(own['experience_records_used'],0)
        # No recursive learned source can be hidden in the new container version.
        learned=self.root/'nested';self.run_family(2,'--planner','experience-order','--experience',mixed,'--output',learned)
        for flag in ['--max-power','--max-degree']:
            mismatch=self.run_family(2,'--planner','experience-order','--experience',mixed,flag,1)
            self.assertEqual(mismatch['experience_records_used'],0)
        header,_,body=mixed.read_text().split('\n',2)
        changed=body.replace('"status":"VERIFIED_ON_SAMPLES"','"status":"REFUTED"',1)
        mixed.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
        self.call('experience','inspect',mixed,ok=False)
        mixed.unlink();old_store.unlink();new_source.unlink();old_source.unlink()
        self.assertEqual(self.call('replay',learned)['experience_records_used'],1)
    def test_maximum_slot_id_and_projection(self):
        coefficient=Fraction(2047,16)
        f=lambda x:(coefficient*x+coefficient)/(x*x+coefficient*x+coefficient)
        origin=self.root/'large'
        out=self.call('research','--strategy','portfolio-quadratic','--max-degree',2,'--max-power',2,
            '--min',0,'--max',2047,'--max-denominator',16,
            '--train',self.pairs('large-train',[(x,f(x)) for x in range(5)]),'--output',origin)
        self.assertEqual(out['status'],'CANDIDATE')
        self.assertEqual(out['hypotheses'][0]['id'],2**62-1)
        self.assertEqual(self.call('replay',origin),out)
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot large\n')
        selected=self.root/'selected'
        selection=self.call('compatibility','--rules',rules,'--from-run',origin,'--at',0,'--output',selected)
        self.assertEqual(selection['candidates'][0]['value'],'1')
        self.assertEqual(self.call('replay',selected),selection)
        self.call('research','--strategy','portfolio-quadratic','--min',-2048,'--max',2047,
            '--max-denominator',16,'--train',self.root/'large-train',ok=False)
if __name__=='__main__':unittest.main()
