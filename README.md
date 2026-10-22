# BRUCE_raw

BRUCE_raw is a bounded C++20 research core for local, reproducible
mathematical investigation. Its primary interface is the Linux command-line
program `bruce`. The current CMake project version is **Core 0.102.0**.

The core investigates explicitly selected, bounded mathematical families;
records results and replay data; and offers an optional local browser inspector
for existing Phase-DAG V2/V3 and DAG-Scheduler V4 bundles. It keeps control, publication, and local-file
boundaries explicit. A successful
command does not by itself establish a complete search or a general
mathematical proof.

## Evidence status

| Status | Meaning |
| --- | --- |
| **IMPLEMENTED AND VERIFIED** | Current source, tests, CI configuration, reproducible runs, or generated CLI validation evidence support the claim. |
| **IMPLEMENTED, BUT NOT CURRENTLY VERIFIED** | Source exists, but this documentation does not cite current verification evidence for the claim. |
| **DOCUMENTED / CONCEPT** | A design, roadmap, ADR, or other documentation record; it is not an implementation claim. |
| **PROPOSAL** | A suggested future change. |
| **OPEN DECISION** | A choice that must be made before implementation or policy can be treated as settled. |
| **NOT DERIVABLE FROM THE REPOSITORY** | The tracked repository does not provide adequate evidence for a claim. |

Documentation is useful context, but it is not proof of runtime behavior. For
current boundaries, start with the [Blueprint](docs/architecture/03-blueprint-consolidation.md)
and the [control and publication contract](docs/contracts/06-control-and-publication.md).

## What is implemented

**IMPLEMENTED AND VERIFIED:** the CMake build defines `bruce` and
`bruce-mod-builder`; the core links GMP/GMPXX and OpenSSL Crypto, and CTest
registers unit and integration coverage. The CLI implements bounded research,
exact/replay-oriented workflows, compatibility selection, evidence and
experience records, explicit local publication, and signed installable
Data/Logic Mod routing and execution boundaries. The [CLI manual](docs/cli/README.md) is the authoritative user-facing
reference for commands, formats, limits, and generated examples.

**IMPLEMENTED AND VERIFIED:** `Language` and `NanoCell` are optional static
mods built into the executable when their CMake switches are enabled. They are
not dynamically loaded executable plugins.

**IMPLEMENTED AND VERIFIED:** `tools/bruce_mcp.py` is a separate local stdio
adapter for a small typed BRUCE MCP surface. It exposes bounded calculation,
research on approved CSV files, run inspection/replay/assessment/gating, and
fixed demos. Successful CLI JSON is available both as compatible text content
and, for MCP `2025-06-18`, structured MCP content; negotiated `2024-11-05`
clients retain text-only compatibility. The adapter does not start a network
listener or accept arbitrary commands or file paths.
See the [local MCP design](docs/superpowers/specs/2026-10-17-bruce-mcp-stdio-design.md).

**IMPLEMENTIERT UND BELEGT:** `tools/bruce_ai.py` is a separate local
Ollama assistant with explicit JSON configuration, a command index,
validated execution previews and exact `run` confirmation. Its CTest workflow
uses local HTTP mocks and fixture executables, requiring no Ollama or models.
It downloads no models, routes no persistent BRUCE changes, and keeps raw
BRUCE results separate from untrusted model explanations. Real model
availability and answer quality are **IMPLEMENTIERT, ABER NICHT AKTUELL
VERIFIZIERT** by that mock workflow. See the
[local Ollama assistant handbook](docs/cli/local-ollama-assistant.md) for
configuration, profile switching, index creation and limits.

**IMPLEMENTIERT UND BELEGT:** adapter version `0.3` supports
an opt-in local `bruce-mcp-trace-v1` audit trace. Use an existing private
directory and a new absolute trace filename; both trace variables are required:

```sh
BRUCE_MCP_RUN_ID=benchmark-r2-assisted \
BRUCE_MCP_TRACE_PATH=/absolute/private/benchmark-r2-assisted.jsonl \
BRUCE_MCP_BRUCE=/absolute/path/to/bruce \
BRUCE_MCP_ASSET_ROOT=/absolute/path/to/assets \
python3 tools/bruce_mcp.py
python3 tools/verify_bruce_mcp_trace.py /absolute/private/benchmark-r2-assisted.jsonl
```

