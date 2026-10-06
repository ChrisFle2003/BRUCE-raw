import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve());ROOT=Path(__file__).resolve().parents[2]
class DomainCacheTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,text=True,timeout=30)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def task(self,planner,*extra):
        return self.call('research','--strategy','expression','--planner',planner,'--equivalence','rational-q-modular',
            '--min',3,'--max',3,'--max-nodes',9,'--operators','-^/','--max-operations',10000000,
            '--train',ROOT/'examples/q-domain-zero-training.csv','--holdout',ROOT/'examples/q-domain-zero-holdout.csv',*extra)
    def test_same_groups_less_work_and_replay(self):
        for planner in ['grammar','indexed-grammar','cached-grammar']:
            baseline=self.task(planner)
            path=self.root/planner
            cached=self.task(planner,'--domain-cache','equal-64','--output',path)
            self.assertEqual(cached['hypotheses'],baseline['hypotheses'])
            self.assertEqual(cached['candidates_executed'],baseline['candidates_executed'])
            self.assertEqual(cached['stored_source_hypotheses'],128)
            self.assertEqual(cached['status'],'INCOMPLETE')
            self.assertGreater(cached['domain_cache_hits'],0)
            self.assertLess(cached['operations_used'],baseline['operations_used'])
            self.assertLessEqual(cached['domain_cache_entries'],64)
            self.assertLessEqual(cached['domain_cache_key_bytes'],64*4096)
            self.assertGreater(cached['domain_cache_operations'],0)
            self.assertEqual(cached['engine'],'bruce-expression-v9')
            self.assertTrue(path.read_text().startswith('BRUCE_EXPRESSION_BUNDLE_V9\n'))
            self.assertEqual(self.call('replay',path),cached)
    def test_complete_selection_source_removal_and_limits(self):
        args=['research','--strategy','expression','--equivalence','rational-q-modular',
              '--min',1,'--max',1,'--max-nodes',7,
              '--train',ROOT/'examples/expression-training.csv',
              '--holdout',ROOT/'examples/expression-holdout.csv']
        for planner in ['grammar','indexed-grammar','cached-grammar']:
            old=self.call(*args,'--planner',planner)
            output=self.root/planner
            run=self.call(*args,'--planner',planner,'--domain-cache','equal-64','--output',output)
            self.assertEqual(run['status'],'VERIFIED_ON_SAMPLES')
            self.assertEqual(run['hypotheses'],old['hypotheses'])
            self.assertGreater(run['domain_cache_hits'],0)
            self.assertEqual(self.call('replay',output),run)
            selection=self.root/(planner+'selection')
            chosen=self.call('compatibility','--rules',ROOT/'examples/quadratic-fraction-selection.rules',
                '--from-run',output,'--at',0,'--output',selection)
            self.assertEqual(chosen['status'],'SELECTED')
            self.assertTrue(all('expression-q-cached-v9-' in c['domain'] for c in chosen['candidates']))
            self.call('compatibility','--rules',ROOT/'examples/quadratic-fraction-selection.rules',
                '--from-run',output,'--at',-1,ok=False)
            output.unlink();self.assertEqual(self.call('replay',selection),chosen)
            for flag,value in [('--max-operations',1),('--max-results',1),('--max-trace',2),('--max-trace-bytes',256)]:
                partial=self.root/(planner+flag)
                limited=self.call(*args,'--planner',planner,'--domain-cache','equal-64',flag,value,'--output',partial)
                self.assertEqual(limited['status'],'INCOMPLETE')
                self.assertEqual(self.call('replay',partial),limited)

    def test_metadata_and_scope(self):
        path=self.root/'run';self.task('cached-grammar','--domain-cache','equal-64','--output',path)
        original=path.read_text()
        for old,new in [('equal-64-key4096-v1','none'),('"domain_cache_hits":','"domain_cache_hits":9'),('bruce-expression-v9','bruce-expression-v8')]:
            header,_,body=original.split('\n',2);self.assertIn(old,body);body=body.replace(old,new,1)
            path.write_text(header+'\n'+hashlib.sha256(body.encode()).hexdigest()+'\n'+body)
            self.call('replay',path,ok=False)
        for strategy,eq in [('expression','syntax'),('expression','rational-q'),('portfolio-expression','rational-q'),('affine','syntax')]:
            self.call('research','--strategy',strategy,'--equivalence',eq,'--domain-cache','equal-64',
                '--train',ROOT/'examples/q-domain-zero-training.csv',ok=False)
        self.call('research','--strategy','expression','--domain-cache','unknown','--train',ROOT/'examples/q-domain-zero-training.csv',ok=False)
if __name__=='__main__':unittest.main()
