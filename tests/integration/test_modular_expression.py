import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve());ROOT=Path(__file__).resolve().parents[2]
class ModularExpressionTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],text=True,capture_output=True,timeout=30)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def task(self,planner,mode,*extra,strategy='expression',order='portfolio'):
        args=['research','--strategy',strategy,'--equivalence',mode,'--operators','-^/',
              '--min',3,'--max',3,'--max-nodes',9,'--max-operations',10000000,
              '--train',ROOT/'examples/q-domain-zero-training.csv','--holdout',ROOT/'examples/q-domain-zero-holdout.csv']
        args+=['--planner',planner] if strategy=='expression' else ['--expression-planner',planner,'--planner',order,'--max-power',3,'--max-degree',3]
        return self.call(*args,*extra)
    def test_actual_cubic_merge_all_planners_and_portfolio(self):
        findings=[]
        for strategy in ['expression','portfolio-expression']:
            for planner in ['grammar','indexed-grammar','cached-grammar']:
                old=self.task(planner,'rational-q',strategy=strategy)
                output=self.root/(strategy+planner)
                new=self.task(planner,'rational-q-modular','--output',output,strategy=strategy)
                self.assertEqual(self.call('replay',output),new)
                self.assertEqual((new['status'],new['stop_reason']),('INCOMPLETE','ResultBudgetExceeded' if strategy=='expression' else 'ChildIncomplete'))
                self.assertEqual(new['stored_source_hypotheses'],128)
                self.assertFalse(new['general_proof'])
                self.assertGreater(new['domain_modular_certificates'],0)
                self.assertEqual(len(old['hypotheses'])-len(new['hypotheses']),1)
                left=old if strategy=='expression' else next(c for c in old['strategy_runs'] if c['strategy']=='expression')
                right=new if strategy=='expression' else next(c for c in new['strategy_runs'] if c['strategy']=='expression')
                def origins(run):
                    return sorted((o['id'],o['formula'],o['status']) for h in run['hypotheses'] for o in h['expression_origins'])
                self.assertEqual(origins(left),origins(right))
                cubic=next(o for h in left['hypotheses'] if h['normal_form']['excluded_polynomial']==['-3','0','0','1'] for o in h['expression_origins'])
                group=next(h for h in right['hypotheses'] if any(o['id']==cubic['id'] for o in h['expression_origins']))
                self.assertEqual(group['normal_form']['excluded_polynomial'],['1'])
                if strategy=='portfolio-expression':
                    adaptive=self.task(planner,'rational-q-modular',strategy=strategy,order='adaptive-order')
                    self.assertEqual(adaptive['hypotheses'],new['hypotheses'])
                findings.append(right['hypotheses'])
        self.assertTrue(all(h==findings[0] for h in findings))
    def test_complete_run_poles_selection_and_metadata(self):
        train=self.root/'train';train.write_text('x,y\n2,1\n3,1\n')
        hold=self.root/'hold';hold.write_text('x,y\n4,1\n')
        args=['research','--strategy','expression','--equivalence','rational-q-modular','--operators','/',
              '--min',1,'--max',1,'--max-nodes',3,'--train',train,'--holdout',hold]
        for planner in ['grammar','indexed-grammar','cached-grammar']:
            path=self.root/planner;run=self.call(*args,'--planner',planner,'--output',path)
            self.assertEqual(run['engine'],'bruce-expression-v8')
            self.assertEqual(run['status'],'AMBIGUOUS');self.assertEqual(len(run['hypotheses']),2)
            self.assertTrue(path.read_text().startswith('BRUCE_EXPRESSION_BUNDLE_V8\n'))
            self.assertEqual(self.call('replay',path),run)
            rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot modular\n')
            selected=self.root/(planner+'selected')
            choice=self.call('compatibility','--rules',rules,'--from-run',path,'--at',2,'--output',selected)
            self.assertTrue(all('expression-q-modular-' in c['domain'] for c in choice['candidates']))
            self.call('compatibility','--rules',rules,'--from-run',path,'--at',0,ok=False)
            original=path.read_text()
            for old,new in [('bruce-expression-v8','bruce-expression-v7'),('rational-domain-gcd-modular-v1','rational-domain-gcd-quadratic-v1'),('rational-q-modular-domain-v1','rational-q-domain-v1'),('"domain_modular_certificates":0','"domain_modular_certificates":1')]:
                header,_,body=original.split('\n',2);self.assertIn(old,body);body=body.replace(old,new,1)
                path.write_text(header+'\n'+hashlib.sha256(body.encode()).hexdigest()+'\n'+body)
                self.call('replay',path,ok=False)
            path.unlink();self.assertEqual(self.call('replay',selected),choice)
        self.call('research','--strategy','polynomial','--equivalence','rational-q-modular','--train',train,ok=False)
if __name__=='__main__':unittest.main()
