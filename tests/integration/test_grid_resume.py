import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve())
class GridResumeTests(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
 def tearDown(self):self.tmp.cleanup()
 def call(self,*args,ok=True):
  p=subprocess.run([BIN,*map(str,args)],capture_output=True,text=True,timeout=20)
  self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
  return json.loads(p.stdout if ok else p.stderr)
 def test_continue_cache_and_chain_replay(self):
  paths='2,6;02,06;0,0;4,3;2,6'
  args=['grid-batch','--paths',paths,'--zero-zero','stop']
  full=self.call(*args);parent=self.root/'parent'
  first=self.call(*args,'--max-query-steps',1,'--output',parent)
  child=self.root/'child';out=self.call('grid-resume','--from-run',parent,'--additional-queries',2,'--output',child)
  self.assertEqual(out['batch']['queries'],full['queries'][:3]);self.assertEqual(out['new_evaluations'],0)
  self.assertEqual(out['audit_evaluations'],2*first['evaluations']);self.assertEqual(out['generation'],1)
  self.assertEqual(out['status'],'INCOMPLETE');self.assertEqual(self.call('replay',child),out)
  final=self.root/'final';last=self.call('grid-resume','--from-run',child,'--additional-queries',2,'--output',final)
  self.assertEqual(last['batch']['queries'],full['queries']);self.assertEqual(last['batch']['cache'],full['cache'])
  self.assertEqual(last['new_evaluations'],1);self.assertEqual(last['status'],'COMPLETED')
  self.assertEqual(last['generation'],2);parent.unlink();child.unlink();self.assertEqual(self.call('replay',final),last)
  self.call('grid-resume','--from-run',final,'--additional-queries',1,ok=False)
 def test_completed_continuation_is_research_source(self):
  p=self.root/'partial';self.call('grid-batch','--paths','0,2;1,2;2,2','--max-query-steps',1,'--output',p)
  t=self.root/'train';self.call('grid-resume','--from-run',p,'--additional-queries',2,'--output',t)
  h=self.root/'hold';self.call('grid-batch','--paths','3,2','--output',h)
  r=self.root/'research';out=self.call('research','--strategy','polynomial','--domain','rational','--max-degree',2,
   '--grid-train',t,'--grid-holdout',h,'--max-total-operations',100000,'--output',r)
  self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES');p.unlink();t.unlink();h.unlink();self.assertEqual(self.call('replay',r),out)
 def test_limits_and_tampering(self):
  p=self.root/'parent';self.call('grid-batch','--paths',';'.join(['2,6']*10),'--max-query-steps',0,'--output',p)
  original=p
  for n in range(8):
   child=self.root/f'child-{n}';self.call('grid-resume','--from-run',p,'--additional-queries',1,'--output',child);p=child
  bad=self.root/'bad';self.call('grid-resume','--from-run',p,'--additional-queries',1,'--output',bad,ok=False);self.assertFalse(bad.exists())
  for n in (0,1025):self.call('grid-resume','--from-run',original,'--additional-queries',n,ok=False)
  child=self.root/'tamper';self.call('grid-resume','--from-run',original,'--additional-queries',1,'--output',child)
  saved=child.read_text();header,_,body=saved.split('\n',2)
  for old,new in [('additional_queries 1','additional_queries 2'),('verified-prefix-cache-rebuild-v1','unknown-policy'),('"new_evaluations":1','"new_evaluations":0')]:
   self.assertIn(old,body);changed=body.replace(old,new,1)
   child.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed);self.call('replay',child,ok=False)
if __name__=='__main__':unittest.main()
