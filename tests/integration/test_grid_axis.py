import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve())
class GridAxisTests(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name);self.serial=0
 def tearDown(self):self.tmp.cleanup()
 def call(self,*args,ok=True):
  p=subprocess.run([BIN,*map(str,args)],capture_output=True,text=True,timeout=20)
  self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
  return json.loads(p.stdout if ok else p.stderr)
 def grid(self,paths,*args):
  self.serial+=1;p=self.root/str(self.serial);self.call('grid-batch','--paths',paths,*args,'--output',p);return p
 def research(self,t,h=None,*args,ok=True):
  return self.call('research','--strategy','polynomial','--domain','rational','--max-degree',2,
   '--grid-train',t,'--grid-axis','y',*(['--grid-holdout',h] if h else []),*args,ok=ok)
 def test_y_pipeline_and_selection(self):
  t=self.grid('1,0;1,1;1,2');h=self.grid('1,3;1,4');p=self.root/'research'
  out=self.research(t,h,'--output',p)
  self.assertEqual(out['status'],'VERIFIED_ON_SAMPLES')
  self.assertEqual(out['grid_origin']['base'],1);self.assertNotIn('exponent',out['grid_origin'])
  self.assertEqual(out['grid_origin']['projection'],'fixed-slice-leaf-y-v1')
  self.assertEqual(out['research']['hypotheses'][0]['coefficients'],['1'])
  self.assertTrue(p.read_text().startswith('BRUCE_GRID_RESEARCH_BUNDLE_V2\n'))
  rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot y-grid\n');selected=self.root/'selected'
  choice=self.call('compatibility','--rules',rules,'--from-run',p,'--at',7,'--output',selected)
  self.assertEqual(choice['candidates'][0]['value'],'1')
  self.call('compatibility','--rules',rules,'--from-run',p,'--at',8,ok=False)
  t.unlink();h.unlink();rules.unlink();self.assertEqual(self.call('replay',p),out);p.unlink()
  self.assertEqual(self.call('replay',selected),choice)
 def test_every_base_maps_exponent_to_power_and_matches_csv(self):
  for base in range(8):
   t=self.grid(';'.join(f'{base},{y}' for y in range(3)))
   h=self.grid(';'.join(f'{base},{y}' for y in range(3,8)))
   csv=self.root/'train.csv';csv.write_text('x,y\n'+''.join(f'{y},{base**y}\n' for y in range(3)))
   hold=self.root/'hold.csv';hold.write_text('x,y\n'+''.join(f'{y},{base**y}\n' for y in range(3,8)))
   p=self.root/f'result-{base}';out=self.research(t,h,'--output',p)
   plain=self.call('research','--strategy','polynomial','--domain','rational','--max-degree',2,'--train',csv,'--holdout',hold)
   self.assertEqual(out['research'],plain);self.assertEqual(self.call('replay',p),out)
   if base>=2:self.assertNotEqual(out['status'],'VERIFIED_ON_SAMPLES')
 def test_bad_axis_slices_duplicates_stops_and_scope(self):
  t=self.grid('1,0;1,1')
  for bad in [self.grid('1,0;2,1'),self.grid('1,0;01,00'),self.grid('1,1/1,0;2,1/1,1'),
              self.grid('0,0;0,1','--zero-zero','stop'),self.grid('1,0;1,1','--mode','metric')]:
   self.research(bad,ok=False)
  for hold in [self.grid('2,2'),self.grid('1,1'),self.grid('1,2','--max-bits',8)]:self.research(t,hold,ok=False)
  for args in [('--grid-axis','z','--grid-train',t),('--grid-axis','y','--train',t)]:
   p=self.root/'invalid';self.call('research',*args,'--output',p,ok=False);self.assertFalse(p.exists())
 def test_x_default_unchanged_and_y_tampering(self):
  t=self.grid('0,2;1,2;2,2');h=self.grid('3,2');a=self.root/'a';b=self.root/'b'
  args=['research','--strategy','polynomial','--domain','rational','--max-degree',2,'--grid-train',t,'--grid-holdout',h]
  self.call(*args,'--output',a);self.call(*args,'--grid-axis','x','--output',b);self.assertEqual(a.read_bytes(),b.read_bytes())
  t=self.grid('1,0;1,1;1,2');h=self.grid('1,3');p=self.root/'y';self.research(t,h,'--output',p)
  header,_,body=p.read_text().split('\n',2)
  for old,new in [('fixed-slice-leaf-y-v1','fixed-slice-leaf-x-v1'),('"base":1','"base":2'),('bruce-grid-research-v2','bruce-grid-research-v1')]:
   self.assertIn(old,body);changed=body.replace(old,new,1)
   p.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed);self.call('replay',p,ok=False)
  p.write_text('BRUCE_GRID_RESEARCH_BUNDLE_V1\n'+hashlib.sha256(body.encode()).hexdigest()+'\n'+body);self.call('replay',p,ok=False)
 def test_archived_x_research_bundle(self):
  p=Path(__file__).resolve().parents[1]/'fixtures/core-038-grid-research-v1.bruce'
  self.assertEqual(self.call('replay',p),json.loads(p.read_text().rsplit('RESULT\n',1)[1]))
if __name__=='__main__':unittest.main()
