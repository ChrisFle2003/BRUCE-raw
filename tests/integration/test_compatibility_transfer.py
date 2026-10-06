import os, signal, time
import hashlib,json,pathlib,subprocess,sys,tempfile,unittest
BIN=str(pathlib.Path(sys.argv.pop(1)).resolve())
class Transfer(unittest.TestCase):
 def test_transfer(self):
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d)
   def call(*args,ok=True):
    r=subprocess.run([BIN,*map(str,args)],capture_output=True,text=True,timeout=30);self.assertEqual(r.returncode,0 if ok else 2,r.stderr);return json.loads(r.stdout if ok else r.stderr)
   (p/'train').write_text('x,y\n0,1\n1,3\n');(p/'hold').write_text('x,y\n2,5\n')
   rules='BRUCE_RULES_V1\nsnapshot a\nrule positive positive value ge 0 0 1 manual -\nrule context context layer eq 0 0 0 manual -\n';(p/'rules').write_text(rules)
   call('research','--train',p/'train','--holdout',p/'hold','--output',p/'source')
   call('compatibility-plan','--from-run',p/'source','--rules',p/'rules','--at',3,'--output',p/'plan')
   planning=['compatibility-plan','--from-run',p/'source','--rules',p/'rules','--at',3]
   planned=call(*planning,'--max-transfer-checkpoints',1000000,'--transfer-cancellation','checkpoints','--output',p/'bounded-plan')
   self.assertEqual(planned['contract'],'controlled-transfer-v1')
   self.assertEqual((p/'bounded-plan').read_bytes(),(p/'plan').read_bytes())
   used_plan=planned['usage']['checkpoints'];self.assertGreater(used_plan,2)
   call(*planning,'--max-transfer-checkpoints',used_plan,'--output',p/'exact-plan')
   self.assertEqual((p/'exact-plan').read_bytes(),(p/'plan').read_bytes())
   for limit in [0,used_plan-1]:
    error=call(*planning,'--max-transfer-checkpoints',limit,'--output',p/'stopped-plan',ok=False)
    self.assertIn('TransferCheckpointBudgetExceeded',error['error']);self.assertFalse((p/'stopped-plan').exists())
   direct=call('compatibility','--from-run',p/'source','--rules',p/'rules','--at',3,'--output',p/'direct')
   result=call('compatibility-apply',p/'plan','--from-run',p/'source','--rules',p/'rules','--output',p/'result')
   self.assertEqual(result['result'],direct);self.assertEqual((p/'result').read_bytes(),(p/'direct').read_bytes());self.assertEqual(call('replay',p/'result'),direct)
   bounded=call('compatibility-apply',p/'plan','--from-run',p/'source','--rules',p/'rules','--max-transfer-checkpoints',1000000,'--transfer-cancellation','checkpoints','--output',p/'bounded')
   self.assertEqual(bounded['result'],result);self.assertGreater(bounded['usage']['checkpoints'],2)
   self.assertEqual((p/'bounded').read_bytes(),(p/'result').read_bytes())
   error=call('compatibility-apply',p/'plan','--from-run',p/'source','--rules',p/'rules','--max-transfer-checkpoints',0,'--output',p/'stopped',ok=False)
   self.assertIn('TransferCheckpointBudgetExceeded',error['error']);self.assertFalse((p/'stopped').exists())
   args=['compatibility-apply',p/'plan','--from-run',p/'source','--rules',p/'rules']
   # Zero budget must stop before opening even a missing input file.
   for command in [planning,args]:
    missing=list(command);missing[missing.index('--from-run')+1]=p/'absent-source'
    error=call(*missing,'--max-transfer-checkpoints',0,'--output',p/'no-read-output',ok=False)
    self.assertIn('TransferCheckpointBudgetExceeded',error['error']);self.assertFalse((p/'no-read-output').exists())
   used=bounded['usage']['checkpoints']
   call(*args,'--max-transfer-checkpoints',used,'--output',p/'exact')
   self.assertEqual((p/'exact').read_bytes(),(p/'result').read_bytes())
   call(*args,'--max-transfer-checkpoints',used-1,'--output',p/'short',ok=False);self.assertFalse((p/'short').exists())
   # FIFO open synchronizes with the already-installed signal handler before apply.
   os.mkfifo(p/'fifo')
   proc=subprocess.Popen([BIN,*['compatibility-apply',str(p/'plan'),'--from-run',str(p/'fifo'),'--rules',str(p/'rules')],'--transfer-cancellation','checkpoints','--output',str(p/'signal-stopped')],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
   fd=None
   try:
    deadline=time.monotonic()+5
    while fd is None:
     try: fd=os.open(p/'fifo',os.O_WRONLY|os.O_NONBLOCK)
     except OSError:
      if time.monotonic()>deadline: raise
      time.sleep(0.01)
    proc.send_signal(signal.SIGTERM);os.set_blocking(fd,True)
    data=(p/'source').read_bytes()
    while data: data=data[os.write(fd,data):]
    os.close(fd);fd=None
    stdout,stderr=proc.communicate(timeout=5)
    self.assertEqual(proc.returncode,2,stderr);self.assertEqual(json.loads(stderr)['error'],'TransferCancelled');self.assertFalse((p/'signal-stopped').exists())
   finally:
    if fd is not None: os.close(fd)
    if proc.poll() is None: proc.kill();proc.wait()
   # FIFO open synchronizes with the already-installed signal handler before planning.
   os.mkfifo(p/'plan-fifo')
   proc=subprocess.Popen([BIN,*['compatibility-plan','--from-run',str(p/'plan-fifo'),'--rules',str(p/'rules'),'--at','3'],'--transfer-cancellation','checkpoints','--output',str(p/'plan-signal-stopped')],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
   fd=None
   try:
    deadline=time.monotonic()+5
    while fd is None:
     try: fd=os.open(p/'plan-fifo',os.O_WRONLY|os.O_NONBLOCK)
     except OSError:
      if time.monotonic()>deadline: raise
      time.sleep(0.01)
    proc.send_signal(signal.SIGTERM);os.set_blocking(fd,True)
    data=(p/'source').read_bytes()
    while data: data=data[os.write(fd,data):]
    os.close(fd);fd=None
    stdout,stderr=proc.communicate(timeout=5)
    self.assertEqual(proc.returncode,2,stderr);self.assertEqual(json.loads(stderr)['error'],'TransferCancelled');self.assertFalse((p/'plan-signal-stopped').exists())
   finally:
    if fd is not None: os.close(fd)
    if proc.poll() is None: proc.kill();proc.wait()
   before=(p/'result').read_bytes();call('compatibility-apply',p/'plan','--from-run',p/'source','--rules',p/'rules','--output',p/'result',ok=False);self.assertEqual((p/'result').read_bytes(),before)
   (p/'rules').write_text(rules.replace('snapshot a','snapshot b'))
   self.assertEqual(call('compatibility-apply',p/'plan','--from-run',p/'source','--rules',p/'rules','--output',p/'bad',ok=False)['error'],'CompatibilityTransferRulesChanged')
   (p/'rules').write_text(rules.replace('snapshot a','snapshot   a')) # canonical binding
   call('compatibility-apply',p/'plan','--from-run',p/'source','--rules',p/'rules')
   call('compatibility-apply',p/'plan','--from-run',p/'source','--rules',p/'rules','--at',4,ok=False)
   call('compatibility-plan','--from-run',p/'source','--rules',p/'rules','--at',3,'--max-source-bytes',0,'--output',p/'zero',ok=False)
   call('compatibility-plan','--from-run',p/'source','--rules',p/'rules','--at',3,'--max-checks',1,'--output',p/'limited')
   self.assertEqual(call('compatibility-apply',p/'limited','--from-run',p/'source','--rules',p/'rules')['result']['status'],'INCOMPLETE')
   original=(p/'source').read_bytes();(p/'source').write_bytes(original+b'x')
   self.assertEqual(call('compatibility-apply',p/'plan','--from-run',p/'source','--rules',p/'rules',ok=False)['error'],'CompatibilityTransferSourceChanged')
   body=(p/'plan').read_bytes().split(b'\n',1)[1].replace(hashlib.sha256(original).hexdigest().encode(),hashlib.sha256(b'invalid').hexdigest().encode());(p/'forged').write_bytes(hashlib.sha256(body).hexdigest().encode()+b'\n'+body);(p/'source').write_bytes(b'invalid')
   call('compatibility-apply',p/'forged','--from-run',p/'source','--rules',p/'rules','--output',p/'bad',ok=False)
   self.assertFalse((p/'bad').exists())
unittest.main()
