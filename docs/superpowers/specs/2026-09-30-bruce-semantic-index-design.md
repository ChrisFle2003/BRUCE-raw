# BRUCE Semantic Index — Design Specification

## Purpose

Build a local Python CLI that reads the BRUCE Markdown and JSON references,
creates a locally embedded semantic index for discoverability, and separately
validates a *proposed* BRUCE command line against only machine-readable
reference rules. It never invokes `bruce`.

## Authoritative inputs

- Primary structured source:
  `docs/cli/bruce-cli-reference.json` (`bruce-cli-reference-v1`, CLI 0.102.0).
- Supplemental full-text source: `docs/cli/bruce-cli-reference.md`.

The loader must parse the JSON first, require its expected top-level fields,
and retain source path, digest, version, and load result. Markdown is always
loaded as text for segmentation and retrieval. A structured reference rule
wins when it conflicts with prose, except that a documented JSON defect is
recorded instead of silently resolved.

## Product boundary

The tool has three independent operations:

1. `build-index` builds semantic items and obtains embeddings from a local
   Ollama endpoint.
2. `search` ranks stored items and recommends relevant BRUCE command paths;
   every result is labelled a recommendation, not a syntax proof.
3. `validate-command` parses a supplied command string and checks only
   deterministic rules represented by the reference. It emits JSON only and
   never calls BRUCE.

There is no cloud API, no automatic model installation, no shell command
execution beyond this program, and no command recommendation is executable by
the tool.

## Architecture

Use Python's standard library only, targeting Python 3.11 or later:

| Module | Responsibility |
| --- | --- |
| `reference.py` | Strict JSON loading, Markdown loading, normalized command records, and conflict notes. |
| `segmenter.py` | Stable document/paragraph/sentence/token segmentation with source offsets. |
| `ollama.py` | Timeout- and retry-bounded local `/api/embeddings` client; no pull action. |
| `indexer.py` | Semantic item assembly, normalized vectors, index validation, JSON/JSONL/SQLite export. |
| `search.py` | Local synonym expansion, query embedding, cosine ranking, result aggregation by command. |
| `validator.py` | `shlex` command parsing and structured-rule validation with explicit partial findings. |
| `cli.py` | Argument parser, machine-readable error output where required, and no BRUCE process launch. |

`pyproject.toml` exposes the package as `python -m bruce_semantic_index`; tests
use `unittest` so CI does not require Ollama or package downloads.

## Reference normalization and conflicts

The loader creates a validated internal representation of the documented 30
JSON command entries. It must also record these known source differences in
`validation.conflicts`:

1. Markdown forbids the `--` argument separator; JSON says `---Trenner`.
   Validation rejects `--`, and the mismatch is recorded.
2. Markdown documents `bruce`, `bruce help`, and `bruce --help`; JSON has no
   command entries for them. They are indexed from Markdown but validation is
   `partial` for this supplemental rule.
3. Markdown has a serialisation-cancellation family not present in structured
   JSON. It is indexed but never globally admitted by the validator.
4. JSON's `grid-batch` inherits options in prose instead of enumerating them.
5. Markdown documents research `--max-total-operations`; JSON omits it.
6. JSON groups all `experience` actions into one entry, making action-specific
   required options and positional forms incomplete.

No option, default, alias, or constraint is created to paper over these gaps.

## Segmentation and index schema

Markdown produces one `document` item, paragraph items, sentence items, and
token/word items. Tokenization preserves options such as `--max-bits`, CLI
paths, ASCII words, German umlauts, English words, numbers, and punctuation
boundaries. Every item has a deterministic ID from source path, kind, and byte
offset; source references retain line/offset context.

The index also contains one `command` item per JSON command record and one
`option` item per explicitly listed option. Each item includes text,
normalized text, language hint, source information, command path, optional
option name, metadata, and one finite non-empty normalized embedding.

The JSON export follows the user-supplied top-level schema and adds only
documented metadata. JSONL writes a metadata header followed by one item per
line. SQLite stores metadata, items, and embeddings in separate documented
tables. All three are validated after writing.

## Ollama and model configuration

The default model is `all-minilm:22m`. Precedence is CLI `--model`, a local JSON
file passed with `--config` (`model` and/or `base_url`),
`BRUCE_SEMANTIC_INDEX_MODEL`, then the default. Base URL follows the same
pattern with default `http://localhost:11434`.

The client calls only local `POST /api/embeddings`, applies bounded timeouts
and retries, and rejects empty, non-numeric, or dimension-inconsistent
responses. Connection errors explain that Ollama must be running locally.
Missing-model responses provide an `ollama pull <model>` hint but never pull
the model. Embeddings are L2-normalized and queried by cosine similarity.

## Search

Before embedding, a local editable JSON synonym map expands selected German and
English terms, including `fortsetzen/resume`, `prüfen/validate/check`, and
`Regeln/rules`. Expansion supplements the original query and does not claim
translation equivalence. Search reads an existing valid index, checks vector
dimensions, ranks by cosine score, groups supporting items by `command_path`,
and prints score, command path, documented syntax, concise reason, and
explicitly listed options. Output says that results are recommendations only.

## Deterministic validation

`validate-command` takes a single command string, uses `shlex.split`, and
returns JSON for valid, invalid, or partially checkable input. It checks:

- exact `bruce` executable and known JSON command path;
- no short options, no `--name=value`, no standalone `--`, duplicate options,
  unknown options, or missing option values;
- explicitly listed required options, positionals, exclusive/required-with
  relations, enumerated values, and simple integer/range/format constraints
  when structured data expresses them;
- exact case sensitivity and separate negative numeric arguments.

The Markdown-only top-level forms `bruce`, `bruce help`, and `bruce --help`
are recognized as documented supplemental help forms and return `partial`
with a source note; no subcommand `--help` is admitted.

The validator reports `partial` and `unchecked_constraints` for free-text,
file-system, semantic, action-specific, parser-scope, bundle, or mathematical
rules that cannot be derived from structured JSON. It does not infer allowed
options from shared families or Markdown prose.

## Error behaviour

Failures are clear and non-destructive: missing input, invalid JSON, empty
Markdown, unavailable Ollama, missing model, empty embedding, dimension
mismatch, and corrupt index each receive an actionable error. The CLI reserves
exit 0 for successful own processing, exit 2 for input/validation/tool errors;
these tool exit codes are not BRUCE result assertions.

## Tests and acceptance criteria

Tests must run without Ollama, cloud access, or BRUCE execution. Unit coverage
includes JSON reference loading, recorded conflicts, Unicode/CLI-token
segmentation, global validation rules, positional and option validation,
partial-result boundaries, JSON/JSONL/SQLite round trips, and semantic ranking
with a mock embedding client. An opt-in integration test checks Ollama only
when the local endpoint is reachable.

Acceptance evidence is a fresh `python -m unittest discover -v` run plus:

- JSON index parse and schema-shape check;
- JSONL and SQLite round trips;
- mocked German and English search examples;
- valid and invalid command-validation JSON examples;
- confirmation that no test or implementation invokes `bruce`.

## Explicit uncertainty

`all-minilm:22m` may not be installed under that exact local tag, and its German /
English quality varies by installed model. Local aliases improve retrieval but
do not replace multilingual embeddings. The supplied BRUCE reference has
machine-readable gaps; `partial` therefore means “not proven by this tool”,
not “valid BRUCE syntax”.
