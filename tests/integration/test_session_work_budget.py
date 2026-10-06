import json, subprocess, sys, tempfile, unittest
from pathlib import Path
BIN = str(Path(sys.argv.pop(1)).resolve())
class SessionWorkBudget(unittest.TestCase):
 def call(self, *args, ok=True):
  p = subprocess.run([BIN, *map(str,args)], capture_output=True, text=True)
  self.assertEqual(p.returncode, 0 if ok else 2, p.stderr)
  return json.loads(p.stdout if ok else p.stderr)
 def test_shared_budget_and_export(self):
  with tempfile.TemporaryDirectory() as d:
   plain, bounded, failed = (Path(d)/name for name in ('plain','bounded','failed'))
   args = ['grid-batch','--paths','2,6;4,3','--verification','replay']
   original = self.call(*args,'--output',plain)
   out = self.call(*args,'--max-session-checkpoints',100000,'--output',bounded)
   self.assertEqual(out['result'],original['result'])
   self.assertEqual(plain.read_bytes(),bounded.read_bytes())
   self.assertEqual(out['execution']['contract'],'application-execution-v14')
   usage = out['execution']['session_budget']
   self.assertEqual(usage['model'],'session-checkpoints-v1')
   self.assertEqual(usage['max_checkpoints'],100000)
   phases = usage['by_phase']
   self.assertGreater(phases['EXECUTING'],1)
   self.assertGreater(phases['SERIALIZING'],2)
   self.assertGreater(phases['REPLAYING'],2)
   self.assertEqual(usage['checkpoints'],sum(phases.values()))
   n = usage['checkpoints']
   exact = self.call(*args,'--max-session-checkpoints',n)
   self.assertEqual(exact['result'],original['result'])
   for limit, phase in ((0,'EXECUTING'),(phases['EXECUTING'],'SERIALIZING'),
                        (phases['EXECUTING']+phases['SERIALIZING'],'REPLAYING'),(n-1,'REPLAYING')):
    error = self.call(*args,'--max-session-checkpoints',limit,'--output',failed,ok=False)
    self.assertEqual(error['execution']['contract'],'application-execution-failure-v2')
    self.assertEqual(error['execution']['failed_at'],phase)
    self.assertEqual(error['execution']['session_budget']['checkpoints'],limit)
    self.assertIn('SessionCheckpointBudgetExceeded',error['execution']['cause'])
    self.assertNotIn('result',error)
    self.assertFalse(failed.exists())
   failed.write_text('keep')
   self.call(*args,'--max-session-checkpoints',0,'--output',failed,ok=False)
   self.assertEqual(failed.read_text(),'keep')
 def test_admission_and_atomic_grid(self):
  args = ['grid','--path','2,6']
  error = self.call(*args,'--max-session-checkpoints',1,ok=False)
  self.assertEqual(error,{'error':'SessionBudgetRequiresVerification'})
  for value in ('-1','bad','18446744073709551616'):
   error = self.call(*args,'--verification','replay','--max-session-checkpoints',value,ok=False)
   self.assertEqual(set(error),{'error'})
  out = self.call(*args,'--verification','replay','--max-session-checkpoints','18446744073709551615')
  self.assertEqual(out['execution']['session_budget']['by_phase']['EXECUTING'],1)
  self.assertEqual(out['execution']['session_budget']['by_phase']['REPLAYING'],2)
if __name__ == '__main__': unittest.main()
