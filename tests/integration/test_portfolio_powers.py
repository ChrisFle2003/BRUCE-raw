import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve());ROOT=Path(__file__).resolve().parents[2]
class PortfolioPowerTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=40)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def task(self,start=1,ops='+^',ep='indexed-grammar',*args):
        train=self.root/'train';hold=self.root/'hold'
        train.write_text('x,y\n'+''.join(f'{x},{x**3+x}\n' for x in range(start,start+4)))
        hold.write_text('x,y\n0,0\n-2,-10\n')
        return self.call('research','--strategy','portfolio-expression','--operators',ops,'--expression-planner',ep,
            '--min',1,'--max',3,'--max-power',2,'--max-degree',2,'--max-nodes',5,'--train',train,'--holdout',hold,*args)
    def test_orders_scope_and_replay(self):
        same=None
        for order,version in [('portfolio',17),('adaptive-order',18)]:
            for ep in ['grammar','indexed-grammar']:
                old=self.task(1,'+*',ep,'--planner',order)
                path=self.root/(ep+order);new=self.task(1,'^+',ep,'--planner',order,'--output',path)
                self.assertEqual(new['status'],'VERIFIED_ON_SAMPLES');self.assertEqual(new['engine'],f'bruce-portfolio-v{version}')
                self.assertEqual(new['expression_operators'],'+^');self.assertEqual(new['power_exponent_policy'],'literal-2-to-8-v1')
                self.assertEqual(len(new['strategy_runs']),5)
                self.assertEqual(new['strategy_runs'][:4],old['strategy_runs'][:4])
                self.assertEqual(new['strategy_runs'][4]['grammar_expressions'],130)
                self.assertEqual(new['strategy_runs'][4]['engine'],'bruce-expression-v5')
                self.assertEqual(new['hypotheses'][0]['normal_form']['numerator'],['0','1','0','1'])
                self.assertEqual({x['strategy'] for x in new['hypotheses'][0]['origins']},{'expression'})
                if same:self.assertEqual(new['hypotheses'],same['hypotheses'])
                same=new;self.assertEqual(self.call('replay',path),new)
    def test_experience_contexts_revoke_and_embedded_replay(self):
        store=None;first_id=None
        for start,ops,ep,order in [(1,'+^','indexed-grammar','portfolio'),(2,'+^','grammar','adaptive-order'),(3,'+*^','indexed-grammar','adaptive-order')]:
            source=self.root/f'source{start}';self.task(start,ops,ep,'--planner',order,'--output',source)
            nxt=self.root/f'store{start}';extra=['--store',store] if store else []
            report=self.call('experience','add',*extra,'--from-run',source,'--output',nxt)
            if first_id is None:first_id=report['records'][0]['id']
            self.assertEqual(report['records'][0]['id'],first_id);store=nxt
        self.assertEqual(report['engine'],'bruce-experience-v6')
        for ops,ep in [('+^','indexed-grammar'),('+^','grammar'),('+*^','indexed-grammar')]:
            target=self.task(7,ops,ep,'--planner','experience-order','--experience',store)
            self.assertEqual(target['experience_records_used'],1);self.assertEqual(target['experience_votes'],[0,0,0,0,1])
            self.assertEqual(target['strategy_order'][0],'expression');self.assertEqual(len(target['strategy_runs']),5)
            self.assertEqual(target['hypotheses'],self.task(7,ops,ep,'--planner','adaptive-order')['hypotheses'])
        self.assertEqual(self.task(1,'+^','indexed-grammar','--planner','experience-order','--experience',store)['experience_records_used'],0)
        self.assertEqual(self.task(7,'+*','indexed-grammar','--planner','experience-order','--experience',store)['experience_records_used'],0)
        duplicate=self.root/'duplicate';self.task(1,'+*^','indexed-grammar','--output',duplicate)
        self.call('experience','add','--store',store,'--from-run',duplicate,'--output',self.root/'baddup',ok=False)
        learned=self.root/'learned';out=self.task(7,'+^','indexed-grammar','--planner','experience-order','--experience',store,'--output',learned)
        self.assertEqual(out['engine'],'bruce-portfolio-v19')
        self.call('experience','add','--from-run',learned,'--output',self.root/'bad',ok=False)
        revoked=self.root/'revoked';self.call('experience','revoke','--store',store,'--id',first_id,'--reason','Quelle widerrufen','--output',revoked)
        fallback=self.task(7,'+^','indexed-grammar','--planner','experience-order','--experience',revoked)
        self.assertEqual(fallback['experience_records_used'],0)
        self.assertEqual(fallback['strategy_order'],self.task(7,'+^','indexed-grammar','--planner','adaptive-order')['strategy_order'])
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot power-portfolio\n')
        selection=self.root/'selection';projected=self.call('compatibility','--rules',rules,'--from-run',learned,'--at',-2,'--output',selection)
        self.assertEqual(projected['status'],'SELECTED');self.assertEqual(projected['candidates'][0]['value'],'-10')
        for p in self.root.iterdir():
            if p not in [learned,selection]:p.unlink()
        self.assertEqual(self.call('replay',learned),out);learned.unlink()
        self.assertEqual(self.call('replay',selection),projected)
    def test_limits_tampering_and_history(self):
        source=self.root/'source';self.task(1,'+^','indexed-grammar','--output',source)
        store=self.root/'store';self.call('experience','add','--from-run',source,'--output',store)
        for ep in ['grammar','indexed-grammar']:
            for order in ['portfolio','adaptive-order','experience-order']:
                extra=['--experience',store] if order=='experience-order' else []
                for flag,n in [('--max-operations',1),('--max-candidates',1),('--max-trace',2),('--max-trace-bytes',256)]:
                    path=self.root/(ep+order+flag);out=self.task(2,'+^',ep,'--planner',order,*extra,flag,n,'--output',path)
                    self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',path),out)
        header,_,body=source.read_text().split('\n',2)
        for before,after in [('expression_operators +^','expression_operators ^+'),('expression_operators +^','expression_operators +*'),
                ('literal-2-to-8-v1','literal-2-to-9-v1'),('block-index-v1','invalid-index'),('"grammar_expressions":130','"grammar_expressions":129')]:
            changed=body.replace(before,after);self.assertNotEqual(changed,body)
            source.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed);self.call('replay',source,ok=False)
        for name in ['expression-v5.bruce','portfolio-v16.bruce']:
            path=ROOT/'tests/fixtures'/name
            self.assertEqual(self.call('replay',path),json.loads(path.read_text().rsplit('RESULT\n',1)[1]))
        old=ROOT/'tests/fixtures/portfolio-v16.bruce';header,_,body=old.read_text().split('\n',2)
        changed=body.replace('expression_operators +*','expression_operators +^')
        source.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed);self.call('replay',source,ok=False)
    def test_mixed_legacy_store_poles_and_result_limit(self):
        train=self.root/'old-train';hold=self.root/'old-hold'
        train.write_text('x,y\n1,2\n2,6\n3,12\n4,20\n');hold.write_text('x,y\n0,0\n-2,2\n')
        old=self.root/'old';self.call('research','--strategy','portfolio-expression','--operators','+*',
            '--min',1,'--max',1,'--max-power',2,'--max-degree',2,'--max-nodes',5,'--train',train,'--holdout',hold,'--output',old)
        store=self.root/'old-store';before=self.call('experience','add','--from-run',old,'--output',store)
        self.assertEqual(before['engine'],'bruce-experience-v5')
        source=self.root/'new';self.task(1,'+^','indexed-grammar','--output',source)
        mixed=self.root/'mixed';after=self.call('experience','add','--store',store,'--from-run',source,'--output',mixed)
        self.assertEqual(after['engine'],'bruce-experience-v6');self.assertEqual(after['records'][0],before['records'][0])
        self.assertEqual(self.task(7,'+^','indexed-grammar','--planner','experience-order','--experience',store)['experience_records_used'],0)
        self.assertEqual(self.task(7,'+^','indexed-grammar','--planner','experience-order','--experience',mixed)['experience_records_used'],1)
        # A rehashed downgrade cannot put power evidence into a V5 snapshot.
        header,_,body=mixed.read_text().split('\n',2);body=body.replace('policy routing-experience-v6','policy routing-experience-v5',1)
        mixed.write_text('BRUCE_EXPERIENCE_V5\n'+hashlib.sha256(body.encode()).hexdigest()+'\n'+body)
        self.assertEqual(self.call('experience','inspect',mixed,ok=False)['error'],'NoncanonicalExperienceSnapshot')
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot powered-poles\n')
        train.write_text('x,y\n2,1/4\n3,1/9\n4,1/16\n5,1/25\n');hold.write_text('x,y\n-1,1\n')
        for ep in ['grammar','indexed-grammar']:
            path=self.root/ep
            args=['research','--strategy','portfolio-expression','--operators','/^','--expression-planner',ep,
                '--min',1,'--max',2,'--max-nodes',5,'--max-degree',2,'--max-power',2,'--train',train,'--holdout',hold]
            run=self.call(*args,'--output',path)
            self.assertEqual(run['status'],'VERIFIED_ON_SAMPLES');self.assertEqual(run['hypotheses'][0]['normal_form']['excluded_polynomial'],['0','1'])
            self.assertEqual(self.call('replay',path),run)
            self.call('compatibility','--rules',rules,'--from-run',path,'--at',0,ok=False)
            self.assertEqual(self.call('compatibility','--rules',rules,'--from-run',path,'--at',-1)['candidates'][0]['value'],'1')
            limited=self.root/(ep+'limited');out=self.call(*args,'--max-results',1,'--output',limited)
            self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',limited),out)
            self.call('compatibility','--rules',rules,'--from-run',limited,'--at',-1,ok=False)
if __name__=='__main__':unittest.main()
