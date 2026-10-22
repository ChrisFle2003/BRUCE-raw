# Local Ollama assistant V1

**IMPLEMENTIERT UND BELEGT:** `tools/bruce_ai.py` is a separate Python
assistant for a bounded catalog of BRUCE commands. The integration test
`tests/integration/test_bruce_ai.py`, registered as CTest
`bruce_local_ai_workflows`, uses temporary loopback HTTP mocks and fixture
executables only. It requires no Ollama, models or real BRUCE binary.
The C++ core, stdio MCP adapter and HTTP inspector are separate interfaces;
see the [CLI manual](README.md) and [project overview](../../README.md).

**IMPLEMENTIERT, ABER NICHT AKTUELL VERIFIZIERT:** actual Ollama/model
availability, answer quality and execution with real models are not
established by the mock tests. Python 3.11+ is the intended runtime; current
mock verification uses Python 3.13, with Python 3.11 grammar checks only.

## Prerequisites and explicit configuration

Use Linux/POSIX, Python 3.11+ and its standard library, an already built
executable `bruce`, an independently provisioned local Ollama daemon and two
locally available models supporting embeddings and JSON chat respectively.
BRUCE bundles no models. This tool never downloads models or invokes
`ollama pull`; a missing model requires separate user action. Model names
below are configuration examples, not installation guarantees.

Save this complete example as `assistant.json` after replacing installation
paths and selecting models already available locally. All configured paths
must be absolute. The executable and allowed root directories must exist;
the index parent directory must exist. No directory is created recursively.

```json
{
  "schema": "bruce-local-ai-config-v1",
  "active_profile": "default",
  "bruce_program": "/absolute/path/to/bruce",
  "index_path": "/absolute/private/bruce-ai-index.json",
  "allowed_roots": ["/absolute/path/to/bruce-data"],
  "require_confirmation": true,
  "profiles": {
    "default": {
      "ollama_url": "http://127.0.0.1:11434",
      "embedding_model": "all-minilm",
      "response_model": "qwen3.5",
      "top_k": 5,
      "connect_timeout_ms": 1000,
      "request_timeout_ms": 30000
    },
    "alternate": {
      "ollama_url": "http://127.0.0.1:11434",
      "embedding_model": "other-embedding:latest",
      "response_model": "other-chat:latest",
      "top_k": 3,
      "connect_timeout_ms": 1000,
      "request_timeout_ms": 30000
    }
  }
}
```

Every operation requires `--config`; there is no environment-based or hidden
global configuration. Unknown fields, duplicate JSON keys, invalid paths,
nonpositive/noninteger limits, and any value other than literal `true` for
`require_confirmation` are rejected. Every profile is validated, including
unselected profiles. Model names must be nonblank tags without whitespace.
Explicit `:cloud` and `-cloud` tag suffixes are rejected case-insensitively.

## Simple local starter

**IMPLEMENTIERT UND BELEGT:**
[`scripts/bruce-local-ai.sh`](../../scripts/bruce-local-ai.sh) is a small
convenience entry point. It only forwards four existing assistant operations;
it does not download models, create directories, overwrite an index, bypass
the preview, or add any executable command surface.

Start by copying the complete local-only
[`examples/bruce-local-ai-config.example.json`](../../examples/bruce-local-ai-config.example.json)
to a private location. Replace only its three absolute paths with your BRUCE
executable, a new index filename whose parent already exists, and a directory
that contains the data you deliberately permit BRUCE to read. The supplied
model tags are examples of locally installed Ollama models; change them in the
JSON if your local tags differ.

```sh
cp examples/bruce-local-ai-config.example.json /absolute/private/assistant.json
$EDITOR /absolute/private/assistant.json

scripts/bruce-local-ai.sh config-check /absolute/private/assistant.json
scripts/bruce-local-ai.sh index-build /absolute/private/assistant.json
scripts/bruce-local-ai.sh ask /absolute/private/assistant.json 'Berechne 7 plus 5 exakt.'
```

The last command first prints a JSON preview. Inspect its `argv` and type
exactly `run` plus Enter only if it is the intended local operation. The
starter preserves standard input, so it cannot confirm on your behalf.

Three common forms are:

