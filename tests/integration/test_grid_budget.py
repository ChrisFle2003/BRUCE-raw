import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve())
class GridBudgetTests(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
 def tearDown(self):self.tmp.cleanup()
 def call(self,*args,ok=True):
  p=subprocess.run([BIN,*map(str,args)],capture_output=True,text=True,timeout=20)
  self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
  return json.loads(p.stdout if ok else p.stderr)
 def sources(self,axis='x'):
  t=self.root/'train';h=self.root/'hold'
  self.call('grid-batch','--paths','0,2;1,2;2,2' if axis=='x' else '1,0;1,1;1,2','--output',t)
  self.call('grid-batch','--paths','3,2;4,2' if axis=='x' else '1,3;1,4','--output',h)
  return t,h,t.stat().st_size+h.stat().st_size
 def args(self,t,h,axis='x'):
  return ['research','--strategy','polynomial','--domain','rational','--max-degree',2,'--grid-axis',axis,'--grid-train',t,'--grid-holdout',h]
 def test_exact_total_thresholds_and_replay(self):
  t,h,cost=self.sources();args=self.args(t,h)
  plain=self.call(*args);work=plain['research']['operations_used']
  self.assertGreater(work,1)
  for remaining in (1,work-1,work,work+1):
   p=self.root/f'run-{remaining}';out=self.call(*args,'--max-total-operations',cost+remaining,'--output',p)
   budget=out['work_budget'];self.assertEqual(budget['source_operations'],cost)
   self.assertEqual(budget['research_limit'],remaining)
   self.assertEqual(budget['used'],cost+out['research']['operations_used']);self.assertLessEqual(budget['used'],budget['limit'])
   self.assertEqual(out['status'],'INCOMPLETE' if remaining<work else plain['status'])
   self.assertEqual(self.call('replay',p),out)
  for n in (0,cost-1,cost,10000001):
   p=self.root/'bad';self.call(*args,'--max-total-operations',n,'--output',p,ok=False);self.assertFalse(p.exists())
 def test_local_budget_still_applies_and_y_selection_retains_sources(self):
  t,h,cost=self.sources('y');args=self.args(t,h,'y');p=self.root/'run'
  out=self.call(*args,'--max-total-operations',cost+1000,'--max-operations',1,'--output',p)
  self.assertEqual(out['work_budget']['research_limit'],1);self.assertEqual(out['status'],'INCOMPLETE')
  self.assertEqual(self.call('replay',p),out)
  full=self.root/'full';out=self.call(*args,'--max-total-operations',cost+1000,'--output',full)
  rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot budget-test\n');selected=self.root/'selected'
  self.call('compatibility','--rules',rules,'--from-run',p,'--at',7,ok=False)
  choice=self.call('compatibility','--rules',rules,'--from-run',full,'--at',7,'--output',selected)
  self.assertEqual(choice['candidates'][0]['value'],'1')
  t.unlink();h.unlink();rules.unlink();self.assertEqual(self.call('replay',full),out);full.unlink()
  self.assertEqual(self.call('replay',selected),choice)
 def test_invalid_scope_and_rehashed_metadata(self):
  t,h,cost=self.sources();args=self.args(t,h);p=self.root/'run'
  self.call('research','--train',t,'--max-total-operations',10000,ok=False)
  out=self.call(*args,'--max-total-operations',cost+100,'--output',p)
  original=p.read_text();header,_,body=original.split('\n',2)
  self.assertEqual(header,'BRUCE_GRID_RESEARCH_BUNDLE_V3')
  for old,new in [('grid-source-bytes-plus-research-v1','unknown-policy'),
                  (f'total_operations {cost+100}',f'total_operations {cost}'),
                  ('requested_operations 1000000','requested_operations 1'),
                  ('"source_operations":'+str(cost),'"source_operations":0')]:
   self.assertIn(old,body);changed=body.replace(old,new,1)
   p.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed);self.call('replay',p,ok=False)
  p.write_text(original+'trailing');self.call('replay',p,ok=False)
if __name__=='__main__':unittest.main()
