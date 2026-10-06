import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve())
class GridResearchTests(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name);self.serial=0
 def tearDown(self):self.tmp.cleanup()
 def call(self,*args,ok=True):
  p=subprocess.run([BIN,*map(str,args)],text=True,capture_output=True,timeout=30)
  self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
  return json.loads(p.stdout if ok else p.stderr)
 def grid(self,paths,*args):
  self.serial+=1;p=self.root/str(self.serial)
  self.call('grid-batch','--paths',paths,*args,'--output',p);return p
 def research(self,train,hold=None,*args,ok=True):
  return self.call('research','--strategy','polynomial','--domain','rational','--max-degree',2,
                   '--grid-train',train,*(['--grid-holdout',hold] if hold else []),*args,ok=ok)
 def test_full_pipeline_and_self_contained_selection(self):
  t=self.grid('0,2;1,2;2,2');h=self.grid('3,2;4,2');p=self.root/'research'
  out=self.research(t,h,'--output',p)
  self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
  self.assertEqual(out['research']['hypotheses'][0]['coefficients'],['0','0','1'])
  self.assertEqual(out['grid_origin']['projection'],'fixed-slice-leaf-x-v1')
  self.assertEqual(self.call('replay',p),out)
  rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot grid-test\n')
  selected=self.root/'selected'
  choice=self.call('compatibility','--from-run',p,'--rules',rules,'--at',5,'--output',selected)
  self.assertEqual(choice['candidates'][0]['value'],'25')
  self.call('compatibility','--from-run',p,'--rules',rules,'--at',8,ok=False)
  t.unlink();h.unlink();rules.unlink()
  self.assertEqual(self.call('replay',p),out);p.unlink()
  self.assertEqual(self.call('replay',selected),choice)
 def test_reject_ambiguous_projection_and_sources(self):
  good=self.grid('1,2;2,2')
  for bad in [self.grid('1,2;2,3'),self.grid('1,1/1,2;2,1/2,2'),
              self.grid('1,2;01,02'),self.grid('0,0;1,0','--zero-zero','stop'),
              self.grid('1,2;2,2','--mode','metric'),self.grid('1,2;2,2','--mode','modular','--modulus',7)]:
   self.research(bad,ok=False)
  for hold in [self.grid('2,2;3,2'),self.grid('3,3'),self.grid('1,1/3,2'),self.grid('3,2','--max-bits',8)]:
   self.research(good,hold,ok=False)
  p=self.root/'invalid';self.research(good,None,'--train',good,'--output',p,ok=False);self.assertFalse(p.exists())
  self.call('research','--grid-train',good,'--domain','int64',ok=False)
  self.call('research','--grid-holdout',good,ok=False)
 def test_cache_independence_csv_equivalence_and_incomplete(self):
  t=self.grid('0,2;1,2;2,2','--cache-bytes',0);h=self.grid('3,2')
  csv=self.root/'train.csv';csv.write_text('x,y\n0,0\n1,1\n2,4\n')
  hold=self.root/'hold.csv';hold.write_text('x,y\n3,9\n')
  out=self.research(t,h)
  plain=self.call('research','--strategy','polynomial','--domain','rational','--max-degree',2,'--train',csv,'--holdout',hold)
  self.assertEqual(out['research'],plain)
  p=self.root/'partial';partial=self.research(t,h,'--max-operations',1,'--output',p)
  self.assertEqual(partial['status'],'INCOMPLETE');self.assertEqual(self.call('replay',p),partial)
 def test_rehashed_tampering_and_exclusive(self):
  t=self.grid('0,2;1,2;2,2');h=self.grid('3,2');p=self.root/'run'
  self.research(t,h,'--output',p);original=p.read_text()
  self.research(t,h,'--output',p,ok=False);self.assertEqual(p.read_text(),original)
  header,_,body=original.split('\n',2)
  for old,new in [('fixed-slice-leaf-x-v1','fixed-slice-leaf-y-v1'),('"value":"4"','"value":"5"'),
                  ('training_bytes ','training_bytes 9'),('"exponent":2','"exponent":3')]:
   self.assertIn(old,body);changed=body.replace(old,new,1)
   p.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
   self.call('replay',p,ok=False)
  p.write_text(original+'trailing');self.call('replay',p,ok=False)
 def test_existing_strategy_paths_match_csv(self):
  t=self.grid('0,2;1,2;2,2');h=self.grid('3,2;4,2')
  csv=self.root/'train.csv';csv.write_text('x,y\n0,0\n1,1\n2,4\n')
  hold=self.root/'hold.csv';hold.write_text('x,y\n3,9\n4,16\n')
  for i,args in enumerate([
   ['--strategy','affine','--domain','bigint','--min',0,'--max',2],
   ['--strategy','power-affine','--planner','adaptive','--max-power',2,'--min',0,'--max',1],
   ['--strategy','portfolio','--max-power',2,'--max-degree',2,'--min',0,'--max',1],
   ['--strategy','portfolio-expression','--max-power',2,'--max-degree',2,'--max-nodes',3,'--min',0,'--max',1,'--max-denominator',1]]):
   p=self.root/f'family-{i}'
   out=self.call('research',*args,'--grid-train',t,'--grid-holdout',h,'--output',p)
   plain=self.call('research',*args,'--train',csv,'--holdout',hold)
   self.assertEqual(out['research'],plain);self.assertEqual(self.call('replay',p),out)
 def test_explicit_empty_holdout_is_not_omitted(self):
  t=self.grid('0,2;1,2;2,2');h=self.root/'empty';h.write_text('');p=self.root/'invalid'
  self.research(t,h,'--output',p,ok=False);self.assertFalse(p.exists())
if __name__=='__main__':unittest.main()
