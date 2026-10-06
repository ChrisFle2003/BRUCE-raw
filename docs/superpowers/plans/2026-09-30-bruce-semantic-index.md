# BRUCE Semantic Index Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a local, Ollama-only Python CLI that indexes the supplied
BRUCE references, recommends documented command paths, and separately
partially validates proposed command strings without executing BRUCE.

**Architecture:** A standard-library Python package separates reference
normalization, text segmentation, Ollama embeddings, export/index storage,
semantic ranking, and deterministic validation. The valid JSON reference is
the structured primary source; Markdown is fully indexed and source gaps are
recorded rather than filled with invented CLI rules.

**Tech Stack:** Python 3.11+, `unittest`, `argparse`, `dataclasses`, `json`,
`sqlite3`, `urllib.request`, `shlex`, and local Ollama `/api/embeddings`.

**Spec:** `docs/superpowers/specs/2026-09-30-bruce-semantic-index-design.md`

## Global Constraints

- Use only local Ollama; make no cloud request and never invoke `ollama pull`.
- Default model is `all-minilm:22m`; CLI overrides configuration, configuration
  overrides `BRUCE_SEMANTIC_INDEX_MODEL`, then the default applies.
- Never execute `bruce` or a recommended command.
- Preserve only documented BRUCE paths, flags, defaults, and rules; record
  Markdown-vs-JSON differences visibly.
- Require Python 3.11+ and use no third-party runtime or test dependency.
- Semantic output is a recommendation, never a syntax proof; validation emits
  `partial` when structured reference data cannot establish a rule.

## Review Focus

1. A user-supplied `--name=value` or standalone `--` must be rejected before
   any command-path recommendation can be mistaken for validation.
2. A negative number such as `--min -8` must remain an option value, not be
   rejected as a short option.
3. Differing vector dimensions or empty Ollama embeddings must prevent export
   of a corrupt index.
4. JSON reference gaps (notably `experience` and `grid-batch`) must result in
   `partial`, never a fabricated hard validation rule.
5. German/English synonym expansion must retain the original query and must
   not make the search path network-dependent.

---

## File structure

```
pyproject.toml
README.md
bruce_semantic_index/
  __init__.py
  __main__.py
  cli.py
  config.py
  models.py
  reference.py
  segmenter.py
  validator.py
  ollama.py
  indexer.py
  search.py
  synonyms.json
tests/
  test_reference.py
  test_segmenter.py
  test_validator.py
  test_ollama.py
  test_indexer.py
  test_search.py
  test_cli.py
```

### Task 1: Package foundation, reference loader, and segmentation

**Files:**
- Create: `pyproject.toml`, `bruce_semantic_index/{__init__,__main__,models,config,reference,segmenter}.py`
- Create: `tests/test_reference.py`, `tests/test_segmenter.py`

**Interfaces:**
- Produces `Settings`, `ReferenceData`, `CommandRecord`, `OptionRecord`, `Segment`, and
  `load_reference(json_path: Path, markdown_path: Path) -> ReferenceData`.
- Produces `segment_markdown(text: str, source_path: str) -> list[Segment]`.

- [ ] **Step 1: Write failing reference-loader tests**
  Assert that the supplied JSON loads as 30 command records, rejects malformed
  top-level data, and records the known separator/help/serialization/
  grid-batch/research/experience differences.

- [ ] **Step 2: Run the reference tests to verify failure**
  Run: `python -m unittest tests.test_reference -v`.
  Expected: import failure because package code does not exist.

- [ ] **Step 3: Implement models, configuration precedence, and `load_reference`**
  Parse JSON with strict top-level type checks; read non-empty Markdown;
  retain source paths, SHA-256 digests, command records, global rules, shared
  families, notes, and conflicts. Implement `resolve_settings` with CLI >
  `--config` local JSON (`model`, `base_url`) > environment > defaults. Do not
  derive missing commands/options.

- [ ] **Step 4: Run the reference tests to verify success**
  Run: `python -m unittest tests.test_reference -v`.
  Expected: all reference-loader cases pass.

- [ ] **Step 5: Write failing segmentation tests**
  Assert stable source IDs/offsets for document, paragraph, sentence, and
  tokens including `--max-bits`, `grid-resume`, German umlauts, and English
  words.

- [ ] **Step 6: Run segmentation tests to verify failure**
  Run: `python -m unittest tests.test_segmenter -v`.
  Expected: missing `segment_markdown` behavior.

- [ ] **Step 7: Implement `segment_markdown`**
  Use deterministic paragraph, sentence, and token scans that retain options
  and byte/character positions; reject empty source text.

- [ ] **Step 8: Run Task 1 tests**
  Run: `python -m unittest tests.test_reference tests.test_segmenter -v`.
  Expected: all cases pass.

### Task 2: Deterministic command validator

**Files:**
- Create: `bruce_semantic_index/validator.py`
- Create: `tests/test_validator.py`

**Interfaces:**
- Consumes `ReferenceData` from Task 1.
- Produces `validate_command(command: str, reference: ReferenceData) -> dict`.

- [ ] **Step 1: Write failing validator tests**
  Cover a valid `grid-resume` call; Markdown-only top-level help returning
  `partial`; unknown/duplicate/missing options;
  `--name=value`; standalone `--`; exact enum/range checking; mutual
  exclusions; negative `--min -8`; and expected `partial` for non-formal
  Experience and inherited-grid constraints.

- [ ] **Step 2: Run validator tests to verify failure**
  Run: `python -m unittest tests.test_validator -v`.
  Expected: missing `validate_command`.

- [ ] **Step 3: Implement `validate_command`**
  Use `shlex.split`; greedily resolve only JSON command paths; enforce explicit
  options/positionals and structured `required`, `exclusive_with`,
  `required_with`, values, simple ranges/formats. Return JSON-serializable
  `valid|invalid|partial`, errors, warnings, and unchecked constraints.