```sh
# Exact arithmetic
scripts/bruce-local-ai.sh ask /absolute/private/assistant.json 'Berechne 2 hoch 10 modulo 1000 exakt.'

# Read an existing run that lives under allowed_roots
scripts/bruce-local-ai.sh ask /absolute/private/assistant.json 'Prüfe den vorhandenen Run sample.bruce nur lesend.'

# Bounded research; train.csv and holdout.csv must be under allowed_roots
scripts/bruce-local-ai.sh ask /absolute/private/assistant.json 'Research mit train.csv und holdout.csv: Domain bigint, Bereich -8 bis 8, max_bits 64, max_denominator 1, max_candidates 100, max_operations 1000, max_trace 10, max_results 10 und max_trace_bytes 4096.'
```

To choose another local model pair, add another complete profile to the JSON,
set `active_profile`, build a fresh index at a new `index_path`, then use the
same profile consistently for `index-build` and `ask`. The helper also accepts
an optional profile name as its final argument, for example
`scripts/bruce-local-ai.sh config-check /absolute/private/assistant.json alternate`.

Only HTTP origins with literal `127.0.0.1` or `[::1]` and an explicit port
are accepted. `localhost`, HTTPS, credentials, non-root paths, queries and
fragments are rejected. Proxy settings cannot redirect requests; HTTP
redirects are refused. `config check` performs metadata-only `POST /api/show`
checks for both selected models, without inference or writes:

```sh
python3 tools/bruce_ai.py config check --config assistant.json
python3 tools/bruce_ai.py config check --config assistant.json --profile alternate
```

The same locality preflight runs before every embedding or chat request.
It admits only `details.format == "gguf"`, a nonblank
`model_info["general.architecture"]`, and omitted or empty-string
`remote_host`/`remote_model`. Missing, unsupported or remote metadata fails
closed, including remote aliases with ordinary local-looking names. This
trusts the local daemon's metadata; it cannot protect against a malicious
daemon or model replacement between preflight and inference.

`--profile NAME` applies to one invocation and does not rewrite
`active_profile` on disk. Replace alternate model examples before use. The
index path is shared configuration, not a separate per-profile default.

## Build the command index explicitly

Run from the repository root, after configuring your paths:

```sh
python3 tools/bruce_ai.py index build --config assistant.json
python3 tools/bruce_ai.py ask --config assistant.json --question 'Calculate 7 plus 5 exactly'
```

Index V2 (`bruce-ai-command-index-v2`) embeds exactly each repository catalog
entry's concise human synopsis, in deterministic command-ID order. Technical
JSON, argument schemas, CLI syntax and source paths are excluded from embedding
input; the complete catalog still governs candidate validation and execution.
Index creation does not crawl user files or the repository. It stores normalized
vectors, the complete catalog digest, profile name, embedding tag and dimension.
`ask` embeds the question, ranks locally, then sends the question and selected catalog records
to the response model. BRUCE input files are not automatically sent to the
proposal model. Completed BRUCE JSON is sent for explanation.

Routing and explanation use nonstreaming JSON chat with explicit `think: false`
to request disabled model thinking where the configured model supports that
control during these bounded structured requests. Embedding requests do not
include this chat option; configured deadlines still apply.

An absent, invalid or stale index blocks routing. After upgrading from the V1
index format to V2, explicitly rebuild the index: every V1 index is rejected
with `AssistantIndexStale`, even if catalog, model, profile and dimension match.
Editing only the stored schema label cannot regenerate compatible embeddings.
Changes of catalog, profile name, embedding tag or dimension also require an
explicit fresh index. Changing only a response model within the same profile
does not invalidate embeddings.
There is no automatic rebuild or overwrite: index publication is exclusive.
For a rebuild, preserve the old index and set `index_path` to a new absolute
filename with an existing parent, then run `index build`. To use another
profile, select it consistently during build and ask:

```sh
python3 tools/bruce_ai.py index build --config assistant.json --profile alternate
python3 tools/bruce_ai.py ask --config assistant.json --profile alternate --question 'Calculate 7 plus 5 exactly'
```

These alternate commands assume you have selected a fresh `index_path`.
Replacing weights behind the same model tag is not detected if the dimension
stays the same; rebuild explicitly after such a change.

## Preview, confirmation and path boundary

`ask` emits and flushes a `bruce-local-ai-preview-v1` JSON line with catalog
command ID, ranking scores, canonical `argv`, missing inputs, side-effect
class and `confirmation_required: "run"`. Inspect it before typing exactly
`run` and Enter. Only `run` with an optional LF terminator starts BRUCE.
Case changes, spaces, CRLF, rejection and EOF without the token cannot start
it. There is no confirmation-bypass flag. Missing required input produces
an empty argv preview and `AssistantInputRequired` without execution.

