import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
BIN=str(Path(sys.argv.pop(1)).resolve())
ROOT=Path(__file__).resolve().parents[2]
class ExpressionEquivalenceTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=40)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def pairs(self,name,rows):
        p=self.root/name;p.write_text('x,y\n'+''.join(f'{x},{y}\n' for x,y in rows));return p
    def example(self,*opts):
        return self.call('research','--strategy','expression','--equivalence','rational','--min',1,'--max',1,
            '--train',ROOT/'examples/expression-training.csv','--holdout',ROOT/'examples/expression-holdout.csv',*opts)
    def test_domain_preserving_merge_provenance_and_replay(self):
        origin=self.root/'run';out=self.example('--output',origin)
        self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
        self.assertFalse(out['general_proof'])
        self.assertEqual((out['grammar_expressions'],out['candidates_executed']),(2802,2802))
        self.assertEqual(out['stored_source_hypotheses'],5)
        self.assertEqual(len(out['hypotheses']),2)
        good,bad=out['hypotheses']
        self.assertEqual((good['status'],bad['status']),('VERIFIED_ON_SAMPLES','REFUTED'))
        self.assertEqual(len(good['expression_origins']),3)
        self.assertEqual(len(bad['expression_origins']),2)
        for h in [good,bad]:
            self.assertEqual(h['normal_form']['numerator'],['0','0','1'])
            self.assertEqual(h['normal_form']['denominator'],['1','1'])
        self.assertEqual(good['normal_form']['excluded_polynomial'],['1','1'])
        self.assertEqual(bad['normal_form']['excluded_polynomial'],['0','1','1'])
        old=self.call('research','--strategy','expression','--min',1,'--max',1,
            '--train',ROOT/'examples/expression-training.csv','--holdout',ROOT/'examples/expression-holdout.csv')
        source=lambda h:(h['id'],h['status'],h['formula'],json.dumps(h['expression'],sort_keys=True))
        self.assertEqual(sorted(source(h) for h in old['hypotheses']),
            sorted(source(o) for h in out['hypotheses'] for o in h['expression_origins']))
        self.assertGreater(out['normalization_operations'],0)
        self.assertEqual(self.call('replay',origin),out)
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot equivalent\n')
        selected=self.root/'selected'
        selection=self.call('compatibility','--rules',rules,'--from-run',origin,'--at',0,'--output',selected)
        self.assertEqual(selection['status'],'SELECTED');self.assertEqual(selection['candidates'][0]['value'],'0')
        self.call('compatibility','--rules',rules,'--from-run',origin,'--at',-1,ok=False)
        origin.unlink();rules.unlink();self.assertEqual(self.call('replay',selected),selection)
    def test_cancellation_does_not_remove_holes(self):
        train=self.pairs('train',[(2,1),(3,1)])
        out=self.call('research','--strategy','expression','--equivalence','rational','--min',1,'--max',1,
            '--max-nodes',3,'--train',train)
        self.assertEqual(out['status'],'AMBIGUOUS')
        groups={tuple(h['normal_form']['excluded_polynomial']):h for h in out['hypotheses']}
        self.assertEqual(set(groups),{('1',),('0','1')})
        self.assertEqual(groups[('0','1')]['formula'],'(x/x)')
        self.assertEqual({o['formula'] for o in groups[('1',)]['expression_origins']},{'1','(1*1)','(1/1)'})
    def test_normalization_bit_limit_and_source_cap(self):
        train=self.pairs('zero',[(0,0)]);bundle=self.root/'bits'
        out=self.call('research','--strategy','expression','--equivalence','rational','--min',1,'--max',1,
            '--max-nodes',3,'--max-bits',1,'--train',train,'--output',bundle)
        self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(out['stop_reason'],'IntegerBitLimit')
        self.assertGreater(out['normalization_operations'],0)
        self.assertEqual(out['hypotheses'][-1]['formula'],'(x+x)')
        self.assertEqual(out['hypotheses'][-1]['status'],'INCOMPLETE')
        self.assertEqual(self.call('replay',bundle),out)
        train=self.pairs('constant',[(2,1),(3,1)])
        capped=self.call('research','--strategy','expression','--equivalence','rational','--min',1,'--max',1,
            '--max-nodes',3,'--train',train,'--max-results',1)
        self.assertEqual((capped['status'],capped['stop_reason']),('INCOMPLETE','ResultBudgetExceeded'))
        self.assertEqual(capped['stored_source_hypotheses'],1)
    def test_limits_versions_and_old_fixture(self):
        for flag,n in [('--max-operations',1),('--max-candidates',1),('--max-trace',2),('--max-trace-bytes',256),('--max-results',1)]:
            p=self.root/flag[2:];out=self.example(flag,n,'--output',p)
            self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',p),out)
        train=self.pairs('train',[(2,1)])
        self.call('research','--strategy','affine','--equivalence','rational','--train',train,ok=False)
        self.call('research','--strategy','expression','--equivalence','unknown','--train',train,ok=False)
        p=self.root/'run';self.example('--output',p)
        header,_,body=p.read_text().split('\n',2)
        for changed in [body.replace('rational-domain-v1','unknown-domain-v1'),body.replace('"excluded_polynomial":["1","1"]','"excluded_polynomial":["1"]',1),body.replace('"expression_origins":[{"id":738','"expression_origins":[{"id":739',1)]:
            self.assertNotEqual(changed,body);p.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
            self.call('replay',p,ok=False)
        fixture=ROOT/'tests/fixtures/expression-v1.bruce'
        self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().split('RESULT\n',1)[1]))
if __name__=='__main__':unittest.main()
