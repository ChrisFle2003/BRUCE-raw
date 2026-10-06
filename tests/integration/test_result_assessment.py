import json
import os
import signal
import time
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BIN = str(Path(sys.argv.pop(1)).resolve())


class AssessmentTests(unittest.TestCase):
    def call(self, *args, code=0):
        run = subprocess.run([BIN, *map(str, args)], capture_output=True, text=True)
        self.assertEqual(run.returncode, code, run.stderr)
        return json.loads(run.stdout if code == 0 else run.stderr)

    def test_completion_is_not_proof(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, argv, complete in [
                ('stop', ['grid', '--path', '0,0', '--zero-zero', 'stop'], True),
                ('partial', ['grid-batch', '--paths', '2,6;4,3', '--max-query-steps', '0'], False),
            ]:
                path = root/name
                self.call(*argv, '--output', path)
                assessment = self.call('assess-run', path)
                self.assertEqual(assessment['result_complete'], complete)
                self.assertFalse(assessment['proof_established'])
                self.assertEqual(assessment['claim_scope'], 'grid-evaluation')
                self.assertTrue(assessment['replay_checked'])

    def test_scoped_comparison_proof_and_tampering(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'comparison'
            self.call('compare', '--left-numerator', '1', '--left-denominator', '1',
                      '--right-numerator', '2', '--right-denominator', '1', '--output', path)
            out = self.call('assess-run', path)
            self.assertTrue(out['proof_established'])
            self.assertEqual(out['claim_scope'], 'supplied-polynomial-fractions-over-Q')
            self.assertEqual(out['status'], 'PROVEN_DIFFERENT')
            path.write_text(path.read_text() + 'tampered')
            self.call('assess-run', path, code=2)

    def test_historical_research_remains_sample_evidence(self):
        fixtures = Path(__file__).resolve().parents[1]/'fixtures'
        for path in fixtures.glob('*.bruce'):
            out = self.call('assess-run', path)
            old = self.call('inspect-run', path)
            self.assertEqual(out['status'], old['status'])
            self.assertEqual(out['result_complete'], old['lifecycle']=='COMPLETED')
            if 'research' in out['kind']:
                self.assertEqual(out['claim_scope'], 'declared-family-sample-evidence')
                self.assertFalse(out['proof_established'])

    def test_incomplete_comparison_cannot_claim_proof(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'partial'
            self.call('compare', '--left-numerator', '1', '--left-denominator', '1',
                      '--right-numerator', '1', '--right-denominator', '1',
                      '--max-operations', 1, '--output', path)
            out = self.call('assess-run', path)
            self.assertFalse(out['result_complete'])
            self.assertFalse(out['proof_established'])


    def test_gate_run_replay_and_application_scope(self):
        fixtures = Path(__file__).resolve().parents[1]/'fixtures'
        for path in fixtures.glob('*.bruce'):
            out = self.call('gate-run', path)
            old = self.call('assess-run', path)
            self.assertTrue(out['replay_checked'])
            self.assertEqual(out['contract'], 'application-gates-v1')
            self.assertEqual(out['kind'], old['kind'])
            self.assertFalse(out['learn_promotion'])
            self.assertFalse(out['general_proof'])
            if 'research' not in out['kind']:
                self.assertFalse(out['applicable'])
                self.assertEqual(out['selection']['reason'], 'UnsupportedApplicationKind')
        with tempfile.TemporaryDirectory() as directory:
            bad = Path(directory)/'bad'
            bad.write_text((fixtures/'research-v1.bruce').read_text()+'tampered')
            self.call('gate-run', bad, code=2)
            self.call('gate-run', bad, 'extra', code=2)

    def test_gate_source_matches_real_import(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train = root/'train.csv';train.write_text('x,y\n0,1\n1,2\n2,3\n')
            hold = root/'hold.csv';hold.write_text('x,y\n3,4\n')
            for name, extra, accepted in [
                ('verified', ['--holdout', hold, '--max-operations', '100000'], True),
                ('no-holdout', ['--max-operations','100000'], False),
                ('partial', ['--holdout', hold, '--max-operations', '1'], False),
            ]:
                bundle=root/(name+'.bruce')
                self.call('research','--domain','rational','--strategy','portfolio-rational',
                          '--planner','portfolio','--train',train,'--min','0','--max','1',
                          '--max-denominator','1','--max-degree','2',*extra,'--output',bundle)
                out=self.call('gate-run',bundle)
                self.assertTrue(out['applicable'])
                self.assertEqual(out['experience_source']['allowed'],accepted,out)
                store=root/(name+'.store')
                imported=self.call('experience','add','--from-run',bundle,'--output',store,code=0 if accepted else 2)
                if accepted:
                    bounded=self.call('gate-run',bundle,'--max-session-checkpoints','1000000')
                    self.assertEqual(bounded['result'],out)
                    count=bounded['usage']['checkpoints']
                    stopped=self.call('gate-run',bundle,'--max-session-checkpoints',count-1,code=2)
                    self.assertTrue(stopped['error'].startswith('SessionCheckpointBudgetExceeded:'))
                if not accepted:
                    self.assertEqual(out['experience_source']['reason'], imported['error'])
                self.assertEqual(out['selection']['allowed'],name!='partial')
                if name=='partial':
                    self.assertEqual(out['selection']['reason'],'StatusNotCompleted')

    def test_gate_checkpoint_budget(self):
        source=Path(__file__).resolve().parents[1]/'fixtures'/'research-v1.bruce'
        ordinary=self.call('gate-run',source)
        full=self.call('gate-run',source,'--max-session-checkpoints','1000000')
        self.assertEqual(full['contract'],'application-gates-budget-v1')
        self.assertEqual(full['result'],ordinary)
        used=full['usage']['checkpoints']
        self.assertGreater(used,0)
        self.assertEqual(full['usage']['by_phase']['REPLAYING'],used)
        exact=self.call('gate-run',source,'--max-session-checkpoints',used)
        self.assertEqual(exact['result'],ordinary)
        for limit in [0,used-1]:
            out=self.call('gate-run',source,'--max-session-checkpoints',limit,code=2)
            self.assertTrue(out['error'].startswith('SessionCheckpointBudgetExceeded:'))
        self.call('gate-run',source,'--max-session-checkpoints','-1',code=2)
        self.call('gate-run',source,'--unknown','1',code=2)

    def test_gate_input_byte_budget(self):
        source=Path(__file__).resolve().parents[1]/'fixtures'/'research-v1.bruce'
        size=source.stat().st_size
        expected=self.call('gate-run',source)
        self.assertEqual(self.call('gate-run',source,'--max-replay-bytes',size),expected)
        both=self.call('gate-run',source,'--max-replay-bytes',size,'--max-session-checkpoints',100000)
        self.assertEqual(both['result'],expected)
        for limit in [0,size-1]:
            self.assertEqual(self.call('gate-run',source,'--max-replay-bytes',limit,code=2)['error'],'InputByteLimit')
        self.call('gate-run',source,'--max-replay-bytes','-1',code=2)
        with tempfile.TemporaryDirectory() as directory:
            invalid=Path(directory)/'invalid'
            invalid.write_text('not a bundle')
            self.assertEqual(self.call('gate-run',invalid,'--max-replay-bytes',1,code=2)['error'],'InputByteLimit')
            with invalid.open('wb') as stream:
                stream.truncate(32*1024*1024+1)
            self.assertEqual(self.call('gate-run',invalid,'--max-replay-bytes',2**64-1,code=2)['error'],'InputByteLimit')

    def test_single_grid_gate_control(self):
        source=Path(__file__).resolve().parents[1]/'fixtures'/'core-036-grid-v1.bruce'
        expected=self.call('gate-run',source)
        out=self.call('gate-run',source,'--max-session-checkpoints',1000,'--cancellation','checkpoints')
        self.assertEqual(out['result'],expected)
        self.assertFalse(out['result']['applicable'])
        used=out['usage']['checkpoints']
        self.assertGreaterEqual(used,4)
        self.assertEqual(self.call('gate-run',source,'--max-session-checkpoints',used)['result'],expected)
        self.assertTrue(self.call('gate-run',source,'--max-session-checkpoints',used-1,code=2)['error'].startswith('SessionCheckpointBudgetExceeded:'))

    def test_gate_cancellation_option(self):
        source=Path(__file__).resolve().parents[1]/'fixtures'/'research-v1.bruce'
        expected=self.call('gate-run',source)
        for mode in ['none','checkpoints']:
            self.assertEqual(self.call('gate-run',source,'--cancellation',mode),expected)
        self.call('gate-run',source,'--cancellation','invalid',code=2)

    @unittest.skipUnless(Path('/proc/self/status').exists() and hasattr(os,'mkfifo'), 'Linux signal synchronization')
    def test_gate_signals_before_first_checkpoint(self):
        payload=(Path(__file__).resolve().parents[1]/'fixtures'/'research-v1.bruce').read_bytes()
        for sig in [signal.SIGINT,signal.SIGTERM]:
            with tempfile.TemporaryDirectory() as directory:
                fifo=Path(directory)/'input';os.mkfifo(fifo)
                process=subprocess.Popen([BIN,'gate-run',str(fifo),'--cancellation','checkpoints','--max-session-checkpoints','0'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                try:
                    deadline=time.monotonic()+5
                    while True:
                        self.assertIsNone(process.poll(),'Process exited before signal synchronization')
                        status=Path(f'/proc/{process.pid}/status').read_text()
                        mask=int(next(line.split()[1] for line in status.splitlines() if line.startswith('SigCgt:')),16)
                        if mask & (1 << (sig-1)):
                            break
                        if time.monotonic()>deadline:
                            self.fail('Signal handler not installed')
                        time.sleep(0.005)
                    process.send_signal(sig)
                    with fifo.open('wb') as stream:
                        stream.write(payload)
                    out,err=process.communicate(timeout=5)
                    self.assertEqual(process.returncode,2,err)
                    self.assertEqual(out,b'')
                    self.assertEqual(json.loads(err)['error'],'GateInspectionCancelled')
                finally:
                    if process.poll() is None:
                        process.kill();process.communicate()

if __name__ == '__main__':
    unittest.main()
