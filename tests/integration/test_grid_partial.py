import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve())
class GridPartialTests(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
 def tearDown(self):self.tmp.cleanup()
 def call(self,*args,ok=True):
  p=subprocess.run([BIN,*map(str,args)],capture_output=True,text=True,timeout=20)
  self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
  return json.loads(p.stdout if ok else p.stderr)
 def test_every_prefix_preserves_values_and_cache(self):
  for mode in ('exact','metric','modular'):
   args=['grid-batch','--paths','2,6;02,06;0,0;4,3;2,6','--zero-zero','stop','--mode',mode]
   if mode=='modular':args+=['--modulus',7]
   full=self.call(*args)
   for n in range(6):
    p=self.root/f'{mode}-{n}';out=self.call(*args,'--max-query-steps',n,'--output',p)
    self.assertEqual(out['queries'],full['queries'][:n])
    self.assertEqual(out['processed_queries'],n)
    self.assertEqual(out['status'],'INCOMPLETE' if n<5 else 'COMPLETED')
    self.assertEqual(out['stop_reason'],'QueryBudgetExceeded' if n<5 else 'Completed')
    self.assertEqual(self.call('replay',p),out)
   self.assertEqual(self.call(*args,'--max-query-steps',2)['cache']['hits'],1)
 def test_research_rejects_partial_but_accepts_complete_v2(self):
  partial=self.root/'partial';full=self.root/'full';hold=self.root/'hold'
  for p,n in [(partial,2),(full,3)]:self.call('grid-batch','--paths','0,2;1,2;2,2','--max-query-steps',n,'--output',p)
  self.call('grid-batch','--paths','3,2','--output',hold)
  args=['research','--strategy','polynomial','--domain','rational','--max-degree',2]
  bad=self.root/'bad';self.call(*args,'--grid-train',partial,'--output',bad,ok=False);self.assertFalse(bad.exists())
  self.call(*args,'--grid-train',full,'--grid-holdout',partial,ok=False)
  p=self.root/'research';out=self.call(*args,'--grid-train',full,'--grid-holdout',hold,'--max-total-operations',100000,'--output',p)
  self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES');full.unlink();hold.unlink();self.assertEqual(self.call('replay',p),out)
 def test_bounds_and_rehashed_forgery(self):
  for args in [('--max-query-steps',1025),('--max-query-steps',-1)]:
   self.call('grid-batch','--paths','2,6',*args,ok=False)
  p=self.root/'bad';self.call('grid-batch','--paths','2,6;bad','--max-query-steps',0,'--output',p,ok=False);self.assertFalse(p.exists())
  p=self.root/'run';self.call('grid-batch','--paths','2,6;2,6','--max-query-steps',1,'--output',p)
  header,_,body=p.read_text().split('\n',2);self.assertEqual(header,'BRUCE_GRID_BATCH_BUNDLE_V2')
  for old,new in [('query_steps 1','query_steps 2'),('query-boundaries-v1','unknown-policy'),
                  ('"status":"INCOMPLETE"','"status":"COMPLETED"'),('"processed_queries":1','"processed_queries":2')]:
   self.assertIn(old,body);changed=body.replace(old,new,1)
   p.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed);self.call('replay',p,ok=False)
if __name__=='__main__':unittest.main()
