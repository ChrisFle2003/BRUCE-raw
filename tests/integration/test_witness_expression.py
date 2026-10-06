import hashlib,json,re,subprocess,sys,tempfile,unittest
from fractions import Fraction
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve());ROOT=Path(__file__).resolve().parents[2]
class WitnessExpressionTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],text=True,capture_output=True,timeout=30)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def task(self,planner,mode,*extra):
        return self.call('research','--strategy','expression','--planner',planner,'--equivalence',mode,
            '--operators','-^/','--min',3,'--max',3,'--max-nodes',9,'--max-operations',10000000,
            '--train',ROOT/'examples/q-domain-zero-training.csv','--holdout',ROOT/'examples/q-domain-zero-holdout.csv',*extra)
    def test_real_witnesses_preserve_groups_and_originals(self):
        for planner in ['grammar','indexed-grammar','cached-grammar']:
            old=self.task(planner,'rational-q-modular');path=self.root/planner
            new=self.task(planner,'rational-q-witness','--output',path)
            self.assertEqual(new['engine'],'bruce-expression-v10')
            self.assertEqual(new['hypotheses'],old['hypotheses'])
            self.assertEqual(new['stored_source_hypotheses'],128)
            self.assertEqual(new['status'],'INCOMPLETE')
            self.assertFalse(new['general_proof'])
            self.assertEqual(self.call('replay',path),new)
            self.assertTrue(path.read_text().startswith('BRUCE_EXPRESSION_BUNDLE_V10\n'))
            path=self.root/(planner+'extended')
            new=self.call('research','--strategy','expression','--planner',planner,'--equivalence','rational-q-witness',
                '--operators','-^/','--min',3,'--max',3,'--max-nodes',9,'--max-results',256,'--max-operations',10000000,
                '--train',ROOT/'examples/witness-zero-training.csv','--holdout',ROOT/'examples/witness-zero-holdout.csv','--output',path)
            self.assertGreater(new['domain_bounded_witnesses'],0)
            self.assertGreater(new['domain_witness_candidates'],0)
            self.assertEqual(new['stored_source_hypotheses'],256)
            self.assertEqual(self.call('replay',path),new)
            rules=self.root/'incomplete-rules';rules.write_text('BRUCE_RULES_V1\nsnapshot witness\n')
            self.call('compatibility','--rules',rules,'--from-run',path,'--at',5,ok=False)
            original=path.read_text()
            header,_,body=original.split('\n',2)
            self.assertIn('domain_witness=0',body)
            body=body.replace('domain_witness=0','domain_witness=7',1)
            path.write_text(header+'\n'+hashlib.sha256(body.encode()).hexdigest()+'\n'+body)
            self.call('replay',path,ok=False)
            # Substitute into the stored original trees, independently of the
            # C++ normalizer; representative guards alone would miss origin bugs.
            originals={o['id']:o['expression'] for h in new['hypotheses'] for o in h['expression_origins']}
            def excluded(tree,x):
                values=[]
                try:
                    for node in tree:
                        op=node['op']
                        if op=='x':value=x
                        elif op=='c':value=Fraction(node['value'])
                        else:
                            a,b=values[node['left']],values[node['right']]
                            if op=='+':value=a+b
                            elif op=='-':value=a-b
                            elif op=='*':value=a*b
                            elif op=='/':value=a/b
                            else:
                                assert op=='^' and b.denominator==1
                                value=a**int(b)
                        values.append(value)
                except ZeroDivisionError:return True
                return False
            checked=0
            for event in new['trace']:
                m=re.search(r'EXPRESSION id=(\d+).*domain_witness=([^ ]+) witness_against=(\d+) excluded_side=(left|right)',event)
                if not m:continue
                ident,x,other,side=m.groups();x=Fraction(x)
                if int(ident) not in originals or int(other) not in originals:continue
                self.assertEqual((excluded(originals[int(ident)],x),excluded(originals[int(other)],x)),(side=='left',side=='right'))
                checked+=1
            self.assertGreater(checked,0)
    def test_full_research_selected_value_and_pole(self):
        for planner in ['grammar','indexed-grammar','cached-grammar']:
            source=self.root/planner;selected=self.root/(planner+'choice')
            run=self.call('research','--strategy','expression','--planner',planner,'--equivalence','rational-q-witness',
                '--min',1,'--max',1,'--max-nodes',7,'--train',ROOT/'examples/expression-training.csv',
                '--holdout',ROOT/'examples/expression-holdout.csv','--output',source)
            self.assertEqual(run['status'],'VERIFIED_ON_SAMPLES')
            self.assertEqual(self.call('replay',source),run)
            choice=self.call('compatibility','--rules',ROOT/'examples/quadratic-fraction-selection.rules',
                '--from-run',source,'--at',0,'--output',selected)
            self.assertEqual(choice['status'],'SELECTED')
            self.assertEqual(choice['candidates'][0]['value'],'0')
            self.call('compatibility','--rules',ROOT/'examples/quadratic-fraction-selection.rules',
                '--from-run',source,'--at',-1,ok=False)
            source.unlink();self.assertEqual(self.call('replay',selected),choice)

    def test_complete_selection_and_version_tampering(self):
        train=self.root/'train';train.write_text('x,y\n2,1\n3,1\n')
        hold=self.root/'hold';hold.write_text('x,y\n4,1\n')
        args=['research','--strategy','expression','--equivalence','rational-q-witness','--operators','/',
              '--min',1,'--max',1,'--max-nodes',3,'--train',train,'--holdout',hold]
        for planner in ['grammar','indexed-grammar','cached-grammar']:
            path=self.root/planner;run=self.call(*args,'--planner',planner,'--output',path)
            self.assertEqual(run['status'],'AMBIGUOUS');self.assertEqual(len(run['hypotheses']),2)
            self.assertEqual(run['domain_bounded_witnesses'],0)
            self.assertEqual(self.call('replay',path),run)
            rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot witness\n')
            selected=self.root/(planner+'selected')
            choice=self.call('compatibility','--rules',rules,'--from-run',path,'--at',2,'--output',selected)
            self.assertTrue(all('expression-q-witness-v10-' in c['domain'] for c in choice['candidates']))
            self.call('compatibility','--rules',rules,'--from-run',path,'--at',0,ok=False)
            original=path.read_text()
            for old,new in [('bruce-expression-v10','bruce-expression-v8'),
                            ('rational-domain-gcd-modular-witness16-v1','rational-domain-gcd-modular-v1'),
                            ('"domain_bounded_witnesses":0','"domain_bounded_witnesses":1')]:
                header,_,body=original.split('\n',2);self.assertIn(old,body);body=body.replace(old,new,1)
                path.write_text(header+'\n'+hashlib.sha256(body.encode()).hexdigest()+'\n'+body)
                self.call('replay',path,ok=False)
            path.unlink();self.assertEqual(self.call('replay',selected),choice)
        self.call(*args,'--domain-cache','equal-64',ok=False)
        self.call(*args,'--max-results',257,ok=False)
        self.call('research','--strategy','portfolio-expression','--equivalence','rational-q-witness','--domain-cache','equal-64','--train',train,ok=False)
        self.call('research','--strategy','polynomial','--equivalence','rational-q-witness','--train',train,ok=False)
if __name__=='__main__':unittest.main()
