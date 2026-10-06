import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve());ROOT=Path(__file__).resolve().parents[2]
class QPortfolioTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],text=True,capture_output=True,timeout=40)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def task(self,ep='cached-grammar',order='portfolio',target=False,*extra,eq='rational-q-modular',cache='equal-64'):
        name='expression-experience-target' if target else 'expression'
        return self.call('research','--strategy','portfolio-expression','--equivalence',eq,'--domain-cache',cache if eq=='rational-q-modular' else 'none','--expression-planner',ep,
            '--planner',order,'--min',1,'--max',1,'--max-nodes',7,'--max-power',2,'--max-degree',2,
            '--train',ROOT/f'examples/{name}-training.csv','--holdout',ROOT/f'examples/{name}-holdout.csv',*extra)
    def test_planners_orders_and_unchanged_children(self):
        findings=[]
        for ep in ['grammar','indexed-grammar','cached-grammar']:
            for order,version in [('portfolio',29),('adaptive-order',30)]:
                path=self.root/f'{ep}-{order}'
                old=self.task(ep,order,False,cache='none')
                run=self.task(ep,order,False,'--output',path)
                self.assertEqual(run['engine'],f'bruce-portfolio-v{version}')
                self.assertEqual(run['equivalence'],'rational-q-modular-domain-v1')
                self.assertEqual(run['status'],'VERIFIED_ON_SAMPLES')
                children={c['strategy']:c for c in run['strategy_runs']}
                previous={c['strategy']:c for c in old['strategy_runs']}
                for family in ['power-affine','polynomial','rational-linear','rational-quadratic']:
                    self.assertEqual(children[family],previous[family])
                self.assertEqual(children['expression']['engine'],'bruce-expression-v9')
                self.assertLessEqual(run['domain_cache_entries'],128)
                self.assertLessEqual(run['domain_cache_key_bytes'],128*4096)
                self.assertLessEqual(run['domain_cache_operations'],run['domain_comparison_operations'])
                self.assertEqual(run['operations_used'],sum(c['operations_used'] for c in run['strategy_runs'])+run['union_operations']+run.get('ordering_operations',0))
                self.assertEqual(self.call('replay',path),run)
                findings.append(run['hypotheses'])
        self.assertTrue(all(h==findings[0] for h in findings))
    def test_experience_isolation_revoke_selection_and_replay(self):
        for ep in ['grammar','indexed-grammar','cached-grammar']:
            source=self.root/(ep+'-source');self.task(ep,'adaptive-order',False,'--output',source)
            store=self.root/(ep+'-store');report=self.call('experience','add','--from-run',source,'--output',store)
            self.assertEqual(report['engine'],'bruce-experience-v10')
            self.assertEqual(report['records'][0]['votes'],[0,0,0,0,1])
            self.assertEqual(self.task(ep,'experience-order',False,'--experience',store)['experience_records_used'],0)
            for other in ['grammar','indexed-grammar','cached-grammar']:
                mismatch=self.task(other,'experience-order',True,'--experience',store)
                self.assertEqual(mismatch['experience_records_used'],1 if other==ep else 0)
            old=self.task(ep,'experience-order',True,'--experience',store,cache='none')
            self.assertEqual(old['experience_records_used'],0)
            output=self.root/(ep+'-target');run=self.task(ep,'experience-order',True,'--experience',store,'--output',output)
            baseline=self.task(ep,'adaptive-order',True)
            self.assertEqual(run['engine'],'bruce-portfolio-v31')
            self.assertEqual(run['hypotheses'],baseline['hypotheses'])
            self.assertEqual(run['strategy_order'][0],'expression');self.assertEqual(len(run['strategy_runs']),5)
            self.call('experience','add','--from-run',output,'--output',self.root/'bad',ok=False)
            revoked=self.root/(ep+'-revoked');self.call('experience','revoke','--store',store,'--id',report['records'][0]['id'],'--reason','Q Quelle widerrufen','--output',revoked)
            fallback=self.task(ep,'experience-order',True,'--experience',revoked)
            self.assertEqual(fallback['experience_records_used'],0);self.assertEqual(fallback['strategy_order'],baseline['strategy_order'])
            rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot q-portfolio\n')
            selected=self.root/(ep+'-selected');selection=self.call('compatibility','--rules',rules,'--from-run',output,'--at',0,'--output',selected)
            self.assertEqual(selection['status'],'SELECTED');self.assertIn('portfolio-q-cached-v31',selection['candidates'][0]['domain'])
            self.call('compatibility','--rules',rules,'--from-run',output,'--at',-1,ok=False)
            source.unlink();store.unlink();revoked.unlink();rules.unlink()
            self.assertEqual(self.call('replay',output),run);output.unlink()
            self.assertEqual(self.call('replay',selected),selection)
    def test_complete_cross_family_merge(self):
        args=['research','--strategy','portfolio-expression','--operators','+/',
              '--min',0,'--max',1,'--max-power',1,'--max-degree',1,'--max-nodes',3,
              '--train',ROOT/'examples/q-domain-zero-training.csv','--holdout',ROOT/'examples/q-domain-zero-holdout.csv']
        old=self.call(*args,'--equivalence','rational')
        self.assertEqual(old['status'],'AMBIGUOUS')
        findings=[]
        for ep in ['grammar','indexed-grammar','cached-grammar']:
            for order in ['portfolio','adaptive-order']:
                path=self.root/(ep+order)
                new=self.call(*args,'--equivalence','rational-q-modular','--domain-cache','equal-64','--expression-planner',ep,'--planner',order,'--output',path)
                self.assertEqual(new['status'],'VERIFIED_ON_SAMPLES')
                self.assertEqual(len(new['hypotheses']),1)
                self.assertGreater(new['union_domain_cache_hits'],0)
                self.assertEqual(new['stored_source_hypotheses'],old['stored_source_hypotheses'])
                self.assertGreater(new['domain_comparisons'],0)
                self.assertEqual(self.call('replay',path),new)
                oldorigins=sorted((o['strategy'],o['id'],o['status']) for h in old['hypotheses'] for o in h['origins'])
                neworigins=sorted((o['strategy'],o['id'],o['status']) for h in new['hypotheses'] for o in h['origins'])
                self.assertEqual(oldorigins,neworigins);findings.append(new['hypotheses'])
        self.assertTrue(all(h==findings[0] for h in findings))
    def test_mixed_store_keeps_old_records_and_rejects_downgrade(self):
        legacy=self.root/'legacy';self.task('cached-grammar','portfolio',True,'--output',legacy,cache='none')
        first=self.root/'first';old=self.call('experience','add','--from-run',legacy,'--output',first)
        source=self.root/'source';self.task('cached-grammar','portfolio',False,'--output',source)
        mixed=self.root/'mixed';new=self.call('experience','add','--store',first,'--from-run',source,'--output',mixed)
        self.assertEqual(new['engine'],'bruce-experience-v10');self.assertEqual(new['records'][0],old['records'][0])
        self.assertEqual(self.call('experience','inspect',mixed),new)
        self.assertEqual(self.task('cached-grammar','experience-order',True,'--experience',mixed)['experience_records_used'],1)
        self.assertEqual(self.task('cached-grammar','experience-order',False,'--experience',mixed,cache='none')['experience_records_used'],1)
        duplicate=self.root/'duplicate';self.task('indexed-grammar','portfolio',False,'--output',duplicate)
        failure=self.call('experience','add','--store',mixed,'--from-run',duplicate,'--output',self.root/'bad-dup',ok=False)
        self.assertEqual(failure['error'],'DuplicateExperienceTrainingInput')
        _,_,body=mixed.read_text().split('\n',2);body=body.replace('policy routing-experience-v10','policy routing-experience-v9',1)
        mixed.write_text('BRUCE_EXPERIENCE_V9\n'+hashlib.sha256(body.encode()).hexdigest()+'\n'+body)
        self.assertEqual(self.call('experience','inspect',mixed,ok=False)['error'],'NoncanonicalExperienceSnapshot')

    def test_operator_subsets_and_literal_powers(self):
        for ep in ['grammar','indexed-grammar','cached-grammar']:
            for mask in range(1,32):
                ops=''.join(op for i,op in enumerate('+-*/^') if mask & (1<<i))
                args=['research','--strategy','portfolio-expression','--equivalence','rational-q-modular','--domain-cache','equal-64',
                      '--expression-planner',ep,'--operators',ops,'--min',1,'--max',3,
                      '--max-nodes',5,'--max-power',2,'--max-degree',2,
                      '--train',ROOT/'examples/expression-power-training.csv',
                      '--holdout',ROOT/'examples/expression-power-holdout.csv']
                fixed=self.call(*args)
                output=self.root/'ops.bruce'
                adaptive=self.call(*args,'--planner','adaptive-order','--output',output)
                self.assertEqual(fixed['hypotheses'],adaptive['hypotheses'])
                self.assertNotEqual(fixed['status'],'INCOMPLETE')
                self.assertEqual(self.call('replay',output),adaptive);output.unlink()
                expr=next(c for c in fixed['strategy_runs'] if c['strategy']=='expression')
                self.assertEqual(expr['equivalence'],'rational-q-modular-domain-v1')
                self.assertEqual(expr['operators'],ops)
                if ops=='+^':
                    self.assertTrue(any('^' in o['formula'] for h in expr['hypotheses'] for o in h['expression_origins']))

    def test_cubic_cache_reuses_proofs_with_same_originals(self):
        for ep in ['grammar','indexed-grammar','cached-grammar']:
            for order in ['portfolio','adaptive-order']:
                args=['research','--strategy','portfolio-expression','--equivalence','rational-q-modular',
                      '--expression-planner',ep,'--planner',order,'--operators','-^/','--min',3,'--max',3,
                      '--max-nodes',9,'--max-power',3,'--max-degree',3,'--max-operations',10000000,
                      '--train',ROOT/'examples/q-domain-zero-training.csv','--holdout',ROOT/'examples/q-domain-zero-holdout.csv']
                old=self.call(*args);output=self.root/(ep+order)
                run=self.call(*args,'--domain-cache','equal-64','--output',output)
                self.assertEqual(run['hypotheses'],old['hypotheses'])
                self.assertEqual(run['status'],'INCOMPLETE');self.assertEqual(run['stored_source_hypotheses'],128)
                self.assertGreater(run['domain_cache_hits'],0)
                self.assertLess(run['operations_used'],old['operations_used'])
                self.assertEqual(self.call('replay',output),run)
                def sources(r):
                    expr=next(c for c in r['strategy_runs'] if c['strategy']=='expression')
                    return [(o['id'],o['formula'],o['status']) for h in expr['hypotheses'] for o in h['expression_origins']]
                self.assertEqual(sources(old),sources(run))

    def test_limits_and_metadata(self):
        source=self.root/'source';self.task('cached-grammar','portfolio',False,'--output',source)
        store=self.root/'store';self.call('experience','add','--from-run',source,'--output',store)
        for order in ['portfolio','adaptive-order','experience-order']:
            extra=['--experience',store] if order=='experience-order' else []
            for flag,value in [('--max-operations',1),('--max-candidates',1),('--max-results',1),('--max-trace',2),('--max-trace-bytes',256)]:
                path=self.root/(order+flag)
                run=self.task('cached-grammar',order,True,*extra,flag,value,'--output',path)
                self.assertEqual(run['status'],'INCOMPLETE');self.assertEqual(self.call('replay',path),run)
        original=source.read_text();header,_,body=original.split('\n',2)
        for old,new in [('q-domain-union-v1','normal-form-union-v1'),('rational-q-modular-domain-v1','rational-domain-v1'),
                        ('domain_cache_policy equal-64-key4096-v1','domain_cache_policy none'),('domain_cache_scope expression-and-union-separate-v1','domain_cache_scope shared'),('direct-map-64-size3-v1','none'),('rational-domain-gcd-modular-v1','unknown'),
                        ('"domain_comparisons":','"domain_comparisons":9')]:
            changed=body.replace(old,new,1);self.assertNotEqual(changed,body)
            source.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
            self.call('replay',source,ok=False)
if __name__=='__main__':unittest.main()
