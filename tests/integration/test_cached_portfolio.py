import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve());ROOT=Path(__file__).resolve().parents[2]
class CachedPortfolioTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=40)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def task(self,ep='cached-grammar',ops='+-*/',target=False,*args):
        name='expression-power' if '^' in ops else 'expression'
        if target:name+='-target' if '^' in ops else '-experience-target'
        return self.call('research','--strategy','portfolio-expression','--expression-planner',ep,'--operators',ops,
            '--min',1,'--max',3 if '^' in ops else 1,'--max-power',2,'--max-degree',2,'--max-nodes',5 if '^' in ops else 7,
            '--train',ROOT/f'examples/{name}-training.csv','--holdout',ROOT/f'examples/{name}-holdout.csv',*args)
    def test_scope_same_results_and_budget_saving(self):
        for ops in ['+-*/','+*/','+^','+-*/^']:
            for order,version in [('portfolio',20),('adaptive-order',21)]:
                path=self.root/(ops.replace('/','d')+order)
                old=self.task('indexed-grammar',ops,False,'--planner',order)
                new=self.task('cached-grammar',ops,False,'--planner',order,'--output',path)
                self.assertEqual(new['engine'],f'bruce-portfolio-v{version}')
                self.assertEqual(new['cache_policy'],'direct-map-64-size3-v1')
                self.assertEqual(new['expression_operators'],ops)
                self.assertEqual(new['hypotheses'],old['hypotheses']);self.assertEqual(new['status'],old['status'])
                children={c['strategy']:c for c in new['strategy_runs']};oldchildren={c['strategy']:c for c in old['strategy_runs']}
                for family in ['power-affine','polynomial','rational-linear','rational-quadratic']:self.assertEqual(children[family],oldchildren[family])
                expr=children['expression'];self.assertEqual(expr['engine'],'bruce-expression-v6')
                self.assertEqual(expr['hypotheses'],oldchildren['expression']['hypotheses']);self.assertGreater(expr['cache_hits'],0)
                self.assertLessEqual(expr['cache_entries'],64);self.assertEqual(self.call('replay',path),new)
                if ops=='+-*/':
                    self.assertLess(new['operations_used'],old['operations_used'])
                    self.assertEqual(old['operations_used']-new['operations_used'],oldchildren['expression']['planning_operations']-expr['planning_operations'])
                    self.assertEqual(self.task('cached-grammar',ops,False,'--planner',order,'--max-operations',new['operations_used'])['status'],new['status'])
                    self.assertEqual(self.task('indexed-grammar',ops,False,'--planner',order,'--max-operations',new['operations_used'])['status'],'INCOMPLETE')
    def test_learning_revoke_and_embedded_replay(self):
        for ops in ['+-*/','+^']:
            source=self.root/('source'+str(len(ops)));out=self.task('cached-grammar',ops,False,'--planner','adaptive-order','--output',source)
            store=self.root/('store'+str(len(ops)));report=self.call('experience','add','--from-run',source,'--output',store)
            self.assertEqual(report['engine'],'bruce-experience-v7');self.assertEqual(report['records'][0]['votes'],[0,0,0,0,1])
            own=self.task('cached-grammar',ops,False,'--planner','experience-order','--experience',store)
            self.assertEqual(own['experience_records_used'],0)
            mismatch=self.task('indexed-grammar',ops,True,'--planner','experience-order','--experience',store)
            self.assertEqual(mismatch['experience_records_used'],0)
            learned=self.root/('learned'+str(len(ops)));run=self.task('cached-grammar',ops,True,'--planner','experience-order','--experience',store,'--output',learned)
            baseline=self.task('cached-grammar',ops,True,'--planner','adaptive-order')
            self.assertEqual(run['engine'],'bruce-portfolio-v22');self.assertEqual(run['experience_records_used'],1)
            self.assertEqual(run['strategy_order'][0],'expression');self.assertEqual(len(run['strategy_runs']),5)
            self.assertEqual(run['hypotheses'],baseline['hypotheses'])
            self.call('experience','add','--from-run',learned,'--output',self.root/'bad',ok=False)
            revoked=self.root/('revoked'+str(len(ops)));self.call('experience','revoke','--store',store,'--id',report['records'][0]['id'],'--reason','Cachequelle widerrufen','--output',revoked)
            fallback=self.task('cached-grammar',ops,True,'--planner','experience-order','--experience',revoked)
            self.assertEqual(fallback['experience_records_used'],0);self.assertEqual(fallback['strategy_order'],baseline['strategy_order'])
            rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot cached-portfolio\n')
            selection=self.root/('selection'+str(len(ops)));projected=self.call('compatibility','--rules',rules,'--from-run',learned,'--at',0,'--output',selection)
            self.assertEqual(projected['status'],'SELECTED');self.assertEqual(projected['candidates'][0]['value'],'0')
            source.unlink();store.unlink();revoked.unlink();rules.unlink()
            self.assertEqual(self.call('replay',learned),run);learned.unlink()
            self.assertEqual(self.call('replay',selection),projected)
    def test_limits_versions_and_history(self):
        source=self.root/'source';self.task('cached-grammar','+-*/',False,'--output',source)
        store=self.root/'store';self.call('experience','add','--from-run',source,'--output',store)
        for order in ['portfolio','adaptive-order','experience-order']:
            extra=['--experience',store] if order=='experience-order' else []
            for flag,n in [('--max-operations',1),('--max-candidates',1),('--max-trace',2),('--max-trace-bytes',256),('--max-results',1)]:
                path=self.root/(order+flag);out=self.task('cached-grammar','+-*/',True,'--planner',order,*extra,flag,n,'--output',path)
                self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',path),out)
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot cache-poles\n')
        self.call('compatibility','--rules',rules,'--from-run',source,'--at',-1,ok=False)
        header,_,body=source.read_text().split('\n',2)
        for before,after in [('subtree-cache-v1','block-index-v1'),('direct-map-64-size3-v1','direct-map-32-size3-v1'),
            ('literal-2-to-8-v1','literal-2-to-9-v1'),('expression_operators +-*/','expression_operators +*/'),('"cache_hits":','"bad_hits":')]:
            changed=body.replace(before,after);self.assertNotEqual(changed,body)
            source.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed);self.call('replay',source,ok=False)
        for name in ['expression-v6.bruce','portfolio-v19.bruce']:
            path=ROOT/'tests/fixtures'/name
            self.assertEqual(self.call('replay',path),json.loads(path.read_text().rsplit('RESULT\n',1)[1]))
        old=ROOT/'tests/fixtures/portfolio-v19.bruce';h,_,b=old.read_text().split('\n',2);b=b.replace('expression_planner block-index-v1','expression_planner subtree-cache-v1')
        source.write_text(h+'\n'+hashlib.sha256(b.encode()).hexdigest()+'\n'+b);self.call('replay',source,ok=False)
    def test_mixed_contexts_duplicate_and_downgrade(self):
        def run(start,ep,ops,path=None):
            train=self.root/'train';hold=self.root/'hold'
            train.write_text('x,y\n'+''.join(f'{x},{x*x+x}\n' for x in range(start,start+4)))
            hold.write_text('x,y\n0,0\n-2,2\n')
            args=['research','--strategy','portfolio-expression','--expression-planner',ep,'--operators',ops,
                '--min',1,'--max',1,'--max-power',2,'--max-degree',2,'--max-nodes',5,'--train',train,'--holdout',hold]
            return self.call(*args,*(['--output',path] if path else ['--planner','experience-order','--experience',store]))
        store=None;first=None
        for start,ep,ops in [(1,'indexed-grammar','+*'),(2,'cached-grammar','+*'),(3,'cached-grammar','+-*')]:
            source=self.root/f'source{start}';run(start,ep,ops,source)
            nxt=self.root/f'store{start}';report=self.call('experience','add',*(['--store',store] if store else []),'--from-run',source,'--output',nxt)
            if first is None:first=report['records'][0]
            self.assertEqual(report['records'][0],first);store=nxt
        self.assertEqual(report['engine'],'bruce-experience-v7')
        for ep,ops in [('indexed-grammar','+*'),('cached-grammar','+*'),('cached-grammar','+-*')]:
            learned=run(7,ep,ops)
            self.assertEqual(learned['experience_records_used'],1);self.assertEqual(learned['experience_votes'],[0,0,0,0,1])
        duplicate=self.root/'duplicate';run(1,'cached-grammar','+*',duplicate)
        self.assertEqual(self.call('experience','add','--store',store,'--from-run',duplicate,'--output',self.root/'baddup',ok=False)['error'],'DuplicateExperienceTrainingInput')
        _,_,body=store.read_text().split('\n',2);body=body.replace('policy routing-experience-v7','policy routing-experience-v6',1)
        store.write_text('BRUCE_EXPERIENCE_V6\n'+hashlib.sha256(body.encode()).hexdigest()+'\n'+body)
        self.assertEqual(self.call('experience','inspect',store,ok=False)['error'],'NoncanonicalExperienceSnapshot')
if __name__=='__main__':unittest.main()
