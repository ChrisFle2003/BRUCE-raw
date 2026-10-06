import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve())
class GridReplayTests(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
 def tearDown(self):self.tmp.cleanup()
 def call(self,*args,ok=True):
  p=subprocess.run([BIN,*map(str,args)],text=True,capture_output=True,timeout=10)
  self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
  return json.loads(p.stdout if ok else p.stderr)
 def test_every_local_value_and_mode_replays(self):
  for x in range(8):
   for y in range(8):
    for mode in ('exact','modular','metric'):
     path=self.root/f'{x}-{y}-{mode}'
     args=['grid','--path',f'7,7/{x},{y}','--mode',mode]
     if mode=='modular':args+=['--modulus',7]
     out=self.call(*args,'--output',path)
     self.assertEqual(self.call(*args),out)
     self.assertEqual(self.call('replay',path),out)
     self.assertTrue(path.read_text().startswith('BRUCE_GRID_BUNDLE_V1\n'))
     self.assertEqual(out['evaluations'],1)
     if mode=='metric':self.assertEqual(out['bit_length'],pow(x,y).bit_length())
     else:self.assertEqual(int(out['value']),pow(x,y,7) if mode=='modular' else pow(x,y))
 def test_depth_identity_and_stops(self):
  for depth in (1,12,64):
   for mode in ('exact','metric','modular'):
    path=self.root/f'{depth}-{mode}'
    args=['grid','--path','/'.join(['7,7']*(depth-1)+['0,0']),'--zero-zero','stop','--mode',mode]
    if mode=='modular':args+=['--modulus',1]
    out=self.call(*args,'--output',path)
    self.assertEqual((out['status'],out['stop_scope'],out['evaluations']),('STOPPED','BRANCH',0))
    self.assertNotIn('value',out);self.assertNotIn('bit_length',out)
    self.assertEqual(self.call('replay',path),out)
  a=self.call('grid','--path','2,6');b=self.call('grid','--path','4,3')
  self.assertEqual(a['value'],b['value']);self.assertNotEqual(a['path'],b['path'])
  self.assertEqual(self.call('grid','--path','0,0/2,6','--zero-zero','stop')['value'],'64')
 def test_boundaries_and_exclusive_export(self):
  path=self.root/'run'
  good=self.call('grid','--path','2,6','--max-bits',7,'--output',path);original=path.read_bytes()
  self.call('grid','--path','3,3','--output',path,ok=False);self.assertEqual(path.read_bytes(),original)
  for args in [('--path','2,6','--max-bits',6),('--path','2,6','--max-bits',0),
               ('--path','8,0'),('--path','0,0','--mode','modular','--modulus',0,'--zero-zero','stop'),
               ('--path','2,6','--modulus',7),('--path','2,6','--mode','modular'),
               ('--path','2,6','--mode','other'),('--path','2,6','--zero-zero','other'),
               ('--path','/'.join(['1,1']*65)),('--path','2,6/')]:
   missing=self.root/'invalid'
   self.call('grid',*args,'--output',missing,ok=False);self.assertFalse(missing.exists())
  self.assertEqual(self.call('replay',path),good)
  self.assertEqual(self.call('grid','--path','2,6','--mode','metric','--max-bits',1)['bit_length'],7)
  self.assertEqual(self.call('grid','--path','2,6','--mode','modular','--modulus',1)['value'],'0')
 def test_rehashed_forgery_and_original_path(self):
  path=self.root/'proof';out=self.call('grid','--path','02,06/4,3','--output',path)
  self.assertEqual(out['path'],'02,06/4,3');self.assertEqual(self.call('replay',path),out)
  original=path.read_text();header,_,body=original.split('\n',2)
  for old,new in [('"value":"64"','"value":"65"'),('mode exact','mode metric'),
                  ('zero_zero one','zero_zero stop'),('max_bits 64','max_bits 6'),
                  ('bruce-grid-v1','bruce-grid-v2'),('LEAF_LOCAL_POWER_V1','LEAF_LOCAL_POWER_V2'),
                  ('PATH_VALIDATED','PATH_SKIPPED')]:
   self.assertIn(old,body)
   changed=body.replace(old,new,1)
   path.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
   # A changed policy that has no semantic effect still differs from the
   # original canonical trace, which includes its explicit parameters.
   self.call('replay',path,ok=False)
  path.write_text(original+'garbage');self.call('replay',path,ok=False)
if __name__=='__main__':unittest.main()
