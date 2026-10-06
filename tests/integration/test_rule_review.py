import pathlib, subprocess, sys, tempfile, unittest, pty, os, select, time
BIN = str(pathlib.Path(sys.argv.pop(1)).resolve())
class Review(unittest.TestCase):
 def test_review(self):
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d); old=p/'old'; new=p/'new'; proposal=p/'proposal'; out=p/'approved'
   old.write_text('BRUCE_RULES_V1\nsnapshot old\n'); new.write_text('BRUCE_RULES_V1\nsnapshot new\nrule r positive value ge 0 0 1 manual -\n')
   def run(*args): return subprocess.run([BIN,*map(str,args)],capture_output=True,text=True)
   self.assertEqual(run('rules-propose',old,new,proposal).returncode,0)
   self.assertNotEqual(run('rules-approve',proposal,old,out).returncode,0); self.assertFalse(out.exists())
   def confirm(answer):
    master,slave=pty.openpty(); proc=subprocess.Popen([BIN,'rules-approve',str(proposal),str(old),str(out)],stdin=slave,stdout=slave,stderr=slave); os.close(slave)
    data=b''; deadline=time.monotonic()+5
    while b'> ' not in data and time.monotonic()<deadline:
     if select.select([master],[],[],0.1)[0]: data+=os.read(master,65536)
    self.assertIn(b'CONFIRM ',data)
    token=data.split(b'CONFIRM ')[-1].split()[0]
    os.write(master,(b'CONFIRM '+token if answer else b'no')+b'\n'); proc.wait(timeout=5); os.close(master); return proc.returncode
   self.assertNotEqual(confirm(False),0); self.assertFalse(out.exists())
   self.assertEqual(confirm(True),0); self.assertEqual(out.read_text(),new.read_text())
   self.assertNotEqual(confirm(True),0) # exclusive export preserves existing file
   old.write_text('BRUCE_RULES_V1\nsnapshot changed\n')
   self.assertIn('RuleBaseChanged',run('rules-approve',proposal,old,p/'other').stderr)
   proposal.write_text(proposal.read_text()+'tamper')
   self.assertNotEqual(run('rules-approve',proposal,old,p/'other').returncode,0)
unittest.main()
