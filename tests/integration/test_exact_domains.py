from fractions import Fraction
import hashlib
import json
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest

BIN=str(Path(sys.argv.pop(1)).resolve())


class ExactDomainsTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(); self.root=Path(self.tmp.name)

    def tearDown(self): self.tmp.cleanup()

    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=15)
        if ok:
            self.assertEqual(p.returncode,0,p.stderr)
            return json.loads(p.stdout)
        self.assertEqual(p.returncode,2,p.stderr)
        return json.loads(p.stderr)['error']

    def pairs(self,name,rows):
        p=self.root/name
        p.write_text('x,y\n'+''.join(f'{x},{y}\n' for x,y in rows))
        return p

    def calc(self,op,a,b,domain='bigint',*extra):
        return self.call('calculate','--domain',domain,'--op',op,'--left',a,'--right',b,*extra)

    def test_big_integer_and_exact_fraction(self):
        n=10**100+123
        self.assertEqual(self.calc('mul',n,n)['value'],str(n*n))
        self.assertEqual(self.calc('add','1/3','1/6','rational')['value'],'1/2')
        self.assertEqual(self.calc('div','-2/3','4/9','rational')['value'],'-3/2')
        self.assertEqual(self.calc('pow',2,256)['value'],str(2**256))
        self.assertEqual(self.calc('pow','-2/3',5,'rational')['value'],'-32/243')

    def test_integer_remainder_and_zero(self):
        for a,b in [(-7,3),(-7,-3),(7,-3),(10**100+1,17)]:
            out=self.calc('divmod',a,b)
            self.assertEqual(int(out['quotient'])*b+int(out['remainder']),a)
            self.assertTrue(0<=int(out['remainder'])<abs(b))
        self.assertIn('NonIntegralDivision',self.call('calculate','--op','div','--left',7,'--right',3,ok=False))
        for value in ('1/0','2/0','1//2','1.5','1e9','+3'):
            self.call('calculate','--domain','rational','--op','add','--left',value,'--right',0,ok=False)
        self.call('calculate','--op','divmod','--left',1,'--right',0,ok=False)

    def test_independent_arithmetic_oracle(self):
        rng=random.Random(887)
        for _ in range(20):
            a=Fraction(rng.randrange(-10**70,10**70),rng.randrange(1,10**30))
            b=Fraction(rng.randrange(1,10**70),rng.randrange(1,10**30))
            for op,expected in [('add',a+b),('sub',a-b),('mul',a*b),('div',a/b)]:
                self.assertEqual(self.calc(op,a,b,'rational')['value'],str(expected))
        n=10**70+57; modulus=10**60+7
        out=self.call('calculate','--op','modpow','--left',n,'--right',12345,'--modulus',modulus)
        self.assertEqual(out['value'],str(pow(n,12345,modulus)))

    def test_bigint_research_new_process_replay(self):
        n=10**100
        train=self.pairs('train.csv',[(n,2*n+3),(n+1,2*(n+1)+3)])
        hold=self.pairs('hold.csv',[(n+9,2*(n+9)+3)])
        export=self.root/'run.bruce'
        out=self.call('research','--domain','bigint','--train',train,'--holdout',hold,'--output',export)
        self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
        self.assertEqual([(h['a'],h['b']) for h in out['hypotheses']],[('2','3')])
        self.assertFalse(out['general_proof'])
        train.unlink(); hold.unlink()
        self.assertEqual(self.call('replay',export),out)
        header,_,payload=export.read_text().split('\n',2)
        payload=payload.replace('"general_proof":false','"general_proof":true')
        export.write_text(header+'\n'+hashlib.sha256(payload.encode()).hexdigest()+'\n'+payload)
        self.call('replay',export,ok=False)

    def test_rational_research_and_counterexample(self):
        a,b=Fraction(1,2),Fraction(1,3)
        train=self.pairs('train.csv',[(x,a*x+b) for x in (Fraction(0),Fraction(2),Fraction(7,3))])
        hold=self.pairs('hold.csv',[(Fraction(1,7),a*Fraction(1,7)+b)])
        export=self.root/'rational.bruce'
        out=self.call('research','--domain','rational','--train',train,'--holdout',hold,'--min',-2,'--max',2,'--max-denominator',3,'--output',export)
        self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
        self.assertEqual([(h['a'],h['b']) for h in out['hypotheses']],[('1/2','1/3')])
        self.assertEqual(self.call('replay',export),out)
        hold=self.pairs('hold.csv',[(Fraction(1,7),99)])
        bad=self.call('research','--domain','rational','--train',train,'--holdout',hold,'--min',-2,'--max',2,'--max-denominator',3)
        self.assertEqual(bad['status'],'REFUTED')

    def test_large_coefficients_and_canonical_duplicate(self):
        n=10**50
        train=self.pairs('large.csv',[(0,n+1),(1,2*n+1)])
        out=self.call('research','--domain','bigint','--train',train,'--min',n,'--max',n+2)
        self.assertEqual([(h['a'],h['b']) for h in out['hypotheses']],[(str(n),str(n+1))])
        train=self.pairs('dupe.csv',[('1/2',1),('2/4',2)])
        self.call('research','--domain','rational','--train',train,ok=False)

    def test_bit_and_trace_limits_are_incomplete_not_refutation(self):
        self.call('calculate','--op','pow','--left',2,'--right',1000000000,'--max-bits',64,ok=False)
        train=self.pairs('train.csv',[(255,255)])
        export=self.root/'limit.bruce'
        out=self.call('research','--domain','bigint','--train',train,'--min',2,'--max',3,'--max-bits',8,'--output',export)
        self.assertEqual((out['status'],out['stop_reason']),('INCOMPLETE','IntegerBitLimit'))
        self.assertEqual(self.call('replay',export),out)
        out=self.call('research','--domain','bigint','--train',train,'--max-trace-bytes',256)
        self.assertEqual((out['status'],out['stop_reason']),('INCOMPLETE','TraceByteBudgetExceeded'))

    def test_compatibility_consumes_new_export_without_narrowing(self):
        n=10**80
        train=self.pairs('train.csv',[(n,2*n+3),(n+1,2*(n+1)+3)])
        export=self.root/'big.bruce'
        self.call('research','--domain','bigint','--train',train,'--output',export)
        rules=self.root/'rules'; rules.write_text('BRUCE_RULES_V1\nsnapshot test\nrule p positive value eq 23 0 5 manual -\n')
        selected=self.call('compatibility','--rules',rules,'--from-run',export,'--at',10)
        self.assertEqual(selected['status'],'SELECTED')
        self.assertEqual(selected['candidates'][0]['value'],'23')
        self.call('compatibility','--rules',rules,'--from-run',export,'--at',9223372036854775807,ok=False)

    def test_historical_z_replay_and_fractional_projection(self):
        fixture=Path(__file__).resolve().parents[1]/'fixtures/compatibility-v1.bruce'
        expected=json.loads(fixture.read_text().split('RESULT\n',1)[1])
        self.assertEqual(self.call('replay',fixture),expected)
        train=self.pairs('fraction.csv',[(0,0),(2,1)])
        export=self.root/'fraction.bruce'
        self.call('research','--domain','rational','--train',train,'--min',0,'--max',1,'--max-denominator',2,'--output',export)
        rules=self.root/'rules'; rules.write_text('BRUCE_RULES_V1\nsnapshot fraction\n')
        self.assertIn('fractional value',self.call('compatibility','--rules',rules,'--from-run',export,'--at',1,ok=False))
        selection=self.root/'selection.bruce'
        out=self.call('compatibility','--rules',rules,'--from-run',export,'--at',2,'--output',selection)
        export.unlink(); rules.unlink(); train.unlink()
        self.assertEqual(self.call('replay',selection),out)


if __name__=='__main__': unittest.main()
