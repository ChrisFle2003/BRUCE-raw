import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
BIN=str(Path(sys.argv.pop(1)).resolve());ROOT=Path(__file__).resolve().parents[2]
class PortfolioOperatorTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=40)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def task(self,start=1,ops='+*',ep='indexed-grammar',*args):
        train=self.root/'train';hold=self.root/'hold'
        train.write_text('x,y\n'+''.join(f'{x},{x*x+x}\n' for x in range(start,start+4)))
        hold.write_text(f'x,y\n0,0\n{start+5},{(start+5)*(start+6)}\n')
        return self.call('research','--strategy','portfolio-expression','--operators',ops,'--expression-planner',ep,
            '--min',1,'--max',1,'--max-power',2,'--max-degree',2,'--max-nodes',5,'--train',train,'--holdout',hold,*args)
    def test_all_orders_planners_and_child_scope(self):
        for order in ['portfolio','adaptive-order']:
            same=None
            for ep in ['grammar','indexed-grammar']:
                old=self.task(1,'+-*/',ep,'--planner',order)
                path=self.root/(ep+order);new=self.task(1,'*+',ep,'--planner',order,'--output',path)
                self.assertEqual(new['expression_operators'],'+*');self.assertEqual(new['status'],'VERIFIED_ON_SAMPLES')
                self.assertEqual(len(new['strategy_runs']),5)
                self.assertEqual(new['strategy_runs'][:4],old['strategy_runs'][:4])
                self.assertEqual(new['strategy_runs'][4]['grammar_expressions'],32)
                self.assertEqual(new['strategy_runs'][4]['operators'],'+*')
                self.assertLess(new['operations_used'],old['operations_used'])
                self.assertEqual(new['hypotheses'][0]['normal_form'],old['hypotheses'][0]['normal_form'])
                if same:self.assertEqual(new['hypotheses'],same['hypotheses'])
                same=new
                self.assertEqual(self.call('replay',path),new)
                self.assertEqual(self.task(1,'+*',ep,'--planner',order,'--max-operations',new['operations_used'])['status'],new['status'])
                self.assertEqual(self.task(1,'+-*/',ep,'--planner',order,'--max-operations',new['operations_used'])['status'],'INCOMPLETE')
        # Only the expression child is restricted: x+1 is found by other families even when it may only multiply.
        train=self.root/'linear';train.write_text('x,y\n2,3\n3,4\n4,5\n5,6\n')
        hold=self.root/'linear-hold';hold.write_text('x,y\n0,1\n7,8\n')
        run=self.call('research','--strategy','portfolio-expression','--operators','*','--min',1,'--max',1,
            '--max-power',1,'--max-degree',1,'--max-nodes',3,'--train',train,'--holdout',hold)
        self.assertEqual(run['status'],'VERIFIED_ON_SAMPLES')
        self.assertEqual(run['strategy_runs'][4]['status'],'NO_CANDIDATE')
        self.assertEqual({o['strategy'] for o in run['hypotheses'][0]['origins']},{'power-affine','polynomial'})
    def test_mixed_contexts_revoke_self_exclusion(self):
        store=None;first_id=None
        for start,ops,ep,order in [(1,'+-*/','indexed-grammar','portfolio'),(2,'+*','indexed-grammar','adaptive-order'),
                                   (3,'+*','grammar','portfolio'),(4,'+-*','indexed-grammar','portfolio')]:
            source=self.root/f'source{start}';self.task(start,ops,ep,'--planner',order,'--output',source)
            nxt=self.root/f'store{start}'
            args=['--store',store] if store else []
            report=self.call('experience','add',*args,'--from-run',source,'--output',nxt)
            if start==1:first_id=report['records'][0]['id']
            self.assertEqual(report['records'][0]['id'],first_id);store=nxt
        self.assertTrue(store.read_text().startswith('BRUCE_EXPERIENCE_V5\n'))
        for ops,ep in [('+-*/','indexed-grammar'),('*+','indexed-grammar'),('+*','grammar'),('+-*','indexed-grammar')]:
            target=self.task(7,ops,ep,'--planner','experience-order','--experience',store)
            self.assertEqual(target['experience_records_used'],1);self.assertEqual(target['experience_votes'],[0,0,0,0,1])
            self.assertEqual(target['strategy_order'][0],'expression')
        own=self.task(2,'+*','indexed-grammar','--planner','experience-order','--experience',store)
        self.assertEqual(own['experience_records_used'],0)
        mismatch=self.task(7,'+*','indexed-grammar','--planner','experience-order','--experience',self.root/'store1')
        self.assertEqual(mismatch['experience_records_used'],0)
        learned=self.root/'learned';out=self.task(7,'+*','indexed-grammar','--planner','experience-order','--experience',store,'--output',learned)
        self.call('experience','add','--from-run',learned,'--output',self.root/'bad',ok=False)
        revoked=self.root/'revoked';self.call('experience','revoke','--store',store,'--id',report['records'][1]['id'],
            '--reason','Operatorquelle widerrufen','--output',revoked)
        fallback=self.task(7,'+*','indexed-grammar','--planner','experience-order','--experience',revoked)
        self.assertEqual(fallback['experience_records_used'],0)
        self.assertEqual(fallback['strategy_order'],self.task(7,'+*','indexed-grammar','--planner','adaptive-order')['strategy_order'])
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot selected-portfolio\n')
        selection=self.root/'selection';projected=self.call('compatibility','--rules',rules,'--from-run',learned,'--at',0,'--output',selection)
        self.assertEqual(projected['status'],'SELECTED');self.assertEqual(projected['candidates'][0]['value'],'0')
        for p in self.root.iterdir():
            if p not in [learned,selection]:p.unlink()
        self.assertEqual(self.call('replay',learned),out);learned.unlink()
        self.assertEqual(self.call('replay',selection),projected)
    def test_selected_division_preserves_poles(self):
        rules=self.root/'pole-rules';rules.write_text('BRUCE_RULES_V1\nsnapshot selected-poles\n')
        for ep in ['grammar','indexed-grammar']:
            for order in ['portfolio','adaptive-order']:
                path=self.root/(ep+order+'fraction')
                out=self.call('research','--strategy','portfolio-expression','--operators','+*/','--expression-planner',ep,
                    '--planner',order,'--min',1,'--max',1,'--max-power',2,'--max-degree',2,'--max-nodes',7,
                    '--train',ROOT/'examples/expression-training.csv','--holdout',ROOT/'examples/expression-holdout.csv','--output',path)
                self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
                self.assertEqual(self.call('replay',path),out)
                self.assertEqual(self.call('compatibility','--rules',rules,'--from-run',path,'--at',0)['status'],'SELECTED')
                self.call('compatibility','--rules',rules,'--from-run',path,'--at',-1,ok=False)
    def test_limits_tampering_and_history(self):
        source=self.root/'source';self.task(1,'+*','indexed-grammar','--output',source)
        store=self.root/'store';self.call('experience','add','--from-run',source,'--output',store)
        for ep in ['grammar','indexed-grammar']:
            for order in ['portfolio','adaptive-order','experience-order']:
                extra=['--experience',store] if order=='experience-order' else []
                for flag,n in [('--max-operations',1),('--max-candidates',1),('--max-trace',2),('--max-trace-bytes',256),('--max-results',1)]:
                    path=self.root/(ep+order+flag)
                    out=self.task(2,'+*',ep,'--planner',order,*extra,flag,n,'--output',path)
                    self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',path),out)
        for ops in ['', '++','%']:
            self.call('research','--strategy','portfolio-expression','--operators',ops,'--train',self.root/'train',ok=False)
        for strategy in ['affine','portfolio-quadratic']:
            self.call('research','--strategy',strategy,'--operators','+*','--train',self.root/'train',ok=False)
        header,_,body=source.read_text().split('\n',2)
        for before,after in [('expression_operators +*','expression_operators *+'),('expression_operators +*','expression_operators +-*/'),
                ('expression_operators +*','expression_operators +'),('block-index-v1','invalid-index'),('"grammar_expressions":32','"grammar_expressions":31')]:
            changed=body.replace(before,after);self.assertNotEqual(changed,body)
            source.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
            self.call('replay',source,ok=False)
        for name in ['expression-v4.bruce','portfolio-v13.bruce','portfolio-v10.bruce']:
            path=ROOT/'tests/fixtures'/name
            self.assertEqual(self.call('replay',path),json.loads(path.read_text().rsplit('RESULT\n',1)[1]))
if __name__=='__main__':unittest.main()
