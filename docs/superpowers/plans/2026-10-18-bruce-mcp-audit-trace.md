# BRUCE MCP Audit Trace v1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an opt-in, process-bound, hash-chained JSONL audit trace to the local BRUCE stdio MCP adapter, together with an offline verifier.

**Architecture:** A dependency-free trace module owns secure file creation, canonical records, hash chaining, and verification. The existing adapter is refactored into a testable message-processing loop, then wires the trace writer around the exact request/response boundary without changing tool schemas or the trace-disabled protocol behavior.

**Tech Stack:** Python 3 standard library, JSON-RPC/MCP over stdio, `unittest`, CMake/CTest, SHA-256.

**Spec:** `docs/superpowers/specs/2026-10-18-bruce-mcp-audit-trace-design.md`

## Global Constraints

- Tracing is off unless both `BRUCE_MCP_RUN_ID` and `BRUCE_MCP_TRACE_PATH` are set.
- The feature remains local and stdio-only; add no listener, service, database, operating-system user, C++ command, MCP tool, or global Codex configuration.
- Trace targets are absolute, new regular files created exclusively with mode `0600`; existing files, target symlinks, symlinked parents, unsafe parents, and partial configuration fail closed.
- `bruce-mcp-trace-v1` uses canonical UTF-8 JSONL, contiguous sequence numbers, and a SHA-256 previous-hash chain.
- Requests and emitted responses within existing limits are complete; oversized requests retain only length and digest plus the complete rejection response.
- Do not record environment values, configured executable paths, asset-root paths, subprocess arguments, or Python exception details.
- The verifier is read-only and never starts BRUCE, reads assets, replays calls, or edits a trace.
- Keep the seven existing MCP tools and their schemas unchanged; bump only `serverInfo.version` from `0.2` to `0.3`.
- Preserve initialization and response compatibility for MCP protocol versions `2025-06-18` and `2024-11-05`.
- Do not hand-edit `docs/cli/source-index.md`, `docs/cli/examples.json`, or `docs/cli/validation.md`.
- The repository has no usable Git metadata. Before each task inspect status anyway; if Exit 128 persists, compare against `/tmp/bruce-mcp-trace-baseline-UJpmqn/source`, record the proposed commit message, and do not claim a commit exists.
- Run wrapper-based builds only from a full temporary source copy whose path contains neither whitespace nor `:`.

## Review Focus

- **UTF-8 byte boundary:** exactly-at-limit and one-byte-over-limit multibyte requests must retain existing `MessageTooLarge` behavior. The size decision counts the supplied terminator; recorded request text, byte count and digest strip exactly one LF or CRLF. Task 3 pins both cases.
- **Line termination:** EOF without a trailing newline and preserved CRLF must produce one exchange with the documented adapter-observed text. Task 3 empirical subprocess evidence corrected the original assumption of universal newline normalization; Python subprocess stdin preserves CRLF here.
- **Competing ownership:** two writers targeting the same path must never share or truncate a trace; Task 1 proves exclusive creation by accepting exactly one owner.
- **Broken stdout after trace write:** a client-side pipe failure must not produce a valid completed trace; Task 3 proves the process exits nonzero and the verifier reports `INCOMPLETE`.
- **Metadata leakage:** executable path, asset-root path, unrelated environment values, and internal exception text must not appear in a valid trace; Task 3 scans the complete artifact.

---

## File Structure

- Create `tools/bruce_mcp_trace.py`: trace constants, secure writer, canonical hashing, environment configuration, and pure verifier.
- Create `tools/verify_bruce_mcp_trace.py`: thin command-line wrapper around `verify_trace`.
- Modify `tools/bruce_mcp.py`: testable message result/processing functions, adapter version `0.3`, optional writer lifecycle, and fail-closed emission order.
- Modify `tests/integration/test_bruce_mcp.py`: writer, verifier, adapter integration, security, boundary, and failure tests within the existing `bruce_mcp_workflows` CTest.
- Modify `README.md`: current feature statement and local launch/verify example.
- Modify `docs/architecture/04-local-security-boundaries.md`: authoritative security, confidentiality, and non-identity boundary.
- Modify `docs/superpowers/specs/2026-10-18-bruce-mcp-audit-trace-design.md`: change status only after fresh acceptance evidence exists.

