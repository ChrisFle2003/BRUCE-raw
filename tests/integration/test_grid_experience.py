import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve())
class GridExperience(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
 def tearDown(self):self.tmp.cleanup()
 def call(self,*args,ok=True):
  p=subprocess.run([BIN,*map(str,args)],capture_output=True,text=True,timeout=30)
  self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
  return json.loads(p.stdout if ok else p.stderr)
 def source(self,name='source',axis='x',extra=()):
  t=self.root/(name+'train');h=self.root/(name+'hold');p=self.root/name
  paths='0,2;1,2;2,2' if axis=='x' else '1,0;1,1;1,2'
  partial=self.root/(name+'partial')
  self.call('grid-batch','--paths',paths,'--max-query-steps',1,'--output',partial)
  self.call('grid-resume','--from-run',partial,'--additional-queries',2,'--output',t)
  self.call('grid-batch','--paths','3,2' if axis=='x' else '1,3','--output',h)
  self.call('research','--strategy','portfolio-rational','--max-degree',2,'--min',-2,'--max',2,'--max-power',2,'--max-denominator',1,'--grid-train',t,'--grid-holdout',h,'--grid-axis',axis,*extra,'--output',p)
  return p
 def test_origin_votes_revoke_and_selfcontained(self):
  source=self.source(extra=('--max-total-operations',200000));store=self.root/'store'
  report=self.call('experience','add','--from-run',source,'--output',store)
  self.assertEqual(report['engine'],'bruce-experience-v12');self.assertEqual(report['active_records'],1)
  self.assertEqual(report['records'][0]['source_digest'],hashlib.sha256(source.read_bytes()).hexdigest())
  self.assertTrue(report['records'][0]['grid_origin'])
  train=self.root/'train.csv';hold=self.root/'hold.csv';train.write_text('x,y\n4,16\n5,25\n6,36\n');hold.write_text('x,y\n7,49\n')
  args=['research','--strategy','portfolio-rational','--max-degree',2,'--min',-2,'--max',2,'--max-power',2,'--max-denominator',1,'--train',train,'--holdout',hold,'--planner','experience-order']
  result=self.root/'learned';learned=self.call(*args,'--experience',store,'--output',result)
  self.assertEqual(learned['experience_records_used'],1);self.assertEqual(learned['experience_votes'],report['records'][0]['votes'])
  revoked=self.root/'revoked';self.call('experience','revoke','--store',store,'--id',report['records'][0]['id'],'--reason','test','--output',revoked)
  self.assertEqual(self.call(*args,'--experience',revoked)['experience_records_used'],0)
  for p in self.root.iterdir():
   if p!=result:p.unlink()
  self.assertEqual(self.call('replay',result),learned)
 def test_versions_duplicates_and_limits(self):
  for axis in ('x','y'):
   source=self.source(axis,axis);store=self.root/(axis+'store');self.call('experience','add','--from-run',source,'--output',store)
   self.call('experience','add','--store',store,'--from-run',source,'--output',self.root/'dup',ok=False)
  big=self.source('big',extra=('--max-operations',1000001))
  error=self.call('experience','add','--from-run',big,'--output',self.root/'bad',ok=False)
  self.assertIn('ExperienceSourceOperationLimit',str(error))
  partial=self.source('incomplete',extra=('--max-candidates',1))
  self.call('experience','add','--from-run',partial,'--output',self.root/'bad',ok=False)
 def test_csv_duplicate_and_mixed_snapshot(self):
  source=self.source();store=self.root/'store';first=self.call('experience','add','--from-run',source,'--output',store)
  t=self.root/'t.csv';h=self.root/'h.csv';t.write_text('x,y\n2,4\n0,0\n1,1\n');h.write_text('x,y\n3,9\n')
  args=['research','--strategy','portfolio-rational','--max-degree',2,'--min',-2,'--max',2,'--max-power',2,'--max-denominator',1,'--train',t,'--holdout',h]
  duplicate=self.root/'duplicate';self.call(*args,'--output',duplicate)
  self.call('experience','add','--store',store,'--from-run',duplicate,'--output',self.root/'bad',ok=False)
  t.write_text('x,y\n4,16\n5,25\n6,36\n');h.write_text('x,y\n7,49\n')
  other=self.root/'other';self.call(*args,'--output',other)
  mixed=self.root/'mixed';report=self.call('experience','add','--store',store,'--from-run',other,'--output',mixed)
  self.assertEqual(report['active_records'],2);self.assertEqual([r['grid_origin'] for r in report['records']],[True,False])
  self.assertEqual(self.call('experience','inspect',mixed),report)
  for name,extra in [('single',['--strategy','polynomial']),('nohold',[])]:
   p=self.root/name
   call=['research','--strategy','portfolio-rational','--max-degree',2,'--grid-train',self.root/'sourcetrain']
   if name=='single':call=['research','--strategy','polynomial','--grid-train',self.root/'sourcetrain','--grid-holdout',self.root/'sourcehold']
   self.call(*call,'--output',p)
   self.call('experience','add','--from-run',p,'--output',self.root/'bad',ok=False)
 def test_recursive_and_tampered_sources_rejected(self):
  source=self.source();store=self.root/'store';self.call('experience','add','--from-run',source,'--output',store)
  recursive=self.source('recursive',extra=('--planner','experience-order','--experience',store))
  self.call('experience','add','--from-run',recursive,'--output',self.root/'bad',ok=False)
  header,_,body=source.read_text().split('\n',2);changed=body.replace('"value":"4"','"value":"5"',1);self.assertNotEqual(changed,body)
  source.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
  self.call('experience','add','--from-run',source,'--output',self.root/'bad',ok=False)
if __name__=='__main__':unittest.main()
