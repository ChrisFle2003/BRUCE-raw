import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve())
class GridCacheTests(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
 def tearDown(self):self.tmp.cleanup()
 def call(self,*args,ok=True):
  p=subprocess.run([BIN,*map(str,args)],text=True,capture_output=True,timeout=10)
  self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
  return json.loads(p.stdout if ok else p.stderr)
 def test_address_identity_replay_and_modes(self):
  for mode in ('exact','metric','modular'):
   args=['--mode',mode]+(['--modulus',7] if mode=='modular' else [])
   outpath=self.root/mode
   out=self.call('grid-batch','--paths','2,6;02,06;4,3;1,1/2,6;2,6',*args,'--output',outpath)
   self.assertEqual(self.call('replay',outpath),out)
   self.assertEqual((out['cache']['hits'],out['cache']['misses'],out['evaluations']),(2,3,3))
   self.assertEqual(out['spec']['version'],'grid-spec-v1')
   keys=[x['key'] for x in out['queries']]
   self.assertEqual(keys[0],keys[1]);self.assertEqual(keys[0],keys[4])
   self.assertEqual(len(set(keys)),3)
   for q,path in zip(out['queries'],['2,6','02,06','4,3','1,1/2,6','2,6']):
    self.assertEqual(q['result'],self.call('grid','--path',path,*args))
 def test_disabled_small_and_exact_cache_budget(self):
  paths='2,6;2,6'
  probe=self.call('grid-batch','--paths','2,6')
  cost=probe['cache']['bytes']
  self.assertGreater(cost,0)
  for limit in (0,1,cost-1,cost):
   out=self.call('grid-batch','--paths',paths,'--cache-bytes',limit)
   self.assertLessEqual(out['cache']['peak_bytes'],limit)
   self.assertEqual(out['cache']['hits'],1 if limit==cost else 0)
   self.assertEqual(out['evaluations'],1 if limit==cost else 2)
  # One entry fits; a different same-length address evicts it deterministically.
  out=self.call('grid-batch','--paths','2,6;4,3;2,6','--cache-bytes',cost)
  self.assertEqual((out['cache']['hits'],out['cache']['evictions']),(0,2))
 def test_stop_is_local_and_never_cached(self):
  p=self.root/'stop'
  out=self.call('grid-batch','--paths','0,0;2,6;0,0;0,0/2,6;2,6','--zero-zero','stop','--output',p)
  self.assertEqual(self.call('replay',p),out)
  self.assertEqual((out['evaluations'],out['cache']['hits'],out['cache']['bypasses']),(2,1,2))
  self.assertEqual([q['result']['status'] for q in out['queries']],['STOPPED','EVALUATED','STOPPED','EVALUATED','EVALUATED'])
 def test_invalid_no_export_and_exclusive(self):
  for args in [('--paths','2,6;8,0'),('--paths','2,6;'),('--paths',''),
               ('--paths',';2,6'),('--paths','2,6','--cache-bytes',1048577),
               ('--paths',';'.join(['1,1']*1025)),('--paths','2,6','--max-bits',6),
               ('--paths','0,0','--zero-zero','stop','--mode','modular','--modulus',0)]:
   p=self.root/'bad';self.call('grid-batch',*args,'--output',p,ok=False);self.assertFalse(p.exists())
  p=self.root/'existing';self.call('grid-batch','--paths','2,6','--output',p);original=p.read_bytes()
  self.call('grid-batch','--paths','4,3','--output',p,ok=False);self.assertEqual(p.read_bytes(),original)
 def test_rehashed_forgery(self):
  p=self.root/'run';self.call('grid-batch','--paths','2,6;2,6','--output',p)
  header,_,body=p.read_text().split('\n',2)
  self.assertEqual(header,'BRUCE_GRID_BATCH_BUNDLE_V1')
  for old,new in [('grid-spec-v1','grid-spec-v2'),('cache_bytes 65536','cache_bytes 0'),
                  ('MISS_INSERT','HIT'),('"value":"64"','"value":"65"'),
                  ('zero_zero one','zero_zero stop'),('query_count 2','query_count 1025')]:
   self.assertIn(old,body);changed=body.replace(old,new,1)
   p.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
   self.call('replay',p,ok=False)
  for changed in (body+'garbage',body[:body.index('RESULT')]):
   p.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
   self.call('replay',p,ok=False)
 def test_all_local_values_and_query_limit(self):
  paths=[f'{x},{y}' for y in range(8) for x in range(8)]
  for mode in ('exact','metric','modular'):
   args=['--mode',mode]+(['--modulus',7] if mode=='modular' else [])
   out=self.call('grid-batch','--paths',';'.join(paths*2),*args)
   self.assertEqual((out['cache']['hits'],out['evaluations']),(64,64))
   for q,path in zip(out['queries'],paths*2):
    x,y=map(int,path.split(','));value=pow(x,y)
    if mode=='metric':self.assertEqual(q['result']['bit_length'],value.bit_length())
    else:self.assertEqual(int(q['result']['value']),value%7 if mode=='modular' else value)
  path='/'.join(['7,7']*64)
  out=self.call('grid-batch','--paths',path+';'+path)
  self.assertEqual(out['cache']['hits'],1);self.assertEqual(out['queries'][0]['result']['depth'],64)
  out=self.call('grid-batch','--paths',';'.join(['2,6']*1024))
  self.assertEqual((out['cache']['hits'],out['evaluations']),(1023,1))
  # A new process starts cold; no persistent cache is consulted.
  self.assertEqual(self.call('grid-batch','--paths','2,6')['cache']['hits'],0)
 def test_old_grid_fixture(self):
  p=Path(__file__).resolve().parents[1]/'fixtures/core-036-grid-v1.bruce'
  self.assertEqual(self.call('replay',p),json.loads(p.read_text().rsplit('RESULT\n',1)[1]))
if __name__=='__main__':unittest.main()
