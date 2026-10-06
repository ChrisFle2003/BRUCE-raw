import hashlib
import json
import random
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BIN = str(Path(sys.argv.pop(1)).resolve())
RULES = '''BRUCE_RULES_V1
snapshot demo-v1
rule even positive value mod_eq 2 0 5 manual -
rule large reject value gt 100 0 0 manual -
rule known_bad negative value eq 6 0 0 manual counterexample-6
rule context context x le 7 0 0 manual -
'''


class CompatibilityTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.rules = self.root/'rules.txt'
        self.rules.write_text(RULES)
        self.candidates = self.root/'candidates.csv'

    def tearDown(self):
        self.tmp.cleanup()

    def write_candidates(self, rows):
        self.candidates.write_text('id,domain,layer,x,y,value\n'+''.join(','.join(map(str,r))+'\n' for r in rows))

    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=10)
        if ok:
            self.assertEqual(p.returncode,0,p.stderr)
            return json.loads(p.stdout)
        self.assertEqual(p.returncode,2,p.stderr)
        return json.loads(p.stderr)['error']

    def run_selection(self,*args):
        return self.call('compatibility','--rules',self.rules,'--candidates',self.candidates,*args)

    def test_separate_masks_hard_reject_and_ambiguity(self):
        self.write_candidates([(9,'math',0,2,0,4),(3,'math',0,1,0,2),(5,'math',0,3,0,6),
                               (7,'math',0,4,0,102),(11,'math',0,8,0,8)])
        out=self.run_selection()
        self.assertEqual(out['status'],'AMBIGUOUS')
        self.assertEqual(out['selected_ids'],['3','9'])
        group=out['groups'][0]
        self.assertEqual(group['candidate_ids'],['3','5','7','9','11'])
        self.assertEqual(int(group['positive_match_mask'],16),31)
        self.assertEqual(int(group['hard_reject_mask'],16),4)
        self.assertEqual(int(group['negative_evidence_mask'],16),2)
        self.assertEqual(int(group['context_role_mask'],16),15)
        self.assertEqual(int(group['eligible_mask'],16),9)
        self.assertEqual(len(out['trace']),20)
        self.assertEqual(out['rules'][2]['evidence_ref'],'counterexample-6')
        self.assertEqual(out['rules'][0]['operator'],'mod_eq')
        self.assertFalse(out['general_proof'])

    def test_more_than_64_and_stable_order(self):
        rows=[(i,'math',0,i%8,i//8,2) for i in range(130)]
        self.write_candidates(rows)
        a=self.run_selection()
        self.write_candidates(reversed(rows))
        self.assertEqual(a,self.run_selection())
        self.assertEqual([len(g['candidate_ids']) for g in a['groups']],[64,64,2])
        self.assertEqual([int(g['eligible_mask'],16) for g in a['groups']],[(1<<64)-1,(1<<64)-1,3])
        self.assertEqual(len(a['selected_ids']),130)

    def test_snapshot_replay_without_original_files(self):
        self.write_candidates([(1,'math',0,0,0,2),(2,'math',0,1,0,3)])
        export=self.root/'run.bruce'
        old=self.run_selection('--output',export)
        self.rules.write_text(RULES.replace('mod_eq 2 0','mod_eq 2 1'))
        new=self.run_selection()
        self.assertNotEqual(old['snapshot_digest'],new['snapshot_digest'])
        self.assertEqual(old['selected_ids'],['1'])
        self.assertEqual(new['selected_ids'],['2'])
        self.rules.unlink(); self.candidates.unlink()
        self.assertEqual(old,self.call('replay',export))

    def test_budget_never_publishes_partial_winner(self):
        self.write_candidates([(1,'math',0,0,0,2),(2,'math',0,1,0,4)])
        for flag,value,reason in [('--max-candidates',1,'CandidateBudgetExceeded'),
                                  ('--max-checks',1,'OperationBudgetExceeded'),
                                  ('--max-trace',1,'TraceBudgetExceeded')]:
            export=self.root/(flag+'.bruce')
            result=self.run_selection(flag,value,'--output',export)
            self.assertEqual(result['status'],'INCOMPLETE')
            self.assertEqual(result['stop_reason'],reason)
            self.assertEqual(result['selected_ids'],[])
            self.assertEqual(self.call('replay',export),result)

    def test_invalid_snapshot_and_context(self):
        self.write_candidates([(1,'math',0,0,0,2)])
        for bad in [RULES.replace('V1','V9'),RULES.replace('mod_eq 2 0','mod_eq 0 0'),
                    RULES.replace('counterexample-6','-'),RULES.replace('value gt','unknown gt'),
                    RULES.replace('manual','external'),RULES+'rule even positive value eq 2 0 1 manual -\n']:
            self.rules.write_text(bad)
            self.call('compatibility','--rules',self.rules,'--candidates',self.candidates,ok=False)
        self.rules.write_text(RULES)
        for rows in [[(1,'math',0,0,0,2),(1,'math',0,1,0,4)],
                     [(1,'math',0,0,0,2),(2,'other',0,1,0,4)],
                     [(1,'math',0,0,0,2),(2,'math',1,1,0,4)]]:
            self.write_candidates(rows)
            self.call('compatibility','--rules',self.rules,'--candidates',self.candidates,ok=False)

    def test_rehashed_result_and_inner_snapshot_tamper(self):
        self.write_candidates([(1,'math',0,0,0,2)])
        export=self.root/'run.bruce'
        self.run_selection('--output',export)
        header,_,payload=export.read_text().split('\n',2)
        for changed in [payload.replace('"general_proof":false','"general_proof":true'),
                        payload.replace('mod_eq 2 0','mod_eq 2 1'),
                        payload.replace('z-compatibility-v1','z-compatibility-v9')]:
            export.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
            self.call('replay',export,ok=False)

    def test_consumes_real_research_and_preserves_provenance(self):
        train=self.root/'train.csv'; train.write_text('x,y\n0,0\n')
        source=self.root/'research.bruce'
        original=self.call('research','--train',train,'--min',0,'--max',2,'--output',source)
        self.assertEqual(original['status'],'AMBIGUOUS')
        self.rules.write_text('BRUCE_RULES_V1\nsnapshot prefer-v1\nrule prefer positive value eq 2 0 9 manual -\n')
        export=self.root/'selection.bruce'
        selected=self.call('compatibility','--rules',self.rules,'--from-run',source,'--at',1,'--output',export)
        self.assertEqual(selected['status'],'SELECTED')
        self.assertEqual(selected['origin_status'],'AMBIGUOUS')
        self.assertEqual(selected['candidates'][2]['value'],'2')
        self.assertEqual(selected['selected_ids'],['6'])
        self.assertFalse(selected['general_proof'])
        source.unlink(); train.unlink(); self.rules.unlink()
        self.assertEqual(selected,self.call('replay',export))

    def test_negative_modulo_and_integer_boundary(self):
        self.rules.write_text('BRUCE_RULES_V1\nsnapshot negative-v1\nrule odd positive value mod_eq 2 1 10 manual -\n')
        self.write_candidates([(1,'math',0,0,0,-9223372036854775808),(2,'math',0,0,1,-3)])
        out=self.run_selection()
        self.assertEqual(out['selected_ids'],['2'])
        self.assertEqual(out['candidates'][0]['value'],'-9223372036854775808')

    def test_predicates_against_independent_integer_oracle(self):
        rng=random.Random(991)
        for case in range(12):
            rules=[]
            text='BRUCE_RULES_V1\nsnapshot generated-v1\n'
            for index in range(12):
                field=rng.choice(['x','y','z','value','layer'])
                op=rng.choice(['eq','ne','lt','le','gt','ge','mod_eq'])
                kind=rng.choice(['positive','positive','reject','negative','context'])
                arg=rng.randint(1,9) if op=='mod_eq' else rng.randint(-20,20)
                remainder=rng.randrange(arg) if op=='mod_eq' else 0
                weight=rng.randint(-10,10) if kind=='positive' else 0
                rules.append((kind,field,op,arg,remainder,weight))
                evidence=f'negative-{index}' if kind=='negative' else '-'
                text+=f'rule r{index} {kind} {field} {op} {arg} {remainder} {weight} manual {evidence}\n'
            self.rules.write_text(text)
            layer=(1<<64)-1 if case%2 else 0
            rows=[(i,'generated',layer,rng.randrange(16),rng.randrange(16),rng.randint(-40,40)) for i in range(70)]
            self.write_candidates(rows)
            result=self.run_selection()
            eligible=[]
            for row,actual in zip(rows,result['candidates']):
                i,_,layer,x,y,value=row
                fields={'x':x,'y':y,'z':(x+y)*(x+y+1)//2+y,'value':value,'layer':layer}
                score=0; blocked=False
                for kind,field,op,arg,remainder,weight in rules:
                    v=fields[field]
                    matched={'eq':v==arg,'ne':v!=arg,'lt':v<arg,'le':v<=arg,'gt':v>arg,'ge':v>=arg}.get(op)
                    if op=='mod_eq': matched=v%arg==remainder
                    if kind=='positive' and matched: score+=weight
                    if kind in ('reject','negative') and matched: blocked=True
                    if kind=='context' and not matched: blocked=True
                self.assertEqual(actual['score'],score)
                if not blocked: eligible.append((i,score))
            maximum=max((score for _,score in eligible),default=None)
            expected=[str(i) for i,score in eligible if score==maximum]
            self.assertEqual(result['selected_ids'],expected)

    def test_empty_rules_and_all_excluded(self):
        self.write_candidates([(1,'math',0,0,0,2),(2,'math',0,1,0,4)])
        self.rules.write_text('BRUCE_RULES_V1\nsnapshot none-v1\n')
        out=self.run_selection()
        self.assertEqual(out['selected_ids'],['1','2'])
        self.assertEqual(out['checks_used'],0)
        self.rules.write_text('BRUCE_RULES_V1\nsnapshot reject-v1\nrule all reject value ge 0 0 0 manual -\n')
        self.assertEqual(self.run_selection()['status'],'NO_COMPATIBLE_CANDIDATE')

    def test_unsigned_z_and_exact_budget_boundaries(self):
        address=self.call('coordinate','--z',(1<<64)-1)
        self.rules.write_text('BRUCE_RULES_V1\nsnapshot boundary-v1\nrule high positive z gt 9223372036854775807 0 10 manual -\n')
        self.write_candidates([(1,'math',0,int(address['x']),int(address['y']),0),(2,'math',0,0,0,0)])
        exact=self.run_selection('--max-candidates',2,'--max-checks',2,'--max-trace',2)
        self.assertEqual(exact['status'],'SELECTED')
        self.assertEqual(exact['selected_ids'],['1'])
        self.assertEqual(exact['candidates'][0]['z'],str((1<<64)-1))
        for flag in ('--max-candidates','--max-checks','--max-trace'):
            self.assertEqual(self.run_selection(flag,1)['status'],'INCOMPLETE')

    def test_frozen_core_v1_export_remains_compatible(self):
        fixture=Path(__file__).resolve().parents[1]/'fixtures/research-v1.bruce'
        expected=json.loads(fixture.read_text().split('RESULT\n',1)[1])
        self.assertEqual(expected['engine'],'bruce-core-v1')
        self.assertEqual(self.call('replay',fixture),expected)


if __name__=='__main__':
    unittest.main()
