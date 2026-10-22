# Lokaler Ollama-BRUCE-Assistent V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Einen lokalen, JSON-konfigurierbaren Ollama-Begleiter liefern, der BRUCE-Routen per Embedding vorschlägt, nur bestätigte sichere argv-Aufrufe ausführt und BRUCE-JSON getrennt erklärt.

**Architecture:** `tools/bruce_ai.py` bleibt eine Python-3.11-Standardbibliotheks-CLI außerhalb des C++-Kerns. Konfiguration, lokaler Ollama-Transport, erlaubter Command-Katalog, Vektorindex, Vorschlagsvalidator, direkter Executor und Antwortrenderer sind getrennte Module. BRUCE-Roh-JSON bleibt maßgeblich.

**Tech Stack:** Python 3.11+, `argparse`, `dataclasses`, `hashlib`, `json`, `math`, `pathlib`, `subprocess`, `urllib.request`, `unittest`, CMake/CTest.

**Spec:** `docs/superpowers/specs/2026-10-19-local-ollama-bruce-assistant-design.md`

## Global Constraints

- Nur lokales Ollama auf `127.0.0.1` oder `[::1]`; kein Cloud-Endpoint, Redirect oder `ollama pull`.
- Keine Änderung am C++-Core, MCP-Adapter oder HTTP-Inspector.
- Konfiguration ist für jeden Aufruf explizites, striktes JSON; keine Umgebungsvariablen oder versteckten Modell-/Pfaddefaults.
- BRUCE wird nur über festen `argv`-Vektor und `shell=False` gestartet; Modellfelder steuern weder Executable noch Environment, Working Directory oder Unterroute.
- Nur erlaubte Rechen-, Forschungs-, Bundle-, Demo- und lesende Mod-Routen sind routbar. Mod-, Trust-, Release-, Regel- und andere mutierende Routen bleiben ausgeschlossen.
- `require_confirmation` muss exakt `true` sein; nur die Eingabe `run` startet BRUCE.
- Generierte CLI-Artefakte bleiben unberührt, außer der vorgesehene Verifier erzeugt in einer vollständigen temporären Kopie belegte Änderungen.

## Review Focus

1. Nicht-loopback URLs, Redirects, Modellnamen mit Leerzeichen und fehlerhafte HTTP-Antworten dürfen niemals externe Requests oder Pulls auslösen — Task 1.
2. Alte Indizes, Dimensions-, Modell- und Katalogwechsel müssen Routing blockieren — Task 2.
3. Modellvorschläge mit unbekanntem Command, Extra-Feld, Shellzeichen, mutierender Route oder unsicherem Pfad dürfen nie zu argv werden — Task 3.
4. Vorschau oder abgelehnte Bestätigung darf keinen BRUCE-Prozess starten; bestätigte Ausführung muss genau den kanonischen argv-Vektor nutzen — Task 3.
5. Ein Ausfall der LLM-Erklärung nach erfolgreichem BRUCE darf das Rohresultat nicht verlieren oder umdeuten — Task 4.

---

## File structure

```
tools/
  bruce_ai.py
  bruce_ai/{__init__,config,ollama,catalog,index,proposal,executor,response}.py
  bruce_ai_command_catalog_v1.json
tests/integration/test_bruce_ai.py
docs/cli/local-ollama-assistant.md
README.md
docs/cli/README.md
CMakeLists.txt
```

### Task 1: Konfiguration und lokaler Ollama-Client

**Files:**
- Create: `tools/bruce_ai/__init__.py`, `tools/bruce_ai/config.py`, `tools/bruce_ai/ollama.py`
- Create: `tests/integration/test_bruce_ai.py`

**Interfaces:**
- Produces `AssistantConfig`, `AssistantProfile`, `AssistantError(code: str)`.
- Produces `load_config(path: Path, profile_override: str | None) -> AssistantConfig`.
- Produces `LocalOllamaClient.embed(text: str, profile: AssistantProfile) -> list[float]` and `chat_json(messages: list[dict[str, str]], profile: AssistantProfile) -> dict`.

- [ ] **Step 1: Write the failing configuration tests**

Assert the exact `bruce-local-ai-config-v1` schema, profile override and switching between `all-minilm`/ `qwen3.5`; reject unknown fields, non-loopback URL, relative path, empty root, non-true confirmation, non-positive limits and malformed JSON.

- [ ] **Step 2: Run test to verify RED**

Run: `python3 tests/integration/test_bruce_ai.py --case config`

Expected: import failure because configuration code does not exist.

