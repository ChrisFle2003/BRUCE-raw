import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
BIN=str(Path(sys.argv.pop(1)).resolve());ROOT=Path(__file__).resolve().parents[2]
class IndexedExpressionTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=40)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def example(self,planner='indexed-grammar',equivalence='rational',*opts):
        return self.call('research','--strategy','expression','--planner',planner,'--equivalence',equivalence,
            '--min',1,'--max',1,'--max-nodes',7,'--train',ROOT/'examples/expression-training.csv',
            '--holdout',ROOT/'examples/expression-holdout.csv',*opts)
    def test_same_evidence_less_planning_and_replay(self):
        for mode in ['syntax','rational']:
            old=self.example('grammar',mode);bundle=self.root/mode;new=self.example('indexed-grammar',mode,'--output',bundle)
            self.assertEqual(new['hypotheses'],old['hypotheses']);self.assertEqual(new['status'],old['status'])
            self.assertEqual((new['grammar_expressions'],new['candidates_executed']),(2802,2802))
            self.assertLess(new['planning_operations'],old['planning_operations'])
            self.assertEqual(new['operations_used']-new['planning_operations'],old['operations_used']-old['planning_operations'])
            self.assertEqual(new['index_entries'],20)
            self.assertEqual(self.call('replay',bundle),new)
            limited=self.example('grammar',mode,'--max-operations',new['operations_used'])
            self.assertEqual(limited['status'],'INCOMPLETE')
            self.assertEqual(self.example('indexed-grammar',mode,'--max-operations',new['operations_used'])['status'],new['status'])
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot indexed\n')
        selected=self.root/'selected';selection=self.call('compatibility','--rules',rules,'--from-run',bundle,'--at',0,'--output',selected)
        self.assertEqual(selection['status'],'SELECTED');self.assertEqual(selection['candidates'][0]['value'],'0')
        self.call('compatibility','--rules',rules,'--from-run',bundle,'--at',-1,ok=False)
        bundle.unlink();rules.unlink();self.assertEqual(self.call('replay',selected),selection)
    def test_limits_invalid_options_and_integrity(self):
        for flag,n in [('--max-operations',1),('--max-candidates',1),('--max-trace',2),('--max-trace-bytes',256),('--max-results',1)]:
            path=self.root/flag[2:];out=self.example('indexed-grammar','rational',flag,n,'--output',path)
            self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',path),out)
        for strategy in ['affine','portfolio-expression','polynomial']:
            self.call('research','--strategy',strategy,'--planner','indexed-grammar','--train',ROOT/'examples/expression-training.csv',ok=False)
        path=self.root/'run';self.example('indexed-grammar','rational','--output',path)
        header,_,body=path.read_text().split('\n',2)
        for changed in [body.replace('block-index-v1','bad-index-v1'),body.replace('equivalence rational-domain-v1','equivalence bad-mode'),body.replace('"index_entries":20','"index_entries":19')]:
            self.assertNotEqual(changed,body);path.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
            self.call('replay',path,ok=False)
        for name in ['expression-v1.bruce','expression-v2.bruce','portfolio-v8.bruce','portfolio-v10.bruce']:
            fixture=ROOT/'tests/fixtures'/name
            self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().rsplit('RESULT\n',1)[1]))
if __name__=='__main__':unittest.main()
