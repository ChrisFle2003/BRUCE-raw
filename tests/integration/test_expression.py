from fractions import Fraction
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from functools import lru_cache
BIN=str(Path(sys.argv.pop(1)).resolve())
class ExpressionTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=40)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def pairs(self,name,rows):
        p=self.root/name;p.write_text('x,y\n'+''.join(f'{x},{y}\n' for x,y in rows));return p
    def research(self,rows,*opts):
        return self.call('research','--strategy','expression','--min',1,'--max',1,
            '--train',self.pairs('train',rows),*opts)
    def test_nested_expression_holdout_projection_replay(self):
        origin=self.root/'run'
        out=self.research([(x,Fraction(x*x,x+1)) for x in [1,2,3,4]],
            '--max-nodes',7,'--holdout',self.pairs('hold',[(0,0),(5,'25/6')]),'--output',origin)
        self.assertEqual(out['status'],'AMBIGUOUS')
        verified=[h for h in out['hypotheses'] if h['status']=='VERIFIED_ON_SAMPLES']
        self.assertEqual(len(verified),3)
        self.assertIn('((x*x)/(x+1))',{h['formula'] for h in verified})
        self.assertTrue(all(h['domain_conditions']==['(x+1) != 0'] for h in verified))
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot grammar\n')
        selected=self.root/'selected'
        result=self.call('compatibility','--rules',rules,'--from-run',origin,'--at',0,'--output',selected)
        self.assertEqual(result['candidates'][0]['value'],'0')
        self.call('compatibility','--rules',rules,'--from-run',origin,'--at',-1,ok=False)
        (self.root/'train').unlink();(self.root/'hold').unlink()
        self.assertEqual(self.call('replay',origin),out)
        origin.unlink();rules.unlink();self.assertEqual(self.call('replay',selected),result)
    def test_holes_and_no_invented_simplification(self):
        out=self.research([(2,1),(3,1)],'--max-nodes',3,'--holdout',self.pairs('hold',[(0,1)]))
        hole=next(h for h in out['hypotheses'] if h['formula']=='(x/x)')
        self.assertEqual(hole['status'],'REFUTED');self.assertEqual(hole['domain_conditions'],['x != 0'])
        self.assertTrue(any(h['formula']=='1' and h['status']=='VERIFIED_ON_SAMPLES' for h in out['hypotheses']))
    def test_limits_and_version_integrity(self):
        for flag,n in [('--max-candidates',1),('--max-operations',1),('--max-trace',2),('--max-trace-bytes',256),('--max-results',1)]:
            origin=self.root/flag[2:]
            out=self.research([(2,1)],'--max-nodes',5,flag,n,'--output',origin)
            self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',origin),out)
        for opts in [('--domain','bigint'),('--planner','adaptive'),('--max-nodes',10),('--max-nodes',0),('--max-nodes',4),('--max-power',2),('--max-degree',2)]:
            self.call('research','--strategy','expression','--train',self.root/'train',*opts,ok=False)
        p=self.root/'run';self.research([(2,9)],'--max-nodes',3,'--output',p)
        header,_,body=p.read_text().split('\n',2)
        changed=body.replace('binary-arithmetic-v1','unknown-grammar-v1')
        self.assertNotEqual(changed,body);p.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
        self.call('replay',p,ok=False)
        fixture=Path(__file__).resolve().parents[1]/'fixtures/portfolio-v6.bruce'
        self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().split('RESULT\n',1)[1]))
    def test_independent_ordered_grammar_fraction_oracle(self):
        # Generate every ordered tree independently, then normalize only commutation.
        def normalized(t):
            if t[0] in ['x','c']:return t
            left,right=normalized(t[1]),normalized(t[2])
            if t[0] in ['+','*'] and repr(left)>repr(right):left,right=right,left
            return (t[0],left,right)
        @lru_cache(None)
        def trees(n):
            if n==1:return [('x',),('c','1')]
            return [(op,a,b) for k in range(1,n-1,2) for a in trees(k) for b in trees(n-1-k) for op in '+-*/']
        def value(t,x):
            if t[0]=='x':return Fraction(x)
            if t[0]=='c':return Fraction(t[1])
            a,b=value(t[1],x),value(t[2],x)
            return a+b if t[0]=='+' else a-b if t[0]=='-' else a*b if t[0]=='*' else a/b
        def decode(nodes):
            built=[]
            for n in nodes:
                built.append(('x',) if n['op']=='x' else ('c',n['value']) if n['op']=='c' else (n['op'],built[n['left']],built[n['right']]))
            return normalized(built[-1])
        universe={normalized(t) for n in [1,3,5] for t in trees(n)}
        for rows,hold in [([(2,2)],[]), ([(0,0)],[(2,2)]), ([(2,1),(3,1)],[(0,1)])]:
            expected={}
            for t in universe:
                try:
                    if not all(value(t,x)==y for x,y in rows):continue
                except ZeroDivisionError:continue
                status='CANDIDATE'
                if hold:
                    try:status='VERIFIED_ON_SAMPLES' if all(value(t,x)==y for x,y in hold) else 'REFUTED'
                    except ZeroDivisionError:status='REFUTED'
                expected[t]=status
            opts=['--holdout',self.pairs('hold',hold)] if hold else []
            out=self.research(rows,'--max-nodes',5,*opts)
            self.assertNotEqual(out['status'],'INCOMPLETE')
            self.assertEqual(out['grammar_expressions'],len(universe))
            self.assertEqual({decode(h['expression']):h['status'] for h in out['hypotheses']},expected)
            self.assertEqual(len(out['hypotheses']),len(expected))
    def test_fraction_literals_bits_and_family_limits(self):
        train=self.pairs('fraction',[(2,'1/2'),(3,'1/3')])
        out=self.call('research','--strategy','expression','--train',train,'--min',1,'--max',1,
            '--max-denominator',2,'--max-nodes',3)
        self.assertTrue(any(h['formula']=='(1/x)' for h in out['hypotheses']))
        constant=self.call('research','--strategy','expression','--train',self.pairs('constant',[(2,'1/2')]),
            '--min',1,'--max',1,'--max-denominator',2,'--max-nodes',1)
        self.assertEqual(constant['hypotheses'][0]['formula'],'(1/2)')
        bit=self.research([(15,1)],'--max-nodes',3,'--max-bits',4,'--output',self.root/'bits')
        self.assertEqual((bit['status'],bit['stop_reason']),('INCOMPLETE','IntegerBitLimit'))
        self.assertEqual(self.call('replay',self.root/'bits'),bit)
        self.call('research','--strategy','expression','--train',train,'--min',0,'--max',64,ok=False)
        self.call('research','--strategy','rational-quadratic','--train',train,'--max-nodes',3,ok=False)
if __name__=='__main__':unittest.main()
