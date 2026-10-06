import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


BIN = str(Path(sys.argv.pop(1)).resolve())


def field(value):
    return str(len(value)).encode() + b':' + value


def artifact_fields(original):
    header, _, body = original.split(b'\n', 2)
    values, offset = [], 0
    while offset < len(body):
        colon = body.index(b':', offset)
        length = int(body[offset:colon])
        offset = colon + 1 + length
        values.append(body[colon + 1:offset])
    return header, values


def artifact(header, values):
    body = b''.join(map(field, values))
    return header + b'\n' + hashlib.sha256(body).hexdigest().encode() + b'\n' + body


class TaskSpecWorkflows(unittest.TestCase):
    def call(self, *args, ok=True, raw=False):
        proc = subprocess.run([BIN, *map(str, args)], capture_output=True, timeout=60)
        self.assertEqual(proc.returncode, 0 if ok else 2, proc.stderr.decode(errors='replace'))
        self.assertFalse(proc.stderr if ok else proc.stdout)
        output = proc.stdout if ok else proc.stderr
        return output if raw else json.loads(output)

    def samples(self, root):
        train, holdout = root / 'train.csv', root / 'holdout.csv'
        train.write_bytes(b'x,y\n0,1\n1,1/2\n')
        holdout.write_bytes(b'x,y\n2,1/3\n')
        return train, holdout

    def create(self, train, holdout, target, *extra):
        return self.call('task-spec', 'create', '--train', train, '--holdout', holdout,
                         '--route', 'rational-portfolio-v1', '--token-table-version', 'v1',
                         *extra, '--output', target)

    def test_create_and_inspect_are_canonical(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            first, second = root / 'first.spec', root / 'second.spec'
            created = self.create(train, holdout, first)
            self.create(train, holdout, second)
            self.assertTrue(first.read_bytes().startswith(b'BRUCE_TOKENIZED_TASK_SPEC_V1\n'))
            self.assertEqual(first.read_bytes(), second.read_bytes())
            inspected = self.call('task-spec', 'inspect', first)
            self.assertEqual(created, inspected)
            self.assertEqual(inspected['contract'], 'tokenized-task-spec-v1')
            self.assertEqual(inspected['task_spec_version'], 'tokenized-task-spec-v1')
            self.assertEqual(inspected['normalization_version'], 'ascii-whitespace-case-v1')
            self.assertEqual(inspected['token_table_version'], 'v1')
            self.assertEqual(inspected['route'], 'rational-portfolio-v1')
            self.assertEqual(inspected['domain'], 'rational')
            self.assertIsNone(inspected['selected_id'])
            self.assertNotIn('result', inspected)
            self.assertEqual(inspected['assets'], [
                {'alias': 'holdout', 'digest': hashlib.sha256(holdout.read_bytes()).hexdigest()},
                {'alias': 'train', 'digest': hashlib.sha256(train.read_bytes()).hexdigest()},
            ])
            token_ids = inspected['ordered_token_ids']
            self.assertEqual(len(token_ids), 3)
            self.assertEqual(len(set(token_ids)), 3)
            self.assertTrue(all(len(token_id) == 64 for token_id in token_ids))
            self.assertEqual(inspected['roles'], [
                {'role': 'holdout', 'token_id': token_ids[1], 'asset_alias': 'holdout'},
                {'role': 'route', 'token_id': token_ids[2], 'asset_alias': None},
                {'role': 'train', 'token_id': token_ids[0], 'asset_alias': 'train'},
            ])
            self.assertEqual(inspected['budget'], {
                'max_candidates': 100000, 'max_operations': 1000000,
                'max_trace': 100002, 'max_results': 128,
            })
            self.assertEqual(inspected['limits'], {
                'min': '-8', 'max': '8', 'max_bits': 4096, 'max_denominator': 8,
                'max_power': 4, 'max_degree': 3, 'max_trace_bytes': 1048576,
            })
            existing = self.call('task-spec', 'create', '--train', train, '--holdout', holdout,
                                 '--route', 'rational-portfolio-v1', '--token-table-version', 'v1',
                                 '--output', first, ok=False)
            self.assertIn('ExportExistsOrUnavailable', existing['error'])
            self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_execute_replay_preserve_native_evidence_and_exclusive_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            spec, run = root / 'task.spec', root / 'task.run'
            self.create(train, holdout, spec, '--max-candidates', '3', '--max-operations', '200',
                        '--max-trace', '24', '--max-results', '2')
            original_spec = spec.read_bytes()
            executed_stdout = self.call('task-spec', 'execute', spec, '--output', run, raw=True)
            executed = json.loads(executed_stdout)
            self.assertEqual(executed['contract'], 'tokenized-task-run-v1')
            self.assertEqual(executed['spec_digest'], hashlib.sha256(original_spec).hexdigest())
            self.assertEqual(executed['core']['execution']['contract'], 'application-execution-v1')
            self.assertTrue(executed['core']['execution']['replay_verified'])
            self.assertFalse(executed['core']['execution']['result_complete'])
            native = executed['core']['result']
            self.assertEqual(native['status'], 'INCOMPLETE')
            for field in ('general_proof', 'learn_gate', 'publish_gate'):
                self.assertFalse(native[field])
            original_run = run.read_bytes()
            self.assertTrue(original_run.startswith(b'BRUCE_TOKENIZED_TASK_RUN_V1\n'))
            # Replays are self-contained even when source files no longer exist.
            train.unlink()
            holdout.unlink()
            self.assertEqual(self.call('task-spec', 'replay', run, raw=True), executed_stdout)
            self.assertEqual(spec.read_bytes(), original_spec)
            error = self.call('task-spec', 'execute', spec, '--output', run, ok=False)
            self.assertIn('ExportExistsOrUnavailable', error['error'])
            self.assertEqual(run.read_bytes(), original_run)
            self.assertIn('ExportExistsOrUnavailable', self.call(
                'task-spec', 'execute', spec, '--output', spec, ok=False)['error'])
            self.assertEqual(spec.read_bytes(), original_spec)

    def test_canonical_unsupported_table_cannot_inspect_or_execute(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            spec, run = root / 'v2.spec', root / 'rejected.run'
            self.create(train, holdout, spec)
            header, values = artifact_fields(spec.read_bytes())
            values[2] = b'v2'
            replacements = {}
            table_end = 5 + 4 * int(values[4])
            for entry in range(5, table_end, 4):
                seed = field(b'v2') + field(values[entry + 2]) + field(values[entry + 3])
                replacements[values[entry]] = hashlib.sha256(seed).hexdigest().encode()
            values = [replacements.get(value, value) for value in values]
            values[3] = hashlib.sha256(b''.join(map(field, [values[2], *values[4:table_end]]))).hexdigest().encode()
            spec.write_bytes(artifact(header, values))
            # IDs, table digest, every reference and envelope digest all agree on v2.
            for command in [('inspect', spec), ('execute', spec, '--output', run)]:
                with self.subTest(command=command[0]):
                    self.assertIn('UnsupportedTokenTableVersion', self.call('task-spec', *command, ok=False)['error'])
            self.assertFalse(run.exists())

    def test_canonical_v1_rejects_self_consistent_table_order_and_role_tampering(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            spec = root / 'canonical.spec'
            self.create(train, holdout, spec)
            header, original = artifact_fields(spec.read_bytes())
            table_end = 5 + 4 * int(original[4])

            changed_table = original.copy()
            replaced_id = changed_table[5]
            changed_table[6] = b'Learn'
            changed_table[7] = b'learn'
            changed_table[5] = hashlib.sha256(
                field(changed_table[2]) + field(changed_table[7]) + field(changed_table[8])).hexdigest().encode()
            changed_table = [changed_table[5] if value == replaced_id else value for value in changed_table]
            changed_table[3] = hashlib.sha256(
                b''.join(map(field, [changed_table[2], *changed_table[4:table_end]]))).hexdigest().encode()
            tampered = root / 'changed-table.spec'
            tampered.write_bytes(artifact(header, changed_table))
            self.assertIn('NoncanonicalTaskSpecV1Table', self.call('task-spec', 'inspect', tampered, ok=False)['error'])

            reordered = original.copy()
            ordered_start = table_end + 1
            reordered[ordered_start], reordered[ordered_start + 1] = reordered[ordered_start + 1], reordered[ordered_start]
            tampered = root / 'reordered.spec'
            tampered.write_bytes(artifact(header, reordered))
            self.assertIn('NoncanonicalTaskSpecV1TokenOrder', self.call('task-spec', 'inspect', tampered, ok=False)['error'])

            rebound = original.copy()
            cursor = table_end + 1 + int(original[table_end])
            cursor += 1 + 3 * int(original[cursor])
            role_count = int(original[cursor])
            cursor += 1
            for _ in range(role_count):
                if rebound[cursor] == b'holdout':
                    rebound[cursor + 1] = original[table_end + 1]
                    break
                cursor += 3
            tampered = root / 'rebound-role.spec'
            tampered.write_bytes(artifact(header, rebound))
            self.assertIn('NoncanonicalTaskSpecV1RoleBinding', self.call('task-spec', 'inspect', tampered, ok=False)['error'])

            swapped_assets = original.copy()
            cursor = table_end + 1 + int(original[table_end])
            cursor += 1 + 3 * int(original[cursor])
            role_count = int(original[cursor])
            cursor += 1
            aliases = {}
            for _ in range(role_count):
                aliases[swapped_assets[cursor]] = cursor + 2
                cursor += 3
            holdout_alias, train_alias = aliases[b'holdout'], aliases[b'train']
            swapped_assets[holdout_alias], swapped_assets[train_alias] = (
                swapped_assets[train_alias], swapped_assets[holdout_alias])
            tampered = root / 'swapped-assets.spec'
            tampered.write_bytes(artifact(header, swapped_assets))
            self.assertIn('NoncanonicalTaskSpecV1RoleAssetBinding', self.call(
                'task-spec', 'inspect', tampered, ok=False)['error'])

    def test_native_limit_bounds_reject_create_and_canonical_artifacts(self):
        cases = [
            ('--max-denominator', -4, (0, 17)),
            ('--max-candidates', -9, (0, 1000001)),
            ('--max-operations', -8, (0, 10000001)),
            ('--max-trace', -7, (0, 1, 100003)),
            ('--max-results', -6, (0, 129)),
            ('--max-bits', -5, (0, 16385)),
            ('--max-power', -3, (0, 9)),
            ('--max-degree', -2, (0, 9)),
            ('--max-trace-bytes', -1, (0, 255, 4194305)),
            ('--min', -11, (9,)), ('--max', -10, (-9, 4088)),
        ]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            valid = root / 'valid.spec'
            self.create(train, holdout, valid)
            header, original = artifact_fields(valid.read_bytes())
            for option, index, invalid_values in cases:
                for value in invalid_values:
                    with self.subTest(option=option, value=value):
                        rejected = root / f'rejected{option}-{value}.spec'
                        self.call('task-spec', 'create', '--train', train, '--holdout', holdout,
                                  '--route', 'rational-portfolio-v1', '--token-table-version', 'v1',
                                  option, value, '--output', rejected, ok=False)
                        self.assertFalse(rejected.exists())
                    with self.subTest(artifact_option=option, value=value):
                        values = original.copy()
                        values[index] = str(value).encode()
                        malformed, run = root / 'malformed.spec', root / 'rejected.run'
                        malformed.write_bytes(artifact(header, values))
                        self.call('task-spec', 'inspect', malformed, ok=False)
                        self.call('task-spec', 'execute', malformed, '--output', run, ok=False)
                        self.assertFalse(run.exists())

    def test_execute_and_replay_reject_changed_or_unauthorized_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            spec, run = root / 'task.spec', root / 'task.run'
            self.create(train, holdout, spec, '--max-candidates', '3', '--max-operations', '200',
                        '--max-trace', '24', '--max-results', '2')
            self.call('task-spec', 'execute', spec, '--output', run)
            for extra in ('--strategy', '--experience', '--learn', '--publish'):
                rejected = root / 'rejected.run'
                error = self.call('task-spec', 'execute', spec, extra, 'anything', '--output', rejected, ok=False)
                self.assertIn('error', error)
                self.assertFalse(rejected.exists())
            damaged = root / 'changed.run'
            damaged.write_bytes(run.read_bytes() + b'x')
            self.assertIn('TokenizedTaskRunDigestMismatch', self.call(
                'task-spec', 'replay', damaged, ok=False)['error'])
            damaged.write_bytes(b'not a run')
            self.assertIn('InvalidTokenizedTaskRunHeader', self.call(
                'task-spec', 'replay', damaged, ok=False)['error'])
            for original, expected in ((spec.read_bytes() + b'x', 'TaskSpecDigestMismatch'),
                                       (b'not a spec', 'InvalidTaskSpecHeader')):
                damaged.write_bytes(original)
                rejected = root / 'rejected.run'
                self.assertIn(expected, self.call('task-spec', 'execute', damaged,
                                                 '--output', rejected, ok=False)['error'])
                self.assertFalse(rejected.exists())

    def test_create_requires_explicit_train_holdout_and_route(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            target = root / 'missing.spec'
            for flags, expected in [
                (['--holdout', holdout, '--route', 'rational-portfolio-v1', '--token-table-version', 'v1'], 'MissingOption: --train'),
                (['--train', train, '--route', 'rational-portfolio-v1', '--token-table-version', 'v1'], 'MissingOption: --holdout'),
                (['--train', train, '--holdout', holdout, '--token-table-version', 'v1'], 'MissingOption: --route'),
                (['--train', train, '--holdout', holdout, '--route', 'rational-portfolio-v1'], 'MissingOption: --token-table-version'),
                (['--train', train, '--holdout', holdout, '--route', 'unknown', '--token-table-version', 'v1'], 'UnsupportedTaskSpecRoute'),
                (['--train', train, '--holdout', holdout, '--route', 'rational-portfolio-v1', '--token-table-version', 'v2'], 'UnsupportedTokenTableVersion'),
                (['--train', train, '--holdout', train, '--route', 'rational-portfolio-v1', '--token-table-version', 'v1'], 'DuplicateTaskSpecFileAlias'),
            ]:
                with self.subTest(flags=flags):
                    error = self.call('task-spec', 'create', *flags, '--output', target, ok=False)
                    self.assertIn(expected, error['error'])
                    self.assertFalse(target.exists())

    def test_explicit_limits_round_trip_without_executing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            target = root / 'limited.spec'
            self.create(train, holdout, target, '--min', '-2', '--max', '3',
                        '--max-bits', '128', '--max-denominator', '4',
                        '--max-power', '2', '--max-degree', '2',
                        '--max-trace-bytes', '1000', '--max-candidates', '3',
                        '--max-operations', '200', '--max-trace', '24', '--max-results', '2')
            inspected = self.call('task-spec', 'inspect', target)
            self.assertEqual(inspected['budget'], {
                'max_candidates': 3, 'max_operations': 200, 'max_trace': 24,
                'max_results': 2,
            })
            self.assertEqual(inspected['limits'], {
                'min': '-2', 'max': '3', 'max_bits': 128, 'max_denominator': 4,
                'max_power': 2, 'max_degree': 2, 'max_trace_bytes': 1000,
            })
            self.assertNotIn('result', inspected)

    def test_inspect_rejects_noncanonical_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            target = root / 'valid.spec'
            self.create(train, holdout, target)
            tampered = root / 'tampered.spec'
            tampered.write_bytes(target.read_bytes() + b'x')
            error = self.call('task-spec', 'inspect', tampered, ok=False)
            self.assertIn('TaskSpecDigestMismatch', error['error'])

    def test_inspect_exposes_selected_and_unselected_alternative_bindings(self):
        def field(value):
            return str(len(value)).encode() + b':' + value

        def alternative_backed(original):
            header, _, body = original.split(b'\n', 2)
            offset = 0

            def take():
                nonlocal offset
                colon = body.index(b':', offset)
                length = int(body[offset:colon])
                offset = colon + 1 + length
                return body[colon + 1:offset]

            for _ in range(4):
                take()  # spec/normalization/table versions and table digest
            for _ in range(int(take())):
                for _ in range(4):
                    take()  # token entry
            for _ in range(int(take())):
                take()  # ordered token ID
            for _ in range(int(take())):
                for _ in range(3):
                    take()  # asset alias, original bytes, digest
            role_start = offset
            roles = [(take(), take(), take()) for _ in range(int(take()))]
            left_roles = body[role_start:offset]
            self.assertEqual(take(), b'0')  # original has no alternatives
            self.assertEqual(take(), b'')  # original has no selection
            right_roles = field(b'3')
            for role, token_id, alias in roles:
                right_roles += field(role) + field(token_id) + field(alias)
            rewritten = (body[:role_start] + field(b'0') + field(b'2') +
                         field(b'left') + left_roles + field(b'right') + right_roles +
                         field(b'left') + body[offset:])
            return header + b'\n' + hashlib.sha256(rewritten).hexdigest().encode() + b'\n' + rewritten

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            train, holdout = self.samples(root)
            original, alternative = root / 'direct.spec', root / 'alternatives.spec'
            self.create(train, holdout, original)
            alternative.write_bytes(alternative_backed(original.read_bytes()))
            inspected = self.call('task-spec', 'inspect', alternative)
            token_ids = inspected['ordered_token_ids']
            self.assertEqual(inspected['roles'], [])
            self.assertEqual(inspected['selected_id'], 'left')
            self.assertEqual(inspected['alternatives'], [
                {'id': 'left', 'roles': [
                    {'role': 'holdout', 'token_id': token_ids[1], 'asset_alias': 'holdout'},
                    {'role': 'route', 'token_id': token_ids[2], 'asset_alias': None},
                    {'role': 'train', 'token_id': token_ids[0], 'asset_alias': 'train'},
                ]},
                {'id': 'right', 'roles': [
                    {'role': 'holdout', 'token_id': token_ids[1], 'asset_alias': 'holdout'},
                    {'role': 'route', 'token_id': token_ids[2], 'asset_alias': None},
                    {'role': 'train', 'token_id': token_ids[0], 'asset_alias': 'train'},
                ]},
            ])
            self.assertNotIn('result', inspected)


if __name__ == '__main__':
    unittest.main()