- [ ] **Step 3: Implement strict immutable configuration**

Validate all shape/value/path requirements before opening a network connection. Resolve symlinks and reject escaping configured roots. Emit only fixed redacted error codes.

- [ ] **Step 4: Write failing Ollama mock tests**

Use a loopback HTTP mock for finite embeddings, structured chat JSON, refusal, timeout, missing model, redirect, malformed JSON, `NaN` and oversized response. Assert no production code invokes the `ollama` executable.

- [ ] **Step 5: Implement bounded local transport**

Use current documented Ollama embedding/chat endpoints, disabled redirects and fixed byte/time bounds. Never pull or download a model.

- [ ] **Step 6: Run Task 1 tests to verify GREEN**

Run: `python3 tests/integration/test_bruce_ai.py --case config --case ollama`

Expected: all cases pass without a live Ollama.

- [ ] **Step 7: Commit**

```bash
git add tools/bruce_ai tests/integration/test_bruce_ai.py
git commit -m "feat: add local assistant configuration and Ollama client"
```

### Task 2: Command-Katalog und Vektorindex

**Files:**
- Create: `tools/bruce_ai_command_catalog_v1.json`, `tools/bruce_ai/catalog.py`, `tools/bruce_ai/index.py`
- Modify: `tests/integration/test_bruce_ai.py`

**Interfaces:**
- Produces `CatalogEntry`, `IndexDocument`, `load_catalog(path: Path) -> list[CatalogEntry]`.
- Produces `build_index(catalog, profile, client) -> IndexDocument` and `rank(index, query_vector, top_k) -> list[RankedEntry]`.

- [ ] **Step 1: Write failing catalog/index tests**

Pin every V1 allowed route and reject install/activate/update/rollback/trust/release/rules. Cover finite L2 normalization, catalog digest, model ID/dimension mismatch, stable score ties and exclusive atomic index publication.

- [ ] **Step 2: Run test to verify RED**

Run: `python3 tests/integration/test_bruce_ai.py --case catalog --case index`

Expected: missing catalog/index behavior.

- [ ] **Step 3: Implement versioned catalog and index**

Give each route a stable ID, source-backed synopsis, strict named-argument schema and side-effect class. Store catalog digest, model tag, dimension and normalized vectors in the index.

- [ ] **Step 4: Implement local cosine ranking**

Stable-sort equal scores by catalog ID and reject stale catalog/model/dimension state before asking the response model.

- [ ] **Step 5: Run Task 2 tests to verify GREEN**

Run: `python3 tests/integration/test_bruce_ai.py --case catalog --case index`

Expected: deterministic mock-index tests pass.

- [ ] **Step 6: Commit**

```bash
git add tools/bruce_ai tools/bruce_ai_command_catalog_v1.json tests/integration/test_bruce_ai.py
git commit -m "feat: add local assistant command index"
```

### Task 3: Vorschlagsvalidierung, Vorschau und BRUCE-Ausführung

**Files:**
- Create: `tools/bruce_ai/proposal.py`, `tools/bruce_ai/executor.py`
- Modify: `tests/integration/test_bruce_ai.py`

**Interfaces:**
- Produces `Proposal`, `parse_proposal(model_json: dict, catalog: Catalog) -> Proposal`, `canonical_argv(proposal, config) -> list[str]`.
- Produces `execute_confirmed(argv: list[str], confirmation: str, timeout_ms: int) -> BruceProcessResult`.

- [ ] **Step 1: Write failing proposal tests**

Cover both proposal forms, missing input, unknown command, extra field, invalid enum/range, shell metacharacter, free executable, mod/trust/release route, missing/traversal/absolute-outside-root/symlink-outside-root file and output injection.

- [ ] **Step 2: Run test to verify RED**

Run: `python3 tests/integration/test_bruce_ai.py --case proposal`

Expected: missing parser/canonicalization.

- [ ] **Step 3: Implement strict proposal parsing and canonical argv**

Ignore non-schema model content. Only catalog fields in fixed order become argv. Existing input files must resolve under an allowed root; unresolved values return `AssistantInputRequired`.

- [ ] **Step 4: Write failing preview/execution tests**

Use a temporary fake BRUCE executable recording argv. Assert preview/non-`run` starts no process; exact `run` starts direct argv once; exit 0/2, timeout, invalid JSON and bounded streams remain distinct.

- [ ] **Step 5: Implement direct bounded execution**

Use `subprocess` with `shell=False`, no model-influenced environment and fixed limits. Preserve raw BRUCE streams, exitcode and result digest.

