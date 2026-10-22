#!/usr/bin/env python3
"""Check the CLI handbook and regenerate source index and measured examples.

Run from any directory: python3 docs/cli/verify_examples.py /absolute/path/to/bruce
Only docs/cli/source-index.md, examples.json and validation.md are regenerated.
No network access; each run uses a fresh temporary example directory.
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
BINARY = Path(sys.argv[1]).resolve()
records = []


def call(label, args, code=0, expected=None):
    args = list(map(str, args))
    result = subprocess.run([str(BINARY), *args], cwd=work, text=True,
                            capture_output=True, timeout=60)
    if result.returncode != code:
        raise RuntimeError(f'{label}: exit {result.returncode}: {result.stderr}')
    reply = json.loads(result.stdout if code == 0 else result.stderr)
    if expected:
        for path, value in expected.items():
            current = reply
            for key in path.split('.'):
                current = current[key]
            if current != value:
                raise RuntimeError(f'{label}: {path}: {current!r} != {value!r}')
    records.append({'example': label, 'argv': args, 'exit_code': code,
                    'stream': 'stdout' if code == 0 else 'stderr', 'json': reply})
    return reply


with tempfile.TemporaryDirectory(prefix='bruce-cli-reference-') as directory:
    work = Path(directory)
    for name, text in {
        'train.csv': 'x,y\n0,3\n1,5\n4,11\n',
        'hold.csv': 'x,y\n2,7\n10,23\n',
        'square.csv': 'x,y\n0,0\n1,1\n2,4\n',
        'square-hold.csv': 'x,y\n3,9\n',
        'square-new-hold.csv': 'x,y\n4,16\n',
        'target.csv': 'x,y\n4,16\n5,25\n6,36\n',
        'target-hold.csv': 'x,y\n7,49\n',
        'identity.csv': 'x,y\n0,0\n1,1\n2,2\n',
        'identity-hold.csv': 'x,y\n3,3\n',
        'old.rules': 'BRUCE_RULES_V1\nsnapshot old\n',
        'rules.txt': 'BRUCE_RULES_V1\nsnapshot demo\nrule positive positive value ge 0 0 1 manual -\n',
        'many.rules': 'BRUCE_RULES_V1\nsnapshot many\n' + ''.join(f'rule r{i} positive value ge 0 0 1 manual -\n' for i in range(128)),
        'candidates.csv': 'id,domain,layer,x,y,value\n1,demo,0,2,6,64\n2,demo,0,4,3,64\n',
        'nanocell-context.txt': 'nanocell-reference-context-v1',
        'phase-context-a.txt': 'nanocell-reference-context-v1',
        'phase-context-b.txt': 'private-phase-context-v1',
        'phase-dag-v2.spec': (
            'BRUCE_PHASE_DAG_SPEC_V1\ndag-id=cli-example\nmax-started-nodes=3\n'
            'max-receipt-bytes=65536\nmax-trace-entries=32\n\n'
            'node=a\ndepends=\ncore=1020a54050607080\nactive-index=2\n'
            'context-id=reference.context_1\ncontext-file=phase-context-a.txt\n'
            'technique=masked-match-v1\nmax-operations=16\nmax-trace-entries=18\n\n'
            'node=b\ndepends=a\ncore=1020a54050607080\nactive-index=2\n'
            'context-id=private.context_1\ncontext-file=phase-context-b.txt\n'
            'technique=rotate-match-v1\nmax-operations=16\nmax-trace-entries=18\n\n'
            'node=gate\ndepends=a,b\nadapter=evidence-gate-v1\n'),
        'phase-dag-v2-invalid.spec': 'BRUCE_PHASE_DAG_SPEC_INVALID\n',
        'phase-context-v3.txt': 'nanocell-reference-context-v1',
        'phase-dag-v3.spec': (
            'BRUCE_PHASE_DAG_SPEC_V3\ndag-id=cli-example-v3\nmax-started-nodes=3\n'
            'max-receipt-bytes=65536\nmax-trace-entries=32\n\n'
            'node=a\ndepends=\ncore=1020a54050607080\nactive-index=2\n'
            'context-id=reference.context_1\ncontext-file=phase-context-v3.txt\n'
            'technique=masked-match-v1\nmax-operations=16\nmax-trace-entries=18\n\n'
            'node=gate1\ndepends=a\nadapter=payload-gate-v1\n\n'
            'node=gate2\ndepends=gate1\nadapter=payload-gate-v1\n'),
        'phase-dag-v3-invalid.spec': 'BRUCE_PHASE_DAG_SPEC_V1\n',
        # Public RFC 8032 vector 2; no private seed is needed for trust rotation.
        'test-only-next-trust.pub': (
            'BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n'
            'test-next-rfc8032:3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c\n'),

    }.items():
        (work / name).write_text(text)
    inputs = {p.name: p.read_text() for p in sorted(work.iterdir())}
    call('mcp-invalid-port', ['mcp', '--port', '0'], 2, {'error': 'InvalidMcpPort'})
    call('mcp-extra-arguments', ['mcp', '--jobs'], 2, {'error': 'InvalidMcpArguments'})
    version = call('version', ['--version'])
    assert version['version'] == '0.102.0'
    # Public test-only RFC 8032 fixtures. No fixture key is implicitly trusted.
    fixture_dir = ROOT / 'tests/fixtures/runtime_mods'
    (work / 'test-data.brucemod').write_bytes((fixture_dir / 'math-rational-power-v1.brucemod').read_bytes())
    (work / 'test-only-trust.pub').write_bytes((fixture_dir / 'test-truststore-v1.pub').read_bytes())
    for name in ('test-only-trust.pub', 'test-only-next-trust.pub'):
        (work / name).chmod(0o600)
    call('mods-inspect', ['mods', 'inspect', 'test-data.brucemod'],
         expected={'contract': 'mod-package-inspection-v2', 'kind': 'data'})
    call('mods-verify', ['mods', 'verify', 'test-data.brucemod', '--trust-store', 'test-only-trust.pub'],
         expected={'contract': 'mod-package-verification-v2', 'verified': True})
    builtin_catalog = call('mods-list-builtins', ['mods', 'list'],
                           expected={'contract': 'bruce-mod-catalog-v2'})
    assert [entry['id'] for entry in builtin_catalog['entries']] == ['language', 'mcp', 'nanocell']
    installed = call('mods-install', ['mods', 'install', 'test-data.brucemod', '--mod-root', 'runtime-mods',
        '--trust-store', 'test-only-trust.pub'])
    installed_entry = next(entry for entry in installed['entries'] if entry['id'] == 'user.math-rational-power')
    assert installed_entry['lifecycle_state'] == 'installed_inactive'
    call('mods-list', ['mods', 'list', '--mod-root', 'runtime-mods'])
    call('mods-status', ['mods', 'status', 'user.math-rational-power', '--mod-root', 'runtime-mods'],
         expected={'contract': 'bruce-mod-catalog-v2'})
    call('route-inactive', ['route', '--text', '3/4 hoch 2', '--mod-root', 'runtime-mods'],
         expected={'status': 'UNKNOWN', 'selected_template': None})
    call('mods-activate', ['mods', 'activate', 'user.math-rational-power', '1.0.0', '--mod-root', 'runtime-mods'])
    routed = call('route-active', ['route', '--text', '3/4 hoch 2', '--mod-root', 'runtime-mods', '--output', 'route.report'],
         expected={'status': 'MATCHED', 'selected_template': 'math.rational.power.v1'})
    assert routed['candidate_addresses'] == [[1, 1, 5]]
    assert call('route-replay', ['route', 'replay', 'route.report', '--mod-root', 'runtime-mods']) == routed
    routed_v2 = call('route-active-details', ['route', '--text', '3/4 hoch 2', '--mod-root',
        'runtime-mods', '--output', 'route-v2.report', '--mod-details'],
        expected={'contract': 'route-report-v2', 'status': 'MATCHED',
                  'selected_owner.kind': 'data'})
    assert call('route-replay-details', ['route', 'replay', 'route-v2.report', '--mod-root',
        'runtime-mods', '--mod-details']) == routed_v2
    call('mods-disable', ['mods', 'disable', 'user.math-rational-power', '--mod-root', 'runtime-mods'])
    call('route-disabled', ['route', '--text', '3/4 hoch 2', '--mod-root', 'runtime-mods'], expected={'status': 'UNKNOWN'})
    call('route-stale-report', ['route', 'replay', 'route.report', '--mod-root', 'runtime-mods'],
         code=2, expected={'error': 'RouteRegistryDigestMismatch'})
    # A distinct temporary root isolates the generation workflow from V1 routing.
    call('trust-example-install', ['mods', 'install', 'test-data.brucemod',
        '--mod-root', 'trust-example-mods', '--trust-store', 'test-only-trust.pub'])
    call('trust-example-activate-mod', ['mods', 'activate', 'user.math-rational-power',
        '1.0.0', '--mod-root', 'trust-example-mods'])
    def trust(label, action, *extra, code=0, expected=None):
        return call(label, ['mods', 'trust', action, *extra, '--mod-root',
                           'trust-example-mods'], code, expected)
    trust('trust-v1-status-rejected', 'status', code=2,
          expected={'error': 'RuntimeTrustNotImported'})
    trust('trust-import-v1', 'import-v1', expected={
        'contract': 'runtime-mod-trust-v2', 'action': 'import-v1', 'result': 'ok'})
    imported = trust('trust-status', 'status', expected={
        'state.active.generation': '00000000000000000001',
        'state.active.trust_sha256': hashlib.sha256((work / 'test-only-trust.pub').read_bytes()).hexdigest(),
        'state.prepared': None, 'state.previous': None})
    trust('trust-duplicate-import', 'import-v1', code=2,
          expected={'error': 'RuntimeTrustAlreadyImported'})
    for action in ('activate', 'rollback'):
        trust('trust-missing-' + action, action, code=2,
              expected={'error': 'RuntimeTrustTransitionMissing'})
    trust('trust-prepare-retained-key', 'prepare', 'test-only-trust.pub',
          expected={'result': 'ok'})
    trust('trust-verify-retained-key', 'verify', expected={
        'active_incompatible': [], 'prepared_incompatible': [],
        'state.active.generation': '00000000000000000001',
        'state.prepared.generation': '00000000000000000002'})
    trust('trust-safe-activate', 'activate', expected={
        'deactivated': [], 'state.active.generation': '00000000000000000002'})
    trust('trust-safe-rollback', 'rollback', expected={
        'deactivated': [], 'state.active': imported['state']['active']})
    trust('trust-prepare-replacement-key', 'prepare', 'test-only-next-trust.pub',
          expected={'result': 'ok'})
    affected = [{'kind': 'data', 'mod_id': 'user.math-rational-power',
                 'version': '1.0.0', 'signing_key_id': 'test-rfc8032'}]
    trust('trust-verify-replacement-key', 'verify', expected={
        'active_incompatible': [], 'prepared_incompatible': affected,
        'state.prepared.generation': '00000000000000000003'})
    call('trust-external-store-cannot-override', ['mods', 'install', 'test-data.brucemod',
        '--mod-root', 'trust-example-mods', '--trust-store', 'test-only-next-trust.pub'],
        code=2, expected={'error': 'RuntimeModSigningKeyUntrusted'})
    trust('trust-safe-replacement-blocked', 'activate', code=2,
          expected={'error': 'RuntimeTrustActiveModsIncompatible', 'affected': affected})
    trust('trust-emergency-activate', 'activate', '--deactivate-untrusted', expected={
        'deactivated': affected, 'state.active.generation': '00000000000000000003'})
    call('trust-removed-key-activation-rejected', ['mods', 'activate', 'user.math-rational-power',
        '1.0.0', '--mod-root', 'trust-example-mods'], code=2,
        expected={'error': 'RuntimeModSigningKeyUntrusted'})
    call('trust-stale-external-store-rejected', ['mods', 'install', 'test-data.brucemod',
        '--mod-root', 'trust-example-mods', '--trust-store', 'test-only-trust.pub'],
        code=2, expected={'error': 'RuntimeModTrustStoreChanged'})
    trust('trust-verify-historical-package', 'verify', expected={
        'active_incompatible': affected, 'prepared_incompatible': []})
    trust('trust-emergency-rollback-flag', 'rollback', '--deactivate-untrusted', expected={
        'deactivated': [], 'state.active': imported['state']['active']})
    disabled = call('trust-rollback-keeps-mod-disabled', ['mods', 'status',
        'user.math-rational-power', '--mod-root', 'trust-example-mods'])
    assert next(entry for entry in disabled['entries'] if entry['id'] ==
                'user.math-rational-power')['lifecycle_state'] == 'disabled'
    for label, args, error in [
        ('trust-missing-action', ['mods', 'trust'], 'MissingRuntimeTrustAction'),
        ('trust-unknown-action', ['mods', 'trust', 'unknown'], 'UnknownRuntimeTrustAction'),
        ('trust-missing-store', ['mods', 'trust', 'prepare'], 'MissingRuntimeTrustStore'),
        ('trust-missing-root', ['mods', 'trust', 'status'], 'MissingOption'),
        ('trust-missing-root-value', ['mods', 'trust', 'status', '--mod-root'], 'MissingOptionValue'),
        ('trust-duplicate-option', ['mods', 'trust', 'status', '--mod-root', 'trust-example-mods',
                                  '--mod-root', 'trust-example-mods'], 'DuplicateOption'),
        ('trust-unexpected-flag', ['mods', 'trust', 'verify', '--deactivate-untrusted',
                                 '--mod-root', 'trust-example-mods'], 'UnknownOption'),
        ('trust-duplicate-flag', ['mods', 'trust', 'activate', '--deactivate-untrusted',
                                '--deactivate-untrusted', '--mod-root', 'trust-example-mods'], 'DuplicateOption'),
    ]:
        call(label, args, code=2, expected={'error': error})
    call('rules-propose', ['rules-propose', 'old.rules', 'rules.txt', 'change.proposal'], expected={'approved': False})
    call('rules-unchanged', ['rules-propose', 'old.rules', 'old.rules', 'same.proposal'], code=2, expected={'error': 'RuleProposalUnchanged'})
    call('rules-no-terminal', ['rules-approve', 'change.proposal', 'old.rules', 'approved.rules'], code=2, expected={'error': 'RuleApprovalRequiresTerminal'})
    call('rules-stale-base', ['rules-approve', 'change.proposal', 'rules.txt', 'approved.rules'], code=2, expected={'error': 'RuleBaseChanged'})
    assert not (work / 'approved.rules').exists()
    call('rules-stage', ['rules-stage', 'active.store', 'rules.txt', 'active.stage'], expected={'staged': True, 'approved': False})
    call('rules-activate-no-terminal', ['rules-activate', 'active.store', 'active.stage'], code=2, expected={'error': 'RuleApprovalRequiresTerminal'})
    call('rules-double-source', ['compatibility', '--rules', 'rules.txt', '--rule-store', 'active.store', '--candidates', 'candidates.csv'], code=2, expected={'error': 'UseRulesOrRuleStore'})
    assert not (work / 'active.store').exists()
    contracts = call('contracts', ['contracts'])
    call('task-contracts', ['task-contracts'])
    call('gate-contracts', ['gate-contracts'], expected={'contract': 'research-gate-policy-v1', 'general_proof': False})
    call('coordinate', ['coordinate', '--x', 2, '--y', 6], expected={'z': '42', 'local_z': 50})
    call('unpair', ['coordinate', '--z', 42], expected={'x': '2', 'y': '6'})
    for op in ['add', 'sub', 'mul', 'div', 'divmod', 'pow', 'modpow']:
        args = ['calculate', '--op', op, '--left', 12, '--right', 3]
        if op == 'modpow':
            args += ['--modulus', 5]
        call('calculate-' + op, args)
    call('rational', ['calculate', '--op', 'add', '--domain', 'rational', '--left', '1/3', '--right', '1/6'], expected={'value': '1/2'})
    call('negative-divmod', ['calculate', '--op', 'divmod', '--left', -7, '--right', -3], expected={'quotient': '3', 'remainder': '2'})
    call('bigint-beyond-int64', ['calculate', '--op', 'add', '--left', '18446744073709551616', '--right', 1], expected={'value': '18446744073709551617'})
    call('bigint-research', ['research', '--domain', 'bigint', '--train', 'train.csv', '--holdout', 'hold.csv', '--min', -3, '--max', 3], expected={'status': 'VERIFIED_ON_SAMPLES'})
    call('int64-research', ['research', '--train', 'train.csv', '--holdout', 'hold.csv', '--min', -3, '--max', 3, '--verification', 'replay', '--output', 'affine.bruce'], expected={'result.status': 'VERIFIED_ON_SAMPLES'})
    call('inspect-run', ['inspect-run', 'affine.bruce'], expected={'replay_checked': True})
    call('assess-run', ['assess-run', 'affine.bruce'], expected={'replay_checked': True, 'proof_established': False, 'claim_scope': 'declared-family-sample-evidence'})
    gate_report = call('gate-run', ['gate-run', 'affine.bruce'], expected={'replay_checked': True, 'applicable': True, 'selection.allowed': True, 'experience_source.allowed': False})
    explicit_gate_report = call('gate-compatibility-family', ['gate-run', 'affine.bruce', '--compatibility-family', 'gate'], expected={'replay_checked': True, 'applicable': True, 'selection.allowed': True, 'experience_source.allowed': False})
    assert explicit_gate_report == gate_report
    call('gate-byte-budget', ['gate-run', 'affine.bruce', '--max-replay-bytes', 33554432], expected={'replay_checked': True})
    call('gate-cancellation', ['gate-run', 'affine.bruce', '--cancellation', 'checkpoints'], expected={'replay_checked': True})
    call('gate-budget', ['gate-run', 'affine.bruce', '--max-session-checkpoints', 1000000], expected={'contract': 'application-gates-budget-v1', 'result.replay_checked': True})
    call('replay', ['replay', 'affine.bruce'], expected={'status': 'VERIFIED_ON_SAMPLES'})
    language_text = 'Untersuche @train mit @holdout auf rationale Formeln.'
    ambiguous_text = 'Untersuche @left und @right auf rationale Formeln.'
    language_draft = call('language-interpret', ['language', 'interpret', '--text', language_text,
        '--train', 'train.csv', '--holdout', 'hold.csv', '--output', 'language.draft'],
        expected={'contract': 'language-interpretation-v1', 'status': 'UNDERSTOOD',
                  'grammar_version': 'language-grammar-v1'})
    assert language_draft['inputs'][0]['alias'] == 'holdout'
    language_execution = call('language-execute', ['language', 'execute', 'language.draft',
        '--output', 'language.run'], expected={'contract': 'language-execution-v1',
        'result.contract': 'language-run-v1', 'result.core.execution.replay_verified': True})
    assert (work / 'language.run').read_bytes().startswith(b'BRUCE_MOD_RUN_V1\n')
    assert language_execution['result']['core']['result']['general_proof'] is False
    language_replay = call('language-replay', ['language', 'replay', 'language.run'],
        expected={'contract': 'language-replay-v1'})
    assert language_replay['result'] == language_execution['result']
    call('language-ambiguous', ['language', 'interpret', '--text', ambiguous_text,
        '--train', 'train.csv', '--holdout', 'hold.csv', '--output', 'ambiguous.draft'],
        expected={'status': 'AMBIGUOUS'})
    call('language-unselected-rejected', ['language', 'execute', 'ambiguous.draft',
        '--output', 'unselected.run'], code=2,
        expected={'error': 'UnselectedLanguageAlternative'})
    assert not (work / 'unselected.run').exists()
    call('language-select', ['language', 'select', 'ambiguous.draft', '--alternative',
        'train=left;holdout=right', '--output', 'selected.draft'],
        expected={'contract': 'language-selection-v1', 'selected_id': 'train=left;holdout=right'})
    call('language-selected-execute', ['language', 'execute', 'selected.draft',
        '--output', 'selected.run'], expected={'result.interpretation': 'train=left;holdout=right'})
    call('language-selected-replay', ['language', 'replay', 'selected.run'],
        expected={'contract': 'language-replay-v1'})
    call('language-unsupported', ['language', 'interpret', '--text', 'Finde alles.',
        '--train', 'train.csv', '--holdout', 'hold.csv', '--output', 'unsupported.draft'],
        code=2, expected={'error': 'UnsupportedLanguageText'})
    assert not (work / 'unsupported.draft').exists()
    task_spec = call('task-spec-create', ['task-spec', 'create', '--train', 'identity.csv',
        '--holdout', 'identity-hold.csv', '--route', 'rational-portfolio-v1',
        '--token-table-version', 'v1', '--output', 'identity.task-spec'],
        expected={'contract': 'tokenized-task-spec-v1', 'route': 'rational-portfolio-v1',
                  'token_table_version': 'v1'})
    inspected_spec = call('task-spec-inspect', ['task-spec', 'inspect', 'identity.task-spec'],
        expected={'contract': 'tokenized-task-spec-v1', 'domain': 'rational'})
    assert inspected_spec == task_spec
    assert inspected_spec['assets'] == [
        {'alias': name, 'digest': hashlib.sha256((work / filename).read_bytes()).hexdigest()}
        for name, filename in [('holdout', 'identity-hold.csv'), ('train', 'identity.csv')]
    ]
    token_ids = inspected_spec['ordered_token_ids']
    assert inspected_spec['roles'] == [
        {'role': 'holdout', 'token_id': token_ids[1], 'asset_alias': 'holdout'},
        {'role': 'route', 'token_id': token_ids[2], 'asset_alias': None},
        {'role': 'train', 'token_id': token_ids[0], 'asset_alias': 'train'},
    ]
    task_run = call('task-spec-execute', ['task-spec', 'execute', 'identity.task-spec',
        '--output', 'identity.task-run'], expected={'contract': 'tokenized-task-run-v1',
        'core.execution.replay_verified': True,
        'core.result.status': 'INCOMPLETE', 'core.result.stop_reason': 'ChildIncomplete'})
    for field in ('general_proof', 'learn_gate', 'publish_gate'):
        assert task_run['core']['result'][field] is False
    replayed_task = call('task-spec-replay', ['task-spec', 'replay', 'identity.task-run'],
        expected={'contract': 'tokenized-task-run-v1'})
    assert replayed_task == task_run
    call('task-spec-replay-spec-rejected', ['task-spec', 'replay', 'identity.task-spec'],
        code=2, expected={'error': 'InvalidTokenizedTaskRunHeader'})
    nanocell_args = ['nanocell', 'run', '--core', '1020a54050607080',
                     '--active-index', 2, '--context-id', 'reference.context_1',
                     '--context-file', 'nanocell-context.txt', '--technique', 'masked-match-v1']
    nanocell_run = call('nanocell-run', nanocell_args + ['--output', 'matched.nanocell'],
        expected={'contract': 'nanocell-cli-v1', 'status': 'MATCHED',
                  'final_state': 160, 'operations_used': 1})
    assert (work / 'matched.nanocell').is_file()
    assert (work / 'matched.nanocell').read_bytes().startswith(b'BRUCE_MOD_RUN_V1\n')
    nanocell_replay = call('nanocell-replay', ['nanocell', 'replay', 'matched.nanocell'],
        expected={'contract': 'nanocell-cli-v1', 'status': 'MATCHED'})
    assert nanocell_replay == nanocell_run
    call('nanocell-incomplete', ['nanocell', 'run', '--core', '1020824050607080',
        '--active-index', 2, '--context-id', 'reference.context_1',
        '--context-file', 'nanocell-context.txt', '--technique', 'rotate-match-v1',
        '--max-operations', 1], expected={'contract': 'nanocell-cli-v1',
                                         'status': 'INCOMPLETE', 'final_state': 5})
    call('nanocell-context-rejected', nanocell_args[:-1] + ['context-conflict-v1'],
        expected={'contract': 'nanocell-cli-v1', 'status': 'CONTEXT_REJECTED',
                  'final_state': None})
    call('nanocell-invalid-core', nanocell_args[:3] + ['1020a5405060708g'] + nanocell_args[4:],
        code=2, expected={'error': 'InvalidNanoCellCoreHex'})
    call('nanocell-unknown-technique', nanocell_args[:-1] + ['invented-v1'],
        code=2, expected={'error': 'UnknownNanoCellTechnique'})
    phase_run = call('phase-dag-v2-execute', ['phase-dag-v2', 'execute',
        'phase-dag-v2.spec', '--output', 'phase-dag-v2.bundle'],
        expected={'contract': 'phase-dag-v2-cli-v1', 'action': 'execute',
                  'dag_version': 'phase-dag-v2', 'dag_id': 'cli-example', 'node_count': 3})
    phase_bytes = (work / 'phase-dag-v2.bundle').read_bytes()
    assert phase_bytes.startswith(b'BRUCE_PHASE_DAG_RUN_V2\n')
    assert set(phase_run) == {'contract', 'action', 'dag_version', 'dag_id',
                              'node_count', 'bundle_digest', 'receipts'}
    assert phase_run['bundle_digest'] == hashlib.sha256(phase_bytes).hexdigest()
    assert phase_run['receipts'] == [
        {'node_id': node, 'outcome': 'READY', 'failure_code': ''} for node in ('a', 'b', 'gate')]
    # Portability: both readers must confirm the embedded requests, not reopen
    # the original Spec or context files. Their recorded inputs remain above.
    for name in ('phase-dag-v2.spec', 'phase-context-a.txt', 'phase-context-b.txt'):
        (work / name).unlink()
    for action in ('inspect', 'replay'):
        view = call('phase-dag-v2-' + action,
            ['phase-dag-v2', action, 'phase-dag-v2.bundle'])
        assert view == {**phase_run, 'action': action}
        assert (work / 'phase-dag-v2.bundle').read_bytes() == phase_bytes
    call('phase-dag-v2-invalid-spec', ['phase-dag-v2', 'execute',
        'phase-dag-v2-invalid.spec', '--output', 'phase-dag-v2-invalid.bundle'],
        code=2, expected={'error': 'InvalidPhaseDagSpecHeader'})
    assert not (work / 'phase-dag-v2-invalid.bundle').exists()
    phase_v3_run = call('phase-dag-v3-execute', ['phase-dag-v3', 'execute',
        'phase-dag-v3.spec', '--output', 'phase-dag-v3.bundle'],
        expected={'contract': 'phase-dag-v3-cli-v1', 'action': 'execute',
                  'dag_version': 'phase-dag-v3', 'dag_id': 'cli-example-v3', 'node_count': 3})
    phase_v3_bytes = (work / 'phase-dag-v3.bundle').read_bytes()
    assert phase_v3_bytes.startswith(b'BRUCE_PHASE_DAG_RUN_V3\n')
    assert set(phase_v3_run) == {'contract', 'action', 'dag_version', 'dag_id',
                                 'node_count', 'bundle_digest', 'receipts'}
    assert phase_v3_run['bundle_digest'] == hashlib.sha256(phase_v3_bytes).hexdigest()
    assert phase_v3_run['receipts'] == [
        {'node_id': node, 'outcome': 'READY', 'failure_code': ''}
        for node in ('a', 'gate1', 'gate2')]
    # The saved receipts prove both gates emitted COMPLETE and transported
    # their reusable payloads. The report deliberately contains neither bytes.
    assert b'nanocell-phase-payload-v1' in phase_v3_bytes
    assert b'payload-gate-payload-v1' in phase_v3_bytes
    assert phase_v3_bytes.count(b'22:payload-gate-output-v18:COMPLETE1:1') == 2
    for name in ('phase-dag-v3.spec', 'phase-context-v3.txt'):
        (work / name).unlink()
    phase_v3_views = [phase_v3_run]
    for action in ('inspect', 'replay'):
        view = call('phase-dag-v3-' + action,
            ['phase-dag-v3', action, 'phase-dag-v3.bundle'])
        assert view == {**phase_v3_run, 'action': action}
        assert (work / 'phase-dag-v3.bundle').read_bytes() == phase_v3_bytes
        phase_v3_views.append(view)
    for view in phase_v3_views:
        rendered = json.dumps(view)
        for raw in ('nanocell-phase-payload-v1', 'payload-gate-payload-v1',
                    'nanocell-reference-context-v1', '1020a54050607080'):
            assert raw not in rendered
    call('phase-dag-v3-invalid-spec', ['phase-dag-v3', 'execute',
        'phase-dag-v3-invalid.spec', '--output', 'phase-dag-v3-invalid.bundle'],
        code=2, expected={'error': 'InvalidPhaseDagSpecHeader'})
    assert not (work / 'phase-dag-v3-invalid.bundle').exists()
    call('phase-dag-v3-reject-v2', ['phase-dag-v3', 'inspect', 'phase-dag-v2.bundle'],
        code=2, expected={'error': 'InvalidPhaseDagBundleHeader'})
    call('phase-dag-v2-reject-v3', ['phase-dag-v2', 'inspect', 'phase-dag-v3.bundle'],
        code=2, expected={'error': 'InvalidPhaseDagBundleHeader'})
    header, digest, contents = phase_v3_bytes.split(b'\n', 2)
    forged_contents = contents.replace(b'5:READY', b'6:FAILED', 1)
    assert forged_contents != contents
    forged = header + b'\n' + hashlib.sha256(forged_contents).hexdigest().encode() + b'\n' + forged_contents
    corrupt = phase_v3_bytes[:-1] + bytes([phase_v3_bytes[-1] ^ 1])
    for label, bundle, error in (
        ('corrupt', corrupt, 'PhaseDagBundleDigestMismatch'),
        ('forged-receipt', forged, 'PhaseDagReplayMismatch'),
    ):
        name = 'phase-dag-v3-' + label + '.bundle'
        (work / name).write_bytes(bundle)
        for action in ('inspect', 'replay'):
            reply = call('phase-dag-v3-' + label + '-' + action,
                ['phase-dag-v3', action, name], code=2, expected={'error': error})
            assert set(reply) == {'error'}
            assert (work / name).read_bytes() == bundle
    for strategy in contracts['strategies']:
        for planner in strategy['planners']:
            if planner == 'experience-order':
                continue
            args = ['research', '--strategy', strategy['id'], '--planner', planner,
                    '--train', 'identity.csv', '--holdout', 'identity-hold.csv',
                    '--min', 0, '--max', 1, '--domain', 'rational', '--max-denominator', 1]
            if 'expression' in strategy['id']:
                args += ['--max-nodes', 3]
            call(strategy['id'] + '/' + planner, args)
    for equivalence in ['syntax', 'rational', 'rational-q', 'rational-q-modular', 'rational-q-witness']:
        args = ['research', '--strategy', 'expression', '--train', 'identity.csv', '--max-nodes', 3,
                '--equivalence', equivalence, '--operators', '+*^']
        if equivalence == 'rational-q-modular':
            args += ['--domain-cache', 'equal-64']
        call('equivalence-' + equivalence, args)
    for mode in ['exact', 'modular', 'metric']:
        args = ['grid', '--path', '2,6/4,3', '--mode', mode]
        if mode == 'modular':
            args += ['--modulus', 5]
        call('grid-' + mode, args)
    call('grid-stop', ['grid', '--path', '0,0', '--zero-zero', 'stop'], expected={'status': 'STOPPED'})
    call('grid-batch', ['grid-batch', '--paths', '2,6;4,3;2,6', '--max-query-steps', 1, '--output', 'partial.bruce'], expected={'status': 'INCOMPLETE'})
    call('grid-resume', ['grid-resume', '--from-run', 'partial.bruce', '--additional-queries', 2, '--output', 'complete.bruce'], expected={'status': 'COMPLETED'})
    for name, paths in [('gtrain', '0,2;1,2;2,2'), ('ghold', '3,2')]:
        call(name, ['grid-batch', '--paths', paths, '--output', name + '.bruce'])
    grid_research = ['research', '--grid-train', 'gtrain.bruce', '--grid-holdout', 'ghold.bruce', '--strategy', 'polynomial', '--max-degree', 2, '--min', -2, '--max', 2]
    call('grid-research', grid_research + ['--max-total-operations', 1000000])
    comparison = ['compare', '--left-numerator', '-1,0,1', '--left-denominator', '-1,1', '--right-numerator', '1,1', '--right-denominator', 1]
    for policy in ['quadratic', 'modular', 'modular-witness']:
        call('compare-' + policy, comparison + ['--domain-policy', policy], expected={'status': 'PROVEN_DIFFERENT'})
    compatibility = ['compatibility', '--rules', 'rules.txt', '--candidates', 'candidates.csv']
    call('compatibility-csv', compatibility, expected={'status': 'AMBIGUOUS'})
    origin = ['compatibility', '--rules', 'rules.txt', '--from-run', 'affine.bruce', '--at', 2]
    call('compatibility-origin', origin, expected={'status': 'SELECTED'})
    call('compatibility-stop-before-read', ['compatibility-plan', '--from-run', 'missing.bruce', '--rules', 'rules.txt', '--at', 3, '--max-transfer-checkpoints', 0, '--output', 'not-created.plan'], code=2, expected={'error': 'TransferCheckpointBudgetExceeded: checkpoints=0 limit=0'})
    call('experience-stop-before-read', ['experience', 'plan-add', '--from-run', 'missing.bruce', '--max-transfer-checkpoints', 0, '--output', 'not-created.experience-plan'], code=2, expected={'error': 'TransferCheckpointBudgetExceeded: checkpoints=0 limit=0'})
    assert not (work / 'not-created.plan').exists()
    assert not (work / 'not-created.experience-plan').exists()
    call('rule-store-plan-stop-before-load', ['compatibility-plan', '--from-run', 'affine.bruce', '--rule-store', 'missing.store', '--at', 3, '--max-transfer-checkpoints', 1, '--output', 'not-created-store.plan'], code=2, expected={'error': 'TransferCheckpointBudgetExceeded: checkpoints=1 limit=1'})
    call('rule-store-apply-stop-before-load', ['compatibility-apply', 'missing.plan', '--from-run', 'affine.bruce', '--rule-store', 'missing.store', '--max-transfer-checkpoints', 1, '--output', 'not-created-store.bruce'], code=2, expected={'error': 'TransferCheckpointBudgetExceeded: checkpoints=1 limit=1'})
    assert not (work / 'not-created-store.plan').exists()
    assert not (work / 'not-created-store.bruce').exists()
    call('compatibility-plan', ['compatibility-plan', '--from-run', 'affine.bruce', '--rules', 'rules.txt', '--at', 3, '--output', 'selection.plan'], expected={'contract': 'compatibility-transfer-v1'})
    call('compatibility-plan-controlled', ['compatibility-plan', '--from-run', 'affine.bruce', '--rules', 'rules.txt', '--at', 3, '--max-transfer-checkpoints', 1000000, '--transfer-cancellation', 'checkpoints', '--output', 'controlled-selection.plan'], expected={'contract': 'controlled-transfer-v1'})
    call('many-rules-controlled-plan', ['compatibility-plan', '--from-run', 'affine.bruce', '--rules', 'many.rules', '--at', 3, '--max-transfer-checkpoints', 1000000, '--output', 'many.plan'], expected={'contract': 'controlled-transfer-v1'})
    call('many-rules-controlled-apply', ['compatibility-apply', 'many.plan', '--from-run', 'affine.bruce', '--rules', 'many.rules', '--max-transfer-checkpoints', 1000000, '--output', 'many.bruce'], expected={'contract': 'controlled-transfer-v1'})
    call('many-rules-native-budget-plan', ['compatibility-plan', '--from-run', 'affine.bruce', '--rules', 'many.rules', '--at', 3, '--max-checks', 1, '--max-transfer-checkpoints', 1000000, '--output', 'limited-many.plan'], expected={'contract': 'controlled-transfer-v1'})
    call('many-rules-native-budget-apply', ['compatibility-apply', 'limited-many.plan', '--from-run', 'affine.bruce', '--rules', 'many.rules', '--max-transfer-checkpoints', 1000000, '--output', 'limited-many.bruce'], expected={'result.result.status': 'INCOMPLETE', 'result.result.stop_reason': 'OperationBudgetExceeded'})
    call('compatibility-apply', ['compatibility-apply', 'selection.plan', '--from-run', 'affine.bruce', '--rules', 'rules.txt', '--output', 'selection.bruce'], expected={'contract': 'compatibility-transfer-result-v1'})
    call('compatibility-transfer-replay', ['replay', 'selection.bruce'])
    selection_controlled = call('compatibility-transfer-controlled', ['compatibility-apply', 'selection.plan', '--from-run', 'affine.bruce', '--rules', 'rules.txt', '--max-transfer-checkpoints', 1000000, '--transfer-cancellation', 'checkpoints', '--output', 'controlled-selection.bruce'], expected={'contract': 'controlled-transfer-v1'})
    portfolio = ['research', '--strategy', 'portfolio-rational', '--train', 'square.csv', '--max-degree', 2, '--max-power', 2, '--min', -2, '--max', 2, '--max-denominator', 1]
    call('experience-source', portfolio + ['--holdout', 'square-hold.csv', '--output', 'source.bruce'])
    call('experience-plan-add', ['experience', 'plan-add', '--from-run', 'source.bruce', '--max-source-bytes', 1048576, '--max-store-bytes', 0, '--output', 'import.plan'], expected={'action': 'experience-add'})
    call('experience-plan-controlled', ['experience', 'plan-add', '--from-run', 'source.bruce', '--max-transfer-checkpoints', 1000000, '--transfer-cancellation', 'checkpoints', '--output', 'controlled-import.plan'], expected={'contract': 'controlled-transfer-v1'})
    call('experience-apply-add', ['experience', 'apply-add', 'import.plan', '--from-run', 'source.bruce', '--output', 'transferred.experience'], expected={'contract': 'experience-transfer-result-v1', 'result.active_records': 1})
    experience_controlled = call('experience-transfer-controlled', ['experience', 'apply-add', 'import.plan', '--from-run', 'source.bruce', '--max-transfer-checkpoints', 1000000, '--transfer-cancellation', 'checkpoints', '--output', 'controlled.experience'], expected={'contract': 'controlled-transfer-v1'})
    for label, command, measured, output in [
        ('selection', ['compatibility-apply', 'selection.plan', '--from-run', 'affine.bruce', '--rules', 'rules.txt'], selection_controlled, 'cancelled-selection.bruce'),
        ('experience', ['experience', 'apply-add', 'import.plan', '--from-run', 'source.bruce'], experience_controlled, 'cancelled.experience'),
    ]:
        limit = measured['usage']['checkpoints'] - 1
        call(label + '-stop-before-publication', command + ['--max-transfer-checkpoints', limit, '--output', output], code=2, expected={'error': f'TransferCheckpointBudgetExceeded: checkpoints={limit} limit={limit}'})
        assert not (work / output).exists()
        assert not list(work.glob(output + '.tmp.*'))
    snapshot = call('experience-add', ['experience', 'add', '--from-run', 'source.bruce', '--output', 'first.experience'])
    direct_controlled = call('experience-add-controlled', ['experience', 'add', '--from-run', 'source.bruce', '--max-transfer-checkpoints', 1000000, '--transfer-cancellation', 'checkpoints', '--output', 'controlled-first.experience'], expected={'contract': 'controlled-transfer-v1'})
    assert direct_controlled['result'] == snapshot
    assert (work / 'controlled-first.experience').read_bytes() == (work / 'first.experience').read_bytes()
    direct_limit = direct_controlled['usage']['checkpoints'] - 1
    call('experience-add-stop-before-publication', ['experience', 'add', '--from-run', 'source.bruce', '--max-transfer-checkpoints', direct_limit, '--output', 'cancelled-direct.experience'], code=2, expected={'error': f'TransferCheckpointBudgetExceeded: checkpoints={direct_limit} limit={direct_limit}'})
    assert not (work / 'cancelled-direct.experience').exists()
    assert not list(work.glob('cancelled-direct.experience.tmp.*'))
    record = snapshot['records'][0]['id']
    call('experience-inspect', ['experience', 'inspect', 'first.experience'])
    call('replacement-source', portfolio + ['--holdout', 'square-new-hold.csv', '--output', 'source2.bruce'])
    replaced = call('experience-replace', ['experience', 'replace', '--store', 'first.experience', '--id', record, '--reason', 'new holdout', '--from-run', 'source2.bruce', '--output', 'next.experience'])
    call('experience-revoke', ['experience', 'revoke', '--store', 'next.experience', '--id', replaced['records'][-1]['id'], '--reason', 'withdraw', '--output', 'revoked.experience'], expected={'active_records': 0})
    learned = ['research', '--strategy', 'portfolio-rational', '--train', 'target.csv', '--holdout', 'target-hold.csv', '--max-degree', 2, '--max-power', 2, '--min', -2, '--max', 2, '--max-denominator', 1, '--planner', 'experience-order', '--experience', 'first.experience']
    call('experience-order', learned, expected={'experience_records_used': 1})
    for option, value, args in [
        ('--verification-cancellation', 'phase-boundaries', ['grid', '--path', '2,6']),
        ('--verification-cancellation', 'grid-query-boundaries', ['grid-batch', '--paths', '2,6']),
        ('--verification-cancellation', 'research-checkpoints', grid_research),
        ('--verification-cancellation', 'comparison-checkpoints', comparison),
        ('--verification-cancellation', 'compatibility-checkpoints', compatibility),
        ('--serialization-cancellation', 'digest-checkpoints', comparison),
        ('--serialization-cancellation', 'grid-record-checkpoints', grid_research),
        ('--execution-cancellation', 'grid-source-checkpoints', grid_research),
        ('--execution-cancellation', 'experience-checkpoints', learned),
        ('--execution-cancellation', 'compatibility-source-checkpoints', origin),
        ('--execution-cancellation', 'resume-import-checkpoints', ['grid-resume', '--from-run', 'partial.bruce', '--additional-queries', 2]),
    ]:
        call(value, args + ['--verification', 'replay', option, value], expected={'execution.replay_verified': True})
    for option in ['--max-session-checkpoints', '--max-replay-bytes', '--max-serialized-bytes']:
        call(option + '-success', ['grid', '--path', '2,6', '--verification', 'replay', option, 1000000])
        call(option + '-zero', ['grid', '--path', '2,6', '--verification', 'replay', option, 0, '--output', 'forbidden.bruce'], 2, {'execution.state': 'FAILED'})
        assert not (work / 'forbidden.bruce').exists()
    for label, args, error in [
        ('subcommand-help', ['research', '--help'], 'UnknownOption: --help'),
        ('equals-option', ['grid', '--path=2,6'], 'UnknownOption: --path=2,6'),
        ('duplicate-option', ['grid', '--path', '2,6', '--path', '4,3'], 'DuplicateOption: --path'),
        ('missing-value', ['grid', '--path'], 'MissingOptionValue: --path'),
        ('nonintegral-division', ['calculate', '--op', 'div', '--left', 7, '--right', 3], 'NonIntegralDivision: quotient=2 remainder=1'),
        ('holdout-overlap', ['research', '--train', 'train.csv', '--holdout', 'train.csv'], 'HoldoutOverlapOrDuplicate'),
        ('exact-result-limit', ['research', '--domain', 'bigint', '--train', 'train.csv', '--max-results', 129], 'InvalidExactBudget'),
        ('no-overwrite', ['grid', '--path', '2,6', '--output', 'affine.bruce'], None),
    ]:
        call(label, args, 2, {'error': error} if error else None)

source = (ROOT / 'apps/bruce_cli.cpp').read_text()
nanocell_source = (ROOT / 'apps/nanocell_cli.hpp').read_text()
phase_dag_source = (ROOT / 'apps/phase_dag_v2_cli.hpp').read_text()
phase_dag_v3_source = (ROOT / 'apps/phase_dag_v3_cli.hpp').read_text()
runtime_mod_source = (ROOT / 'apps/runtime_mod_cli.hpp').read_text()
mcp_source = (ROOT / 'mods/mcp/src/mod.cpp').read_text()
options = sorted(set(re.findall(r'"(--[a-z][a-z-]*)"', source + nanocell_source + phase_dag_source + phase_dag_v3_source + runtime_mod_source + mcp_source)))
manual = '\n'.join((HERE / f).read_text() for f in ['README.md', 'commands.md', 'formats.md', 'results-and-control.md'])
missing = [option for option in options if option not in manual]
if missing:
    raise RuntimeError(f'Undocumented options: {missing}')
index = ['# Quellenbasierter Options- und Diagnoseindex', '', '[Handbuch](README.md)', '',
         'Stand Core 0.102.0. Automatisch durch `verify_examples.py` aus dem lokalen Quellbaum ermittelt.', '',
         '## Optionen nach Parserzweig', '', 'Gemeinsame Optionen sind absichtlich pro Befehl aufgeführt. Die Liste beschreibt Annahme durch den Parser, nicht die Zulässigkeit jeder Wertkombination; diese steht in der Befehlsreferenz.', '']
command = None
for line in source.splitlines():
    match = re.search(r'command=="([^"]+)"', line)
    if match:
        command = match.group(1)
    if 'const auto args=options(' in line and command not in {'experience', 'compatibility-plan', 'compatibility-apply', 'language', 'task-spec'}:
        names = re.findall(r'"(--[a-z-]+)"', line)
        index += [f'### `{command}`', '', ', '.join(f'`{name}`' for name in names), '']
index += ['### `compatibility-plan`', '',
          '`--from-run`, `--rules`, `--rule-store`, `--at`, `--max-source-bytes`, `--max-candidates`, `--max-checks`, `--max-trace`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`, `--compatibility-family`.', '',
          '### `compatibility-apply`', '',
          'Plan als Positionsargument, danach `--from-run`, `--rules`, `--rule-store`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`, `--compatibility-family`.', '',
          '### `experience`', '',
          '`add`: `--store`, `--from-run`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`, `--compatibility-family`.', '',
          '`revoke`: `--store`, `--id`, `--reason`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`, `--compatibility-family`.', '',
          '`replace`: `--store`, `--id`, `--reason`, `--from-run`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`, `--compatibility-family`.', '',
          '`inspect`: `--compatibility-family`, `--max-management-checkpoints`, `--management-cancellation`.', '',
          '`plan-add`: `--from-run`, `--store`, `--max-source-bytes`, `--max-store-bytes`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`, `--compatibility-family`.', '',
          '`apply-add`: Plan als Positionsargument, danach `--from-run`, `--store`, `--output`, `--max-transfer-checkpoints`, `--transfer-cancellation`, `--compatibility-family`.', '',
          '### `language`', '',
          '`interpret`: `--text`, `--train`, `--holdout`, `--grammar`, `--train-alias`, `--holdout-alias`, `--max-bits`, `--max-denominator`, `--max-candidates`, `--max-operations`, `--max-trace`, `--max-results`, `--output`.', '',
          '`select`: Entwurf als Positionsargument; `--alternative`, `--output`.', '',
          '`execute`: Entwurf als Positionsargument; `--output`.', '',
          '`replay`: Lauf als Positionsargument; keine Optionen.', '',
          '### `task-spec`', '',
          '`create`: `--train`, `--holdout`, `--route`, `--token-table-version`, `--output`, `--min`, `--max`, `--max-bits`, `--max-denominator`, `--max-power`, `--max-degree`, `--max-trace-bytes`, `--max-candidates`, `--max-operations`, `--max-trace`, `--max-results`.', '',
          '`inspect`: Spec als einziges Positionsargument; keine Optionen.', '',
          '`execute`: Spec als Positionsargument; `--output`.', '',
          '`replay`: Lauf als einziges Positionsargument; keine Optionen.', '',
          '### `nanocell`', '',
          '`run`: ' + ', '.join(f'`{name}`' for name in sorted(set(re.findall(r'"(--[a-z][a-z-]*)"', nanocell_source)))) + '.', '',
          '`replay`: Lauf als Positionsargument; keine Optionen.', '',
          '### `phase-dag-v2`', '',
          'Quelle: [apps/phase_dag_v2_cli.hpp](../../apps/phase_dag_v2_cli.hpp). Optional mit NanoCell ON; fester Adapterkatalog `nanocell-phase-v1`/`v1` und `evidence-gate-v1`/`v1`, seriell und begrenzt.', '',
          '`execute`: Spec als Positionsargument, danach verpflichtend ' + ', '.join(f'`{name}`' for name in sorted(set(re.findall(r'"(--[a-z][a-z-]*)"', phase_dag_source)))) + ' mit neuem Bundleziel; exklusiver Export.', '',
          '`inspect` und `replay`: genau ein Bundle als Positionsargument; keine Optionen. Beide bestätigen gespeicherte Belege durch Replay.', '',
          '### `phase-dag-v3`', '',
          'Quelle: [apps/phase_dag_v3_cli.hpp](../../apps/phase_dag_v3_cli.hpp). Optional mit NanoCell ON; getrennte V3-Marker, fester Katalog `nanocell-phase-v3`/`v1` und `payload-gate-v1`/`v1`, direkte Payloads bis 65536 Bytes. MCP prüft bestehende V2-/V3-Bundles read-only über getrennte Inspect-Routen.', '',
          '`execute`: V3-Spec als Positionsargument, danach verpflichtend `--output` mit neuem Bundleziel, anschließend optional `--max-parallel-nodes N` (Standard 1, Bereich 1..4); exklusiver atomarer Export.', '',
          '`inspect` und `replay`: genau ein V3-Bundle als Positionsargument; keine Optionen. Beide bestätigen tatsächliches Replay ohne Originaldateien und ohne Payload-Rohbytes im Report.', '',
          '### `mcp`', '',
          'Keine Zusatzargumente oder genau `--port` mit dezimalem Wert 1..65535; Standard 8080. Optionaler statischer Mod, nur read-only Loopback-Inspector; `POST /api/v1/inspect` für V2 bis 1 MiB und `POST /api/v3/inspect` für V3 bis 5 MiB. Explizite Auswahl, nur Metadaten einschließlich Payloadgrößen und Digests, keine Rohpayloads, Jobs oder Importe.', '',
          '### `mods` und `route`', '',
          '`mods inspect`: Data-/Logic-Paket als Positionsargument; keine Optionen.', '',
          '`mods verify`: Paket als Positionsargument; `--trust-store`.', '',
          '`mods install`: Paket als Positionsargument; `--mod-root`, `--trust-store`.', '',
          '`mods update`: Paket als Positionsargument; `--mod-root`, `--trust-store`.', '',
          '`mods list`: optional `--mod-root`; ohne Root nur Built-ins.', '',
          '`mods status`: Mod-ID und optional Version als Positionsargumente; optional `--mod-root`.', '',
          '`mods run`: Mod-ID als Positionsargument; `--template`, `--input`, `--mod-root`.', '',
          '`mods activate`: Mod-ID und Version als Positionsargumente; `--mod-root`.', '',
          '`mods disable`: Mod-ID als Positionsargument; `--mod-root`.', '',
          '`mods trust status|import-v1|verify`: `--mod-root`; status ist read-only.', '',
          '`mods trust prepare`: kanonischer V1-Truststore als Positionsargument; `--mod-root`; keine Aktivierung.', '',
          '`mods trust activate|rollback`: `--mod-root`, optional einmaliges Flag `--deactivate-untrusted`; standardmäßig Ablehnung bei Vertrauensverlust aktiver Mods.', '',
          '`route`: `--text`, `--mod-root`, optional `--output` und Flag `--mod-details`.', '',
          '`route replay`: Report als Positionsargument; `--mod-root`, optional Flag `--mod-details`.', '',
          '## Diagnose-Literale', '',
          'Aus `Error("…")`, Stopaufrufen und Zuweisungen von `stop_reason` in `src/`, `include/` und der CLI. Dies sind vollständige gefundene Textliterale bzw. Präfixe; dynamisch zusammengesetzte Suffixe und Betriebssystemmeldungen sind nicht enumerierbar. Nicht jeder interne Fehler ist bei jedem CLI-Aufruf erreichbar. Quellverweise zeigen die Entstehungsstelle.', '',
          '| Literal / Präfix | Quelle |', '| --- | --- |']
diagnostics = {}
for path in sorted([*ROOT.glob('src/**/*.cpp'), *ROOT.glob('mods/**/*.cpp'), *ROOT.glob('include/**/*.hpp'), *ROOT.glob('apps/*.hpp'), ROOT / 'apps/bruce_cli.cpp']):
    for number, line in enumerate(path.read_text().splitlines(), 1):
        for message in re.findall(r'(?:Error\(|stop\(|stop_reason\s*=)"([^"\n]+)"', line):
            diagnostics.setdefault(message, (path.relative_to(ROOT).as_posix(), number))
for message, (file, number) in sorted(diagnostics.items()):
    index.append(f'| `{message.replace("|", "&#124;")}` | [{file}:{number}](../../{file}) |')
(HERE / 'source-index.md').write_text('\n'.join(index) + '\n')
(HERE / 'examples.json').write_text(json.dumps({'version': version, 'inputs': inputs, 'examples': records}, ensure_ascii=False, indent=2) + '\n')
# Resolve every relative Markdown file link in the handbook; anchors are handled by readers.
for path in HERE.glob('*.md'):
    for target in re.findall(r'\]\(([^)]+)\)', path.read_text()):
        if target.startswith(('http:', 'https:', '#')):
            continue
        target = target.split('#')[0]
        if not (path.parent / target).exists():
            if target != 'validation.md':
                raise RuntimeError(f'Broken link in {path.name}: {target}')
(HERE / 'validation.md').write_text(
    '# CLI-Dokumentation: Validierung\n\n'
    '[Handbuch](README.md)\n\n'
    f'- Geprüfte CLI-Version: {version["version"]}.\n'
    f'- {len(records)} dokumentierte CLI-Aufrufe ausgeführt; erwartete Exitcodes und gezielte fachliche Aussagen geprüft.\n'
    f'- Alle {len(options)} im CLI-Quelltext gefundenen Optionsnamen sind im erklärenden Handbuch enthalten.\n'
    f'- {len(diagnostics)} Diagnose-Literale/Präfixe mit Quellverweis erfasst.\n'
    '- Relative Markdown-Dateilinks im Handbuch geprüft.\n'
    '- Eingaben, Argumente und vollständige JSON-Antworten stehen in `examples.json`.\n'
    '- MCP-Beispiele prüfen ungültige Ports und nicht unterstützte Job-Argumente; HTTP, Loopback, getrennte V2/V3-Inspect-Routen, 1-MiB-V2-/5-MiB-V3-Grenzen, Replay und Metadatenschutz ohne Rohpayloads werden in den separaten MCP-CTest-Verträgen geprüft.\n'
    '- NanoCell-Beispiele prüfen den äußeren Mod-Export, identisches Replay, `INCOMPLETE`, Kontextablehnung, ungültiges Hex und unbekannte Technik.\n'
    '- Phase-DAG-V2-Beispiele prüfen einen seriellen Drei-Knoten-Spec mit zwei Kontexten, V2-Export, vollständigen Bundle-SHA-256, READY-Receipts, identische bestätigte inspect/replay-Ansichten ohne ursprüngliche Eingaben und die Ablehnung eines ungültigen Spec ohne Export.\n'
    '- Phase-DAG-V3-Beispiele prüfen NanoCell → Payload-Gate → Payload-Gate, COMPLETE-Gateoutputs mit wiederverwendbaren Payloads, getrennte V3-Marker, vollständigen Bundle-SHA-256, READY-Receipts, identisches inspect/replay ohne Originaldateien und Reports ohne Payload-Rohbytes; ungültiger Spec, Versionsverwechslung in beide Richtungen, Korruption und neu gehashtes gefälschtes Receipt werden abgelehnt.\n'
    '- Mod-Beispiele prüfen Data-Paketinspektion, explizites Testvertrauen, Built-in-Katalog, inaktive Installation, Status, Aktivierung, bytekompatibles V1-Routing, opt-in V2-Besitzdetails, identisches Replay, Deaktivierung und Ablehnung einer veralteten Registry-Version; Logic-Build, Update und Worker-Ausführung bleiben den dedizierten CTests vorbehalten.\n'
    '- Trust-V2-Beispiele prüfen ausdrücklichen V1-Import, Generationenstatus, Prepare ohne Aktivierung, Verifikation, sicheren Wechsel und Rollback, blockierten Vertrauensverlust, explizite Notfalldeaktivierung, historische inkompatible Pakete, aktive Trust-Bindung trotz externer Datei und Parserfehler; ausschließlich öffentliche RFC-8032-Testschlüssel und isolierte temporäre Roots. Crash-Recovery, V1-Binary-Fence, Logic-Mod-Notfallrollback und laufende Worker werden durch dedizierte CTests geprüft, nicht durch diese Beispiele.\n'
    '- TaskSpec-Beispiele prüfen explizite Rollen, Asset-Digests, native Evidenz ohne Lern-/Publikations-Gates, identisches Replay und Ablehnung eines Spec als Lauf.\n'
    '- Negative Beispiele prüfen Parserfehler, doppelte Optionen, Holdout-Überlappung, BigInt-Division, Exportverweigerung, die drei Sitzungsbudgets sowie ungewählte und nicht unterstützte Spracheingaben.\n'
    '- Signaloptionen wurden auf gültige Konfiguration und erfolgreiches Replay geprüft; diese Dokumentationsprüfung misst keine Signallatenz und injiziert keine Signale.\n'
    '- Kein vollständiger Test jeder möglichen Optionskombination und kein Ersatz für die Core-Testsuite.\n\n'
    'Reproduktion vom Projektstamm:\n\n```bash\n'
    'bash docs/cli/tutorial.sh ./build/bruce\n'
    'python3 docs/cli/verify_examples.py ./build/bruce\n```\n\n'
    'Der Python-Aufruf regeneriert Options-/Diagnoseindex, Beispielantworten und diese Notiz. '
    'Die erklärenden Kapitel werden nicht automatisch neu geschrieben.\n')
print(json.dumps({'examples': len(records), 'documented_options': len(options), 'diagnostics': len(diagnostics), 'links': 'OK'}))