### Task 1: Trace writer and offline verifier

**Files:**
- Create: `tools/bruce_mcp_trace.py`
- Create: `tools/verify_bruce_mcp_trace.py`
- Modify: `tests/integration/test_bruce_mcp.py`

**Interfaces:**
- Produces: `TRACE_VERSION = "bruce-mcp-trace-v1"`, `ADAPTER_VERSION = "0.3"`.
- Produces: `TraceConfigurationError`, `TraceWriteError`.
- Produces: `canonical_json(value: object) -> str` and `record_hash(record_without_hash: dict) -> str`.
- Produces: `TraceWriter(path: os.PathLike[str] | str, run_id: str, *, wall_clock_ns: Callable[[], int] = time.time_ns, monotonic_ns: Callable[[], int] = time.monotonic_ns)`.
- Produces: `TraceWriter.append_exchange(*, request_text: str | None, request_bytes: int, request_sha256: str, request: object | None, response_text: str | None, response: dict | None, outcome: str, received_unix_ns: int, completed_unix_ns: int, duration_monotonic_ns: int) -> None`.
- Produces: `TraceWriter.close() -> None`, idempotent after the first valid footer.
- Produces: `writer_from_environment(environ: Mapping[str, str]) -> TraceWriter | None`.
- Produces: `verify_trace(path: os.PathLike[str] | str) -> dict[str, object]` with status `VALID`, `INCOMPLETE`, `TAMPERED`, or `INVALID_FORMAT`.
- Produces: verifier CLI accepting exactly one path, emitting one compact JSON object, and returning 0 only for `VALID`.

- [ ] **Step 1: Add failing tests for secure configuration and exclusive ownership**

Add `TraceFormatTests` to `tests/integration/test_bruce_mcp.py`. Import the new module from `tools/` and add tests named:

- `test_trace_environment_requires_both_values`
- `test_trace_rejects_invalid_run_ids_and_relative_paths`
- `test_trace_rejects_existing_file_and_target_symlink`
- `test_trace_rejects_symlinked_parent`
- `test_trace_permission_failure_is_generic`
- `test_two_writers_cannot_own_the_same_path`

Assert absent variables return `None`; partial/invalid configurations raise `TraceConfigurationError`; the first writer creates a regular `0600` file; every second open fails without changing the first file. Mock `os.open` for the permission case and assert the public exception carries only the stable class `TraceConfigurationError`.

- [ ] **Step 2: Run the configuration tests and confirm RED**

Run:

```sh
BRUCE_BINARY=/tmp/bruce-final-verify-YfcjUZ/release-build/bruce \
python3 -m unittest \
  tests.integration.test_bruce_mcp.TraceFormatTests.test_trace_environment_requires_both_values \
  tests.integration.test_bruce_mcp.TraceFormatTests.test_trace_rejects_existing_file_and_target_symlink -v
```

Expected: import or symbol failures because `bruce_mcp_trace` does not exist.

- [ ] **Step 3: Implement configuration validation and secure creation**

Implement the constants, exception classes, run-ID regex, absolute canonical-parent validation, exclusive `os.open` flags, `fstat`, `fchmod(0o600)`, and the `writer_from_environment` pair semantics in `tools/bruce_mcp_trace.py`. Do not create parent directories or expand paths.

- [ ] **Step 4: Run the secure-configuration tests and confirm GREEN**

Run:

```sh
BRUCE_BINARY=/tmp/bruce-final-verify-YfcjUZ/release-build/bruce \
python3 -m unittest tests.integration.test_bruce_mcp.TraceFormatTests -v
```

Expected: configuration and ownership tests pass; later not-yet-added trace tests are absent.

- [ ] **Step 5: Add failing tests for records, hash chaining, and statuses**

Add tests named:

- `test_writer_emits_canonical_valid_trace_and_mode_0600`
- `test_writer_close_is_idempotent`
- `test_verifier_reports_incomplete_without_footer`
- `test_verifier_reports_tampered_changed_deleted_and_reordered_records`
- `test_verifier_reports_invalid_format_for_empty_malformed_and_unknown_records`
- `test_verifier_cli_is_json_and_exit_status_matches_result`
- `test_verifier_does_not_execute_bruce_or_read_assets`

