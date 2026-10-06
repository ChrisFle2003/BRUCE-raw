import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve())
class QExpressionTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.train=self.root/'train.csv';self.train.write_text('x,y\n2,3\n3,4\n')
        self.hold=self.root/'hold.csv';self.hold.write_text('x,y\n-1,0\n')
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],text=True,capture_output=True,timeout=30)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def run_search(self,planner='grammar',*extra,ok=True):
        return self.call('research','--strategy','expression','--planner',planner,'--equivalence','rational-q',
                         '--min',1,'--max',1,'--max-nodes',3,'--train',self.train,'--holdout',self.hold,*extra,ok=ok)
    def test_all_planners_operators_and_replay(self):
        for planner in ['grammar','indexed-grammar','cached-grammar']:
            for ops in ['+-*/','+','+^','+-*/^']:
                with self.subTest(planner=planner,ops=ops):
                    path=self.root/'run.bruce'
                    out=self.run_search(planner,'--operators',ops,'--output',path)
                    self.assertEqual(out['engine'],'bruce-expression-v7')
                    self.assertEqual(out['equivalence'],'rational-q-domain-v1')
                    self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
                    self.assertFalse(out['general_proof'])
                    self.assertEqual(out['hypotheses'][0]['formula'],'(x+1)')
                    self.assertTrue(path.read_text().startswith('BRUCE_EXPRESSION_BUNDLE_V7\n'))
                    self.assertEqual(self.call('replay',path),out)
                    path.unlink()
    def test_poles_selection_and_budget(self):
        self.train.write_text('x,y\n2,1\n3,1\n');self.hold.write_text('x,y\n4,1\n')
        path=self.root/'run.bruce'
        out=self.run_search('cached-grammar','--operators','/','--output',path)
        self.assertEqual(out['status'],'AMBIGUOUS')
        self.assertEqual(len(out['hypotheses']),2) # 1 and x/x stay separate
        self.assertGreater(out['domain_comparisons'],0)
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot test\n')
        selected=self.root/'selected.bruce'
        chosen=self.call('compatibility','--rules',rules,'--from-run',path,'--at',2,'--output',selected)
        self.assertTrue(all('expression-q-' in c['domain'] for c in chosen['candidates']))
        self.call('compatibility','--rules',rules,'--from-run',path,'--at',0,ok=False)
        path.unlink();rules.unlink()
        self.assertEqual(self.call('replay',selected),chosen)
        partial=self.root/'partial.bruce'
        limited=self.run_search('cached-grammar','--operators','/','--max-operations',out['operations_used']-1,'--output',partial)
        self.assertEqual(limited['status'],'INCOMPLETE')
        self.assertEqual(self.call('replay',partial),limited)
    def test_additional_merge_and_unknown_preserve_every_origin(self):
        self.train.write_text('x,y\n'+''.join(f'{x},0\n' for x in range(-3,4)))
        self.hold.write_text('x,y\n5,0\n')
        def origins(run):
            return sorted((o['id'],o['formula'],o['status']) for h in run['hypotheses'] for o in h['expression_origins'])
        for constant in [2,3]:
            runs=[]
            for equivalence in ['rational','rational-q']:
                args=['research','--strategy','expression','--planner','cached-grammar','--equivalence',equivalence,
                      '--operators','-^/','--min',constant,'--max',constant,'--max-nodes',9,
                      '--train',self.train,'--holdout',self.hold,'--max-operations',10000000]
                output=self.root/f'{constant}-{equivalence}.bruce'
                run=self.call(*args,'--output',output);runs.append(run)
                self.assertEqual(self.call('replay',output),run)
                self.assertEqual((run['status'],run['stop_reason']),('INCOMPLETE','ResultBudgetExceeded'))
                self.assertEqual(run['stored_source_hypotheses'],128)
            old,new=runs
            self.assertEqual(old['candidates_executed'],new['candidates_executed'])
            self.assertEqual(origins(old),origins(new))
            if constant==2:
                self.assertEqual(len(old['hypotheses'])-len(new['hypotheses']),1)
                origin=next(o for h in old['hypotheses'] if h['normal_form']['excluded_polynomial']==['-2','0','1'] for o in h['expression_origins'])
                group=next(h for h in new['hypotheses'] if any(o['id']==origin['id'] for o in h['expression_origins']))
                self.assertEqual(group['normal_form']['excluded_polynomial'],['1'])
            else:
                self.assertGreater(new['domain_unknown'],0)
                self.assertEqual(len(old['hypotheses']),len(new['hypotheses']))
                self.assertTrue(any(h['normal_form']['excluded_polynomial']==['-3','0','0','1'] for h in new['hypotheses']))

    def test_full_cached_run_and_all_operator_subsets(self):
        for planner in ['grammar','indexed-grammar','cached-grammar']:
            for mask in range(1,32):
                ops=''.join(op for bit,op in enumerate('+-*/^') if mask & (1<<bit))
                out=self.run_search(planner,'--operators',ops)
                expected='VERIFIED_ON_SAMPLES' if '+' in ops else 'NO_CANDIDATE'
                self.assertEqual(out['status'],expected)
        source=Path(__file__).resolve().parents[2]/'examples'
        findings=[]
        for planner in ['grammar','indexed-grammar','cached-grammar']:
            output=self.root/f'complete-{planner}.bruce'
            out=self.call('research','--strategy','expression','--planner',planner,'--equivalence','rational-q',
                '--min',1,'--max',1,'--max-nodes',7,'--train',source/'quadratic-fraction-training.csv',
                '--holdout',source/'quadratic-fraction-holdout.csv','--output',output)
            self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
            self.assertEqual(self.call('replay',output),out)
            findings.append(out['hypotheses'])
            if planner=='cached-grammar':
                self.assertGreater(out['cache_hits'],0)
                self.assertLessEqual(out['cache_entries'],64)
        self.assertTrue(all(h==findings[0] for h in findings))

    def test_literal_powers_merge_in_a_complete_run(self):
        self.train.write_text('x,y\n2,4\n3,9\n4,16\n');self.hold.write_text('x,y\n-2,4\n')
        results=[]
        for planner in ['grammar','indexed-grammar','cached-grammar']:
            output=self.root/f'power-{planner}.bruce'
            result=self.call('research','--strategy','expression','--planner',planner,'--equivalence','rational-q',
                '--operators','*^/','--min',2,'--max',2,'--max-nodes',5,'--train',self.train,'--holdout',self.hold,'--output',output)
            self.assertEqual(result['status'],'VERIFIED_ON_SAMPLES')
            self.assertGreater(result['domain_comparisons'],0)
            self.assertEqual(result['hypotheses'][0]['normal_form']['numerator'],['0','0','1'])
            formulas=[o['formula'] for o in result['hypotheses'][0]['expression_origins']]
            self.assertTrue(any('^' in f for f in formulas))
            self.assertTrue(any('*' in f for f in formulas))
            self.assertEqual(self.call('replay',output),result)
            results.append(result['hypotheses'])
        self.assertTrue(all(h==results[0] for h in results))

    def test_metadata_and_result_tamper_rejected(self):
        path=self.root/'run.bruce';self.run_search('cached-grammar','--output',path)
        original=path.read_text()
        for old,new in [('rational-q-domain-v1','rational-domain-v1'),('direct-map-64-size3-v1','none'),
                        ('rational-domain-gcd-quadratic-v1','unknown'),('operators +-*/','operators /+*-'),
                        ('"domain_comparisons":','"domain_comparisons":9')]:
            header,_,body=original.split('\n',2);self.assertIn(old,body)
            body=body.replace(old,new,1)
            path.write_text(header+'\n'+hashlib.sha256(body.encode()).hexdigest()+'\n'+body)
            self.call('replay',path,ok=False)
        self.call('research','--strategy','portfolio-expression','--equivalence','syntax','--train',self.train,ok=False)
if __name__=='__main__':unittest.main()
