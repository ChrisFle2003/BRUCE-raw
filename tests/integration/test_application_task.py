import json,subprocess,sys,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve())
class ApplicationTaskTests(unittest.TestCase):
 def test_catalog(self):
  p=subprocess.run([BIN,'task-contracts'],capture_output=True,text=True)
  self.assertEqual(p.returncode,0,p.stderr);r=json.loads(p.stdout)
  self.assertEqual(r['contract'],'application-task-v1');self.assertFalse(r['dynamic_plugins'])
  self.assertEqual(set(r['tasks']),{'research-int64','research-exact','grid','grid-batch','grid-resume','grid-research','fraction-comparison','compatibility','grid-stage-sweep','grid-stage-verify'})
  self.assertEqual(r['cancellation'],'executor-local-checkpoints');self.assertFalse(r['unified_budget_units'])
if __name__=='__main__':unittest.main()
