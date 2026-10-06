import json,subprocess,sys,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve())
class ResearchContract(unittest.TestCase):
 def test_compiled_contract_catalog(self):
  p=subprocess.run([BIN,'contracts'],capture_output=True,text=True)
  self.assertEqual(p.returncode,0,p.stderr);r=json.loads(p.stdout)
  self.assertEqual(r['contract'],'exact-research-task-v1')
  self.assertEqual(len(r['strategies']),10);self.assertFalse(r['dynamic_plugins'])
  self.assertFalse(r['gates']['learn_promotion']);self.assertFalse(r['gates']['publish'])
  self.assertTrue(r['gates']['sample_evidence_only'])
  strategies={s['id']:s for s in r['strategies']}
  self.assertEqual(strategies['polynomial']['default_planner'],'interpolate')
  self.assertEqual(strategies['portfolio-expression']['planners'],['portfolio','adaptive-order','experience-order'])
  self.assertEqual(strategies['expression']['planners'],['grammar','indexed-grammar','cached-grammar'])
  self.assertEqual(strategies['affine']['planners'],['enumerate'])
 def test_gate_policy_catalog(self):
  p=subprocess.run([BIN,'gate-contracts'],capture_output=True,text=True)
  self.assertEqual(p.returncode,0,p.stderr);r=json.loads(p.stdout)
  self.assertEqual(r['contract'],'research-gate-policy-v1')
  self.assertEqual(r['reason_precedence'],['StatusNotCompleted','StopNotCompleted','HoldoutMissing','DependentSource','PortfolioIncomplete','VerifiedHypothesisMissing'])
  self.assertEqual(r['completed_statuses'],['CANDIDATE','NO_CANDIDATE','REFUTED','AMBIGUOUS','VERIFIED_ON_SAMPLES'])
  self.assertFalse(r['learn_promotion']);self.assertFalse(r['publish']);self.assertFalse(r['general_proof'])
  self.assertEqual(r['allowed_reason'],'Allowed')
  p=subprocess.run([BIN,'gate-contracts','extra'],capture_output=True,text=True)
  self.assertEqual(p.returncode,2)
if __name__=='__main__':unittest.main()
