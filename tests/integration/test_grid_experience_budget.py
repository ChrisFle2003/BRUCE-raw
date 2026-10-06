import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve())
class GridExperienceBudget(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
  self.config=['--strategy','portfolio-rational','--max-degree',2,'--min',-2,'--max',2,'--max-power',2,'--max-denominator',1]
  t=self.root/'train';h=self.root/'hold';source=self.root/'source';self.store=self.root/'store'
  self.call('grid-batch','--paths','0,2;1,2;2,2','--output',t);self.call('grid-batch','--paths','3,2','--output',h)
  self.call('research',*self.config,'--grid-train',t,'--grid-holdout',h,'--output',source)
  self.call('experience','add','--from-run',source,'--output',self.store)
  self.t=self.root/'newtrain';self.h=self.root/'newhold'
  self.call('grid-batch','--paths','4,2;5,2;6,2','--output',self.t);self.call('grid-batch','--paths','7,2','--output',self.h)
  self.args=['research',*self.config,'--grid-train',self.t,'--grid-holdout',self.h,'--planner','experience-order','--experience',self.store]
  self.grid=self.t.stat().st_size+self.h.stat().st_size;self.exp=self.store.stat().st_size;self.cost=self.grid+self.exp
 def tearDown(self):self.tmp.cleanup()
 def call(self,*args,ok=True):
  p=subprocess.run([BIN,*map(str,args)],capture_output=True,text=True,timeout=30)
  self.assertEqual(p.returncode,0 if ok else 2,p.stderr);return json.loads(p.stdout if ok else p.stderr)
 def test_thresholds_and_selfcontained(self):
  plain=self.call(*self.args);work=plain['research']['operations_used'];self.assertEqual(plain['research']['experience_records_used'],1)
  for n in (1,work-1,work,work+1):
   p=self.root/f'run{n}';out=self.call(*self.args,'--max-total-operations',self.cost+n,'--output',p)
   b=out['work_budget'];self.assertEqual(b['experience_import_operations'],self.exp);self.assertEqual(b['source_operations'],self.grid)
   self.assertEqual(b['research_limit'],n);self.assertEqual(b['used'],self.cost+out['research']['operations_used'])
   self.assertEqual(out['status'],'INCOMPLETE' if n<work else plain['status']);self.assertEqual(self.call('replay',p),out)
   self.assertTrue(p.read_text().startswith('BRUCE_GRID_RESEARCH_BUNDLE_V4\n'))
  final=self.root/f'run{work}'
  for p in self.root.iterdir():
   if p!=final:p.unlink()
  self.assertEqual(self.call('replay',final)['status'],plain['status'])
 def test_admission_tampering_and_local_limit(self):
  for n in (self.grid+1,self.cost,0,10000001):
   p=self.root/'bad';self.call(*self.args,'--max-total-operations',n,'--output',p,ok=False);self.assertFalse(p.exists())
  p=self.root/'limited';out=self.call(*self.args,'--max-total-operations',self.cost+10000,'--max-operations',1,'--output',p)
  self.assertEqual(out['work_budget']['research_limit'],1);self.assertEqual(self.call('replay',p),out)
  header,_,body=p.read_text().split('\n',2)
  for old,new in [(f'experience_import_operations {self.exp}',f'experience_import_operations {self.exp-1}'),('grid-and-experience-bytes-plus-research-v1','unknown-policy'),('"experience_import_operations":'+str(self.exp),'"experience_import_operations":0')]:
   self.assertIn(old,body);changed=body.replace(old,new,1);p.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed);self.call('replay',p,ok=False)
 def test_selection_and_no_recursive_experience(self):
  p=self.root/'full';out=self.call(*self.args,'--max-total-operations',self.cost+100000,'--output',p)
  rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot experience-budget\n');selected=self.root/'selected'
  choice=self.call('compatibility','--rules',rules,'--from-run',p,'--at',3,'--output',selected)
  self.assertEqual(choice['candidates'][0]['value'],'9')
  self.call('experience','add','--from-run',p,'--output',self.root/'bad',ok=False)
  for path in self.root.iterdir():
   if path!=selected:path.unlink()
  self.assertEqual(self.call('replay',selected),choice)
if __name__=='__main__':unittest.main()
