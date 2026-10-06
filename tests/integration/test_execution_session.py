import json,subprocess,sys,tempfile,unittest,signal,time
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve())
class SessionTests(unittest.TestCase):
 def call(self,*args,ok=True):
  p=subprocess.run([BIN,*map(str,args)],capture_output=True,text=True)
  self.assertEqual(p.returncode,0 if ok else 2,p.stderr);return json.loads(p.stdout if ok else p.stderr)
 def test_checked_complete_partial_and_export(self):
  with tempfile.TemporaryDirectory() as d:
   for budget in (1,2):
    p=Path(d)/f'plain{budget}';q=Path(d)/f'checked{budget}'
    args=['grid-batch','--paths','2,6;4,3','--max-query-steps',budget]
    plain=self.call(*args,'--output',p);checked=self.call(*args,'--verification','replay','--output',q)
    self.assertEqual(checked['result'],plain);self.assertEqual(p.read_bytes(),q.read_bytes())
    self.assertEqual(checked['execution']['phases'],['CREATED','EXECUTING','SERIALIZING','REPLAYING','READY'])
    self.assertTrue(checked['execution']['replay_verified']);self.assertEqual(checked['execution']['result_complete'],budget==2)
    self.assertEqual(self.call('replay',q),plain)
 def test_local_stop_and_failures(self):
  out=self.call('grid','--path','0,0','--zero-zero','stop','--verification','replay')
  self.assertTrue(out['execution']['result_complete']);self.assertEqual(out['result']['status'],'STOPPED')
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'bad';error=self.call('grid','--path','invalid','--verification','replay','--output',p,ok=False)
   self.assertIn('EXECUTING',str(error));self.assertFalse(p.exists())
   self.call('grid','--path','2,6','--verification','unknown','--output',p,ok=False);self.assertFalse(p.exists())
  self.assertEqual(self.call('grid','--path','2,6'),self.call('grid','--path','2,6','--verification','none'))
 def test_structured_failure_and_export_boundary(self):
  with tempfile.TemporaryDirectory() as d:
   target=Path(d)/'result.bruce'
   native=self.call('grid','--path','invalid',ok=False)
   error=self.call('grid','--path','invalid','--verification','replay','--output',target,ok=False)
   self.assertIn('execution',error,'Checked failure must expose its phase without parsing error text')
   self.assertEqual(error['execution'],{
    'contract':'application-execution-failure-v1','state':'FAILED','failed_at':'EXECUTING',
    'phases':['CREATED','EXECUTING','FAILED'],'cause':native['error']})
   self.assertEqual(error['error'],'ExecutionSessionFailed at EXECUTING: '+native['error'])
   self.assertNotIn('result',error);self.assertFalse(target.exists())
   target.write_text('keep existing file')
   again=self.call('grid','--path','invalid','--verification','replay','--output',target,ok=False)
   self.assertEqual(again,error);self.assertEqual(target.read_text(),'keep existing file')
   # Export errors occur after READY and must not be mislabeled as session failures.
   export_error=self.call('grid','--path','2,6','--verification','replay','--output',target,ok=False)
   self.assertEqual(set(export_error),{'error'});self.assertIn('Export',export_error['error'])
   self.assertEqual(target.read_text(),'keep existing file')
   preflight=self.call('grid','--path','2,6','--max-bits','bad','--verification','replay',ok=False)
   self.assertEqual(set(preflight),{'error'})
   escaped=self.call('grid','--path','bad"\n,6','--verification','replay',ok=False)
   self.assertEqual(escaped['execution']['cause'],'InvalidInteger: bad"\n')
 def test_replay_byte_budget_boundary(self):
  with tempfile.TemporaryDirectory() as d:
   source=Path(d)/'source';target=Path(d)/'target'
   args=['grid-batch','--paths','2,6;4,3','--max-query-steps','1','--verification','replay']
   plain=self.call(*args,'--output',source);size=source.stat().st_size
   bounded=self.call(*args,'--max-replay-bytes',size,'--output',target)
   self.assertEqual(bounded['result'],plain['result']);self.assertEqual(source.read_bytes(),target.read_bytes())
   self.assertEqual(bounded['execution']['contract'],'application-execution-v2')
   self.assertEqual(bounded['execution']['verification_budget'],{'max_replay_bytes':size,'replay_bytes':size})
   self.assertFalse(bounded['execution']['result_complete'])
   target.unlink()
   for limit in (size-1,0):
    failure=self.call(*args,'--max-replay-bytes',limit,'--output',target,ok=False)
    self.assertEqual(failure['execution']['failed_at'],'REPLAYING')
    self.assertEqual(failure['execution']['phases'],['CREATED','EXECUTING','SERIALIZING','REPLAYING','FAILED'])
    self.assertEqual(failure['execution']['cause'],f'ReplayByteBudgetExceeded: bytes={size} limit={limit}')
    self.assertNotIn('result',failure);self.assertFalse(target.exists())
   target.write_text('keep')
   self.call(*args,'--max-replay-bytes','0','--output',target,ok=False)
   self.assertEqual(target.read_text(),'keep')
   # The limit also applies when no export was requested.
   self.call(*args,'--max-replay-bytes','0',ok=False)
   for mode in ([],['--verification','none']):
    failure=self.call('grid','--path','2,6',*mode,'--max-replay-bytes','1',ok=False)
    self.assertEqual(failure,{'error':'ReplayBudgetRequiresVerification'})
   for value in ('-1','bad','18446744073709551616'):
    failure=self.call(*args,'--max-replay-bytes',value,ok=False)
    self.assertEqual(set(failure),{'error'})
 @unittest.skipUnless(sys.platform.startswith('linux'),'Signal readiness uses Linux /proc')
 def test_signal_cancels_verification_without_export(self):
  with tempfile.TemporaryDirectory() as d:
   training=Path(d)/'training.csv';training.write_text('x,y\n0,0\n1,1\n')
   target=Path(d)/'result'
   p=subprocess.Popen([BIN,'research','--train',str(training),'--min','-1000000','--max','1000000',
    '--max-candidates','1000000','--max-operations','10000000','--verification','replay',
    '--verification-cancellation','phase-boundaries','--output',str(target)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
   try:
    deadline=time.monotonic()+5;ready=False
    while time.monotonic()<deadline and p.poll() is None:
     try:status=Path(f'/proc/{p.pid}/status').read_text()
     except FileNotFoundError:break
     caught=next((int(line.split()[1],16) for line in status.splitlines() if line.startswith('SigCgt:')),0)
     if caught & (1 << (signal.SIGTERM-1)):
      ready=True;break
     time.sleep(.001)
    self.assertTrue(ready,'Process did not install its signal handler')
    p.send_signal(signal.SIGTERM)
    out,err=p.communicate(timeout=10)
    self.assertEqual(p.returncode,2,err);self.assertEqual(out,'');self.assertFalse(target.exists())
    report=json.loads(err)
    self.assertEqual(report['execution']['cause'],'VerificationCancelled')
    self.assertEqual(report['execution']['state'],'FAILED')
   finally:
    if p.poll() is None:p.kill()
    p.communicate()
 def test_verification_cancellation_mode(self):
  args=['grid','--path','2,6','--verification','replay']
  old=self.call(*args)
  self.assertEqual(self.call(*args,'--verification-cancellation','none'),old)
  new=self.call(*args,'--verification-cancellation','phase-boundaries','--max-replay-bytes','10000')
  self.assertEqual(new['result'],old['result'])
  self.assertEqual(new['execution']['contract'],'application-execution-v3')
  self.assertEqual(new['execution']['verification_cancellation'],'phase-boundaries')
  self.assertTrue(new['execution']['replay_verified'])
  self.call(*args,'--verification-cancellation','bad',ok=False)
  failure=self.call('grid','--path','2,6','--verification-cancellation','phase-boundaries',ok=False)
  self.assertEqual(failure,{'error':'VerificationCancellationRequiresReplay'})
 def test_grid_query_verification_mode(self):
  with tempfile.TemporaryDirectory() as d:
   parent=Path(d)/'parent';old=Path(d)/'old';new=Path(d)/'new'
   args=['grid-batch','--paths','2,6;4,3;3,2','--max-query-steps','1']
   base=self.call(*args,'--verification','replay','--output',parent)
   checked=self.call(*args,'--verification','replay','--verification-cancellation','grid-query-boundaries','--output',new)
   self.assertEqual(checked['result'],base['result']);self.assertEqual(new.read_bytes(),parent.read_bytes())
   self.assertEqual(checked['execution']['contract'],'application-execution-v4')
   self.assertEqual(checked['execution']['verification_cancellation'],'grid-query-boundaries')
   resumed=self.call('grid-resume','--from-run',parent,'--additional-queries','1','--output',old)
   new.unlink()
   checked=self.call('grid-resume','--from-run',parent,'--additional-queries','1','--verification','replay',
    '--verification-cancellation','grid-query-boundaries','--max-replay-bytes','10000','--output',new)
   self.assertEqual(checked['result'],resumed);self.assertEqual(old.read_bytes(),new.read_bytes())
   target=Path(d)/'unsupported'
   error=self.call('grid','--path','2,6','--verification','replay','--verification-cancellation','grid-query-boundaries','--output',target,ok=False)
   self.assertIn('GridReplayCheckpointsRequireBatchOrResume',str(error));self.assertFalse(target.exists())
 def test_research_checkpoint_mode(self):
  with tempfile.TemporaryDirectory() as d:
   source=Path(d)/'samples';source.write_text('x,y\n0,1\n1,2\n')
   for domain in ([],['--domain','rational']):
    old=Path(d)/('old'+str(len(domain)));new=Path(d)/('new'+str(len(domain)))
    args=['research','--train',source,'--min','0','--max','1',*domain,'--verification','replay']
    plain=self.call(*args,'--output',old)
    checked=self.call(*args,'--verification-cancellation','research-checkpoints','--output',new)
    self.assertEqual(checked['result'],plain['result']);self.assertEqual(old.read_bytes(),new.read_bytes())
    self.assertEqual(checked['execution']['contract'],'application-execution-v5')
    self.assertEqual(checked['execution']['verification_cancellation'],'research-checkpoints')
   self.call('grid-batch','--paths','2,6','--verification','replay','--verification-cancellation','research-checkpoints',ok=False)
 def test_composed_grid_research_checkpoints(self):
  with tempfile.TemporaryDirectory() as d:
   train=Path(d)/'train';hold=Path(d)/'hold';plain=Path(d)/'plain';checked=Path(d)/'checked'
   self.call('grid-batch','--paths','0,2;1,2;2,2','--output',train)
   self.call('grid-batch','--paths','3,2','--output',hold)
   total=train.stat().st_size+hold.stat().st_size+1000
   args=['research','--grid-train',train,'--grid-holdout',hold,'--domain','rational',
    '--strategy','polynomial','--planner','interpolate','--max-degree','2','--max-total-operations',total]
   old=self.call(*args,'--output',plain)
   new=self.call(*args,'--verification','replay','--verification-cancellation','research-checkpoints',
    '--max-replay-bytes','1000000','--output',checked)
   self.assertEqual(new['result'],old);self.assertEqual(plain.read_bytes(),checked.read_bytes())
   self.assertEqual(new['execution']['contract'],'application-execution-v5')
   self.assertEqual(self.call('replay',checked),old)
 def test_comparison_checkpoint_mode(self):
  with tempfile.TemporaryDirectory() as d:
   for policy in ('quadratic','modular','modular-witness'):
    old=Path(d)/(policy+'old');new=Path(d)/(policy+'new')
    args=['compare','--left-numerator','-1,0,1','--left-denominator','-1,1',
     '--right-numerator','1,1','--right-denominator','1','--domain-policy',policy]
    plain=self.call(*args,'--output',old)
    checked=self.call(*args,'--verification','replay','--verification-cancellation','comparison-checkpoints',
     '--max-replay-bytes','1000000','--output',new)
    self.assertEqual(checked['result'],plain);self.assertEqual(old.read_bytes(),new.read_bytes())
    self.assertEqual(checked['execution']['contract'],'application-execution-v6')
    self.assertEqual(checked['execution']['verification_cancellation'],'comparison-checkpoints')
   self.call('grid','--path','2,6','--verification','replay','--verification-cancellation','comparison-checkpoints',ok=False)
 def test_compatibility_checkpoint_mode(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);rules=root/'rules';candidates=root/'candidates'
   rules.write_text('BRUCE_RULES_V1\nsnapshot test\nrule even positive value mod_eq 2 0 5 manual -\n')
   candidates.write_text('id,domain,layer,x,y,value\n1,math,0,0,0,2\n2,math,0,1,0,3\n')
   for limits in ([],['--max-checks','1']):
    old=root/'old';new=root/'new'
    for f in (old,new):
     if f.exists():f.unlink()
    args=['compatibility','--rules',rules,'--candidates',candidates,*limits]
    plain=self.call(*args,'--output',old)
    checked=self.call(*args,'--verification','replay','--verification-cancellation','compatibility-checkpoints','--output',new)
    self.assertEqual(checked['result'],plain);self.assertEqual(old.read_bytes(),new.read_bytes())
    self.assertEqual(checked['execution']['contract'],'application-execution-v7')
    self.assertEqual(checked['execution']['verification_cancellation'],'compatibility-checkpoints')
    self.assertEqual(self.call('replay',new),plain)
   failure=self.call('grid','--path','2,6','--verification','replay','--verification-cancellation','compatibility-checkpoints',ok=False)
   self.assertEqual(failure['execution']['failed_at'],'EXECUTING')
 def test_serialization_digest_checkpoints(self):
  with tempfile.TemporaryDirectory() as d:
   old=Path(d)/'old';new=Path(d)/'new'
   args=['grid-batch','--paths','2,6;4,3']
   plain=self.call(*args,'--output',old)
   checked=self.call(*args,'--verification','replay','--serialization-cancellation','digest-checkpoints',
    '--verification-cancellation','grid-query-boundaries','--output',new)
   self.assertEqual(checked['result'],plain);self.assertEqual(old.read_bytes(),new.read_bytes())
   self.assertEqual(checked['execution']['contract'],'application-execution-v8')
   self.assertEqual(checked['execution']['serialization_cancellation'],'digest-checkpoints')
   self.assertEqual(checked['execution']['verification_cancellation'],'grid-query-boundaries')
   self.call(*args,'--serialization-cancellation','digest-checkpoints',ok=False)
   self.call(*args,'--verification','replay','--serialization-cancellation','unknown',ok=False)
 def test_grid_serialization_records(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);old=root/'old';new=root/'new'
   args=['grid-batch','--paths','0,1;1,1;2,1','--max-query-steps','1']
   plain=self.call(*args,'--output',old)
   checked=self.call(*args,'--verification','replay','--serialization-cancellation','grid-record-checkpoints',
    '--verification-cancellation','grid-query-boundaries','--output',new)
   self.assertEqual(checked['result'],plain);self.assertEqual(old.read_bytes(),new.read_bytes())
   self.assertEqual(checked['execution']['contract'],'application-execution-v9')
   self.assertEqual(checked['execution']['serialization_cancellation'],'grid-record-checkpoints')
   out=self.call('grid-resume','--from-run',old,'--additional-queries','2','--verification','replay',
    '--serialization-cancellation','grid-record-checkpoints','--output',root/'resumed')
   self.assertEqual(self.call('replay',root/'resumed'),out['result'])
   failure=self.call('grid','--path','2,6','--verification','replay',
    '--serialization-cancellation','grid-record-checkpoints',ok=False)
   self.assertEqual(failure['execution']['failed_at'],'EXECUTING')
 def test_execution_experience_checkpoints(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);train=root/'train';hold=root/'hold';source=root/'source';store=root/'store'
   train.write_text('x,y\n0,0\n1,1\n2,2\n');hold.write_text('x,y\n3,3\n')
   args=['research','--train',train,'--holdout',hold,'--domain','rational','--strategy','portfolio-rational',
    '--min','-1','--max','1','--max-denominator','1','--max-power','2','--max-degree','2']
   self.call(*args,'--planner','portfolio','--output',source)
   self.call('experience','add','--from-run',source,'--output',store)
   train.write_text('x,y\n4,4\n5,5\n6,6\n');hold.write_text('x,y\n7,7\n')
   learned=[*args,'--planner','experience-order','--experience',store]
   old=root/'old';new=root/'new';plain=self.call(*learned,'--output',old)
   checked=self.call(*learned,'--verification','replay','--execution-cancellation','experience-checkpoints',
    '--verification-cancellation','research-checkpoints','--serialization-cancellation','digest-checkpoints','--output',new)
   self.assertEqual(checked['result'],plain);self.assertEqual(old.read_bytes(),new.read_bytes())
   self.assertEqual(checked['execution']['contract'],'application-execution-v10')
   self.assertEqual(checked['execution']['execution_cancellation'],'experience-checkpoints')
   self.assertEqual(plain['experience_records_used'],1)
   grid=root/'grid';gridhold=root/'gridhold'
   self.call('grid-batch','--paths','4,1;5,1;6,1','--output',grid)
   self.call('grid-batch','--paths','7,1','--output',gridhold)
   combined=self.call('research','--grid-train',grid,'--grid-holdout',gridhold,'--domain','rational',
    '--strategy','portfolio-rational','--planner','experience-order','--experience',store,
    '--min','-1','--max','1','--max-denominator','1','--max-power','2','--max-degree','2',
    '--verification','replay','--execution-cancellation','grid-and-experience-checkpoints')
   self.assertEqual(combined['execution']['contract'],'application-execution-v11')
   self.assertEqual(combined['execution']['execution_cancellation'],'grid-and-experience-checkpoints')

   self.call(*learned,'--execution-cancellation','experience-checkpoints',ok=False)
   failure=self.call(*args,'--planner','portfolio','--verification','replay','--execution-cancellation','experience-checkpoints',ok=False)
   self.assertEqual(failure['execution']['failed_at'],'EXECUTING')
 def test_execution_grid_source_checkpoints(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);train=root/'train';hold=root/'hold'
   self.call('grid-batch','--paths','0,2;1,2;2,2','--output',train)
   self.call('grid-batch','--paths','3,2','--output',hold)
   args=['research','--grid-train',train,'--grid-holdout',hold,'--domain','rational',
    '--strategy','polynomial','--planner','interpolate','--max-degree','2']
   old=root/'old';new=root/'new';plain=self.call(*args,'--output',old)
   checked=self.call(*args,'--verification','replay','--execution-cancellation','grid-source-checkpoints',
    '--verification-cancellation','research-checkpoints','--output',new)
   self.assertEqual(checked['result'],plain);self.assertEqual(old.read_bytes(),new.read_bytes())
   self.assertEqual(checked['execution']['contract'],'application-execution-v11')
   self.assertEqual(checked['execution']['execution_cancellation'],'grid-source-checkpoints')
   failure=self.call('grid','--path','2,6','--verification','replay',
    '--execution-cancellation','grid-source-checkpoints',ok=False)
   self.assertEqual(failure['execution']['failed_at'],'EXECUTING')
 def test_execution_compatibility_source_checkpoints(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);train=root/'train';rules=root/'rules';source=root/'source'
   train.write_text('x,y\n0,0\n1,1\n');rules.write_text('BRUCE_RULES_V1\nsnapshot test\n')
   self.call('research','--train',train,'--min','-1','--max','1','--output',source)
   args=['compatibility','--rules',rules,'--from-run',source,'--at','2']
   old=root/'old';new=root/'new';plain=self.call(*args,'--output',old)
   checked=self.call(*args,'--verification','replay','--execution-cancellation','compatibility-source-checkpoints',
    '--verification-cancellation','compatibility-checkpoints','--serialization-cancellation','digest-checkpoints','--output',new)
   self.assertEqual(checked['result'],plain);self.assertEqual(old.read_bytes(),new.read_bytes())
   self.assertEqual(checked['execution']['contract'],'application-execution-v12')
   self.assertEqual(checked['execution']['execution_cancellation'],'compatibility-source-checkpoints')
   self.call(*args,'--execution-cancellation','compatibility-source-checkpoints',ok=False)
   candidates=root/'candidates';candidates.write_text('id,domain,layer,x,y,value\n1,math,0,0,0,2\n')
   failure=self.call('compatibility','--rules',rules,'--candidates',candidates,'--verification','replay',
    '--execution-cancellation','compatibility-source-checkpoints','--output',root/'denied',ok=False)
   self.assertEqual(failure['execution']['failed_at'],'EXECUTING');self.assertFalse((root/'denied').exists())
 def test_execution_resume_import_checkpoints(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);parent=root/'parent';old=root/'old';new=root/'new'
   self.call('grid-batch','--paths','2,6;2,6;4,3;0,0','--zero-zero','stop','--max-query-steps','1','--output',parent)
   args=['grid-resume','--from-run',parent,'--additional-queries','2']
   plain=self.call(*args,'--output',old)
   checked=self.call(*args,'--verification','replay','--execution-cancellation','resume-import-checkpoints',
    '--verification-cancellation','grid-query-boundaries','--serialization-cancellation','grid-record-checkpoints','--output',new)
   self.assertEqual(checked['result'],plain);self.assertEqual(old.read_bytes(),new.read_bytes())
   self.assertEqual(checked['execution']['contract'],'application-execution-v13')
   self.assertEqual(checked['execution']['execution_cancellation'],'resume-import-checkpoints')
   self.assertEqual(self.call('replay',new),plain)
   self.call(*args,'--execution-cancellation','resume-import-checkpoints',ok=False)
   bad=self.call('grid','--path','2,6','--verification','replay','--execution-cancellation','resume-import-checkpoints',ok=False)
   self.assertEqual(bad['execution']['failed_at'],'EXECUTING')
if __name__=='__main__':unittest.main()