Use fixed injected clocks. Assert header sequence 0 and 64-zero previous hash, contiguous exchanges, one footer, stable hashes, `message_count`, and structured verifier fields `status`, `trace_version`, `run_id`, `message_count`, `final_hash`, and `reason`. For the non-execution test, place an executable sentinel and unreadable asset sentinel in the environment and assert neither is touched.

- [ ] **Step 6: Run the format tests and confirm RED**

Run:

```sh
BRUCE_BINARY=/tmp/bruce-final-verify-YfcjUZ/release-build/bruce \
python3 -m unittest tests.integration.test_bruce_mcp.TraceFormatTests -v
```

Expected: failures for missing record and verification behavior.

- [ ] **Step 7: Implement canonical records, writer lifecycle, and verifier**

Implement the exact signatures in the Interfaces block. Serialize with sorted keys, compact separators, UTF-8, `ensure_ascii=False`, and `allow_nan=False`. Write each complete line with a retrying `os.write` loop followed by `os.fsync`. Validate schema before semantic chain checks so malformed JSON/types return `INVALID_FORMAT`, while sequence, run-ID, count, or hash contradictions return `TAMPERED`; a valid prefix lacking only the footer returns `INCOMPLETE`.

- [ ] **Step 8: Implement the thin verifier CLI**

`tools/verify_bruce_mcp_trace.py` imports `verify_trace`, accepts exactly one positional path, writes one compact JSON object plus newline, returns 0 for `VALID`, and returns 1 for every other status or usage error without exposing exception details.

- [ ] **Step 9: Run all trace-format tests and the existing MCP test file**

Run:

```sh
BRUCE_BINARY=/tmp/bruce-final-verify-YfcjUZ/release-build/bruce \
python3 -m unittest tests.integration.test_bruce_mcp -v
```

Expected: all tests pass; existing MCP cases remain green.

- [ ] **Step 10: Review task diff and record the intended checkpoint**

Run the Git preflight. If unavailable, run:

```sh
diff -u /tmp/bruce-mcp-trace-baseline-UJpmqn/source/tools/bruce_mcp_trace.py tools/bruce_mcp_trace.py || true
diff -u /tmp/bruce-mcp-trace-baseline-UJpmqn/source/tests/integration/test_bruce_mcp.py tests/integration/test_bruce_mcp.py
```

Record proposed commit: `feat(mcp): add hash-chained trace format`.

### Task 2: Testable adapter message boundary without tracing

**Files:**
- Modify: `tools/bruce_mcp.py`
- Modify: `tests/integration/test_bruce_mcp.py`

**Interfaces:**
- Consumes: `ADAPTER_VERSION` from Task 1.
- Produces: immutable `MessageResult(request: object | None, response: dict | None, response_text: str | None, outcome: str)`.
- Produces: `make_reply(identifier: object, result: object | None = None, error: str | None = None, code: int = -32602) -> dict`.
- Produces: `serialize_reply(response: dict) -> str` using the existing compact JSON settings and `allow_nan=False`.
- Produces: `handle_message(message_text: str) -> MessageResult` for all non-oversized messages.
- Produces: `process_stream(input_stream: TextIO, output_stream: TextIO, error_stream: TextIO, trace_writer: TraceWriter | None = None) -> int`.
- Produces: `main(environ: Mapping[str, str] = os.environ) -> int` guarded by `if __name__ == "__main__"`.

- [ ] **Step 1: Add characterization tests for exact trace-disabled stdout**

Extend the test helper to retain raw stdout. Add:

- `test_trace_disabled_stdout_is_exactly_unchanged_except_server_version`
- `test_server_version_is_0_3_without_new_capabilities_or_tools`
- `test_handle_message_returns_notification_without_response`
- `test_importing_adapter_does_not_start_stdio_loop`

Use initialize, initialized notification, tools/list, successful calculation, parse error, invalid envelope, and unknown method. Assert the same response order, compact serialization, error codes/messages, seven-tool set, and schemas already covered by current tests.
Run the transcript once with protocol `2025-06-18` and once with `2024-11-05`; assert the existing modern structured-result and legacy text-response behavior remains unchanged.

