import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve());ROOT=Path(__file__).resolve().parents[2]
class ExpressionPowerTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.train=self.root/'train';self.hold=self.root/'hold'
        self.train.write_text('x,y\n1,2\n2,10\n3,30\n4,68\n');self.hold.write_text('x,y\n0,0\n-2,-10\n')
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=40)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def task(self,*args,ops='+^',nodes=5,low=1,high=3,ok=True):
        return self.call('research','--strategy','expression','--operators',ops,'--min',low,'--max',high,'--max-nodes',nodes,
            '--train',self.train,'--holdout',self.hold,*args,ok=ok)
    def test_cubic_in_smaller_tree_and_identical_planners(self):
        for mode in ['syntax','rational']:
            a=self.task('--equivalence',mode,'--planner','grammar')
            path=self.root/mode;b=self.task('--equivalence',mode,'--planner','indexed-grammar','--output',path)
            self.assertEqual(a['hypotheses'],b['hypotheses']);self.assertEqual(b['status'],'VERIFIED_ON_SAMPLES')
            self.assertEqual(b['grammar_expressions'],130);self.assertEqual(b['engine'],'bruce-expression-v5')
            self.assertEqual(b['operators'],'+^');self.assertEqual(self.call('replay',path),b)
            self.assertTrue(any(n['op']=='^' for h in b['hypotheses'] for n in h['expression']))
            self.assertEqual(self.task('--equivalence',mode,ops='+*')['status'],'NO_CANDIDATE')
            self.assertEqual(self.task('--equivalence',mode,ops='+*',nodes=7)['status'],'VERIFIED_ON_SAMPLES' if mode=='rational' else 'AMBIGUOUS')
    def test_exponent_scope_limits_and_versions(self):
        for ep in ['grammar','indexed-grammar']:
            path=self.root/ep;out=self.task('--planner',ep,'--output',path,ops='^',low=9,high=9,nodes=3)
            self.assertEqual(out['grammar_expressions'],2);self.assertEqual(out['status'],'NO_CANDIDATE')
            self.assertEqual(self.call('replay',path),out)
            for flag,n in [('--max-operations',1),('--max-candidates',1),('--max-trace',2),('--max-trace-bytes',256)]:
                path=self.root/(ep+flag);run=self.task('--planner',ep,flag,n,'--output',path)
                self.assertEqual(run['status'],'INCOMPLETE');self.assertEqual(self.call('replay',path),run)
        self.task(ops='^^',ok=False)
        self.call('research','--strategy','portfolio-quadratic','--operators','+^','--train',self.train,ok=False)
        path=self.root/'tamper';self.task('--output',path)
        header,_,body=path.read_text().split('\n',2)
        for before,after in [('bounded-power-v1','bounded-power-v9'),('literal-2-to-8-v1','literal-2-to-9-v1'),('operators +^','operators +*'),('operators +^','operators ^+'),('"grammar_expressions":130','"grammar_expressions":129')]:
            changed=body.replace(before,after);self.assertNotEqual(changed,body)
            path.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed);self.call('replay',path,ok=False)
        # V4 cannot acquire the new operator by changing its payload.
        old=ROOT/'tests/fixtures/expression-v4.bruce';h,_,oldbody=old.read_text().split('\n',2)
        changed=oldbody.replace('operators +*','operators +^');path.write_text(h+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
        self.call('replay',path,ok=False)
        for name in ['expression-v4.bruce','portfolio-v16.bruce']:
            fixture=ROOT/'tests/fixtures'/name
            self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().rsplit('RESULT\n',1)[1]))
    def test_division_holes_and_normalization_limits(self):
        self.train.write_text('x,y\n2,1/4\n3,1/9\n');self.hold.write_text('x,y\n-1,1\n')
        for ep in ['grammar','indexed-grammar']:
            path=self.root/ep;out=self.task('--planner',ep,'--equivalence','rational','--output',path,ops='/^',high=2)
            self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
            self.assertEqual(out['hypotheses'][0]['normal_form']['excluded_polynomial'],['0','1'])
            rules=self.root/'pole-rules';rules.write_text('BRUCE_RULES_V1\nsnapshot powered-poles\n')
            self.call('compatibility','--rules',rules,'--from-run',path,'--at',0,ok=False)
            self.assertEqual(self.call('replay',path),out)
            limited=self.root/(ep+'results');run=self.task('--planner',ep,'--equivalence','rational','--max-results',1,'--output',limited,ops='/^',high=2)
            self.assertEqual(run['status'],'INCOMPLETE');self.assertEqual(run['stop_reason'],'ResultBudgetExceeded')
            self.assertEqual(self.call('replay',limited),run)
        self.train.write_text('x,y\n0,0\n1,1\n');self.hold.write_text('x,y\n2,'+str(2**512)+'\n')
        path=self.root/'degree';out=self.task('--equivalence','rational','--output',path,ops='^',low=8,high=8,nodes=7)
        self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(out['stop_reason'],'ExpressionNormalizationDegreeLimit')
        self.assertEqual(self.call('replay',path),out)
    def test_eighth_power_and_projection_replay(self):
        self.train.write_text('x,y\n2,256\n3,6561\n');self.hold.write_text('x,y\n0,0\n-1,1\n')
        limited=self.root/'bits';bits=self.task('--max-bits',16,'--output',limited,ops='^',low=8,high=8,nodes=3)
        self.assertEqual(bits['status'],'INCOMPLETE');self.assertEqual(bits['stop_reason'],'IntegerBitLimit')
        self.assertEqual(self.call('replay',limited),bits)
        path=self.root/'power';out=self.task('--planner','indexed-grammar','--equivalence','rational','--output',path,ops='^',low=8,high=8,nodes=3)
        self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES');self.assertEqual(out['grammar_expressions'],4)
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot expression-power\n')
        selection=self.root/'selection';projected=self.call('compatibility','--rules',rules,'--from-run',path,'--at',-2,'--output',selection)
        self.assertEqual(projected['status'],'SELECTED');self.assertEqual(projected['candidates'][0]['value'],'256')
        self.call('compatibility','--rules',rules,'--from-run',path,'--at',1000,ok=False)
        for p in self.root.iterdir():
            if p not in [path,selection]:p.unlink()
        self.assertEqual(self.call('replay',path),out);path.unlink()
        self.assertEqual(self.call('replay',selection),projected)
if __name__=='__main__':unittest.main()
