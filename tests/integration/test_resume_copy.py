import json, subprocess, sys, tempfile
from pathlib import Path
binary=str(Path(sys.argv[1]).resolve())
def call(*args,code=0):
 p=subprocess.run([binary,*map(str,args)],capture_output=True,text=True)
 assert p.returncode==code,p.stderr
 return json.loads(p.stdout if code==0 else p.stderr)
with tempfile.TemporaryDirectory() as directory:
 root=Path(directory);parent=root/'parent';plain=root/'plain';checked=root/'checked';failed=root/'failed'
 call('grid-batch','--paths','0'*70000+'2,6;4,3','--max-query-steps',1,'--output',parent)
 assert parent.stat().st_size>65536
 args=['grid-resume','--from-run',parent,'--additional-queries',1,'--verification','replay']
 original=call(*args,'--output',plain)
 result=call(*args,'--max-session-checkpoints',1000000,'--output',checked)
 assert result['result']==original['result']
 assert checked.read_bytes()==plain.read_bytes()
 assert call('replay',checked)==result['result']
 usage=result['execution']['session_budget']
 limit=usage['by_phase']['EXECUTING']+2
 error=call(*args,'--max-session-checkpoints',limit,'--output',failed,code=2)
 assert error['execution']['failed_at']=='SERIALIZING'
 assert error['execution']['cause'].startswith('SessionCheckpointBudgetExceeded:')
 assert error['execution']['session_budget']['checkpoints']==limit
 assert not failed.exists()
 failed.write_text('preserve')
 call(*args,'--max-session-checkpoints',limit,'--output',failed,code=2)
 assert failed.read_text()=='preserve'
print('Large parent CLI: byte-identical export/replay and failed copy budget without export')