- [ ] **Step 2: Run the characterization tests and confirm RED**

Run the four named tests directly with `python3 -m unittest ... -v`.

Expected: failures because the adapter still executes its loop at import and lacks the result interfaces/version.

- [ ] **Step 3: Refactor reply construction and message dispatch**

Introduce the exact interfaces above. Move the top-level loop into `process_stream`; make `handle_message` return data instead of printing. Preserve the current protocol negotiation, validation, subprocess boundaries, error messages, message limit, notification semantics, and command construction. Construct each response text once with `serialize_reply`.

- [ ] **Step 4: Bump only the MCP server version**

Return `serverInfo.version: "0.3"`; do not add a capability or tool and do not alter input/output schemas.

- [ ] **Step 5: Run the full existing MCP workflow tests**

Run:

```sh
BRUCE_BINARY=/tmp/bruce-final-verify-YfcjUZ/release-build/bruce \
python3 tests/integration/test_bruce_mcp.py -v
```

Expected: all existing and new characterization tests pass.

- [ ] **Step 6: Compare trace-disabled wire output against the baseline adapter**

Feed the same fixed JSONL transcript to the baseline and new scripts with the same BRUCE binary and asset root. Normalize only `serverInfo.version` from `0.2` to `0.3`, then byte-compare stdout and stderr. Expected: identical after that one documented normalization.

- [ ] **Step 7: Review task diff and record the intended checkpoint**

Compare `tools/bruce_mcp.py` and the tests against the saved baseline. Record proposed commit: `refactor(mcp): isolate stdio message processing`.

### Task 3: Wire fail-closed tracing into the adapter

**Files:**
- Modify: `tools/bruce_mcp.py`
- Modify: `tests/integration/test_bruce_mcp.py`

**Interfaces:**
- Consumes: `TraceWriter`, `TraceConfigurationError`, `TraceWriteError`, and `writer_from_environment` from Task 1.
- Consumes: `MessageResult` and `process_stream` from Task 2.
- Produces: trace-disabled behavior unchanged; trace-enabled lifecycle of header, one exchange per input line, and footer on normal EOF.
- Produces: stderr classes `BruceMcpTraceConfigurationError` and `BruceMcpTraceWriteError`, without paths or exception details.
- Produces: process exit 2 for startup configuration failure and exit 3 for runtime trace/write or traced-output failure.

- [ ] **Step 1: Add failing happy-path integration tests**

Add:

- `test_enabled_trace_covers_initialize_notification_list_call_and_eof`
- `test_trace_responses_are_byte_identical_to_stdout_lines`
- `test_trace_file_is_mode_0600_and_verifies_valid`
- `test_trace_contains_no_unrelated_environment_or_configured_paths`
- `test_trace_disabled_creates_no_file`

Launch the real adapter subprocess with isolated temporary paths. Assert exactly one header, four exchanges, one footer; the initialized notification has null response; every non-null `response_text` equals the corresponding raw stdout line; the structured calculation value is `5`; and the trace omits sentinel secret text, the BRUCE executable path, and asset-root path.

- [ ] **Step 2: Run the happy-path tests and confirm RED**

Run the five named tests directly. Expected: no trace file or missing integration behavior.

- [ ] **Step 3: Add trace lifecycle to `main` and `process_stream`**

Create the writer before reading stdin. For each line, capture wall and monotonic start, preserve the existing message-size decision, obtain one `MessageResult`, then append and fsync the exchange before writing the exact prepared response to stdout. On regular EOF call `close`. Catch only the public trace exceptions at `main`, emit the stable stderr class, and return the specified code.

- [ ] **Step 4: Run the happy-path tests and confirm GREEN**

Run the five named tests again. Expected: all pass and the verifier reports `VALID`.

- [ ] **Step 5: Add failing negative and boundary tests**

Add:

- `test_partial_trace_environment_fails_before_stdin`
- `test_existing_target_and_symlink_fail_without_modification`
- `test_parse_and_envelope_errors_are_traced_with_exact_responses`
- `test_oversized_request_records_only_length_digest_and_complete_response`
- `test_utf8_exact_limit_and_one_byte_over_limit_preserve_existing_result`
- `test_eof_without_newline_and_crlf_record_adapter_observed_text`
- `test_injected_trace_write_failure_emits_no_response`
- `test_broken_stdout_leaves_incomplete_trace_and_nonzero_exit`