- [ ] **Step 4: Run Task 2 tests**
  Run: `python -m unittest tests.test_validator -v`.
  Expected: all cases pass.

### Task 3: Ollama client and index/export construction

**Files:**
- Create: `bruce_semantic_index/ollama.py`, `bruce_semantic_index/indexer.py`
- Create: `tests/test_ollama.py`, `tests/test_indexer.py`

**Interfaces:**
- Produces `OllamaClient.embed(texts: Sequence[str]) -> list[list[float]]`.
- Produces `build_index(reference: ReferenceData, client: EmbeddingClient) -> dict`
  and `write_index(index: dict, path: Path, format: str) -> None`.

- [ ] **Step 1: Write failing Ollama/client tests**
  Mock local HTTP responses for a vector, connection refusal, missing model,
  empty vector, retry exhaustion, and non-numeric vector; assert no pull call.

- [ ] **Step 2: Run Ollama tests to verify failure**
  Run: `python -m unittest tests.test_ollama -v`.
  Expected: missing client implementation.

- [ ] **Step 3: Implement bounded local Ollama client**
  Use only `urllib.request` against `/api/embeddings`, with bounded timeout and
  retries. Normalize finite nonzero vectors and emit actionable local errors.

- [ ] **Step 4: Run Ollama tests to verify success**
  Run: `python -m unittest tests.test_ollama -v`.
  Expected: all client cases pass without a live server.

- [ ] **Step 5: Write failing index/export tests**
  Use a fake embedding client to assert document/paragraph/sentence/token/
  command/option items, dimensions, mandatory JSON fields, conflict notes,
  JSON parseability, JSONL one-item records, SQLite tables, and rejected
  dimension mismatch.

- [ ] **Step 6: Run index/export tests to verify failure**
  Run: `python -m unittest tests.test_indexer -v`.
  Expected: missing index/export behavior.

- [ ] **Step 7: Implement index creation and exporters**
  Build deterministic IDs and item metadata; validate every embedding;
  serialize pretty JSON, JSONL header/items, and SQLite metadata/items/
  embeddings tables; re-open each output before reporting success.

- [ ] **Step 8: Run Task 3 tests**
  Run: `python -m unittest tests.test_ollama tests.test_indexer -v`.
  Expected: all cases pass.

### Task 4: Search and editable local aliases

**Files:**
- Create: `bruce_semantic_index/search.py`, `bruce_semantic_index/synonyms.json`
- Create: `tests/test_search.py`

**Interfaces:**
- Produces `expand_query(query: str, aliases: Mapping[str, Sequence[str]]) -> str`.
- Produces `search_index(index: dict, query: str, client: EmbeddingClient, top_k: int) -> list[dict]`.

- [ ] **Step 1: Write failing search tests**
  With a fake query vector, verify cosine order, dimension rejection, grouping
  by command path, syntax/options in results, recommendation marker, and
  DE/EN aliases for resume/fortsetzen, validate/prüfen/check, and rules/Regeln.

- [ ] **Step 2: Run search tests to verify failure**
  Run: `python -m unittest tests.test_search -v`.
  Expected: missing search implementation.

- [ ] **Step 3: Implement alias expansion and cosine search**
  Load only the editable package-local alias JSON, retain original query,
  embed expanded text with the supplied client, and aggregate ranked items
  without invoking BRUCE.

- [ ] **Step 4: Run Task 4 tests**
  Run: `python -m unittest tests.test_search -v`.
  Expected: all cases pass.

### Task 5: CLI, documentation, and end-to-end test suite

**Files:**
- Create: `bruce_semantic_index/cli.py`, `bruce_semantic_index/__main__.py`, `README.md`, `tests/test_cli.py`
- Modify: `pyproject.toml`

**Interfaces:**
- Exposes `python -m bruce_semantic_index build-index|search|validate-command`.

- [ ] **Step 1: Write failing CLI tests**
  Assert that `validate-command` emits JSON only; `build-index` accepts
  `--md`, `--ref`, `--out`, `--model`, format selection, and local endpoint
  configuration; `search` returns recommendation-only output; errors use
  exit 2; and no path launches `bruce`.

- [ ] **Step 2: Run CLI tests to verify failure**
  Run: `python -m unittest tests.test_cli -v`.
  Expected: missing CLI behavior.

- [ ] **Step 3: Implement CLI wiring and README**
  Wire command-line/config/environment precedence, JSON-only validation
  output, documented stdout/stderr/exit codes, README examples, schema,
  optional formats, uncertainty, and strict non-execution boundary.

- [ ] **Step 4: Run Task 5 tests**
  Run: `python -m unittest tests.test_cli -v`.
  Expected: all cases pass.

- [ ] **Step 5: Run full verification**
  Run: `python -m unittest discover -v`.
  Expected: all tests pass with no Ollama, cloud, or BRUCE dependency.

- [ ] **Step 6: Run static package checks**
  Run: `python -m compileall -q bruce_semantic_index` and a small fake-client
  JSON/JSONL/SQLite round-trip check.
  Expected: zero compile errors and readable outputs.

## Plan self-review

- **Coverage:** Tasks 1–5 map respectively to source truth/segmentation,
  validation, Ollama/index/export, search, and CLI/README/acceptance checks.
- **Interfaces:** Each later task consumes the `ReferenceData` and
  `EmbeddingClient` contracts defined by earlier tasks.
- **Review focus:** Each listed failure mode has a named Task 2–4 test.
- **Scope:** No task adds a BRUCE parser, network API, model pull, or execution
  capability; source-reference gaps remain partial findings.
