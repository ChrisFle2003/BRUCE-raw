import fcntl, json, os, pathlib, pty, select, subprocess, sys, tempfile, time, unittest
BIN=str(pathlib.Path(sys.argv.pop(1)).resolve())
class Store(unittest.TestCase):
 def test_lifecycle(self):
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d); store=p/'store'; a=p/'a'; b=p/'b'; a.write_text('BRUCE_RULES_V1\nsnapshot a\n'); b.write_text('BRUCE_RULES_V1\nsnapshot b\nrule r reject value lt 0 0 0 manual -\n')
   def run(*args): return subprocess.run([BIN,*map(str,args)],text=True,capture_output=True)
   def stage(new,path): self.assertEqual(run('rules-stage',store,new,path).returncode,0)
   def start(path):
    m,s=pty.openpty(); proc=subprocess.Popen([BIN,'rules-activate',str(store),str(path)],stdin=s,stdout=s,stderr=s); os.close(s); data=b''; deadline=time.monotonic()+5
    while b'\n> ' not in data and time.monotonic()<deadline:
     if select.select([m],[],[],0.1)[0]: data+=os.read(m,65536)
    self.assertIn(b'CONFIRM ',data); return proc,m,data.split(b'CONFIRM ')[-1].split()[0]
   def finish(session,yes=True):
    proc,m,token=session;os.write(m,(b'CONFIRM '+token if yes else b'no')+b'\n');proc.wait(timeout=5);os.close(m);return proc.returncode
   stage(a,p/'first');self.assertFalse(store.exists())
   self.assertNotEqual(run('rules-activate',store,p/'first').returncode,0)
   self.assertNotEqual(finish(start(p/'first'),False),0);self.assertFalse(store.exists())
   self.assertEqual(finish(start(p/'first')),0)
   initial=store.read_bytes();self.assertEqual(json.loads(run('rules-status',store).stdout)['generation'],1)
   stage(b,p/'second');stage(b,p/'competing'); waiting=start(p/'second')
   self.assertEqual(finish(start(p/'competing')),0)
   self.assertNotEqual(finish(waiting),0); self.assertEqual(json.loads(run('rules-status',store).stdout)['generation'],2)
   stage(a,p/'rollback');self.assertEqual(finish(start(p/'rollback')),0)
   status=json.loads(run('rules-status',store).stdout);self.assertEqual(status['generation'],3);self.assertEqual(len(status['decisions']),3)
   self.assertNotEqual(run('rules-activate',store,p/'first').returncode,0) # ABA prevented
   (p/'c').write_text('id,domain,layer,x,y,value\n1,demo,0,2,6,64\n')
   result=run('compatibility','--rule-store',store,'--candidates',p/'c','--output',p/'run.bruce');self.assertEqual(result.returncode,0,result.stderr)
   self.assertEqual(json.loads(result.stdout)['snapshot_id'],'a')
   replay=run('replay',p/'run.bruce');self.assertEqual(replay.returncode,0)
   self.assertEqual(json.loads(result.stdout)['snapshot_digest'],status['active_digest'])
   stage(b,p/'fourth');before=store.read_bytes()
   with open(str(store)+'.lock','r+') as lock:
    fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    self.assertNotEqual(finish(start(p/'fourth')),0)
    self.assertEqual(store.read_bytes(),before)
   self.assertEqual(finish(start(p/'fourth')),0)
   self.assertEqual(run('replay',p/'run.bruce').stdout,replay.stdout)
   (p/'train').write_text('x,y\n0,1\n1,3\n')
   self.assertEqual(run('research','--train',p/'train','--output',p/'research').returncode,0)
   self.assertEqual(run('compatibility-plan','--from-run',p/'research','--rule-store',store,'--at',3,'--output',p/'selection.plan').returncode,0)
   self.assertEqual(run('compatibility-apply',p/'selection.plan','--from-run',p/'research','--rule-store',store).returncode,0)
   before=store.read_bytes()
   planning=['compatibility-plan','--from-run',p/'research','--rule-store',store,'--at',3]
   applying=['compatibility-apply',p/'selection.plan','--from-run',p/'research','--rule-store',store]
   for index,args in enumerate([planning,applying]):
    plain=run(*args,'--output',p/f'plain-{index}');self.assertEqual(plain.returncode,0,plain.stderr)
    controlled=run(*args,'--max-transfer-checkpoints',1000000,'--transfer-cancellation','checkpoints','--output',p/f'controlled-{index}')
    self.assertEqual(controlled.returncode,0,controlled.stderr)
    report=json.loads(controlled.stdout);self.assertEqual(report['result'],json.loads(plain.stdout))
    self.assertEqual((p/f'plain-{index}').read_bytes(),(p/f'controlled-{index}').read_bytes())
    used=report['usage']['checkpoints']
    exact=run(*args,'--max-transfer-checkpoints',used,'--output',p/f'exact-{index}')
    self.assertEqual(exact.returncode,0,exact.stderr)
    self.assertEqual((p/f'exact-{index}').read_bytes(),(p/f'plain-{index}').read_bytes())
    for limit in [1,used-1]:
     stopped=run(*args,'--max-transfer-checkpoints',limit,'--output',p/'stopped-store')
     self.assertEqual(stopped.returncode,2);self.assertIn('TransferCheckpointBudgetExceeded',json.loads(stopped.stderr)['error'])
     self.assertFalse((p/'stopped-store').exists());self.assertEqual(store.read_bytes(),before)
   self.assertEqual(store.read_bytes(),before)
   stage(a,p/'fifth');self.assertEqual(finish(start(p/'fifth')),0)
   changed=run('compatibility-apply',p/'selection.plan','--from-run',p/'research','--rule-store',store)
   self.assertEqual(changed.returncode,2);self.assertEqual(json.loads(changed.stderr)['error'],'CompatibilityTransferRulesChanged')
   damaged=p/'damaged';damaged.write_bytes((p/'fourth').read_bytes()+b'bad')
   self.assertNotEqual(run('rules-activate',store,damaged).returncode,0)
   self.assertNotEqual(run('compatibility','--rule-store',store,'--rules',a,'--candidates',p/'c').returncode,0)
   self.assertNotEqual(run('rules-activate',p/'other',p/'first').returncode,0)
   store.write_bytes(store.read_bytes()+b'bad');self.assertNotEqual(run('rules-status',store).returncode,0)
   self.assertNotEqual(store.read_bytes(),initial)
unittest.main()