For byte-boundary tests, generate input from explicit UTF-8 bytes and calculate expected SHA-256 over adapter-observed text after removing exactly one LF or CRLF; retain the existing size decision including the supplied terminator. For the injected writer failure call `process_stream` with a fake writer that raises `TraceWriteError` on `append_exchange`. For broken stdout use a test stream that raises `BrokenPipeError` on write and assert no footer.

- [ ] **Step 6: Run the negative tests and confirm RED**

Run the named tests directly. Expected: at least the write-failure, broken-output, oversized-redaction, and line-terminator assertions fail before implementation.

- [ ] **Step 7: Implement fail-closed boundary behavior**

Add stable configuration/write error handling, oversized-request metadata-only recording, removal of exactly one LF or CRLF for recorded text/length/digest, and output-failure conversion to `TraceWriteError`. Preserve the existing size decision including the supplied terminator. Never write a response after an exchange append fails. If stdout fails after the trace append, terminate without a footer so verification is `INCOMPLETE`; document that the trace records the selected payload rather than client receipt.

- [ ] **Step 8: Run the complete MCP integration suite**

Run:

```sh
BRUCE_BINARY=/tmp/bruce-final-verify-YfcjUZ/release-build/bruce \
python3 tests/integration/test_bruce_mcp.py -v
```

Expected: all tests pass with no skipped trace cases.

- [ ] **Step 9: Run focused CTest from the last verified build**

Run:

```sh
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-final-verify-YfcjUZ/release-build \
  -R '^bruce_mcp_workflows$' --output-on-failure
```

If source-path restrictions or the existing build make this evidence stale, do not claim success; defer the authoritative result to Task 4's fresh safe copy.

- [ ] **Step 10: Review task diff and record the intended checkpoint**

Compare adapter, trace module, verifier, and tests against the saved baseline. Record proposed commit: `feat(mcp): record process-bound audit traces`.

### Task 4: Documentation and full acceptance

**Files:**
- Modify: `README.md`
- Modify: `docs/architecture/04-local-security-boundaries.md`
- Modify: `docs/superpowers/specs/2026-10-18-bruce-mcp-audit-trace-design.md`
- Verify only: `docs/cli/source-index.md`
- Verify only: `docs/cli/examples.json`
- Verify only: `docs/cli/validation.md`

**Interfaces:**
- Consumes: the final environment variables, file contract, verifier statuses, and exit behavior from Tasks 1–3.
- Produces: source-backed user documentation that distinguishes the stdio trace from the HTTP inspector, signatures, identities, and mathematical proof.

- [ ] **Step 1: Add documentation assertions before prose changes**

Extend the existing MCP integration test with `test_documented_trace_example_uses_current_environment_and_verifier`, reading the authoritative documentation and asserting it contains both environment names, `bruce-mcp-trace-v1`, the verifier command, all four status names, `0600`, and the non-identity/non-proof boundary.

- [ ] **Step 2: Run the documentation assertion and confirm RED**

Run the single test. Expected: failure because current docs do not describe tracing.

- [ ] **Step 3: Update current documentation**

In `README.md`, add a compact opt-in launch and verification example next to the stdio adapter description. In `docs/architecture/04-local-security-boundaries.md`, document exclusive creation, confidentiality, fail-closed behavior, full boundary content, hash-chain limits, and separation from the HTTP inspector. Do not describe the trace as a signature, identity proof, mathematical proof, or remote audit service.

- [ ] **Step 4: Run focused tests and documentation checks**

Run:

```sh
BRUCE_BINARY=/tmp/bruce-final-verify-YfcjUZ/release-build/bruce \
python3 tests/integration/test_bruce_mcp.py -v
python3 scripts/check_local_doc_links.py
```

Expected: all MCP tests pass and all local links resolve.

- [ ] **Step 5: Create a fresh complete safe source copy**

Create a unique directory under `/tmp` with `mktemp -d`, copy the complete current tree into its `source/` child with `cp -a`, and run every wrapper command only from that no-space path.

