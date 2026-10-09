import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


BIN = str(Path(sys.argv.pop(1)).resolve())
UNIQUE = 'Untersuche @train mit @holdout auf rationale Formeln.'
AMBIGUOUS = 'Untersuche @left und @right auf rationale Formeln.'
V2_UNIQUE = 'Analysiere @train anhand @holdout auf rationale Beziehungen.'
V3_UNIQUE = 'Prüfe @train gegen @holdout auf rationale Formeln.'
MOD_HEADER = b'BRUCE_MOD_RUN_V1\n'


def read_field(body, offset):
    colon = body.index(b':', offset)
    count = int(body[offset:colon])
    start = colon + 1
    return body[start:start + count], start + count


def unpack_mod_run(data):
    header, digest, body = data.split(b'\n', 2)
    assert header + b'\n' == MOD_HEADER
    assert digest == hashlib.sha256(body).hexdigest().encode()
    offset = 0
    fields = []
    for _ in range(4):
        value, offset = read_field(body, offset)
        fields.append(value)
    for _ in range(int(fields[3])):
        value, offset = read_field(body, offset)
        fields.append(value)
    while offset < len(body):
        value, offset = read_field(body, offset)
        fields.append(value)
    assert offset == len(body)
    return fields


def pack_mod_run(fields):
    body = b''.join(str(len(field)).encode() + b':' + field for field in fields)
    return MOD_HEADER + hashlib.sha256(body).hexdigest().encode() + b'\n' + body