- [ ] **Step 6: Run Task 3 tests to verify GREEN**

Run: `python3 tests/integration/test_bruce_ai.py --case proposal --case execution`

Expected: all injection, preview and direct-exec cases pass.

- [ ] **Step 7: Commit**

```bash
git add tools/bruce_ai tests/integration/test_bruce_ai.py
git commit -m "feat: add validated assistant execution preview"
```

### Task 4: Erklärung, Transkript und Benutzer-CLI

**Files:**
- Create: `tools/bruce_ai/response.py`, `tools/bruce_ai.py`
- Modify: `tests/integration/test_bruce_ai.py`

**Interfaces:**
- Produces `render_answer(bruce_result: dict, response_model: LocalOllamaClient) -> AssistantAnswer`.
- Provides `bruce_ai.py index build|ask|explain|config check` with JSON stdout/stderr.

- [ ] **Step 1: Write failing answer/CLI tests**

Assert exact `bruce-local-ai-answer-v1`, source-result digest, model marker, caveats, malformed explanation JSON, answer timeout after successful BRUCE, exclusive optional transcript and absence of automatic logging.

- [ ] **Step 2: Run test to verify RED**

Run: `python3 tests/integration/test_bruce_ai.py --case response --case cli`

Expected: missing renderer and CLI.

- [ ] **Step 3: Implement answer boundary and transcript**

Keep raw BRUCE JSON and digest next to model prose. If answer generation fails, preserve the completed BRUCE result with a bounded explanation-status error. Publish a transcript only after explicit opt-in.

- [ ] **Step 4: Implement the command parser**

Require `--config` on every operation. `ask` prints a preview before reading confirmation; `explain` never starts BRUCE. Reject unknown/duplicate flags and emit JSON only.

- [ ] **Step 5: Run Task 4 tests to verify GREEN**

Run: `python3 tests/integration/test_bruce_ai.py --case response --case cli`

Expected: all mock end-to-end flows pass without Ollama.

- [ ] **Step 6: Commit**

```bash
git add tools/bruce_ai tools/bruce_ai.py tests/integration/test_bruce_ai.py
git commit -m "feat: add local assistant CLI and result explanations"
```

### Task 5: CTest, Dokumentation und Abschlussprüfung

**Files:**
- Modify: `CMakeLists.txt`, `README.md`, `docs/cli/README.md`, `tests/integration/test_bruce_ai.py`
- Create: `docs/cli/local-ollama-assistant.md`

**Interfaces:**
- Registers CTest `bruce_local_ai_workflows` using Python and mock-only setup.
- Documents config, profile switch, explicit index build, preview/confirmation, root boundary, model availability and non-goals.

- [ ] **Step 1: Write the failing CTest/docs contract test**

Assert CTest registration and the handbook’s sample config without touching a real endpoint; include the handbook in the local Markdown-link check.

- [ ] **Step 2: Run test to verify RED**

Run: `python3 tests/integration/test_bruce_ai.py --case ctest-docs`

Expected: missing registration and handbook.

- [ ] **Step 3: Register and document the tool**

Add a Python CTest matching existing Python workflow registration. State that models are neither bundled nor downloaded, only loopback is allowed and model prose is not BRUCE evidence. Do not hand-edit generated CLI artifacts.

- [ ] **Step 4: Run focused verification**

Run: `python3 tests/integration/test_bruce_ai.py`, `ctest --test-dir <build> -R '^bruce_local_ai_workflows$' --output-on-failure`, `python3 scripts/check_local_doc_links.py`, `git diff --check`.

Expected: all focused tests and links pass.

- [ ] **Step 5: Run fresh project profiles**

Use the native no-space path and `scripts/with-host-deps.sh` for a fresh Release configure/build and relevant CTest suite; run applicable Debug/sanitizer checks and disclose pre-existing exclusions.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt README.md docs/cli tests/integration/test_bruce_ai.py
git commit -m "docs: document local Ollama assistant"
```

## Plan self-review

- **Spec coverage:** Tasks 1–4 implement configuration, transport, index, validation, confirmation, execution, explanation and transcript; Task 5 adds CTest, docs and profile verification.
- **Type consistency:** Task 1’s config feeds all tasks; Task 2’s catalog/index feeds Task 3; Task 3’s process result feeds Task 4.
- **Review focus coverage:** Items 1–5 map to Tasks 1, 2, 3, 3 and 4.
- **Scope:** No C++ core change, no pull, no automatic/model-routed mutation, no persistent chat state and no manual generated-CLI edits.