- [ ] **Step 6: Configure and build Release with tests**

From the safe copy run:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-mcp-trace-build \
  -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-mcp-trace-build
```

Expected: configure and build exit 0.

- [ ] **Step 7: Run focused and complete CTest**

Run:

```sh
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-mcp-trace-build \
  -R '^bruce_mcp_workflows$' --output-on-failure
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-mcp-trace-build --output-on-failure
```

Expected: focused test passes; complete CTest reports zero product/test failures. Record any sandbox-only socket failures separately and rerun only affected tests with the required permission.

- [ ] **Step 8: Verify generated CLI artifacts in the safe copy**

From the safe copy run:

```sh
python3 docs/cli/verify_examples.py /tmp/bruce-mcp-trace-build/bruce
cmp docs/cli/source-index.md /home/chrisi/Downloads/My-Bruce-AI-main-beta-002\ \(2\)/docs/cli/source-index.md
cmp docs/cli/examples.json /home/chrisi/Downloads/My-Bruce-AI-main-beta-002\ \(2\)/docs/cli/examples.json
cmp docs/cli/validation.md /home/chrisi/Downloads/My-Bruce-AI-main-beta-002\ \(2\)/docs/cli/validation.md
python3 scripts/check_local_doc_links.py
```

Expected: verifier exits 0, every `cmp` is identical, links pass.

- [ ] **Step 9: Run a real trace acceptance scenario**

Use `/tmp/bruce-mcp-trace-build/bruce`, a temporary approved asset root, and a new trace path. Send initialize, initialized notification, tools/list, `bruce_calculate` add `2+3`, and EOF. Verify:

```sh
python3 tools/verify_bruce_mcp_trace.py /tmp/bruce-mcp-trace-acceptance.jsonl
```

Expected: exit 0 and JSON status `VALID`, message count 4, structured calculation value `5`, mode `0600`, and no configured executable/asset-root strings in the trace.

- [ ] **Step 10: Compare the final working tree against the baseline**

Run `diff -qr` against `/tmp/bruce-mcp-trace-baseline-UJpmqn/source`, excluding only generated local build areas and pre-existing `runs/`. Expected changed set:

- `tools/bruce_mcp_trace.py`
- `tools/verify_bruce_mcp_trace.py`
- `tools/bruce_mcp.py`
- `tests/integration/test_bruce_mcp.py`
- `README.md`
- `docs/architecture/04-local-security-boundaries.md`
- the new spec and this plan

Investigate every additional path before completion.

- [ ] **Step 11: Promote the design status only with fresh evidence**

If and only if Tasks 1–4 pass, change the design status to **IMPLEMENTIERT UND BELEGT** and cite `bruce_mcp_workflows`, the complete CTest count, documentation checks, and the acceptance trace. Otherwise retain the honest partial status.

- [ ] **Step 12: Run final hygiene checks and record the intended checkpoint**

Run `git status`, `git diff --check`, and Git diff inspection. If Git remains unavailable, report Exit 128, run whitespace/conflict-marker checks over only the baseline-changed text files, and use the baseline comparison as the source-of-truth change inventory. Record proposed commit: `docs(mcp): document audit trace boundary`.

## Final Acceptance Criteria

- **IMPLEMENTIERT UND BELEGT:** Trace-disabled MCP behavior is unchanged except the documented server version `0.3`.
- **IMPLEMENTIERT UND BELEGT:** A traced initialize/notification/list/calculate session verifies `VALID`, preserves exact emitted response text, and exposes structured result `5`.
- **IMPLEMENTIERT UND BELEGT:** Existing targets, symlinks, invalid configuration, write failures, oversized input, malformed input, truncation, tampering, and invalid format have explicit passing tests.
- **IMPLEMENTIERT UND BELEGT:** Trace contents omit environment/configuration metadata outside the MCP boundary and are created `0600`.
- **IMPLEMENTIERT UND BELEGT:** Fresh Release build, focused CTest, full CTest, CLI artifact verification, and local Markdown links pass or any environment-only exception is separately evidenced.
- **DOKUMENTIERT / KONZEPT:** Signatures, author identity, hostile-local-user resistance, remote audit, and mathematical proof remain outside the delivered claim.
