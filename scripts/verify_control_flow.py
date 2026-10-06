#!/usr/bin/env python3
"""Accept the bounded research -> selection / experience flow; no production writes."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def verify(binary):
    records = []
    with tempfile.TemporaryDirectory(prefix='bruce-control-acceptance-') as directory:
        root = Path(directory)

        def call(label, args, code=0, error=None):
            args = list(map(str, args))
            process = subprocess.run([str(binary), *args], cwd=root, capture_output=True, text=True, timeout=60)
            require(process.returncode == code, f'{label}: exit {process.returncode}: {process.stderr}')
            reply = json.loads(process.stdout if code == 0 else process.stderr)
            if error:
                require(reply.get('error', '').startswith(error), f'{label}: unexpected error {reply}')
            records.append({'label': label, 'argv': args, 'exit_code': code, 'reply': reply})
            return reply

        def absent(name):
            require(not (root / name).exists(), f'unexpected publication: {name}')
            require(not list(root.glob(name + '.tmp.*')), f'temporary export residue: {name}')

        inputs = {
            'train.csv': 'x,y\n0,2\n1,3/2\n2,4/3\n3,5/4\n',
            'hold.csv': 'x,y\n5,7/6\n',
            'rules.txt': 'BRUCE_RULES_V1\nsnapshot accepted\nrule first positive value ge 0 0 1 manual -\nrule second positive value ge 0 0 1 manual -\n',
        }
        for name, content in inputs.items():
            (root / name).write_text(content)
        version = call('version', ['--version'])
        research = ['research', '--strategy', 'portfolio-rational', '--max-degree', 2,
                    '--train', 'train.csv', '--holdout', 'hold.csv', '--verification', 'replay']
        session = call('research-session', research + ['--max-session-checkpoints', 1000000, '--output', 'source.bruce'])
        require(call('source-replay', ['replay', 'source.bruce']) == session['result'], 'research replay mismatch')
        require(session['execution']['replay_verified'], 'session was not replay verified')
        gate = call('source-gates', ['gate-run', 'source.bruce', '--max-session-checkpoints', 1000000])
        require(gate['usage']['checkpoints'] > 0, 'gate budget not counted')
        require(gate['result']['selection']['allowed'] and gate['result']['experience_source']['allowed'], 'source gates not open')
        require(not gate['result']['general_proof'] and not gate['result']['learn_promotion'], 'sample evidence promoted')
        session_stop = call('session-stop', research + ['--max-session-checkpoints', 0, '--output', 'stopped-session'], 2)
        require('SessionCheckpointBudgetExceeded' in session_stop['execution']['cause'], 'wrong session failure')
        absent('stopped-session')

        def transfer(label, args, output):
            bounded = call(label, args + ['--max-transfer-checkpoints', 1000000, '--output', output])
            require(bounded['contract'] == 'controlled-transfer-v1', 'missing transfer contract')
            count = bounded['usage']['checkpoints']
            exact = output + '.exact'
            call(label + '-exact', args + ['--max-transfer-checkpoints', count, '--output', exact])
            require((root / output).read_bytes() == (root / exact).read_bytes(), 'exact budget changed bytes')
            stopped = output + '.stopped'
            call(label + '-stop', args + ['--max-transfer-checkpoints', count - 1, '--output', stopped], 2, 'TransferCheckpointBudgetExceeded')
            absent(stopped)
            return bounded['result']

        zplan = ['compatibility-plan', '--from-run', 'source.bruce', '--rules', 'rules.txt', '--at', 0]
        transfer('selection-plan', zplan, 'selection.plan')
        zapply = ['compatibility-apply', 'selection.plan', '--from-run', 'source.bruce', '--rules', 'rules.txt']
        selection = transfer('selection-apply', zapply, 'selection.bruce')
        require(call('selection-replay', ['replay', 'selection.bruce']) == selection['result'], 'selection replay mismatch')
        expplan = ['experience', 'plan-add', '--from-run', 'source.bruce']
        transfer('experience-plan', expplan, 'experience.plan')
        expapply = ['experience', 'apply-add', 'experience.plan', '--from-run', 'source.bruce']
        experience = transfer('experience-apply', expapply, 'experience.store')
        require(call('experience-inspect', ['experience', 'inspect', 'experience.store']) == experience['result'], 'snapshot audit mismatch')
        digest = hashlib.sha256((root / 'source.bruce').read_bytes()).hexdigest()
        require(selection['source_digest'] == experience['source_digest'] == digest, 'branches bound to different sources')
        require(experience['result']['active_records'] == 1, 'experience record missing')
        for report, name in [(selection, 'selection.plan'), (experience, 'experience.plan')]:
            require(report['plan_digest'] == hashlib.sha256((root / name).read_bytes()).hexdigest(), 'plan provenance mismatch')
        require(experience['snapshot_digest'] == hashlib.sha256((root / 'experience.store').read_bytes()).hexdigest(), 'snapshot provenance mismatch')

        call('selection-is-not-experience-source', ['experience', 'plan-add', '--from-run', 'selection.bruce', '--output', 'invalid.plan'], 2, 'ExperienceRequiresIndependentPortfolioSource')
        absent('invalid.plan')
        original = (root / 'source.bruce').read_bytes()
        (root / 'source.bruce').write_bytes(original + b'x')
        call('selection-source-binding', zapply + ['--output', 'changed-z'], 2, 'CompatibilityTransferSourceChanged')
        call('experience-source-binding', expapply + ['--output', 'changed-e'], 2, 'ExperienceTransferSourceChanged')
        absent('changed-z'); absent('changed-e')
        (root / 'source.bruce').write_bytes(original)
        (root / 'rules.txt').write_text(inputs['rules.txt'].replace('snapshot accepted', 'snapshot changed'))
        call('rules-binding', zapply + ['--output', 'changed-rules'], 2, 'CompatibilityTransferRulesChanged')
        absent('changed-rules')
        (root / 'rules.txt').write_text(inputs['rules.txt'])
        call('experience-base-binding', expapply + ['--store', 'experience.store', '--output', 'changed-base'], 2, 'ExperienceTransferBaseChanged')
        absent('changed-base')
        call('native-budget-plan', zplan + ['--max-checks', 1, '--output', 'limited.plan'])
        partial = call('native-budget-apply', ['compatibility-apply', 'limited.plan', '--from-run', 'source.bruce', '--rules', 'rules.txt', '--max-transfer-checkpoints', 1000000, '--output', 'partial.bruce'])
        require(partial['result']['result']['status'] == 'INCOMPLETE', 'native limit became hard failure')
        require(call('partial-replay', ['replay', 'partial.bruce']) == partial['result']['result'], 'partial replay mismatch')
        call('human-stage', ['rules-stage', 'active.store', 'rules.txt', 'proposal'])
        call('human-confirmation-required', ['rules-activate', 'active.store', 'proposal'], 2, 'RuleApprovalRequiresTerminal')
        absent('active.store')
        for name, content in inputs.items():
            require((root / name).read_text() == content, f'input changed: {name}')
        artifacts = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(root.iterdir()) if p.is_file()}
    return {'contract': 'bruce-control-flow-acceptance-v1', 'accepted': True, 'version': version,
            'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(), 'inputs': inputs, 'calls': records, 'artifacts_sha256': artifacts,
            'scope': 'independent research source with selection and experience branches; no blueprint-wide release'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--report', type=Path)
    options = parser.parse_args()
    result = verify(options.binary.resolve())
    if options.report:
        with options.report.open('x') as output:
            output.write(json.dumps(result, indent=2, ensure_ascii=False) + '\n')
    print(json.dumps({'contract': result['contract'], 'accepted': True, 'calls': len(result['calls'])}))