The file is exclusively created with mode `0600`. Invalid configuration exits
with code 2 before reading stdin; trace write/sync or response output failure
exits with code 3. The read-only verifier returns `VALID`, `INCOMPLETE`,
`TAMPERED`, or `INVALID_FORMAT`; only `VALID` exits 0. Requests and responses
can contain confidential data and are not automatically redacted. The hash
chain checks recorded consistency, not author identity and not a mathematical proof;
it is no signature, proof of client receipt, or remote audit service. This is
separate from the HTTP inspector. See the [trace contract and integrity limits](docs/superpowers/specs/2026-10-18-bruce-mcp-audit-trace-design.md)
and [local security boundaries](docs/architecture/04-local-security-boundaries.md).

**IMPLEMENTIERT UND BELEGT:** the Phase-DAG V2 C++ library supports bounded,
deterministic execution, canonical bundles, and replay with the fixed NanoCell
adapter. Its local inspector is described below.

**IMPLEMENTIERT UND BELEGT:** Signed-Affine-DAG V1 is a separate local C++20
reference prototype with two fixed in-memory CLI demos:
`bruce signed-affine-dag-v1 demo merge` and
`bruce signed-affine-dag-v1 demo cancellation`. Different paths to integer
value 4 share one semantic state while retaining both provenance records;
their signed balances are respectively 2 and 0. The source state at value 2
is also retained. Reports use `signed-affine-dag-v1-cli-v1` and deterministic
state/path ordering. The demos accept no graph files or free formulas, persist
no runs, and do not extend Phase-DAG V2/V3. See the
[command and result contract](docs/cli/commands.md#signed-affine-dag-v1--feste-referenzdemos),
[core contracts](tests/unit/signed_affine_dag_v1_tests.cpp), and
[real-executable CLI tests](tests/integration/test_signed_affine_dag_v1_cli.py).
The broader [Affine Phase-DAG roadmap](docs/concepts/affine-phase-dag.md)
remains **DOKUMENTIERT / KONZEPT**.

**IMPLEMENTIERT UND BELEGT:** Toy-Relation V1 is an independent local C++20
reference for collisions in a small additive `Z/nZ` group. Its two fixed demos,
`bruce toy-relation-v1 demo collision` and
`bruce toy-relation-v1 demo noninvertible`, are available without NanoCell or
MCP. The first derives the fixed candidate 17 from a collision at 53 and
verifies it independently; the second retains the collision but reports
`NO_UNIQUE_RELATION` without a relation or candidate. Both return exit 0 with
deterministic `toy-relation-v1-cli-v1` JSON. The candidate is not a secret;
this is no discrete-logarithm solver, search or real-world cryptography.
There are no user group parameters, files, curves, persistence or Phase-DAG
V2/V3/Signed-Affine extensions. See the
[exact commands and generator coverage limits](docs/cli/commands.md#toy-relation-v1--feste-referenzdemos),
[core contracts](tests/unit/toy_relation_v1_tests.cpp) and
[real-binary tests](tests/integration/test_toy_relation_v1_cli.py).

**IMPLEMENTIERT UND BELEGT:** Signed-Affine-Relation V1 bridges the unchanged
Signed-Affine-DAG V1 and Toy-Relation V1 through one local, non-cryptographic
toy witness: `bruce signed-affine-relation-v1 demo merge`. It reconstructs
`m × s + c` from the executed DAG trace. For public source 2, `a-add` has
`(m,c)=(1,2)` and `z-double` has `(2,0)`; both reach 4. Mapping
`a=c mod n`, `b=m mod n`, `G=1`, `Q=s mod n` with `n=101` gives a real Toy
collision at 4 and independently verified `candidate_k=2`. This candidate
is the already public source, with no secret recovery, search, DLP solver
or real curve cryptography. The fixed in-memory command accepts no files,
formulas or group parameters and adds no persistence, scheduler or Phase-DAG
V2/V3 interface. See the [exact command and report](docs/cli/commands.md#signed-affine-relation-v1--feste-referenzbrücke),
[core contracts](tests/unit/signed_affine_relation_v1_tests.cpp) and
[real-binary workflows](tests/integration/test_signed_affine_relation_v1_cli.py).
The unchanged CLI example generator does not execute this demo; its generated
examples/validation do not establish this witness. The broader
[Affine Phase-DAG roadmap](docs/concepts/affine-phase-dag.md) remains
**DOKUMENTIERT / KONZEPT**.

**IMPLEMENTIERT UND BELEGT:** DAG-Scheduler V1 is a separate adapter-neutral,
bounded serial C++20 reference. Its three fixed local in-memory commands are
`bruce dag-scheduler-v1 demo mixed`, `bruce dag-scheduler-v1 demo budget` and
`bruce dag-scheduler-v1 demo cancelled`, also with NanoCell/MCP OFF.
Serial Kahn selection starts one callback at a time, always the smallest ready
node ID. Direct successful inputs are producer-ID sorted; terminal receipts
are node-ID sorted, while the logical trace remains chronological and ends
with `COMPLETED`. The `dag-scheduler-v1-cli-v1` reports distinguish expected
failure/blocking, start-budget exhaustion and controlled cancellation; all
three demos return exit 0. They accept no free graphs, callbacks, budgets or
files and add no threads, GPU/network execution, persistence, bundle/replay,
Phase-DAG V2/V3 or general runtime interface. See the
[exact contract and generator coverage limit](docs/cli/commands.md#dag-scheduler-v1--feste-serielle-referenzdemos),
[core contracts](tests/unit/dag_scheduler_v1_tests.cpp) and
[real-binary workflows](tests/integration/test_dag_scheduler_v1_cli.py).
The unchanged example generator does not execute these demos; broader
scheduler/runtime work remains **DOKUMENTIERT / KONZEPT**.

**IMPLEMENTIERT UND BELEGT:** DAG-Scheduler V2 adds bounded direct byte-payload
flow through a separate local serial C++20 core. Its only CLI command,
`bruce dag-scheduler-v2 demo flow`, also works with NanoCell/MCP OFF.
One callback at a time receives its request and verified direct READY inputs
sorted by producer ID. The fixed `a-source`, `b-transform`, `c-independent`
and `d-join` flow returns `dag-scheduler-v2-cli-v1` JSON with digests and sizes,
without raw request or payload bytes. Requests and successful outputs are
limited to 65,536 bytes; the receipt budget includes retained input copies.
The core cannot automatically detect valid but different callback returns
across runs; callback purity remains the embedder's responsibility.
See the [exact command and limits](docs/cli/commands.md#dag-scheduler-v2--fester-direkter-payload-fluss),
[core contracts](tests/unit/dag_scheduler_v2_tests.cpp) and
[real-binary workflows](tests/integration/test_dag_scheduler_v2_cli.py).
The unchanged example generator does not execute V2; its generated examples
are not V2 runtime evidence. V2 bundle/persistence/replay, external stores,
Phase-DAG V3 adapters, parallelism, GPU/network execution and general runtime
orchestration remain **DOKUMENTIERT / KONZEPT**. Existing V1 and Phase-DAG
contracts remain unchanged.

**DOKUMENTIERT / KONZEPT:** free-form dialogue, a general formula prover, GPU
execution, and a general Phase-DAG scheduler or broader runtime orchestration.
Historical Cockpit Job/Import/Language/Redis management remains unimplemented.

## MCP Inspector

**IMPLEMENTIERT UND BELEGT:** the optional static Mod is enabled by
`BRUCE_BUILD_MOD_MCP=ON` (default ON) and requires
`BRUCE_BUILD_MOD_NANOCELL=ON`. `bruce mcp [--port 1..65535]` defaults to
8080 and binds only `127.0.0.1`; an unavailable requested port ends the service
with a nonzero exit, without fallback. It exposes only `GET /`,
`GET /api/v1/health`, `POST /api/v1/inspect`, `POST /api/v3/inspect` and
`POST /api/dag-scheduler-v4/inspect`.

The read-only Phase-DAG V2 and Phase-DAG V3 inspector uses fixed NanoCell
catalogs: NanoCell/Evidence-Gate for V2 and NanoCell/Payload-Gate for V3.
`POST /api/v1/inspect` retains its 1 MiB (1048576 bytes) limit and response
model; `POST /api/v3/inspect` accepts V3 bundles up to 5 MiB (5242880 bytes).
These two routes replay existing bundles; no new DAG executions, no jobs or
imports. Wrong-version bundles are rejected. The browser requires explicit
selection of Phase-DAG V2, Phase-DAG V3 or DAG-Scheduler V4, with no automatic
detection from file bytes. Its JSON and DOM expose metadata only;
`request_bytes`, `context_bytes`, `output_bytes`, `evidence_bytes` and
`payload_bytes` are never exposed. There are no adapter plugins.
Replay/digest establishes bundle consistency, not author identity or a general
mathematical proof. The 5-MiB limit does not override tighter core/catalog limits.

See the [command contract](docs/cli/commands.md#mcp-inspector),
[V2 acceptance evidence](docs/superpowers/specs/2026-10-12-phase-dag-v2-cockpit-inspector-design.md)
and [C++/HTTP/DOM contracts](tests/mods/mcp/test_mcp_inspector.py).

**IMPLEMENTIERT UND BELEGT:** DAG-Scheduler V4 is a separate read-only
structural inspector at exact `POST /api/dag-scheduler-v4/inspect`, with
explicit selection and a 5 MiB (5242880 bytes) limit on local `127.0.0.1`.
It checks the V4 header, complete bundle digest, decoding and canonical
repacking; no callbacks and no re-execution. `VALID` confirms structural
bundle consistency only, not successful replay, callback purity, author
identity or a mathematical proof. A structurally valid alteration with a
recomputed digest may pass; the inspector does not establish that the recorded
receipts or trace were executed. Phase-DAG V2/V3 keep their existing
replay-based routes and independent 1-MiB-V2/5-MiB-V3 limits.

V4 JSON and text-safe DOM show metadata only: `scheduler_version` is
`dag-scheduler-v2`; the overview contains DAG ID, start/receipt budgets and
node count. Nodes expose IDs, dependencies, `callback_id`, `request_size`
and `request_digest`; receipts add outcome/failure code and direct input
references (`producer_node_id`, `result_code`, `payload_size`, `payload_digest`),
plus output version, result code, size and digest or null. Trace entries
contain event and node ID. V4 has no `scheduler_explanation`; the V3 logical
explanation stays hidden and cleared when V4 is selected. No request,
context, output, evidence, input or payload bytes, hex/base64 encodings,
source paths or raw bundle text are returned. Bundles are local confidential
artifacts containing raw requests and payloads; metadata filtering provides
no encryption and no authentication or access control. Bytes remain only in
request memory with no persistence. Reset/version change clears selection
and views and invalidates pending replies; release does not promise physical
memory erasure.

There are no new DAG executions, no jobs or imports, no adapter plugins,
no replay buttons or routes, no persistent uploads, no payload download,
no dynamic plugins and no external access. Free language, a general prover,
GPU/network execution, external payload stores, Redis/Language management,
process starts, filesystem browsing and general runtime orchestration remain
**DOKUMENTIERT / KONZEPT**. The unchanged CLI example verifier covers MCP
argument parsing and V2/V3 examples; V4 structural HTTP/DOM/privacy evidence
comes from the dedicated MCP contracts, not generated examples.


**IMPLEMENTIERT UND BELEGT:** Local installed Mod roots can explicitly import
their bound V1 trust into immutable Trust V2 generations, prepare keys, inspect
compatibility, activate and roll back. Active Mods that lose trust block a
switch unless `--deactivate-untrusted` explicitly commits their deactivation
with it. This uses the invoking Linux user or administrator, with no BRUCE
account, daemon or multi-user service. See the
[commands and running-worker boundary](docs/cli/README.md#local-mod-trust-v2).
The separate [offline release tool](docs/implementation/150-release-signing-contract.md)
signs the exact audited TGZ and verifies a complete release directory against
an independently pinned public key. Establishing a production key through an
explicit ceremony remains **OFFENE ENTSCHEIDUNG**; CI uses test keys and does
not sign or publish production releases.

## Repository map

- [Documentation map](docs/README.md) — the English entry point and reading paths.
- [Repository structure](docs/architecture/repository-structure.md) — source
  ownership, build flow, runtime flow, and mod boundaries.
- [Architecture](docs/architecture/README.md) — current blueprint, invariants,
  and historical architecture context.
- [Contracts](docs/contracts/README.md) — current behavioral, control, data,
  replay, and publication boundaries.
- [CLI manual](docs/cli/README.md) — commands, formats, tutorial, generated
  validation, and local-data-mod use.
- [Implementation records](docs/implementation/README.md) — numbered delivery
  and acceptance records; read them as dated evidence, not a current roadmap.
- [Concepts](docs/concepts/README.md), [ADRs](docs/adr/README.md), and
  [history](docs/history/README.md) — clearly separated contextual material.
- [Contributor guide](CONTRIBUTING.md), [dependency notes](DEPENDENCIES.md),
  [security policy](SECURITY.md), and [license](LICENSE).

## Prerequisites

The CI configuration exercises Ubuntu 24.04 with a C++20 compiler (CI uses
`g++`), CMake 3.20 or later, Ninja, `pkg-config`, GMP/GMPXX development files,
OpenSSL Crypto development files, and Python 3 (including development tools)
for relevant tests and tutorials.

On Debian/Ubuntu:

```sh
sudo apt-get update
sudo apt-get install -y cmake ninja-build g++ pkg-config libgmp-dev libssl-dev python3 python3-dev
```

## Build, test, and install

```sh
cmake -G Ninja -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$HOME/.local"
"$HOME/.local/bin/bruce" --version
```

In the supplied Bruce-AI environment, use the project wrapper when its
dependency path is required. The complete current-source verification path is:

```sh
scripts/verify_current_source.sh
```

It creates a fresh temporary build, runs CTest, verifies generated CLI
documentation in a separate temporary copy, and checks local Markdown links.
It never updates the checked-in generated CLI artifacts.

The individual wrapper commands remain available:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-build
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build --output-on-failure
```

The wrapper and current-source verifier must run from a native project path
that contains neither `:` nor whitespace. GVFS SFTP paths or whitespace can
split the bundled `pkg-config` search paths. Use a native safe path or a
complete temporary copy without these characters instead.

A local executable proves behavior only for the source revision and build
configuration that produced it. Rebuild before treating `build/bruce` as
evidence for changed sources.

## First commands

```sh
./build/bruce --version
./build/bruce --help
bash docs/cli/tutorial.sh ./build/bruce
```

Use only explicit arguments and explicitly named files. BRUCE does not use a
hidden configuration file or environment variable to select a strategy. The
manual explains command-specific exports, replay files, result statuses, and
the distinction between an exit status and a mathematical claim.

## Mods: built-ins and installable local packages

The static build switches are:

```sh
cmake -G Ninja -S . -B build \
  -DBRUCE_BUILD_MOD_LANGUAGE=ON \
  -DBRUCE_BUILD_MOD_NANOCELL=ON \
  -DBRUCE_BUILD_MOD_MCP=ON
```

Setting a switch to `OFF` omits that mod and its associated CLI command.
Disabling NanoCell also requires `BRUCE_BUILD_MOD_MCP=OFF`; otherwise
configuration fails with `McpRequiresNanoCell`. These are optional static
libraries, with no runtime discovery. `bruce mods list` reports Language,
NanoCell, and MCP in every build; disabled build features have
`build_state: "not_built"` rather than disappearing from discovery.

**IMPLEMENTIERT UND BELEGT:** Installable local packages are a separate
mechanism. Data Mods add signed, schema-checked Tree routing data. Logic Mods
embed the same signed routing data plus one signed, hash-bound native worker
that can run only through the existing Linux Logic-Mod sandbox. Both kinds use
the same explicit lifecycle:

```sh
bruce-mod-builder data SOURCE_DIR --output MOD.brucemod \
  --trust-store-output TRUST.pub --signing-env SIGNING.env
bruce-mod-builder logic SOURCE_DIR --output MOD.brucemod \
  --trust-store-output TRUST.pub --signing-env SIGNING.env

bruce mods inspect MOD.brucemod
bruce mods verify MOD.brucemod --trust-store TRUST.pub
bruce mods install MOD.brucemod --mod-root MOD_ROOT --trust-store TRUST.pub
bruce mods activate MOD_ID VERSION --mod-root MOD_ROOT
bruce mods update NEW_MOD.brucemod --mod-root MOD_ROOT --trust-store TRUST.pub
bruce mods list --mod-root MOD_ROOT
bruce mods status MOD_ID --mod-root MOD_ROOT
bruce mods run MOD_ID --template TEMPLATE --input TEXT --mod-root MOD_ROOT
bruce mods disable MOD_ID --mod-root MOD_ROOT
```

Installation is inactive; activation is explicit. Multiple versions remain
installed side by side. `update` atomically installs and activates the supplied
version, keeps older versions for explicit rollback, and is idempotent for the
same package. The default `route` report stays byte-compatible V1;
`--mod-details` opts into V2 candidate and selected-owner metadata.

There is no automatic download, system-wide daemon, multi-user service, or
dedicated operating-system account. The invoking local user supplies and owns
the private mod root, initial trust store, and signing material. See the
[Local Mod Lifecycle V2 contract](docs/contracts/10-local-mod-lifecycle-v2.md)
and the [local-data-mod example](examples/local-data-mods/math-rational-power/README.md).

## Where to read next

CLI users should begin with the [CLI manual](docs/cli/README.md) and its
[tutorial](docs/cli/tutorial.sh). Contributors should read
[CONTRIBUTING.md](CONTRIBUTING.md), [AGENTS.md](AGENTS.md), the
[repository structure guide](docs/architecture/repository-structure.md), and
the [test strategy](docs/implementation/01-test-strategy.md). Architecture
reviewers should begin with the [Blueprint](docs/architecture/03-blueprint-consolidation.md)
and the [control and publication contract](docs/contracts/06-control-and-publication.md).

Contributions are governed by [CONTRIBUTING.md](CONTRIBUTING.md). Report
security vulnerabilities as described in [SECURITY.md](SECURITY.md), not in a
public issue. Rights, licensing, and redistribution constraints are in
[LICENSE](LICENSE), [NOTICE](NOTICE), and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