The 24 catalog routes cover bounded calculation, coordinates,
grid/compare/research and existing-run workflows, nine fixed demos, and
read-only `mods list`, `mods inspect`, `mods verify`, `mods trust status`.
Research exposes bounded affine CSV inputs, not every research strategy.
Installation, activation, updates, rollback, trust transitions, release
signing, rules and output/export options cannot be routed. Model extras
never become executable paths, free argv, environment settings, working
directories or additional routes.

Routed input files/directories must exist with the required type beneath an
explicit `allowed_roots` entry. Both lexical and resolved containment are
checked; traversal, external absolute paths, symlink escapes, ambiguous
relative matches across roots and special files are refused. Relative input
paths resolve beneath allowed roots, not the shell's current directory.
The configured program/index and explicit explain/transcript paths are
separate user choices and need not be inside data roots.

Execution uses direct argv with no shell, closed stdin, a fixed minimal
environment and a private process group. Runtime honors `request_timeout_ms`
with a 120-second ceiling. Retained stdout is capped at 1 MiB and stderr at
256 KiB; timeout or overflow terminates the process group. HTTP requests are
capped at 256 KiB and responses at 1 MiB, with connect/request ceilings of
10/120 seconds; lower configured values apply.

## Results, explanation and optional transcript

A `bruce-local-ai-result-v1` line keeps authoritative BRUCE JSON, exact
stdout/stderr bytes encoded as base64, SHA-256 of retained stdout, argv,
exit code, process/parse status and truncation flags under `bruce`.
Raw BRUCE JSON is evidence; model prose is not BRUCE evidence or a proof.
The separate `answer` is marked `untrusted_model_explanation` and carries
the source digest and caveats. A digest identifies bytes; it is neither a
signature nor an approval. For truncated output it covers retained bytes only.

Invalid/incomplete BRUCE output has no model answer. Failed or interrupted
explanation preserves the completed BRUCE result and reports the explanation
error separately. JSON records use stdout; assistant errors use fixed JSON
codes on stderr and exit 2. Otherwise the process exit code controls `ask`'s
exit status. Failed runs and invalid JSON retain their original streams.

To explain an existing regular JSON-object file without starting BRUCE:

```sh
python3 tools/bruce_ai.py explain --config assistant.json --input /absolute/path/to/result.json
```

The input is bounded to 1 MiB; final symlinks and FIFOs are rejected.
The envelope marks it as `provided` with empty argv and null exit code,
making no execution claim. Supply raw BRUCE JSON, not an assistant envelope.

There is no automatic prompt/result log, persistent chat session or memory.
Only explicit `ask --transcript-out` requests a local transcript:

```sh
python3 tools/bruce_ai.py ask --config assistant.json --question 'Calculate 7 plus 5 exactly' --transcript-out /absolute/private/new-transcript.json
```

Publication occurs only after a completed exit-0 process with valid JSON and
after the result is emitted. Explanation failure may be recorded alongside
that successful result. The versioned transcript contains question, profile,
catalog/index digests, preview, result and explanation; it is not an audit
proof or signature. Private mode-0600 staging and atomic exclusive publication
prevent replacing an existing destination. Index and transcript publication
promise complete no-clobber visibility, not directory entry crash durability.
They require filesystem hard-link support.

## Verification and known limits

```sh
python3 tests/integration/test_bruce_ai.py
ctest --test-dir /absolute/path/to/build -R '^bruce_local_ai_workflows$' --output-on-failure
python3 scripts/check_local_doc_links.py
```

The tests open temporary loopback sockets; a sandbox must permit them.
The CTest is classified `sanitizer-excluded` because it exercises Python and
fixture programs, not instrumented C++. Release and unsanitized Debug suites
include it. Passing mock tests do not establish real model quality.

The executable, repository catalog, daemon and local filesystem are trusted.
Path validation cannot prevent a hostile actor swapping files or parent
directories after validation. POSIX descriptor/process facilities are required;
missing no-follow/nonblocking admission flags fail closed. There is no
model-routed persistent mutation, background service, download, automatic
Experience/rule/Trust change or persistent chat. Explicit index and opt-in
transcript writes are the assistant's local publication operations.