class LanguageWorkflows(unittest.TestCase):
    def call(self, *args, ok=True):
        proc = subprocess.run([BIN, *map(str, args)], capture_output=True, timeout=60)
        self.assertEqual(proc.returncode, 0 if ok else 2, proc.stderr.decode(errors='replace'))
        self.assertFalse(proc.stderr if ok else proc.stdout)
        return json.loads(proc.stdout if ok else proc.stderr)

    def samples(self, root):
        train = root / 'train.csv'
        holdout = root / 'holdout.csv'
        train.write_bytes(b'x,y\n0,3\n1,5\n4,11\n')
        holdout.write_bytes(b'x,y\n2,7\n10,23\n')
        return train, holdout

    def interpret(self, text, train, holdout, target, *limits):
        return self.call('language', 'interpret', '--text', text, '--train', train,
                         '--holdout', holdout, *limits, '--output', target)

    def test_unique_interpret_execute_replay_preserves_native_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            draft, run = root / 'draft', root / 'run'
            interpretation = self.interpret(UNIQUE, train, holdout, draft)
            self.assertEqual(interpretation['contract'], 'language-interpretation-v1')
            self.assertEqual(interpretation['status'], 'UNDERSTOOD')
            self.assertEqual(interpretation['grammar_version'], 'language-grammar-v1')
            self.assertEqual(interpretation['raw_text'], UNIQUE)
            self.assertEqual(interpretation['alternatives'][0]['id'], 'rational-portfolio-v1')
            self.assertIsNone(interpretation['selected_id'])
            self.assertEqual({item['alias']: item['digest'] for item in interpretation['inputs']}, {
                'train': hashlib.sha256(train.read_bytes()).hexdigest(),
                'holdout': hashlib.sha256(holdout.read_bytes()).hexdigest()})
            self.assertTrue(draft.read_bytes().startswith(b'BRUCE_LANGUAGE_DRAFT_V1\n'))
            execution = self.call('language', 'execute', draft, '--output', run)
            self.assertEqual(execution['contract'], 'language-execution-v1')
            self.assertEqual(execution['result']['contract'], 'language-run-v1')
            self.assertEqual(execution['result']['core']['execution']['replay_verified'], True)
            self.assertFalse(execution['result']['core']['result']['general_proof'])
            self.assertFalse(execution['result']['core']['result']['learn_gate'])
            self.assertFalse(execution['result']['core']['result']['publish_gate'])
            self.assertIn(execution['result']['core']['result']['status'],
                          ('VERIFIED_ON_SAMPLES', 'INCOMPLETE'))
            stored = run.read_bytes()
            fields = unpack_mod_run(stored)
            self.assertEqual(fields[0:6], [b'language', b'language-mod-v1', b'bruce-mod-api-v1',
                                           b'2', b'0', b'1'])
            self.assertEqual(fields[6], b'bruce-language-draft-v1')
            self.assertEqual(fields[8], hashlib.sha256(fields[7]).hexdigest().encode())
            self.assertEqual(fields[9], b'bruce-language-run-v1')
            self.assertEqual(fields[11], hashlib.sha256(fields[10]).hexdigest().encode())
            replay = self.call('language', 'replay', run)
            self.assertEqual(replay, {'contract': 'language-replay-v1', 'result': execution['result']})
            self.assertEqual(run.read_bytes(), stored)

    def test_ambiguous_requires_selection_before_execution(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            draft, selected, run = root / 'draft', root / 'selected', root / 'run'
            result = self.interpret(AMBIGUOUS, train, holdout, draft)
            self.assertEqual(result['status'], 'AMBIGUOUS')
            self.assertEqual([item['id'] for item in result['alternatives']],
                             ['train=left;holdout=right', 'train=right;holdout=left'])
            self.assertEqual([item['alias'] for item in result['inputs']], ['left', 'right'])
            error = self.call('language', 'execute', draft, '--output', run, ok=False)
            self.assertIn('UnselectedLanguageAlternative', error['error'])
            self.assertFalse(run.exists())
            selection = self.call('language', 'select', draft, '--alternative',
                                  'train=left;holdout=right', '--output', selected)
            self.assertEqual(selection['contract'], 'language-selection-v1')
            self.assertEqual(selection['status'], 'AMBIGUOUS')
            self.assertEqual(selection['selected_id'], 'train=left;holdout=right')
            self.assertEqual(selection['selected_alternative']['id'], 'train=left;holdout=right')
            execution = self.call('language', 'execute', selected, '--output', run)
            self.assertEqual(execution['result']['interpretation'], 'train=left;holdout=right')
            self.assertTrue(execution['result']['core']['execution']['replay_verified'])
            self.assertEqual(self.call('language', 'replay', run)['result'], execution['result'])

    def test_ambiguous_reverse_selection_executes_and_replays(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            draft, selected, run = root / 'draft', root / 'selected', root / 'run'
            interpretation = self.interpret(AMBIGUOUS, train, holdout, draft)
            self.assertEqual(interpretation['status'], 'AMBIGUOUS')
            selection = self.call('language', 'select', draft, '--alternative',
                                  'train=right;holdout=left', '--output', selected)
            self.assertEqual(selection['selected_id'], 'train=right;holdout=left')
            self.assertEqual(selection['selected_alternative']['id'], 'train=right;holdout=left')
            execution = self.call('language', 'execute', selected, '--output', run)
            self.assertEqual(execution['result']['interpretation'], 'train=right;holdout=left')
            self.assertTrue(execution['result']['core']['execution']['replay_verified'])
            self.assertEqual(self.call('language', 'replay', run)['result'], execution['result'])

    def test_valid_selection_preserves_existing_export_target(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            draft, target = root / 'draft', root / 'selected'
            interpretation = self.interpret(AMBIGUOUS, train, holdout, draft)
            self.assertEqual(interpretation['status'], 'AMBIGUOUS')
            target.write_bytes(b'keep selected target exactly')
            error = self.call('language', 'select', draft, '--alternative',
                              'train=right;holdout=left', '--output', target, ok=False)
            self.assertIn('ExportExistsOrUnavailable', error['error'])
            self.assertEqual(target.read_bytes(), b'keep selected target exactly')

    def test_unsupported_and_invalid_utf8_leave_no_draft(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            target = root / 'draft'
            failure = self.call('language', 'interpret', '--text', 'Finde alles.', '--train', train,
                                '--holdout', holdout, '--output', target, ok=False)
            self.assertIn('UnsupportedLanguage', failure['error'])
            self.assertFalse(target.exists())
            argv = [str(BIN).encode(), b'language', b'interpret', b'--text', b'\xff',
                    b'--train', str(train).encode(), b'--holdout', str(holdout).encode(),
                    b'--output', str(target).encode()]
            proc = subprocess.run(argv, capture_output=True, timeout=60)
            self.assertEqual(proc.returncode, 2)
            self.assertIn('InvalidLanguageUtf8', json.loads(proc.stderr)['error'])
            self.assertFalse(target.exists())

    def test_tampering_and_exclusive_exports(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            draft, run = root / 'draft', root / 'run'
            self.interpret(UNIQUE, train, holdout, draft)
            original = draft.read_bytes()
            malformed = root / 'malformed'
            malformed.write_bytes(original + b'x')
            target = root / 'target'
            self.call('language', 'execute', malformed, '--output', target, ok=False)
            self.assertFalse(target.exists())
            source_changed = root / 'source-changed'
            header, digest, body = original.split(b'\n', 2)
            self.assertIn(b'x,y\n0,3\n', body)
            body = body.replace(b'x,y\n0,3\n', b'x,y\n0,4\n', 1)
            source_changed.write_bytes(header + b'\n' + hashlib.sha256(body).hexdigest().encode() + b'\n' + body)
            error = self.call('language', 'execute', source_changed, '--output', target, ok=False)
            self.assertIn('LanguageInputDigestMismatch', error['error'])
            self.assertFalse(target.exists())
            self.call('language', 'execute', draft, '--output', run)
            fields = unpack_mod_run(run.read_bytes())
            changed_run = root / 'changed-run'
            changed_run.write_bytes(run.read_bytes() + b'x')
            self.call('language', 'replay', changed_run, ok=False)
            changed_payload = list(fields)
            changed_payload[10] = b'not-a-language-run'
            changed_payload[11] = hashlib.sha256(changed_payload[10]).hexdigest().encode()
            changed_run.write_bytes(pack_mod_run(changed_payload))
            self.call('language', 'replay', changed_run, ok=False)
            changed_draft = list(fields)
            changed_draft[7] = b'not-a-language-draft'
            changed_draft[8] = hashlib.sha256(changed_draft[7]).hexdigest().encode()
            changed_run.write_bytes(pack_mod_run(changed_draft))
            self.call('language', 'replay', changed_run, ok=False)
            changed_manifest = list(fields)
            changed_manifest[1] = b'language-mod-v2'
            changed_run.write_bytes(pack_mod_run(changed_manifest))
            error = self.call('language', 'replay', changed_run, ok=False)
            self.assertIn('ModManifestMismatch', error['error'])
            target.write_bytes(b'keep')
            self.call('language', 'execute', draft, '--output', target, ok=False)
            self.assertEqual(target.read_bytes(), b'keep')
            self.call('language', 'select', draft, '--alternative', 'bad', '--output', target, ok=False)
            self.assertEqual(target.read_bytes(), b'keep')

    def test_small_core_budget_remains_incomplete(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            draft, run = root / 'draft', root / 'run'
            self.interpret(UNIQUE, train, holdout, draft, '--max-operations', '1')
            execution = self.call('language', 'execute', draft, '--output', run)
            self.assertEqual(execution['result']['core']['result']['status'], 'INCOMPLETE')
            self.assertFalse(execution['result']['core']['execution']['result_complete'])
            self.assertFalse(execution['result']['core']['result']['general_proof'])
            self.assertEqual(execution['result']['core']['result']['operations_used'], 1)
            self.assertEqual(execution['result']['core']['result']['strategy_runs'][0]['stop_reason'],
                             'OperationBudgetExceeded')
            self.assertEqual(self.call('language', 'replay', run)['result'], execution['result'])

    def test_v2_free_language_is_explicit_and_replayable(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            draft, run = root / 'draft-v2', root / 'run-v2'
            interpretation = self.interpret(V2_UNIQUE, train, holdout, draft, '--grammar', 'v2')
            self.assertEqual(interpretation['contract'], 'language-interpretation-v2')
            self.assertEqual(interpretation['status'], 'UNDERSTOOD')
            self.assertEqual(interpretation['grammar_version'], 'language-grammar-v2')
            self.assertEqual(interpretation['raw_text'], V2_UNIQUE)
            self.assertEqual(interpretation['normalized_text'],
                             'untersuche @train mit @holdout auf rationale formeln.')
            self.assertTrue(draft.read_bytes().startswith(b'BRUCE_LANGUAGE_DRAFT_V2\n'))
            execution = self.call('language', 'execute', draft, '--output', run)
            self.assertEqual(execution['contract'], 'language-execution-v2')
            self.assertEqual(execution['result']['contract'], 'language-run-v2')
            self.assertTrue(execution['result']['core']['execution']['replay_verified'])
            self.assertEqual(self.call('language', 'replay', run),
                             {'contract': 'language-replay-v2', 'result': execution['result']})
            stored = run.read_bytes()
            self.assertTrue(stored.startswith(b'BRUCE_MOD_RUN_V1\n'))
            fields = unpack_mod_run(stored)
            self.assertEqual(fields[6], b'bruce-language-draft-v2')
            self.assertEqual(fields[9], b'bruce-language-run-v2')
            self.assertEqual(run.read_bytes(), stored)

    def test_v2_unknown_text_maps_to_exact_error_without_draft(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            target = root / 'unsupported-v2'
            failure = self.call('language', 'interpret', '--grammar', 'v2', '--text',
                                'Finde die beste Formel.', '--train', train,
                                '--holdout', holdout, '--output', target, ok=False)
            self.assertEqual(failure, {'error': 'UnsupportedLanguageText'})
            self.assertFalse(target.exists())

    def test_v3_free_language_is_explicit_and_replayable(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            draft, run = root / 'draft-v3', root / 'run-v3'
            interpretation = self.interpret(V3_UNIQUE, train, holdout, draft, '--grammar', 'v3')
            self.assertEqual(interpretation['contract'], 'language-interpretation-v3')
            self.assertEqual(interpretation['status'], 'UNDERSTOOD')
            self.assertEqual(interpretation['grammar_version'], 'language-grammar-v3')
            self.assertEqual(interpretation['raw_text'], V3_UNIQUE)
            self.assertEqual(interpretation['normalized_text'],
                             'untersuche @train mit @holdout auf rationale formeln.')
            self.assertTrue(draft.read_bytes().startswith(b'BRUCE_LANGUAGE_DRAFT_V3\n'))
            execution = self.call('language', 'execute', draft, '--output', run)
            self.assertEqual(execution['contract'], 'language-execution-v3')
            self.assertEqual(execution['result']['contract'], 'language-run-v3')
            self.assertTrue(execution['result']['core']['execution']['replay_verified'])
            self.assertEqual(self.call('language', 'replay', run),
                             {'contract': 'language-replay-v3', 'result': execution['result']})
            stored = run.read_bytes()
            fields = unpack_mod_run(stored)
            self.assertEqual(fields[6], b'bruce-language-draft-v3')
            self.assertEqual(fields[9], b'bruce-language-run-v3')
            self.assertEqual(run.read_bytes(), stored)

    def test_v3_unknown_text_maps_to_exact_error_without_draft(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            target = root / 'unsupported-v3'
            failure = self.call('language', 'interpret', '--grammar', 'v3', '--text',
                                'Finde die beste Formel.', '--train', train,
                                '--holdout', holdout, '--output', target, ok=False)
            self.assertEqual(failure, {'error': 'UnsupportedLanguageText'})
            self.assertFalse(target.exists())

    def test_v2_ambiguity_is_visible_and_requires_selection(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            draft, selected, run = root / 'ambiguous-v2', root / 'selected-v2', root / 'run-v2'
            text = 'Analysiere @left und @right auf rationale Formeln.'
            result = self.interpret(text, train, holdout, draft, '--grammar', 'v2',
                                    '--train-alias', 'left', '--holdout-alias', 'right')
            self.assertEqual(result['contract'], 'language-interpretation-v2')
            self.assertEqual(result['status'], 'AMBIGUOUS')
            self.assertEqual([item['id'] for item in result['alternatives']],
                             ['train=left;holdout=right', 'train=right;holdout=left'])
            error = self.call('language', 'execute', draft, '--output', run, ok=False)
            self.assertIn('UnselectedLanguageAlternative', error['error'])
            self.assertFalse(run.exists())
            selection = self.call('language', 'select', draft, '--alternative',
                                  'train=left;holdout=right', '--output', selected)
            self.assertEqual(selection['contract'], 'language-selection-v2')
            execution = self.call('language', 'execute', selected, '--output', run)
            self.assertEqual(execution['contract'], 'language-execution-v2')
            self.assertEqual(self.call('language', 'replay', run)['result'], execution['result'])

    def test_existing_research_json_and_bundle_are_stable(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            first, second = root / 'first', root / 'second'
            args = ('research', '--train', train, '--holdout', holdout,
                    '--min', '0', '--max', '3')
            baseline = self.call(*args, '--output', first)
            self.assertEqual(baseline['engine'], 'bruce-core-v1')
            self.assertEqual(baseline['status'], 'VERIFIED_ON_SAMPLES')
            self.assertEqual(baseline['stop_reason'], 'Completed')
            self.assertFalse(baseline['general_proof'])
            self.assertFalse(baseline['learn_gate'])
            self.assertFalse(baseline['publish_gate'])
            self.assertEqual(hashlib.sha256(first.read_bytes()).hexdigest(),
                             'c22eb8a153f1596ba4ee0cac31e135a3f9153aebb9921c5771d4d62666e0f342')
            repeated = self.call(*args, '--output', second)
            self.assertEqual(repeated, baseline)
            self.assertEqual(second.read_bytes(), first.read_bytes())
            self.assertEqual(self.call('replay', first), baseline)


if __name__ == '__main__':
    unittest.main()
