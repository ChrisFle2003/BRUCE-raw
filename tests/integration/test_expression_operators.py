import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
BIN=str(Path(sys.argv.pop(1)).resolve());ROOT=Path(__file__).resolve().parents[2]
class OperatorTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        (self.root/'train').write_text('x,y\n1,2\n2,6\n3,12\n4,20\n')
        (self.root/'hold').write_text('x,y\n0,0\n5,30\n')
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=30)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def task(self,*opts,ok=True,nodes=5):
        return self.call('research','--strategy','expression','--min',1,'--max',1,'--max-nodes',nodes,
            '--train',self.root/'train','--holdout',self.root/'hold',*opts,ok=ok)
    def test_declared_grammar_reduces_space_and_preserves_evaluation(self):
        for mode in ['syntax','rational']:
            old=self.task('--equivalence',mode)
            self.assertEqual(self.task('--equivalence',mode,'--operators','/*-+'),old)
            previous=None
            for planner in ['grammar','indexed-grammar']:
                path=self.root/(mode+planner)
                out=self.task('--equivalence',mode,'--planner',planner,'--operators','*+','--output',path)
                self.assertEqual(out['operators'],'+*');self.assertEqual(out['engine'],'bruce-expression-v4')
                self.assertEqual(out['grammar_expressions'],32);self.assertEqual(old['grammar_expressions'],184)
                self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES' if mode=='rational' else 'AMBIGUOUS')
                self.assertLess(out['operations_used'],old['operations_used'])
                for h in out['hypotheses']:
                    self.assertTrue(all(n['op'] in ['x','c','+','*'] for n in h['expression']))
                if previous:self.assertEqual(out['hypotheses'],previous['hypotheses'])
                previous=out
                self.assertEqual(self.call('replay',path),out)
            negative=self.task('--equivalence',mode,'--operators','+')
            self.assertEqual(negative['status'],'NO_CANDIDATE');self.assertEqual(negative['operators'],'+')
    def test_validation_limits_and_tampering(self):
        for invalid in ['', '++', '%', '+,*', ' +', '+-*/+']:
            self.task('--operators',invalid,ok=False)
        for strategy in ['affine','portfolio-rational','portfolio-quadratic']:
            self.call('research','--strategy',strategy,'--operators','+-*/','--train',self.root/'train',ok=False)
        for planner in ['grammar','indexed-grammar']:
            for flag,n in [('--max-operations',1),('--max-candidates',1),('--max-trace',2),('--max-trace-bytes',256)]:
                path=self.root/(planner+flag)
                out=self.task('--planner',planner,'--operators','+*',flag,n,'--output',path)
                self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',path),out)
        for mode in ['syntax','rational']:
            for planner in ['grammar','indexed-grammar']:
                path=self.root/(mode+planner+'results')
                out=self.task('--equivalence',mode,'--planner',planner,'--operators','+*','--max-results',1,'--output',path)
                self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(out['stop_reason'],'ResultBudgetExceeded')
                self.assertEqual(self.call('replay',path),out)
                rules=self.root/'limit-rules';rules.write_text('BRUCE_RULES_V1\nsnapshot limited\n')
                self.call('compatibility','--rules',rules,'--from-run',path,'--at',0,ok=False)
        path=self.root/'source';self.task('--operators','+*','--output',path)
        header,_,body=path.read_text().split('\n',2)
        for before,after in [('operators +*','operators +'),('operators +*','operators *+'),('selected-binary-v1','selected-binary-v9'),('size-ranked-v1','invalid-planner'),('"grammar_expressions":32','"grammar_expressions":31')]:
            changed=body.replace(before,after);self.assertNotEqual(body,changed)
            path.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
            self.call('replay',path,ok=False)
        for name in ['portfolio-v13.bruce','expression-v3.bruce']:
            path=ROOT/'tests/fixtures'/name
            self.assertEqual(self.call('replay',path),json.loads(path.read_text().rsplit('RESULT\n',1)[1]))
    def test_division_holes_and_self_contained_projection(self):
        (self.root/'train').write_text('x,y\n2,1/2\n3,1/3\n')
        (self.root/'hold').write_text('x,y\n-1,-1\n')
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot operators\n')
        run=self.root/'run';out=self.task('--operators','/','--equivalence','rational','--planner','indexed-grammar','--output',run,nodes=3)
        self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
        selection=self.root/'selection';result=self.call('compatibility','--rules',rules,'--from-run',run,'--at',-1,'--output',selection)
        self.assertEqual(result['status'],'SELECTED');self.assertEqual(result['candidates'][0]['value'],'-1')
        self.call('compatibility','--rules',rules,'--from-run',run,'--at',0,ok=False)
        for p in self.root.iterdir():
            if p not in [run,selection]:p.unlink()
        self.assertEqual(self.call('replay',run),out);run.unlink()
        self.assertEqual(self.call('replay',selection),result)
if __name__=='__main__':unittest.main()
