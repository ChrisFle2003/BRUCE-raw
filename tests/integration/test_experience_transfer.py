import os, signal, time
import hashlib,json,pathlib,subprocess,sys,tempfile,unittest
BIN=str(pathlib.Path(sys.argv.pop(1)).resolve())
class Transfer(unittest.TestCase):
 def test_bound_import(self):
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d)
   def call(*args,ok=True):
    result=subprocess.run([BIN,*map(str,args)],capture_output=True,text=True,timeout=30)
    self.assertEqual(result.returncode,0 if ok else 2,result.stderr)
    return json.loads(result.stdout if ok else result.stderr)
   (p/'train').write_text('x,y\n0,2\n1,3/2\n2,4/3\n3,5/4\n');(p/'hold').write_text('x,y\n5,7/6\n')
   call('research','--strategy','portfolio-rational','--max-degree',2,'--train',p/'train','--holdout',p/'hold','--output',p/'source')
   call('experience','plan-add','--from-run',p/'source','--output',p/'plan')
   planning=['experience','plan-add','--from-run',p/'source']
   planned=call(*planning,'--max-transfer-checkpoints',1000000,'--transfer-cancellation','checkpoints','--output',p/'bounded-plan')
   self.assertEqual(planned['contract'],'controlled-transfer-v1')
   self.assertEqual((p/'bounded-plan').read_bytes(),(p/'plan').read_bytes())
   used_plan=planned['usage']['checkpoints'];self.assertGreater(used_plan,2)
   call(*planning,'--max-transfer-checkpoints',used_plan,'--output',p/'exact-plan')
   self.assertEqual((p/'exact-plan').read_bytes(),(p/'plan').read_bytes())
   for limit in [0,used_plan-1]:
    error=call(*planning,'--max-transfer-checkpoints',limit,'--output',p/'stopped-plan',ok=False)
    self.assertIn('TransferCheckpointBudgetExceeded',error['error']);self.assertFalse((p/'stopped-plan').exists())
   baseline=call('experience','add','--from-run',p/'source','--output',p/'baseline')
   applied=call('experience','apply-add',p/'plan','--from-run',p/'source','--output',p/'result')
   self.assertEqual(applied['result'],baseline);self.assertEqual((p/'result').read_bytes(),(p/'baseline').read_bytes())
   self.assertEqual(applied['source_digest'],hashlib.sha256((p/'source').read_bytes()).hexdigest())
   self.assertEqual(applied['snapshot_digest'],hashlib.sha256((p/'result').read_bytes()).hexdigest())
   bounded=call('experience','apply-add',p/'plan','--from-run',p/'source','--max-transfer-checkpoints',1000000,'--transfer-cancellation','checkpoints','--output',p/'bounded')
   self.assertEqual(bounded['result'],applied);self.assertGreater(bounded['usage']['checkpoints'],2)
   self.assertEqual((p/'bounded').read_bytes(),(p/'result').read_bytes())
   error=call('experience','apply-add',p/'plan','--from-run',p/'source','--max-transfer-checkpoints',0,'--output',p/'stopped',ok=False)
   self.assertIn('TransferCheckpointBudgetExceeded',error['error']);self.assertFalse((p/'stopped').exists())
   args=['experience','apply-add',p/'plan','--from-run',p/'source']
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
   proc=subprocess.Popen([BIN,*['experience','apply-add',str(p/'plan'),'--from-run',str(p/'fifo')],'--transfer-cancellation','checkpoints','--output',str(p/'signal-stopped')],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
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
   proc=subprocess.Popen([BIN,*['experience','plan-add','--from-run',str(p/'plan-fifo')],'--transfer-cancellation','checkpoints','--output',str(p/'plan-signal-stopped')],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
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
   before=(p/'result').read_bytes()
   call('experience','apply-add',p/'plan','--from-run',p/'source','--output',p/'result',ok=False);self.assertEqual((p/'result').read_bytes(),before)
   self.assertEqual(call('experience','apply-add',p/'plan','--from-run',p/'source','--store',p/'baseline','--output',p/'bad',ok=False)['error'],'ExperienceTransferBaseChanged')
   original=(p/'source').read_bytes();(p/'source').write_bytes(original+b'x')
   self.assertEqual(call('experience','apply-add',p/'plan','--from-run',p/'source','--output',p/'bad',ok=False)['error'],'ExperienceTransferSourceChanged')
   (p/'source').write_bytes(original)
   call('experience','plan-add','--from-run',p/'source','--max-source-bytes',0,'--output',p/'zero',ok=False)
   # A nonempty destination is explicitly bound and rechecked as well.
   (p/'train').write_text('x,y\n0,3\n1,5/2\n2,7/3\n3,9/4\n');(p/'hold').write_text('x,y\n5,13/6\n')
   call('research','--strategy','portfolio-rational','--max-degree',2,'--train',p/'train','--holdout',p/'hold','--output',p/'source2')
   call('experience','plan-add','--from-run',p/'source2','--store',p/'baseline','--output',p/'plan2')
   next_result=call('experience','apply-add',p/'plan2','--from-run',p/'source2','--store',p/'baseline','--output',p/'next')
   self.assertEqual(next_result['result']['active_records'],2)
   self.assertEqual(call('experience','apply-add',p/'plan2','--from-run',p/'source2','--output',p/'bad',ok=False)['error'],'ExperienceTransferBaseChanged')
   call('experience','plan-add','--from-run',p/'source','--store',p/'baseline','--output',p/'duplicate',ok=False)
   self.assertFalse((p/'duplicate').exists())
   call('experience','plan-add','--from-run',p/'source','--max-source-bytes',1048577,'--output',p/'large',ok=False)
   plan=(p/'plan').read_bytes();(p/'plan').write_bytes(plan+b'x')
   call('experience','apply-add',p/'plan','--from-run',p/'source','--output',p/'bad',ok=False)
   # A recomputed plan digest cannot make an invalid mathematical source eligible.
   (p/'source').write_text('invalid')
   body=plan.split(b'\n',1)[1].replace(hashlib.sha256(original).hexdigest().encode(),hashlib.sha256(b'invalid').hexdigest().encode())
   (p/'forged').write_bytes(hashlib.sha256(body).hexdigest().encode()+b'\n'+body)
   call('experience','apply-add',p/'forged','--from-run',p/'source','--output',p/'bad',ok=False)
   self.assertFalse((p/'bad').exists())
unittest.main()
