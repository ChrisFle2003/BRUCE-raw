from fractions import Fraction
import os
import signal
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
BIN=str(Path(sys.argv.pop(1)).resolve())
class ExperienceTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([BIN,*map(str,args)],capture_output=True,timeout=30)
        self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
        return json.loads(p.stdout if ok else p.stderr)
    def pairs(self,name,rows):
        p=self.root/name;p.write_text('x,y\n'+''.join(f'{x},{y}\n' for x,y in rows));return p
    def research(self,k,*opts):
        f=lambda x:Fraction(k*x+k+1,x+1)
        train=self.pairs('train',[(x,f(x)) for x in range(4)])
        hold=self.pairs('hold',[(5,f(5))])
        return self.call('research','--strategy','portfolio-rational','--max-degree',2,'--train',train,'--holdout',hold,*opts)
    def source(self,k=1,*opts):
        p=self.root/('source'+str(k));self.research(k,'--output',p,*opts);return p
    def store(self):
        p=self.root/'store';report=self.call('experience','add','--from-run',self.source(),'--output',p);return p,report
    def test_store_changes_order_and_selfcontained_replay(self):
        store,report=self.store()
        self.assertEqual(report['active_records'],1);self.assertFalse(report['learn_gate'])
        origin=self.root/'learned'
        learned=self.research(2,'--planner','experience-order','--experience',store,'--output',origin)
        baseline=self.research(2,'--planner','adaptive-order')
        self.assertEqual(baseline['strategy_order'][0],'power-affine')
        self.assertEqual(learned['strategy_order'][0],'rational-linear')
        self.assertEqual(learned['experience_records_used'],1)
        self.assertEqual(learned['experience_votes'],[0,0,1])
        self.assertEqual(learned['status'],baseline['status'])
        self.assertEqual(sorted(learned['hypotheses'],key=lambda h:h['id']),sorted(baseline['hypotheses'],key=lambda h:h['id']))
        for p in self.root.iterdir():
            if p!=origin:p.unlink()
        self.assertEqual(self.call('replay',origin),learned)
    def test_duplicate_input_and_noneligible_sources_are_rejected(self):
        store,_=self.store();before=store.read_bytes()
        self.call('experience','add','--store',store,'--from-run',self.root/'source1','--output',self.root/'dup',ok=False)
        # Reordering training rows is not a new experience.
        train=self.pairs('reordered',[(x,Fraction(x+2,x+1)) for x in reversed(range(4))])
        source=self.root/'reordered-run'
        self.call('research','--strategy','portfolio-rational','--max-degree',2,'--train',train,'--holdout',self.root/'hold','--output',source)
        self.call('experience','add','--store',store,'--from-run',source,'--output',self.root/'dup2',ok=False)
        for name,extra in [('partial',['--max-candidates',1]),('nohold',[])]:
            p=self.root/name
            if name=='partial':self.research(3,'--output',p,*extra)
            else:self.call('research','--strategy','portfolio-rational','--max-degree',2,'--train',self.root/'train','--output',p)
            self.call('experience','add','--from-run',p,'--output',self.root/(name+'store'),ok=False)
        self.assertEqual(store.read_bytes(),before)
        same=self.research(1,'--planner','experience-order','--experience',store)
        self.assertEqual(same['experience_records_used'],0)
    def test_append_revoke_and_snapshot_stability(self):
        store,first=self.store();original=store.read_bytes()
        next_store=self.root/'next';self.call('experience','add','--store',store,'--from-run',self.source(2),'--output',next_store)
        self.assertEqual(store.read_bytes(),original)
        report=self.call('experience','inspect',next_store)
        self.assertEqual((report['events'],report['active_records']),(2,2))
        self.assertEqual(report['records'][0]['id'],first['records'][0]['id'])
        revoked=self.root/'revoked'
        self.call('experience','revoke','--store',next_store,'--id',first['records'][0]['id'],'--reason','test correction','--output',revoked)
        r=self.call('experience','inspect',revoked)
        self.assertEqual((r['events'],r['active_records']),(3,1))
        self.assertEqual(r['corrections'][0]['reason'],'test correction')
        self.call('experience','revoke','--store',revoked,'--id',first['records'][0]['id'],'--reason','again','--output',self.root/'twice',ok=False)
        learned=self.research(3,'--planner','experience-order','--experience',revoked)
        self.assertEqual(learned['experience_records_used'],1)
        self.call('experience','add','--from-run',self.root/'source1','--output',store,ok=False)
    def test_context_holdout_and_candidate_budget(self):
        store,_=self.store()
        mismatch=self.research(2,'--planner','experience-order','--experience',store,'--max-power',2)
        self.assertEqual(mismatch['experience_records_used'],0)
        full=self.research(2,'--planner','experience-order','--experience',store)
        partial=self.research(2,'--planner','experience-order','--experience',store,'--max-candidates',1)
        self.assertEqual(partial['status'],'INCOMPLETE');self.assertEqual(partial['hypotheses'][0]['status'],'VERIFIED_ON_SAMPLES')
        bad=self.pairs('bad',[(5,99)])
        no=self.call('research','--strategy','portfolio-rational','--planner','experience-order','--experience',store,
                     '--train',self.root/'train','--holdout',bad,'--max-degree',2)
        self.assertEqual((full['strategy_order'],full['experience_votes']),(no['strategy_order'],no['experience_votes']))
        for flag,n in [('--max-operations',1),('--max-trace',2),('--max-trace-bytes',256)]:
            p=self.root/flag[2:];r=self.research(2,'--planner','experience-order','--experience',store,flag,n,'--output',p)
            self.assertEqual(r['status'],'INCOMPLETE');self.assertEqual(self.call('replay',p),r)
    def test_integrity_versions_and_no_recursive_learning(self):
        store,_=self.store();learned=self.root/'learned';self.research(2,'--planner','experience-order','--experience',store,'--output',learned)
        self.call('experience','add','--from-run',learned,'--output',self.root/'recursive',ok=False)
        header,_,body=store.read_text().split('\n',2)
        changed=body.replace('"status":"VERIFIED_ON_SAMPLES"','"status":"REFUTED"',1)
        self.assertNotEqual(changed,body)
        store.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
        self.call('experience','inspect',store,ok=False)
        self.research_error(store)
        fixture=Path(__file__).resolve().parents[1]/'fixtures/portfolio-v3.bruce'
        self.assertEqual(self.call('replay',fixture),json.loads(fixture.read_text().split('RESULT\n',1)[1]))
    def test_v4_projection_and_snapshot_revoke_fallback(self):
        store,report=self.store();origin=self.root/'run';self.research(2,'--planner','experience-order','--experience',store,'--output',origin)
        rules=self.root/'rules';rules.write_text('BRUCE_RULES_V1\nsnapshot experienced\n')
        selected=self.root/'selected'
        result=self.call('compatibility','--rules',rules,'--from-run',origin,'--at',0,'--output',selected)
        self.assertEqual(result['candidates'][0]['value'],'3')
        self.assertIn('Pole',self.call('compatibility','--rules',rules,'--from-run',origin,'--at',-1,ok=False)['error'])
        revoked=self.root/'revoked';self.call('experience','revoke','--store',store,'--id',report['records'][0]['id'],
                                          '--reason','withdraw evidence','--output',revoked)
        fallback=self.research(2,'--planner','experience-order','--experience',revoked)
        self.assertEqual((fallback['strategy_order'][0],fallback['experience_records_used']),('power-affine',0))
        for p in self.root.iterdir():
            if p!=selected:p.unlink()
        self.assertEqual(self.call('replay',selected),result)

    def test_source_replay_budget_cannot_be_hidden_in_whitespace(self):
        source=self.source();head,_,body=source.read_text().split('\n',2)
        body=body.replace('\noperations 1000000\n','\n operations 10000000\n',1)+'\noperations 1\n'
        source.write_text(head+'\n'+hashlib.sha256(body.encode()).hexdigest()+'\n'+body)
        error=self.call('experience','add','--from-run',source,'--output',self.root/'bad-store',ok=False)
        self.assertEqual(error['error'],'ExperienceSourceOperationLimit')

    def test_controlled_add_preserves_bytes_and_prevents_publication(self):
        def unpublished(path):
            self.assertFalse(path.exists())
            self.assertFalse(list(self.root.glob(path.name+'.tmp.*')))

        source=self.source()
        plain_output=self.root/'plain'
        plain=self.call('experience','add','--from-run',source,'--output',plain_output)
        bounded_output=self.root/'bounded'
        bounded=self.call('experience','add','--from-run',source,
                          '--max-transfer-checkpoints',1000000,
                          '--transfer-cancellation','checkpoints','--output',bounded_output)
        self.assertEqual(bounded['contract'],'controlled-transfer-v1')
        self.assertEqual(bounded['result'],plain)
        self.assertEqual(bounded_output.read_bytes(),plain_output.read_bytes())
        none_output=self.root/'none'
        none=self.call('experience','add','--from-run',source,
                       '--transfer-cancellation','none','--output',none_output)
        self.assertEqual(none['contract'],'controlled-transfer-v1')
        self.assertEqual(none['result'],plain)
        self.assertEqual(none_output.read_bytes(),plain_output.read_bytes())
        used=bounded['usage']['checkpoints']
        self.assertGreater(used,2)
        exact_output=self.root/'exact'
        exact=self.call('experience','add','--from-run',source,
                        '--max-transfer-checkpoints',used,'--output',exact_output)
        self.assertEqual(exact['result'],plain)
        self.assertEqual(exact_output.read_bytes(),plain_output.read_bytes())
        stopped=self.root/'stopped'
        error=self.call('experience','add','--from-run',source,
                        '--max-transfer-checkpoints',used-1,'--output',stopped,ok=False)
        self.assertIn('TransferCheckpointBudgetExceeded',error['error'])
        unpublished(stopped)
        missing=self.root/'missing-source'
        no_read=self.root/'no-read-output'
        error=self.call('experience','add','--from-run',missing,
                        '--max-transfer-checkpoints',0,'--output',no_read,ok=False)
        self.assertIn('TransferCheckpointBudgetExceeded',error['error'])
        unpublished(no_read)
        zero=self.root/'zero-output'
        error=self.call('experience','add','--from-run',source,
                        '--max-transfer-checkpoints',0,'--output',zero,ok=False)
        self.assertIn('TransferCheckpointBudgetExceeded',error['error'])
        unpublished(zero)

        second=self.source(2)
        stored_plain=self.root/'stored-plain'
        plain_with_store=self.call('experience','add','--store',plain_output,'--from-run',second,'--output',stored_plain)
        stored_bounded=self.root/'stored-bounded'
        bounded_with_store=self.call('experience','add','--store',plain_output,'--from-run',second,
                                     '--max-transfer-checkpoints',1000000,'--output',stored_bounded)
        self.assertEqual(bounded_with_store['result'],plain_with_store)
        self.assertEqual(stored_bounded.read_bytes(),stored_plain.read_bytes())

        existing=self.root/'existing';existing.write_bytes(b'keep')
        error=self.call('experience','add','--from-run',source,
                        '--max-transfer-checkpoints',1000000,'--output',existing,ok=False)
        self.assertIn('ExportExistsOrUnavailable',error['error'])
        self.assertEqual(existing.read_bytes(),b'keep')
        self.assertFalse(list(self.root.glob(existing.name+'.tmp.*')))

        fifo=self.root/'fifo';os.mkfifo(fifo)
        signal_stopped=self.root/'signal-stopped'
        proc=subprocess.Popen([BIN,'experience','add','--from-run',str(fifo),
                               '--transfer-cancellation','checkpoints','--output',str(signal_stopped)],
                              stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        fd=None
        try:
            deadline=time.monotonic()+5
            while fd is None:
                try: fd=os.open(fifo,os.O_WRONLY|os.O_NONBLOCK)
                except OSError:
                    if time.monotonic()>deadline: raise
                    time.sleep(0.01)
            proc.send_signal(signal.SIGTERM);os.set_blocking(fd,True)
            data=source.read_bytes()
            while data: data=data[os.write(fd,data):]
            os.close(fd);fd=None
            _,stderr=proc.communicate(timeout=5)
            self.assertEqual(proc.returncode,2,stderr)
            self.assertEqual(json.loads(stderr)['error'],'TransferCancelled')
            unpublished(signal_stopped)
        finally:
            if fd is not None: os.close(fd)
            if proc.poll() is None: proc.kill();proc.wait()

        error=self.call('experience','inspect',plain_output,'--max-transfer-checkpoints',1,ok=False)
        self.assertEqual(error['error'],'Usage: bruce experience inspect SNAPSHOT')

    def test_controlled_revoke_preserves_bytes_and_prevents_publication(self):
        def unpublished(path):
            self.assertFalse(path.exists())
            self.assertFalse(list(self.root.glob(path.name+'.tmp.*')))

        source=self.source()
        store=self.root/'store'
        initial=self.call('experience','add','--from-run',source,'--output',store)
        target=initial['records'][0]['id']
        plain_output=self.root/'plain-revoke'
        plain=self.call('experience','revoke','--store',store,'--id',target,'--reason','withdraw','--output',plain_output)
        bounded_output=self.root/'bounded-revoke'
        bounded=self.call('experience','revoke','--store',store,'--id',target,'--reason','withdraw',
                          '--max-transfer-checkpoints',1000000,'--output',bounded_output)
        self.assertEqual(bounded['contract'],'controlled-transfer-v1')
        self.assertEqual(bounded['result'],plain)
        self.assertEqual(bounded_output.read_bytes(),plain_output.read_bytes())
        none_output=self.root/'none-revoke'
        none=self.call('experience','revoke','--store',store,'--id',target,'--reason','withdraw',
                       '--transfer-cancellation','none','--output',none_output)
        self.assertEqual(none['contract'],'controlled-transfer-v1')
        self.assertEqual(none['result'],plain)
        self.assertEqual(none_output.read_bytes(),plain_output.read_bytes())
        used=bounded['usage']['checkpoints']
        exact_output=self.root/'exact-revoke'
        exact=self.call('experience','revoke','--store',store,'--id',target,'--reason','withdraw',
                        '--max-transfer-checkpoints',used,'--output',exact_output)
        self.assertEqual(exact['result'],plain)
        self.assertEqual(exact_output.read_bytes(),plain_output.read_bytes())
        stopped=self.root/'stopped-revoke'
        error=self.call('experience','revoke','--store',store,'--id',target,'--reason','withdraw',
                        '--max-transfer-checkpoints',used-1,'--output',stopped,ok=False)
        self.assertIn('TransferCheckpointBudgetExceeded',error['error'])
        unpublished(stopped)
        missing_store=self.root/'missing-store'
        no_store=self.root/'no-store-revoke'
        error=self.call('experience','revoke','--store',missing_store,'--id',target,'--reason','withdraw',
                        '--max-transfer-checkpoints',0,'--output',no_store,ok=False)
        self.assertIn('TransferCheckpointBudgetExceeded',error['error'])
        unpublished(no_store)
        no_base=self.root/'no-base-revoke'
        error=self.call('experience','revoke','--id',target,'--reason','withdraw',
                        '--max-transfer-checkpoints',0,'--output',no_base,ok=False)
        self.assertIn('TransferCheckpointBudgetExceeded',error['error'])
        unpublished(no_base)
        existing=self.root/'existing-revoke';existing.write_bytes(b'keep')
        error=self.call('experience','revoke','--store',store,'--id',target,'--reason','withdraw',
                        '--max-transfer-checkpoints',1000000,'--output',existing,ok=False)
        self.assertIn('ExportExistsOrUnavailable',error['error'])
        self.assertEqual(existing.read_bytes(),b'keep')
        self.assertFalse(list(self.root.glob(existing.name+'.tmp.*')))

        fifo=self.root/'revoke-fifo';os.mkfifo(fifo)
        signal_stopped=self.root/'signal-revoke'
        proc=subprocess.Popen([BIN,'experience','revoke','--store',str(fifo),'--id',target,'--reason','withdraw',
                               '--transfer-cancellation','checkpoints','--output',str(signal_stopped)],
                              stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        fd=None
        try:
            deadline=time.monotonic()+5
            while fd is None:
                try: fd=os.open(fifo,os.O_WRONLY|os.O_NONBLOCK)
                except OSError:
                    if time.monotonic()>deadline: raise
                    time.sleep(0.01)
            proc.send_signal(signal.SIGTERM);os.set_blocking(fd,True)
            data=store.read_bytes()
            while data: data=data[os.write(fd,data):]
            os.close(fd);fd=None
            stdout,stderr=proc.communicate(timeout=5)
            self.assertEqual(proc.returncode,2,stderr)
            self.assertEqual(stdout,b'')
            self.assertEqual(json.loads(stderr)['error'],'TransferCancelled')
            unpublished(signal_stopped)
        finally:
            if fd is not None: os.close(fd)
            if proc.poll() is None: proc.kill();proc.wait()

    def research_error(self,store):
        self.call('research','--strategy','portfolio-rational','--planner','experience-order','--experience',store,
                  '--train',self.root/'train','--holdout',self.root/'hold','--max-degree',2,ok=False)
if __name__=='__main__':unittest.main()
