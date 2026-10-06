import json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve())
class InspectionTests(unittest.TestCase):
 def call(self,*args,ok=True):
  p=subprocess.run([BIN,*map(str,args)],capture_output=True,text=True)
  self.assertEqual(p.returncode,0 if ok else 2,p.stderr);return json.loads(p.stdout if ok else p.stderr)
 def test_grid_partial_and_local_stop(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'partial';self.call('grid-batch','--paths','2,6;4,3','--max-query-steps',1,'--output',p)
   out=self.call('inspect-run',p);self.assertEqual(out['lifecycle'],'INCOMPLETE');self.assertEqual(out['kind'],'grid-batch');self.assertTrue(out['replay_checked'])
   q=Path(d)/'stop';self.call('grid','--path','0,0','--zero-zero','stop','--output',q)
   out=self.call('inspect-run',q);self.assertEqual(out['lifecycle'],'COMPLETED');self.assertEqual(out['status'],'STOPPED')
   self.assertNotIn('general_proof',out)
   q.write_text(q.read_text()+'tampered');self.call('inspect-run',q,ok=False)
 def test_comparison_claim_is_preserved(self):
  with tempfile.TemporaryDirectory() as d:
   for value in ('1','2'):
    p=Path(d)/value;native=self.call('compare','--left-numerator','1','--left-denominator','1','--right-numerator',value,'--right-denominator','1','--output',p)
    out=self.call('inspect-run',p)
    self.assertEqual(out['kind'],'fraction-comparison');self.assertEqual(out['lifecycle'],'COMPLETED')
    self.assertEqual(out['status'],native['status']);self.assertNotIn('general_proof',out)
 def test_fixtures(self):
  fixtures=Path(__file__).resolve().parents[1]/'fixtures'
  for p in fixtures.glob('*.bruce'):
   original=self.call('replay',p);out=self.call('inspect-run',p)
   self.assertTrue(out['replay_checked']);self.assertEqual(out['status'],original['status'])
if __name__=='__main__':unittest.main()
