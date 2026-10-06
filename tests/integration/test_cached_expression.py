import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve());ROOT=Path(__file__).resolve().parents[2]
class CachedExpressionTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=40)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def task(self,planner,*args,ops='+-*/',mode='rational',power=False):
        name='expression-power' if power else 'expression'
        return self.call('research','--strategy','expression','--planner',planner,'--operators',ops,'--equivalence',mode,
            '--min',1,'--max',3 if power else 1,'--max-nodes',5 if power else 7,
            '--train',ROOT/f'examples/{name}-training.csv','--holdout',ROOT/f'examples/{name}-holdout.csv',*args)
    def test_same_candidates_less_planning_and_replay(self):
        for mode in ['syntax','rational']:
            path=self.root/mode;old=self.task('indexed-grammar',mode=mode)
            new=self.task('cached-grammar','--output',path,mode=mode)
            self.assertEqual(new['engine'],'bruce-expression-v6');self.assertEqual(new['planner'],'subtree-cache-v1')
            self.assertEqual(new['hypotheses'],old['hypotheses']);self.assertEqual(new['status'],old['status'])
            self.assertEqual(new['grammar_expressions'],old['grammar_expressions'])
            self.assertEqual(new['candidates_executed'],old['candidates_executed'])
            self.assertEqual(new['trace'][1:],old['trace'][1:])
            self.assertGreater(new['cache_hits'],0);self.assertLessEqual(new['cache_entries'],64)
            self.assertLess(new['planning_operations'],old['planning_operations'])
            self.assertEqual(old['operations_used']-new['operations_used'],old['planning_operations']-new['planning_operations'])
            self.assertEqual(self.call('replay',path),new)
            self.assertEqual(self.task('cached-grammar','--max-operations',new['operations_used'],mode=mode)['status'],new['status'])
            self.assertEqual(self.task('indexed-grammar','--max-operations',new['operations_used'],mode=mode)['status'],'INCOMPLETE')
    def test_selected_powers_and_history(self):
        for ops,power in [('+*/',False),('+^',True),('+-*/^',True)]:
            path=self.root/('power'+str(len(ops)))
            old=self.task('indexed-grammar',ops=ops,power=power)
            new=self.task('cached-grammar','--output',path,ops=ops,power=power)
            self.assertEqual(new['hypotheses'],old['hypotheses']);self.assertEqual(self.call('replay',path),new)
        for name in ['expression-v5.bruce','portfolio-v19.bruce']:
            path=ROOT/'tests/fixtures'/name
            self.assertEqual(self.call('replay',path),json.loads(path.read_text().rsplit('RESULT\n',1)[1]))
    def test_limits_tampering_and_selection(self):
        for flag,n in [('--max-operations',1),('--max-candidates',1),('--max-trace',2),('--max-trace-bytes',256),('--max-results',1)]:
            path=self.root/flag;out=self.task('cached-grammar',flag,n,'--output',path)
            self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',path),out)
        path=self.root/'full';out=self.task('cached-grammar','--output',path)
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot cached-expression\n')
        selection=self.root/'selection';selected=self.call('compatibility','--rules',rules,'--from-run',path,'--at',0,'--output',selection)
        self.assertEqual(selected['status'],'SELECTED');self.assertEqual(selected['candidates'][0]['value'],'0')
        self.call('compatibility','--rules',rules,'--from-run',path,'--at',-1,ok=False)
        header,_,body=path.read_text().split('\n',2)
        for before,after in [('subtree-cache-v1','subtree-cache-v2'),('direct-map-64-size3-v1','direct-map-32-size3-v1'),('operators +-*/','operators +*'),('"cache_hits":','"tampered_hits":')]:
            changed=body.replace(before,after);self.assertNotEqual(body,changed)
            path.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed);self.call('replay',path,ok=False)
        path.unlink();rules.unlink();self.assertEqual(self.call('replay',selection),selected)
    def test_no_subtrees_and_standalone_scope(self):
        train=self.root/'train';train.write_text('x,y\n2,4\n3,9\n')
        hold=self.root/'hold';hold.write_text('x,y\n0,0\n-1,1\n')
        args=['research','--strategy','expression','--operators','*','--min',1,'--max',1,'--max-nodes',3,'--train',train,'--holdout',hold]
        plain=self.call(*args,'--planner','indexed-grammar')
        cached=self.call(*args,'--planner','cached-grammar')
        self.assertEqual(cached['status'],'VERIFIED_ON_SAMPLES')
        self.assertEqual(cached['hypotheses'],plain['hypotheses']);self.assertEqual(cached['operations_used'],plain['operations_used'])
        self.assertEqual([cached[k] for k in ['cache_hits','cache_misses','cache_entries','cache_evictions']],[0,0,0,0])
        self.assertEqual(self.call(*args,'--planner','cached-grammar'),cached)
        self.assertEqual(self.call('research','--strategy','portfolio-expression','--expression-planner','unknown-grammar','--train',train,ok=False)['error'],'UnknownExpressionPlanner')
        self.call('research','--strategy','portfolio-expression','--planner','cached-grammar','--train',train,ok=False)
if __name__=='__main__':unittest.main()
