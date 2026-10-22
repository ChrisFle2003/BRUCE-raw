#!/usr/bin/env python3
import contextlib, dataclasses, hashlib, importlib, io, json, os, pathlib, stat, subprocess, sys, tempfile, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parents[2]
SERVER = ROOT / 'tools/bruce_mcp.py'
sys.path.insert(0, str(ROOT / 'tools'))
import bruce_mcp_trace as trace
BRUCE_BINARY = os.environ.get('BRUCE_BINARY') or (sys.argv[1] if len(sys.argv) == 2 else None)
if len(sys.argv) == 2:
 sys.argv.pop()

class McpTests(unittest.TestCase):
 def call(self, messages, asset_root=ROOT / 'tests/fixtures', bruce_binary=BRUCE_BINARY):
  self.assertIsNotNone(BRUCE_BINARY, 'set BRUCE_BINARY or pass the BRUCE executable as the only argument')
  environment = {key: value for key, value in os.environ.items()
                 if key not in {'BRUCE_MCP_RUN_ID', 'BRUCE_MCP_TRACE_PATH'}}
  p = subprocess.run(
   [sys.executable, SERVER], input=''.join((m if isinstance(m, str) else json.dumps(m)) + '\n' for m in messages),
   text=True, capture_output=True,
   env={**environment, 'BRUCE_MCP_BRUCE': bruce_binary, 'BRUCE_MCP_ASSET_ROOT': str(asset_root)})
  self.assertEqual(p.returncode, 0, p.stderr)
  self.raw_stdout, self.raw_stderr = p.stdout, p.stderr
  return [json.loads(x) for x in p.stdout.splitlines()]
 def test_trace_disabled_stdout_is_exactly_unchanged_except_server_version(self):
  tool_list_hashes = {'2025-06-18': '924330de807c0e61b7f3cca88bcd8bd6a768d1f63ddae56980577aaa3f1bcf60',
                      '2024-11-05': '6b1569403142aba6cfa8be8b5deed5471a6f9deb8ed19fb0152e278cb1dd682c'}
  for protocol, tool_list_hash in tool_list_hashes.items():
   with self.subTest(protocol=protocol):
    replies = self.call([
     {'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':protocol}},
     {'jsonrpc':'2.0','method':'notifications/initialized'},
     {'jsonrpc':'2.0','id':2,'method':'tools/list'},
     {'jsonrpc':'2.0','id':3,'method':'tools/call','params':{'name':'bruce_calculate','arguments':{'op':'add','left':'2','right':'3'}}},
     '{not json}', [], {'jsonrpc':'2.0','id':4,'method':'unknown'}])
    self.assertEqual(self.raw_stderr, '')
    self.assertEqual([r['id'] for r in replies], [1, 2, 3, None, None, 4])
    expected_initialize = {'jsonrpc':'2.0','id':1,'result':{'protocolVersion':protocol,'capabilities':{'tools':{}},'serverInfo':{'name':'bruce-mcp','version':'0.3'}}}
    self.assertEqual(replies[0], expected_initialize)
    tool_list_text = self.raw_stdout.splitlines()[1]
    self.assertEqual(hashlib.sha256(tool_list_text.encode()).hexdigest(), tool_list_hash)
    calculation = {'domain':'bigint','operation':'add','max_bits':4096,'value':'5'}
    expected_calculation = {'content':[{'type':'text','text':'{"domain":"bigint","operation":"add","max_bits":4096,"value":"5"}\n'}],'isError':False}
    if protocol == '2025-06-18':
     expected_calculation['structuredContent'] = calculation
    expected = [expected_initialize, replies[1], {'jsonrpc':'2.0','id':3,'result':expected_calculation},
                {'jsonrpc':'2.0','id':None,'error':{'code':-32700,'message':'ParseError'}},
                {'jsonrpc':'2.0','id':None,'error':{'code':-32600,'message':'InvalidRequest'}},
                {'jsonrpc':'2.0','id':4,'error':{'code':-32601,'message':'MethodNotFound'}}]
    self.assertEqual(self.raw_stdout, ''.join(json.dumps(r, separators=(',', ':'), allow_nan=False) + '\n' for r in expected))
 def test_server_version_is_0_3_without_new_capabilities_or_tools(self):
  replies = self.call([{'jsonrpc':'2.0','id':1,'method':'initialize'}, {'jsonrpc':'2.0','id':2,'method':'tools/list'}])
  self.assertEqual(replies[0]['result'], {'protocolVersion':'2025-06-18','capabilities':{'tools':{}},'serverInfo':{'name':'bruce-mcp','version':'0.3'}})
  self.assertEqual([t['name'] for t in replies[1]['result']['tools']],
                   ['bruce_calculate', 'bruce_inspect_run', 'bruce_replay_run', 'bruce_assess_run', 'bruce_gate_run', 'bruce_research', 'bruce_demo'])
 def test_ordinary_compatibility_calls_ignore_ambient_trace_configuration(self):
  environment = {key: value for key, value in os.environ.items()
                 if key not in {'BRUCE_MCP_RUN_ID', 'BRUCE_MCP_TRACE_PATH'}}
  with tempfile.TemporaryDirectory() as directory:
   for index, keys in enumerate(((), ('BRUCE_MCP_RUN_ID',), ('BRUCE_MCP_TRACE_PATH',),
                                ('BRUCE_MCP_RUN_ID', 'BRUCE_MCP_TRACE_PATH'))):
    target = pathlib.Path(directory) / f'ambient-{index}.jsonl'
    variables = {'BRUCE_MCP_RUN_ID':'ambient-run', 'BRUCE_MCP_TRACE_PATH':str(target)}
    with self.subTest(keys=keys), mock.patch.dict(os.environ,
         {**environment, **{key: variables[key] for key in keys}}, clear=True):
     self.test_trace_disabled_stdout_is_exactly_unchanged_except_server_version()
     self.assertFalse(target.exists(), 'ordinary tests must not create an ambient trace')
   self.assertEqual(list(pathlib.Path(directory).iterdir()), [])
 def test_handle_message_returns_notification_without_response(self):
  with mock.patch.object(sys, 'stdin', io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
   adapter = importlib.import_module('bruce_mcp')
  self.assertTrue(hasattr(adapter, 'handle_message'), 'adapter must expose handle_message')
  for request in ({'jsonrpc':'2.0','method':'notifications/initialized'},
                  {'jsonrpc':'2.0','id':9,'method':'notifications/initialized'},
                  {'jsonrpc':'2.0','method':'unknown'}):
   result = adapter.handle_message(json.dumps(request))
   self.assertEqual(result.request, request)
   self.assertIsNone(result.response)
   self.assertIsNone(result.response_text)
   self.assertEqual(result.outcome, 'notification')
   with self.assertRaises(dataclasses.FrozenInstanceError):
    result.outcome = 'response'
  output, errors = io.StringIO(), io.StringIO()
  self.assertEqual(adapter.process_stream(io.StringIO('{not json}\n'), output, errors), 0)
  self.assertEqual(output.getvalue(), '{"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"ParseError"}}\n')
  self.assertEqual(errors.getvalue(), '')
 def test_importing_adapter_does_not_start_stdio_loop(self):
  program = ('import sys\n'
             'class UnreadableInput:\n'
             ' def __iter__(self): raise AssertionError("adapter read stdin during import")\n'
             'sys.stdin = UnreadableInput()\n'
             'import bruce_mcp\n'
             'print("imported")\n')
  p = subprocess.run([sys.executable, '-c', program], text=True, capture_output=True,
                     env={**os.environ, 'PYTHONPATH':str(ROOT / 'tools')})
  self.assertEqual(p.returncode, 0, p.stderr)
  self.assertEqual(p.stdout, 'imported\n')
  self.assertEqual(p.stderr, '')
 def test_initialize_and_list(self):
  replies=self.call([
   {'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2025-06-18','capabilities':{},'clientInfo':{'name':'bruce-test','version':'1'}}},
   {'jsonrpc':'2.0','method':'notifications/initialized'},
   {'jsonrpc':'2.0','id':2,'method':'tools/list'}])
  self.assertEqual(len(replies), 2)
  self.assertEqual(replies[0]['result']['serverInfo']['name'],'bruce-mcp')
  self.assertEqual(replies[0]['result']['protocolVersion'],'2025-06-18')
  self.assertEqual(replies[1]['result']['tools'][0]['name'],'bruce_calculate')
  tools = {tool['name']: tool for tool in replies[1]['result']['tools']}
  self.assertIn('bruce_research', tools)
  self.assertIn('bruce_assess_run', tools)
  self.assertIn('bruce_gate_run', tools)
  calculate = tools['bruce_calculate']['inputSchema']
  self.assertFalse(calculate['additionalProperties'])
  self.assertEqual(calculate['properties']['domain']['enum'], ['bigint', 'rational'])
  self.assertEqual(calculate['properties']['max_bits']['minimum'], 1)
  self.assertEqual(calculate['properties']['max_bits']['maximum'], 16384)
  self.assertEqual(calculate['properties']['modulus']['type'], 'string')
  self.assertIn('outputSchema', tools['bruce_calculate'])
  calculate_output = tools['bruce_calculate']['outputSchema']
  self.assertFalse(calculate_output['additionalProperties'])
  self.assertEqual(len(calculate_output['oneOf']), 2)
  self.assertNotIn('divmod', calculate_output['oneOf'][0]['properties']['operation']['enum'])
  self.assertEqual(calculate_output['oneOf'][1]['properties']['operation']['const'], 'divmod')
  self.assertEqual(tools['bruce_inspect_run']['inputSchema']['properties']['asset']['minLength'], 1)
  self.assertIn('[^/', tools['bruce_research']['inputSchema']['properties']['train']['pattern'])
 def test_legacy_protocol_retains_text_compatibility(self):
  replies=self.call([
   {'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2024-11-05','capabilities':{},'clientInfo':{'name':'legacy-test','version':'1'}}},
   {'jsonrpc':'2.0','id':2,'method':'tools/list'},
   {'jsonrpc':'2.0','id':3,'method':'tools/call','params':{'name':'bruce_calculate','arguments':{'op':'add','left':'2','right':'3'}}}])
  self.assertEqual(replies[0]['result']['protocolVersion'], '2024-11-05')
  self.assertNotIn('outputSchema', replies[1]['result']['tools'][0])
  self.assertNotIn('structuredContent', replies[2]['result'])
  self.assertIn('"value":"5"', replies[2]['result']['content'][0]['text'])
 def test_calculate(self):
  reply=self.call([{'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'bruce_calculate','arguments':{'op':'add','left':'2','right':'3'}}}])[0]
  self.assertIn('"value":"5"',reply['result']['content'][0]['text'])
  self.assertEqual(reply['result']['structuredContent']['value'], '5')
 def test_calculate_forwards_bounded_domain_options(self):
  rational=self.call([{'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'bruce_calculate','arguments':{'op':'add','left':'1/3','right':'1/6','domain':'rational','max_bits':128}}}])[0]
  self.assertEqual(rational['result']['structuredContent'], {'domain':'rational','operation':'add','max_bits':128,'value':'1/2'})
  modular=self.call([{'jsonrpc':'2.0','id':2,'method':'tools/call','params':{'name':'bruce_calculate','arguments':{'op':'modpow','left':'2','right':'10','modulus':'17','max_bits':128}}}])[0]
  self.assertEqual(modular['result']['structuredContent']['value'], '4')
 def test_calculate_rejects_invalid_option_types_and_ranges(self):
  messages = [
   {'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'bruce_calculate','arguments':{'op':'add','left':'2','right':'3','max_bits':0}}},
   {'jsonrpc':'2.0','id':2,'method':'tools/call','params':{'name':'bruce_calculate','arguments':{'op':'add','left':2,'right':'3'}}},
   {'jsonrpc':'2.0','id':3,'method':'tools/call','params':{'name':'bruce_calculate','arguments':{'op':'add','left':'2','right':'3','extra':True}}},
   {'jsonrpc':'2.0','id':4,'method':'tools/call','params':{'name':'bruce_calculate','arguments':{'op':'add','left':'2','right':'3','domain':[]}}},
   {'jsonrpc':'2.0','id':5,'method':'tools/call','params':{'name':'bruce_calculate','arguments':{'op':'add','left':'','right':'3'}}},
  ]
  for reply in self.call(messages):
   self.assertEqual(reply['error']['message'], 'InvalidToolArguments')
 def test_calculate_rejects_invalid_cross_field_combinations(self):
  arguments = [
   {'op':'modpow','left':'2','right':'10'},
   {'op':'add','left':'2','right':'3','modulus':'17'},
   {'op':'modpow','left':'2','right':'10','modulus':'17','domain':'rational'},
   {'op':'divmod','left':'5','right':'2','domain':'rational'},
   {'op':'modpow','left':'2','right':'10','modulus':'0'},
   {'op':'modpow','left':'2','right':'10','modulus':'-17'},
   {'op':'modpow','left':'2','right':'10','modulus':'seventeen'},
  ]
  messages = [{'jsonrpc':'2.0','id':index,'method':'tools/call','params':{'name':'bruce_calculate','arguments':value}}
              for index, value in enumerate(arguments, 1)]
  for reply in self.call(messages):
   self.assertEqual(reply['error']['message'], 'InvalidToolArguments')
 def test_rejects_unknown_tool(self):
  reply=self.call([{'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'shell','arguments':{}}}])[0]
  self.assertEqual(reply['error']['message'],'InvalidToolArguments')
 def test_rejects_wrong_argument_types_consistently(self):
  messages = [
   {'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'bruce_demo','arguments':{'demo':[]}}},
   {'jsonrpc':'2.0','id':2,'method':'tools/call','params':{'name':'bruce_inspect_run','arguments':{'asset':42}}},
   {'jsonrpc':'2.0','id':3,'method':'tools/call','params':{'name':'bruce_research','arguments':{'train':42,'holdout':'affine-holdout.csv'}}},
  ]
  for reply in self.call(messages):
   self.assertEqual(reply['error']['message'], 'InvalidToolArguments')
 def test_rejects_malformed_message(self):
  p = subprocess.run([sys.executable, SERVER], input='{not json}\n', text=True, capture_output=True)
  self.assertEqual(json.loads(p.stdout)['error']['code'], -32700)
 def test_rejects_malformed_envelopes_without_internal_details(self):
  replies=self.call([
   {'jsonrpc':'2.0','id':1,'method':'tools/call','params':[]},
   {'jsonrpc':'2.0','id':2,'method':['tools/call']},
   [],
   {'jsonrpc':'2.0','id':4,'method':'unknown'},
  ])
  self.assertEqual([(reply['error']['code'], reply['error']['message']) for reply in replies], [
   (-32602, 'InvalidParams'),
   (-32600, 'InvalidRequest'),
   (-32600, 'InvalidRequest'),
   (-32601, 'MethodNotFound'),
  ])
 def test_rejects_oversized_message(self):
  p = subprocess.run([sys.executable, SERVER], input=' ' * (16 * 1024) + '\n', text=True, capture_output=True)
  self.assertEqual(json.loads(p.stdout)['error']['message'], 'MessageTooLarge')
 def test_rejects_asset_traversal(self):
  reply=self.call([{'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'bruce_inspect_run','arguments':{'asset':'../README.md'}}}])[0]
  self.assertEqual(reply['error']['message'],'InvalidAssetName')
 def test_inspects_approved_run_asset(self):
  reply=self.call([{'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'bruce_inspect_run','arguments':{'asset':'exact-v1.bruce'}}}])[0]
  self.assertFalse(reply['result']['isError'])
  self.assertIsInstance(reply['result']['structuredContent'], dict)
 def test_replays_approved_run_asset(self):
  reply=self.call([{'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'bruce_replay_run','arguments':{'asset':'exact-v1.bruce'}}}])[0]
  self.assertFalse(reply['result']['isError'])
  self.assertIsInstance(reply['result']['structuredContent'], dict)
 def test_assesses_approved_run_asset(self):
  reply=self.call([{'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'bruce_assess_run','arguments':{'asset':'exact-v1.bruce'}}}])[0]
  self.assertFalse(reply['result']['isError'])
  self.assertEqual(reply['result']['structuredContent']['contract'], 'application-assessment-v1')
 def test_gates_approved_run_asset(self):
  reply=self.call([{'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'bruce_gate_run','arguments':{'asset':'exact-v1.bruce'}}}])[0]
  self.assertFalse(reply['result']['isError'])
  self.assertEqual(reply['result']['structuredContent']['contract'], 'application-gates-v1')
 def test_runs_bounded_research_on_approved_csv_assets(self):
  reply = self.call([{'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'bruce_research','arguments':{'train':'affine-training.csv','holdout':'affine-holdout.csv'}}}], ROOT / 'examples')[0]
  self.assertFalse(reply['result']['isError'])
  self.assertIn('"status"', reply['result']['content'][0]['text'])
  self.assertIsInstance(reply['result']['structuredContent'], dict)
 def test_runs_only_enumerated_demo(self):
  reply = self.call([{'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'bruce_demo','arguments':{'demo':'dag_scheduler_v2_flow'}}}])[0]
  self.assertFalse(reply['result']['isError'])
  self.assertIsInstance(reply['result']['structuredContent'], dict)
  rejected = self.call([{'jsonrpc':'2.0','id':2,'method':'tools/call','params':{'name':'bruce_demo','arguments':{'demo':'anything'}}}])[0]
  self.assertEqual(rejected['error']['message'], 'InvalidToolArguments')
 def test_stops_subprocess_output_at_limit(self):
  with tempfile.TemporaryDirectory() as directory:
   fake = pathlib.Path(directory) / 'large-output'
   fake.write_text('#!/usr/bin/env python3\nimport sys\nsys.stdout.write("x" * (128 * 1024))\n')
   fake.chmod(fake.stat().st_mode | stat.S_IXUSR)
   reply = self.call([{'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'bruce_calculate','arguments':{'op':'add','left':'2','right':'3'}}}], bruce_binary=str(fake))[0]
  self.assertTrue(reply['result']['isError'])
  self.assertIn('output limit', reply['result']['content'][0]['text'])
 def test_rejects_non_json_success_output(self):
  with tempfile.TemporaryDirectory() as directory:
   fake = pathlib.Path(directory) / 'plain-output'
   fake.write_text('#!/usr/bin/env python3\nprint("not json")\n')
   fake.chmod(fake.stat().st_mode | stat.S_IXUSR)
   reply = self.call([{'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'bruce_calculate','arguments':{'op':'add','left':'2','right':'3'}}}], bruce_binary=str(fake))[0]
  self.assertTrue(reply['result']['isError'])
  self.assertEqual(reply['result']['content'][0]['text'], 'BRUCE returned invalid JSON')
 def test_rejects_non_finite_json_success_output(self):
  with tempfile.TemporaryDirectory() as directory:
   fake = pathlib.Path(directory) / 'non-finite-output'
   fake.write_text('#!/usr/bin/env python3\nprint("{\\\"value\\\":NaN}")\n')
   fake.chmod(fake.stat().st_mode | stat.S_IXUSR)
   reply = self.call([{'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'bruce_calculate','arguments':{'op':'add','left':'2','right':'3'}}}], bruce_binary=str(fake))[0]
  self.assertTrue(reply['result']['isError'])
  self.assertEqual(reply['result']['content'][0]['text'], 'BRUCE returned invalid JSON')

class TraceIntegrationTests(unittest.TestCase):
 def test_documented_trace_example_uses_current_environment_and_verifier(self):
  # Catch missing launch/verification instructions and overstated audit claims.
  for relative_path in ('README.md', 'docs/architecture/04-local-security-boundaries.md'):
   documentation = (ROOT / relative_path).read_text(encoding='utf-8')
   with self.subTest(document=relative_path):
    for required in ('BRUCE_MCP_RUN_ID', 'BRUCE_MCP_TRACE_PATH', 'bruce-mcp-trace-v1',
                     'python3 tools/verify_bruce_mcp_trace.py', 'VALID', 'INCOMPLETE',
                     'TAMPERED', 'INVALID_FORMAT', '0600',
                     'not author identity', 'not a mathematical proof'):
     self.assertIn(required, documentation)
 def setUp(self):
  self.directory = tempfile.TemporaryDirectory()
  self.addCleanup(self.directory.cleanup)
  self.path = pathlib.Path(self.directory.name) / 'session.jsonl'
  self.environment = {key:value for key,value in os.environ.items()
                      if key not in {'BRUCE_MCP_RUN_ID', 'BRUCE_MCP_TRACE_PATH'}}
  self.environment.update(BRUCE_MCP_BRUCE=BRUCE_BINARY or 'bruce',
                          BRUCE_MCP_ASSET_ROOT=str(ROOT / 'tests/fixtures'))
 def session(self, data=None, *, enabled=True, extra_environment=None):
  if data is None:
   data = b''.join(json.dumps(message).encode('utf-8') + b'\n' for message in [
    {'jsonrpc':'2.0','id':1,'method':'initialize'},
    {'jsonrpc':'2.0','method':'notifications/initialized'},
    {'jsonrpc':'2.0','id':2,'method':'tools/list'},
    {'jsonrpc':'2.0','id':3,'method':'tools/call','params':{'name':'bruce_calculate','arguments':{'op':'add','left':'2','right':'3'}}}])
  environment = dict(self.environment)
  if enabled:
   environment.update(BRUCE_MCP_RUN_ID='integration-run', BRUCE_MCP_TRACE_PATH=str(self.path))
  environment.update(extra_environment or {})
  return subprocess.run([sys.executable, SERVER], input=data, capture_output=True, env=environment)
 def records(self):
  self.assertTrue(self.path.exists(), 'enabled adapter must create trace before reading input')
  return [json.loads(line) for line in self.path.read_text(encoding='utf-8').splitlines()]
 def test_enabled_trace_covers_initialize_notification_list_call_and_eof(self):
  result = self.session()
  self.assertEqual(result.returncode, 0, result.stderr)
  self.assertEqual(result.stderr, b'')
  records = self.records()
  self.assertEqual([r['record_type'] for r in records], ['header'] + ['exchange'] * 4 + ['footer'])
  self.assertEqual(records[-1]['message_count'], 4)
  self.assertIsNone(records[2]['response'])
  self.assertIsNone(records[2]['response_text'])
  self.assertEqual(records[2]['outcome'], 'notification')
  self.assertEqual(records[4]['response']['result']['structuredContent']['value'], '5')
 def test_trace_responses_are_byte_identical_to_stdout_lines(self):
  result = self.session()
  self.assertEqual(result.returncode, 0, result.stderr)
  responses = [r['response_text'].encode('utf-8') + b'\n' for r in self.records()
               if r['record_type'] == 'exchange' and r['response_text'] is not None]
  self.assertEqual(responses, result.stdout.splitlines(keepends=True))
 def test_trace_file_is_mode_0600_and_verifies_valid(self):
  result = self.session()
  self.assertEqual(result.returncode, 0, result.stderr)
  self.records()
  self.assertEqual(stat.S_IMODE(self.path.stat().st_mode), 0o600)
  self.assertEqual(trace.verify_trace(self.path)['status'], 'VALID')
 def test_trace_contains_no_unrelated_environment_or_configured_paths(self):
  result = self.session(extra_environment={'UNRELATED_SECRET':'trace-secret-must-not-leak'})
  self.assertEqual(result.returncode, 0, result.stderr)
  self.records()
  text = self.path.read_text(encoding='utf-8')
  for secret in ['trace-secret-must-not-leak', self.environment['BRUCE_MCP_BRUCE'],
                 self.environment['BRUCE_MCP_ASSET_ROOT'], str(self.path)]:
   self.assertNotIn(secret, text)
 def test_trace_disabled_creates_no_file(self):
  result = self.session(enabled=False)
  self.assertEqual(result.returncode, 0, result.stderr)
  self.assertFalse(self.path.exists())
  self.assertEqual(list(pathlib.Path(self.directory.name).iterdir()), [])
  self.assertEqual(json.loads(result.stdout.splitlines()[-1])['result']['structuredContent']['value'], '5')
 def test_partial_trace_environment_fails_before_stdin(self):
  for variables in ({'BRUCE_MCP_RUN_ID':'integration-run'}, {'BRUCE_MCP_TRACE_PATH':str(self.path)}):
   with self.subTest(variables=variables):
    process = subprocess.Popen([sys.executable, SERVER], stdin=subprocess.PIPE,
     stdout=subprocess.PIPE, stderr=subprocess.PIPE, env={**self.environment, **variables})
    try:
     process.wait(timeout=3)
    finally:
     if process.poll() is None:
      process.kill()
     stdout, stderr = process.communicate()
    self.assertEqual(process.returncode, 2)
    self.assertEqual(stdout, b'')
    self.assertEqual(stderr, b'BruceMcpTraceConfigurationError\n')
    self.assertFalse(self.path.exists())
 def test_existing_target_and_symlink_fail_without_modification(self):
  existing = pathlib.Path(self.directory.name) / 'existing'
  existing.write_bytes(b'private sentinel content')
  for use_symlink in (False, True):
   with self.subTest(symlink=use_symlink):
    self.path = pathlib.Path(self.directory.name) / 'link' if use_symlink else existing
    if use_symlink:
     self.path.symlink_to(existing)
    result = self.session(b'{not json}\n')
    self.assertEqual(result.returncode, 2)
    self.assertEqual(result.stdout, b'')
    self.assertEqual(result.stderr, b'BruceMcpTraceConfigurationError\n')
    self.assertEqual(existing.read_bytes(), b'private sentinel content')
    if use_symlink:
     self.assertTrue(self.path.is_symlink())
 def test_parse_and_envelope_errors_are_traced_with_exact_responses(self):
  result = self.session(b'{not json}\n[]\n{"jsonrpc":"2.0","id":4,"method":"unknown"}\n')
  self.assertEqual(result.returncode, 0, result.stderr)
  exchanges = self.records()[1:-1]
  self.assertEqual([r['request_text'] for r in exchanges],
                   ['{not json}', '[]', '{"jsonrpc":"2.0","id":4,"method":"unknown"}'])
  self.assertEqual([r['request'] for r in exchanges], [None, [], {'jsonrpc':'2.0','id':4,'method':'unknown'}])
  self.assertEqual([r['response']['error']['code'] for r in exchanges], [-32700, -32600, -32601])
  self.assertEqual(result.stdout, b''.join(r['response_text'].encode('utf-8') + b'\n' for r in exchanges))
  self.assertEqual(trace.verify_trace(self.path)['status'], 'VALID')
 def test_unrepresentable_parsed_requests_keep_raw_text_responses_and_capture(self):
  ordinary = b'{"jsonrpc":"2.0","id":2,"method":"unknown"}'
  ordinary_reply = {'jsonrpc':'2.0','id':2,'error':{'code':-32601,'message':'MethodNotFound'}}
  values = [(b'1e400', False), (b'-1e400', False),
            (b'"\\ud800"', False), (b'"\\udfff"', False),
            (b'1e300', True), (b'"\\ud83d\\ude00"', True)]
  for index, (value, representable) in enumerate(values):
   for invalid_envelope in (False, True):
    with self.subTest(value=value, invalid_envelope=invalid_envelope):
     self.path = pathlib.Path(self.directory.name) / f'representation-{index}-{invalid_envelope}.jsonl'
     if invalid_envelope:
      message = b'[' + value + b']'
      first_reply = {'jsonrpc':'2.0','id':None,'error':{'code':-32600,'message':'InvalidRequest'}}
     else:
      message = b'{"jsonrpc":"2.0","id":1,"method":"unknown","extra":' + value + b'}'
      first_reply = {'jsonrpc':'2.0','id':1,'error':{'code':-32601,'message':'MethodNotFound'}}
     wire = message + b'\n' + ordinary + b'\n'
     disabled = self.session(wire, enabled=False)
     self.assertEqual(disabled.returncode, 0, disabled.stderr)
     self.assertEqual(disabled.stderr, b'')
     expected = [first_reply, ordinary_reply]
     expected_stdout = ''.join(json.dumps(reply, separators=(',', ':')) + '\n' for reply in expected).encode()
     self.assertEqual(disabled.stdout, expected_stdout)
     result = self.session(wire)
     self.assertEqual(result.returncode, 0, result.stderr)
     self.assertEqual(result.stderr, b'')
     self.assertEqual(result.stdout, expected_stdout)
     records = self.records()
     self.assertEqual([r['record_type'] for r in records], ['header', 'exchange', 'exchange', 'footer'])
     self.assertEqual(records[-1]['message_count'], 2)
     for exchange, raw, response in zip(records[1:-1], (message, ordinary), expected):
      self.assertEqual(exchange['request_text'], raw.decode('utf-8'))
      self.assertEqual(exchange['request_bytes'], len(raw))
      self.assertEqual(exchange['request_sha256'], hashlib.sha256(raw).hexdigest())
      self.assertEqual(exchange['response'], response)
      self.assertEqual(exchange['response_text'], json.dumps(response, separators=(',', ':')))
      self.assertEqual(exchange['outcome'], 'response')
     self.assertEqual(records[1]['request'], json.loads(message) if representable else None)
     self.assertEqual(records[2]['request'], {'jsonrpc':'2.0','id':2,'method':'unknown'})
     verification = trace.verify_trace(self.path)
     self.assertEqual(verification['status'], 'VALID')
     self.assertEqual(verification['message_count'], 2)
 def test_oversized_request_records_only_length_digest_and_complete_response(self):
  message = b'oversized-private-sentinel' + b'x' * (16 * 1024)
  result = self.session(message + b'\n')
  self.assertEqual(result.returncode, 0, result.stderr)
  exchange = self.records()[1]
  self.assertIsNone(exchange['request_text'])
  self.assertIsNone(exchange['request'])
  self.assertEqual(exchange['request_bytes'], len(message))
  self.assertEqual(exchange['request_sha256'], hashlib.sha256(message).hexdigest())
  self.assertEqual(exchange['outcome'], 'message_too_large')
  self.assertEqual(exchange['response'], {'jsonrpc':'2.0','id':None,'error':{'code':-32602,'message':'MessageTooLarge'}})
  self.assertEqual(exchange['response_text'].encode('utf-8') + b'\n', result.stdout)
  self.assertNotIn('oversized-private-sentinel', self.path.read_text())
  self.assertLess(self.path.stat().st_size, 4096)
 def test_utf8_exact_limit_and_one_byte_over_limit_preserve_existing_result(self):
  base = '{"jsonrpc":"2.0","id":1,"method":"unknown","padding":"é"}'.encode('utf-8')
  message = base + b' ' * (16 * 1024 - 1 - len(base))
  for index, (wire, observed, expected) in enumerate([
   (message + b'\n', message, 'MethodNotFound'),
   (message + b' \n', message + b' ', 'MessageTooLarge'),
   (message + b'\r\n', message, 'MessageTooLarge'),
   (message + b' ', message + b' ', 'MethodNotFound'),
   (message + b'  ', message + b'  ', 'MessageTooLarge')]):
   with self.subTest(index=index):
    self.path = pathlib.Path(self.directory.name) / f'boundary-{index}.jsonl'
    disabled = self.session(wire, enabled=False)
    result = self.session(wire)
    self.assertEqual(result.returncode, 0, result.stderr)
    self.assertEqual(result.stdout, disabled.stdout)
    self.assertEqual(json.loads(result.stdout)['error']['message'], expected)
    exchange = self.records()[1]
    self.assertEqual(exchange['request_bytes'], len(observed))
    self.assertEqual(exchange['request_sha256'], hashlib.sha256(observed).hexdigest())
    self.assertEqual(exchange['request_text'], observed.decode('utf-8') if expected == 'MethodNotFound' else None)
 def test_eof_without_newline_and_crlf_record_adapter_observed_text(self):
  first = '{"jsonrpc":"2.0","id":1,"method":"é"}'.encode('utf-8')
  second = b'{not json}'
  result = self.session(first + b'\r\n' + second)
  self.assertEqual(result.returncode, 0, result.stderr)
  for exchange, observed in zip(self.records()[1:-1], [first, second]):
   self.assertEqual(exchange['request_text'], observed.decode('utf-8'))
   self.assertEqual(exchange['request_bytes'], len(observed))
   self.assertEqual(exchange['request_sha256'], hashlib.sha256(observed).hexdigest())
  self.assertEqual(len(self.records()), 4)
  self.assertEqual(trace.verify_trace(self.path)['status'], 'VALID')
 def test_injected_trace_write_failure_emits_no_response(self):
  adapter = importlib.import_module('bruce_mcp')
  class FailingWriter:
   def append_exchange(self, **_fields):
    raise trace.TraceWriteError()
   def close(self):
    raise AssertionError('failed exchange must not close trace with footer')
  output, errors = io.StringIO(), io.StringIO()
  with self.assertRaises(trace.TraceWriteError):
   adapter.process_stream(io.StringIO('{not json}\n'), output, errors, FailingWriter())
  self.assertEqual(output.getvalue(), '')
  with mock.patch.object(adapter, 'writer_from_environment', return_value=FailingWriter()), \
       mock.patch.object(sys, 'stdin', io.StringIO('{not json}\n')), \
       mock.patch.object(sys, 'stdout', output), mock.patch.object(sys, 'stderr', errors):
   self.assertEqual(adapter.main({}), 3)
  self.assertEqual(output.getvalue(), '')
  self.assertEqual(errors.getvalue(), 'BruceMcpTraceWriteError\n')
 def test_broken_stdout_leaves_incomplete_trace_and_nonzero_exit(self):
  adapter = importlib.import_module('bruce_mcp')
  class BrokenOutput:
   def write(self, _text):
    raise BrokenPipeError('secret output detail')
   def flush(self):
    pass
  errors = io.StringIO()
  writer = trace.TraceWriter(self.path, 'broken-output')
  self.addCleanup(writer._release)
  with mock.patch.object(adapter, 'writer_from_environment', return_value=writer), \
       mock.patch.object(sys, 'stdin', io.StringIO('{not json}\n')), \
       mock.patch.object(sys, 'stdout', BrokenOutput()), mock.patch.object(sys, 'stderr', errors):
   self.assertEqual(adapter.main({}), 3)
  self.assertEqual(errors.getvalue(), 'BruceMcpTraceWriteError\n')
  self.assertEqual([r['record_type'] for r in self.records()], ['header', 'exchange'])
  self.assertEqual(self.records()[1]['response']['error']['message'], 'ParseError')
  self.assertEqual(trace.verify_trace(self.path)['status'], 'INCOMPLETE')
 def test_closed_stdout_pipe_returns_stable_trace_error(self):
  environment = {**self.environment, 'BRUCE_MCP_RUN_ID':'closed-pipe',
                 'BRUCE_MCP_TRACE_PATH':str(self.path)}
  process = subprocess.Popen([sys.executable, SERVER], stdin=subprocess.PIPE,
   stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=environment)
  process.stdout.close()
  process.stdin.write(b'{not json}\n')
  process.stdin.close()
  try:
   process.wait(timeout=3)
  finally:
   if process.poll() is None:
    process.kill()
    process.wait()
   stderr = process.stderr.read()
   process.stderr.close()
  self.assertEqual(process.returncode, 3, stderr)
  self.assertEqual(stderr, b'BruceMcpTraceWriteError\n')
  self.assertEqual([r['record_type'] for r in self.records()], ['header', 'exchange'])
  self.assertEqual(trace.verify_trace(self.path)['status'], 'INCOMPLETE')
 def test_closed_text_stdout_is_fail_closed_only_when_traced(self):
  adapter = importlib.import_module('bruce_mcp')
  output, errors = io.StringIO(), io.StringIO()
  output.close()
  writer = trace.TraceWriter(self.path, 'closed-text-output')
  self.addCleanup(writer._release)
  with mock.patch.object(adapter, 'writer_from_environment', return_value=writer), \
       mock.patch.object(sys, 'stdin', io.StringIO('{not json}\n')), \
       mock.patch.object(sys, 'stdout', output), mock.patch.object(sys, 'stderr', errors):
   self.assertEqual(adapter.main({}), 3)
  self.assertEqual(errors.getvalue(), 'BruceMcpTraceWriteError\n')
  self.assertEqual([r['record_type'] for r in self.records()], ['header', 'exchange'])
  self.assertEqual(self.records()[1]['response']['error']['message'], 'ParseError')
  self.assertEqual(trace.verify_trace(self.path)['status'], 'INCOMPLETE')
  with self.assertRaises(ValueError):
   adapter.process_stream(io.StringIO('{not json}\n'), output, errors)
 def test_exchange_is_synchronized_before_response_output(self):
  adapter = importlib.import_module('bruce_mcp')
  writer = trace.TraceWriter(self.path, 'ordering')
  self.addCleanup(writer._release)
  synchronized = []
  real_fsync = trace.os.fsync
  def fsync(fd):
   real_fsync(fd)
   synchronized.append(True)
  case = self
  class ObservingOutput(io.StringIO):
   def write(self, text):
    case.assertTrue(synchronized, 'output must follow exchange fsync')
    case.assertEqual(trace.verify_trace(case.path)['status'], 'INCOMPLETE')
    case.assertEqual(case.records()[1]['response']['error']['message'], 'ParseError')
    return super().write(text)
  output = ObservingOutput()
  with mock.patch.object(trace.os, 'fsync', side_effect=fsync):
   self.assertEqual(adapter.process_stream(io.StringIO('{not json}\n'), output, io.StringIO(), writer), 0)
  self.assertEqual(output.getvalue(), self.records()[1]['response_text'] + '\n')
  self.assertEqual(trace.verify_trace(self.path)['status'], 'VALID')

class TraceFormatTests(unittest.TestCase):
 def setUp(self):
  self.directory = tempfile.TemporaryDirectory()
  self.addCleanup(self.directory.cleanup)
  self.path = pathlib.Path(self.directory.name) / 'audit.jsonl'
 def writer(self, path=None, run_id='test-run'):
  writer = trace.TraceWriter(path or self.path, run_id, wall_clock_ns=lambda: 100, monotonic_ns=lambda: 50)
  self.addCleanup(writer.close)
  return writer
 def test_trace_environment_requires_both_values(self):
  self.assertIsNone(trace.writer_from_environment({}))
  for environment in ({'BRUCE_MCP_RUN_ID': 'test-run'}, {'BRUCE_MCP_TRACE_PATH': str(self.path)},
                      {'BRUCE_MCP_RUN_ID': '', 'BRUCE_MCP_TRACE_PATH': str(self.path)},
                      {'BRUCE_MCP_RUN_ID': 'test-run', 'BRUCE_MCP_TRACE_PATH': ''}):
   with self.subTest(environment=environment), self.assertRaises(trace.TraceConfigurationError):
    trace.writer_from_environment(environment)
  writer = trace.writer_from_environment({'BRUCE_MCP_RUN_ID': 'test-run', 'BRUCE_MCP_TRACE_PATH': str(self.path)})
  self.addCleanup(writer.close)
  self.assertTrue(stat.S_ISREG(self.path.stat().st_mode))
  self.assertEqual(stat.S_IMODE(self.path.stat().st_mode), 0o600)
 def test_trace_rejects_invalid_run_ids_and_relative_paths(self):
  for run_id in ('', 'a' * 129, 'has space', 'ä', '../run', 'run\n', 'x\\y'):
   with self.subTest(run_id=run_id), self.assertRaises(trace.TraceConfigurationError):
    self.writer(run_id=run_id)
  with self.assertRaises(trace.TraceConfigurationError):
   self.writer(path=pathlib.Path('relative.jsonl'))
  with self.assertRaises(trace.TraceConfigurationError):
   self.writer(path=self.path.parent / 'missing' / 'audit.jsonl')
  with self.assertRaises(trace.TraceConfigurationError):
   self.writer(path=str(self.path.parent) + '/../' + self.path.parent.name + '/audit.jsonl')
  self.assertFalse(self.path.exists())
 def test_trace_rejects_existing_file_and_target_symlink(self):
  self.path.write_text('keep me')
  with self.assertRaises(trace.TraceConfigurationError):
   self.writer()
  link = self.path.parent / 'link.jsonl'
  link.symlink_to(self.path)
  with self.assertRaises(trace.TraceConfigurationError):
   self.writer(path=link)
  dangling = self.path.parent / 'dangling.jsonl'
  dangling.symlink_to(self.path.parent / 'absent')
  with self.assertRaises(trace.TraceConfigurationError):
   self.writer(path=dangling)
  self.assertEqual(self.path.read_text(), 'keep me')
  self.assertFalse((self.path.parent / 'absent').exists())
 def test_trace_rejects_symlinked_parent(self):
  real = self.path.parent / 'real'
  real.mkdir()
  link = self.path.parent / 'linked'
  link.symlink_to(real, target_is_directory=True)
  with self.assertRaises(trace.TraceConfigurationError):
   self.writer(path=link / 'audit.jsonl')
  self.assertEqual(list(real.iterdir()), [])
 def test_trace_permission_failure_is_generic(self):
  with mock.patch.object(trace.os, 'open', side_effect=PermissionError('secret/path denied')):
   with self.assertRaises(trace.TraceConfigurationError) as raised:
    self.writer()
  self.assertEqual(str(raised.exception), 'TraceConfigurationError')
  self.assertIsNone(raised.exception.__cause__)
 def test_two_writers_cannot_own_the_same_path(self):
  owner = self.writer()
  before = self.path.read_bytes()
  with self.assertRaises(trace.TraceConfigurationError):
   self.writer()
  self.assertEqual(self.path.read_bytes(), before)
  owner.close()
  before = self.path.read_bytes()
  with self.assertRaises(trace.TraceConfigurationError):
   self.writer()
  self.assertEqual(self.path.read_bytes(), before)

 def exchange(self, writer, text='{"jsonrpc":"2.0","id":1,"method":"é"}'):
  response = {'jsonrpc': '2.0', 'id': 1, 'result': {}}
  writer.append_exchange(request_text=text, request_bytes=len(text.encode('utf-8')),
   request_sha256=hashlib.sha256(text.encode('utf-8')).hexdigest(), request=json.loads(text),
   response_text=json.dumps(response, separators=(',', ':')), response=response, outcome='response',
   received_unix_ns=110, completed_unix_ns=120, duration_monotonic_ns=10)
 def complete_trace(self):
  writer = self.writer()
  self.exchange(writer)
  self.exchange(writer)
  writer.close()
  return [json.loads(line) for line in self.path.read_text().splitlines()]
 def rewrite(self, records):
  self.path.write_text(''.join(json.dumps(record, sort_keys=True, separators=(',', ':'), ensure_ascii=False) + '\n' for record in records))
 def assert_status(self, status):
  result = trace.verify_trace(self.path)
  self.assertEqual(result['status'], status, result)
  self.assertGreaterEqual(set(result), {'status', 'trace_version', 'run_id', 'message_count', 'final_hash', 'reason'})
  return result
 def test_writer_emits_canonical_valid_trace_and_mode_0600(self):
  records = self.complete_trace()
  self.assertEqual([r['record_type'] for r in records], ['header', 'exchange', 'exchange', 'footer'])
  self.assertEqual([r['sequence'] for r in records], [0, 1, 2, 3])
  self.assertEqual(records[0]['previous_hash'], '0' * 64)
  self.assertEqual(records[0]['adapter_name'], 'bruce-mcp')
  self.assertEqual(records[0]['adapter_version'], '0.3')
  self.assertEqual(records[0]['created_unix_ns'], 100)
  self.assertEqual(records[-1]['message_count'], 2)
  self.assertEqual(records[-1]['completed_unix_ns'], 100)
  previous = '0' * 64
  for line, record in zip(self.path.read_text().splitlines(), records):
   self.assertEqual(line, json.dumps(record, sort_keys=True, separators=(',', ':'), ensure_ascii=False, allow_nan=False))
   self.assertEqual(record['trace_version'], 'bruce-mcp-trace-v1')
   self.assertEqual(record['run_id'], 'test-run')
   self.assertEqual(record['previous_hash'], previous)
   body = {key: value for key, value in record.items() if key != 'record_hash'}
   expected = hashlib.sha256(json.dumps(body, sort_keys=True, separators=(',', ':'), ensure_ascii=False, allow_nan=False).encode('utf-8')).hexdigest()
   self.assertEqual(record['record_hash'], expected)
   self.assertEqual(trace.record_hash(body), expected)
   previous = expected
  self.assertIn('é', self.path.read_text())
  self.assertEqual(stat.S_IMODE(self.path.stat().st_mode), 0o600)
  result = self.assert_status('VALID')
  self.assertEqual(result['trace_version'], 'bruce-mcp-trace-v1')
  self.assertEqual(result['run_id'], 'test-run')
  self.assertEqual(result['message_count'], 2)
  self.assertEqual(result['final_hash'], records[-1]['record_hash'])
  self.assertEqual(trace.canonical_json({'z': 'é', 'a': 1}), '{"a":1,"z":"é"}')
  with self.assertRaises(ValueError):
   trace.canonical_json({'value': float('nan')})
 def test_writer_close_is_idempotent(self):
  writer = self.writer()
  writer.close()
  before = self.path.read_bytes()
  writer.close()
  self.assertEqual(self.path.read_bytes(), before)
  self.assert_status('VALID')
  with self.assertRaises(trace.TraceWriteError):
   self.exchange(writer)
  self.assertEqual(self.path.read_bytes(), before)
 def test_verifier_reports_incomplete_without_footer(self):
  writer = self.writer()
  self.assert_status('INCOMPLETE')
  self.exchange(writer)
  result = self.assert_status('INCOMPLETE')
  self.assertEqual(result['message_count'], 1)
 def test_verifier_reports_tampered_changed_deleted_and_reordered_records(self):
  records = self.complete_trace()
  variants = []
  changed = json.loads(json.dumps(records))
  changed[1]['outcome'] = 'notification'
  variants.append(changed)
  variants.append([records[0], records[2], records[3]])
  variants.append([records[0], records[2], records[1], records[3]])
  variants.append(records + [records[-1]])
  for field, value in (('sequence', 8), ('run_id', 'other'), ('previous_hash', '1' * 64)):
   changed = json.loads(json.dumps(records))
   changed[1][field] = value
   variants.append(changed)
  changed = json.loads(json.dumps(records))
  changed[-1]['message_count'] = 99
  variants.append(changed)
  # Correct hashes must not hide contradictory run IDs, sequences, or counts.
  for index, field, value in ((1, 'sequence', 8), (1, 'run_id', 'other'), (-1, 'message_count', 99)):
   changed = json.loads(json.dumps(records))
   changed[index][field] = value
   previous = '0' * 64
   for record in changed:
    record['previous_hash'] = previous
    body = {key: item for key, item in record.items() if key != 'record_hash'}
    previous = hashlib.sha256(json.dumps(body, sort_keys=True, separators=(',', ':'), ensure_ascii=False).encode('utf-8')).hexdigest()
    record['record_hash'] = previous
   variants.append(changed)
  for variant in variants:
   with self.subTest(variant=variant):
    self.rewrite(variant)
    self.assert_status('TAMPERED')
 def test_verifier_reports_invalid_format_for_empty_malformed_and_unknown_records(self):
  records = self.complete_trace()
  for data in ('', '{broken}\n', '{}\n', '[]\n', '{"x":NaN}\n', '\n', '{"a":1,"a":2}\n'):
   with self.subTest(data=data):
    self.path.write_text(data)
    self.assert_status('INVALID_FORMAT')
  for field, value in (('record_type', 'unknown'), ('trace_version', 'v2'), ('sequence', True),
                       ('run_id', []), ('created_unix_ns', '100'), ('record_hash', 'bad')):
   changed = json.loads(json.dumps(records))
   changed[0][field] = value
   self.rewrite(changed)
   self.assert_status('INVALID_FORMAT')
  changed = json.loads(json.dumps(records))
  del changed[1]['request_text']
  self.rewrite(changed)
  self.assert_status('INVALID_FORMAT')
  # A malformed later record takes precedence over earlier chain contradictions.
  changed[0]['sequence'] = 7
  self.rewrite(changed)
  self.assert_status('INVALID_FORMAT')
  self.path.write_bytes(b'\xff\n')
  self.assert_status('INVALID_FORMAT')
 def test_verifier_cli_is_json_and_exit_status_matches_result(self):
  records = self.complete_trace()
  verifier = ROOT / 'tools/verify_bruce_mcp_trace.py'
  for expected, arguments in (('VALID', [str(self.path)]), ('INVALID_FORMAT', []),
                              ('INVALID_FORMAT', [str(self.path), 'extra']), ('INVALID_FORMAT', [str(self.path.parent / 'missing')])):
   p = subprocess.run([sys.executable, verifier, *arguments], text=True, capture_output=True)
   self.assertEqual(p.returncode, 0 if expected == 'VALID' else 1, p.stderr)
   self.assertEqual(p.stderr, '')
   self.assertEqual(len(p.stdout.splitlines()), 1)
   self.assertEqual(json.loads(p.stdout)['status'], expected)
   self.assertNotIn(str(self.path.parent), p.stdout)
  self.rewrite(records[:-1])
  p = subprocess.run([sys.executable, verifier, str(self.path)], text=True, capture_output=True)
  self.assertEqual(p.returncode, 1)
  self.assertEqual(json.loads(p.stdout)['status'], 'INCOMPLETE')
  records[1]['outcome'] = 'notification'
  self.rewrite(records)
  p = subprocess.run([sys.executable, verifier, str(self.path)], text=True, capture_output=True)
  self.assertEqual(p.returncode, 1)
  self.assertEqual(json.loads(p.stdout)['status'], 'TAMPERED')
 def test_verifier_does_not_execute_bruce_or_read_assets(self):
  self.complete_trace()
  executable = self.path.parent / 'sentinel'
  touched = self.path.parent / 'executed'
  executable.write_text('#!/bin/sh\ntouch "' + str(touched) + '"\n')
  executable.chmod(0o700)
  asset = self.path.parent / 'private-asset'
  asset.write_text('do not read')
  asset.chmod(0)
  before = self.path.read_bytes()
  with mock.patch.dict(os.environ, {'BRUCE_MCP_BRUCE': str(executable), 'BRUCE_MCP_ASSET_ROOT': str(asset)}):
   p = subprocess.run([sys.executable, ROOT / 'tools/verify_bruce_mcp_trace.py', str(self.path)], text=True, capture_output=True)
  self.assertEqual(p.returncode, 0, p.stderr)
  self.assertEqual(json.loads(p.stdout)['status'], 'VALID')
  self.assertFalse(touched.exists())
  self.assertEqual(self.path.read_bytes(), before)
 def test_writer_retries_interrupted_and_short_writes(self):
  original_write = os.write
  interrupted = [True]
  def partial_write(fd, data):
   if interrupted[0]:
    interrupted[0] = False
    raise InterruptedError()
   return original_write(fd, data[:7])
  with mock.patch.object(trace.os, 'write', side_effect=partial_write):
   writer = self.writer()
   self.exchange(writer)
   writer.close()
  self.assert_status('VALID')
 def test_writer_write_and_sync_failures_are_generic_and_cannot_add_footer(self):
  for operation in ('write', 'fsync'):
   with self.subTest(operation=operation):
    path = self.path.parent / (operation + '.jsonl')
    writer = trace.TraceWriter(path, 'test-run')
    with mock.patch.object(trace.os, operation, side_effect=OSError('private failure details')):
     with self.assertRaises(trace.TraceWriteError) as raised:
      self.exchange(writer)
    self.assertEqual(str(raised.exception), 'TraceWriteError')
    self.assertIsNone(raised.exception.__cause__)
    with self.assertRaises(trace.TraceWriteError):
     writer.close()
    self.assertEqual(trace.verify_trace(path)['status'], 'INCOMPLETE')
 def test_footer_write_or_sync_failure_rolls_back_to_valid_prefix(self):
  for operation in ('write', 'fsync'):
   with self.subTest(operation=operation):
    path = self.path.parent / ('footer-' + operation + '.jsonl')
    writer = trace.TraceWriter(path, 'test-run')
    self.exchange(writer)
    before = path.read_bytes()
    original_write = os.write
    def broken_write(fd, data):
     original_write(fd, data[:11])
     raise OSError('private details')
    with mock.patch.object(trace.os, operation, side_effect=broken_write if operation == 'write' else OSError('private details')):
     with self.assertRaises(trace.TraceWriteError):
      writer.close()
    self.assertEqual(path.read_bytes(), before)
    self.assertEqual(trace.verify_trace(path)['status'], 'INCOMPLETE')
    with self.assertRaises(trace.TraceWriteError):
     writer.close()
 def test_zero_write_fails_without_looping_or_footer(self):
  writer = trace.TraceWriter(self.path, 'test-run')
  with mock.patch.object(trace.os, 'write', return_value=0):
   with self.assertRaises(trace.TraceWriteError):
    self.exchange(writer)
  with self.assertRaises(trace.TraceWriteError):
   writer.close()
  self.assert_status('INCOMPLETE')

if __name__=='__main__': unittest.main()
