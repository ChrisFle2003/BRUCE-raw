# BRUCE Core 0.102.0 – CLI manual

Current as of 19 October 2026. This reference describes the implemented Linux
CLI in `apps/bruce_cli.cpp`, `apps/runtime_mod_cli.hpp`, `apps/nanocell_cli.hpp`,
`apps/phase_dag_v2_cli.hpp`, `apps/phase_dag_v3_cli.hpp`,
`apps/signed_affine_dag_v1_cli.hpp`, `apps/toy_relation_v1_cli.hpp`,
`apps/signed_affine_relation_v1_cli.hpp`, `apps/dag_scheduler_v1_cli.hpp`,
`apps/dag_scheduler_v2_cli.hpp`, `apps/dag_scheduler_v3_cli.hpp`,
`apps/dag_scheduler_v4_cli.hpp`, `apps/logic_mod_artifact_dag_v5_cli.hpp`,
`apps/rule_review.hpp`, and `mods/mcp/src/mod.cpp`, together with the parsers
and executors they invoke.
Architecture designs and planned capabilities are not guaranteed CLI features.
Rights holder: Christopher Flegel.

## Contents

1. [Installation, invocation, and conventions](#installation-and-startup)
2. [Integrated CLI reference: overview, claim scope, and audit](bruce-cli-reference.md)
3. [All commands, options, defaults, and limits](commands.md)
4. [Input files, numbers, and exports](formats.md)
5. [JSON results, budgets, cancellation, and errors](results-and-control.md)
6. [Executable tutorial](tutorial.sh)
7. [Complete option index and diagnostic catalog](source-index.md)
8. [Measured example invocations with complete JSON output](examples.json)
9. [Validation evidence](validation.md)
10. [Benchmark suite V1: methodology and profiles](benchmark.md)
11. [Control overview and end-to-end acceptance workflow](control-overview.md)
12. [Shared result assessment](../implementation/153-result-assessment-usage.md)
13. [Admission reasons and rule contract](../implementation/154-research-gate-reasons-usage.md)
14. [Check admission of a specific run](../implementation/155-run-gates-usage.md)
15. [Local rule review](rule-review.md)
16. [Active rules and decision journal](active-rules.md)
17. [Explicit transfer: research → experience](experience-transfer.md)
18. [Explicit transfer: research → Z selection](compatibility-transfer.md)
19. [Shared budget and cancellation control for transfers](transfer-control.md)
20. [Local files, write failures, and publication](local-files.md)
21. [Local Mod Trust V2](#local-mod-trust-v2)
22. [Offline release signing](../implementation/150-release-signing-contract.md)
23. [Separate local Ollama assistant: configuration, preview and confirmation](local-ollama-assistant.md)

The V5 host-cancellation contract is documented in the
[`dag-scheduler-v5` command section](commands.md#v5-worker-abbruch-v1--explizite-host-steuerung)
and the matching [format section](formats.md#v5-host-abbruch-v1-und-cancel-report).
**IMPLEMENTIERT UND BELEGT:** The opt-in V5 Execute handoff record uses
`execute SPEC --output BUNDLE --handoff-v1` and the read-only
`inspect BUNDLE --handoff-v1`; see the
[command contract](commands.md#v5-execute-handoff-lineage-v1) and
[sidecar format](formats.md#v5-execute-handoff-lineage-v1). The separate
`payload` route uses [Flow V2](commands.md#v5-payload-flow-v2--persistierte-kanten-lineage).
Free language, general proving, GPU/network execution, free plugins,
external stores, recovery/resume and broad orchestration remain
**DOKUMENTIERT / KONZEPT**.

## Installation and startup

From the project root, with a C++20 compiler, CMake 3.20 or later, Ninja,
pkg-config, and development libraries for GMP/GMPXX and OpenSSL Crypto:

```bash
cmake -G Ninja -S . -B build-cli -DCMAKE_BUILD_TYPE=Release
cmake --build build-cli --target bruce -j2
./build-cli/bruce --version
./build-cli/bruce --help
```

For the optional complete test run, execute `cmake --build build-cli -j2`,
then `ctest --test-dir build-cli --output-on-failure`. Python 3 is required
for tests and the tutorial. In Bruce-AI, the existing
`scripts/with-host-deps.sh` wrapper can activate the project-provided system
dependencies before build commands; it is not part of the CLI itself.

Installation without administrator privileges:

```bash
cmake --install build-cli --prefix "$HOME/.local"
"$HOME/.local/bin/bruce" --version
```

The installed program is `bin/bruce`. `share/bruce` contains build
information, source checksums, `LICENSE`, `NOTICE`, `DEPENDENCIES.md`, and
`THIRD_PARTY_NOTICES.md`. This manual lives under `docs/cli` in the source
tree; the existing installation contract does not install it automatically.
The archive contains no third-party runtime libraries. See
[Dependencies](../../DEPENDENCIES.md) for details.

In this manual, `bruce` means either the installed program on the search path
or the full path to the built file. Every example can instead use
`./build/bruce`.

## Invocation rules

- With no argument, `help`, or `--help`, the CLI displays plain-text help.
  These global help forms ignore following arguments. Commands such as
  `research --help` have no command-specific help and report `UnknownOption`.
- Value options have exactly one following value: `--max-bits 4096`.
  `--max-bits=4096`, short options, combined flags, and a general `--`
  delimiter are unsupported. Explicit flags such as `--mod-details` and
  trust-transition `--deactivate-untrusted` take no value.
- Option order is arbitrary, duplicates are errors, and unknown options are
  rejected. Option names and values are case-sensitive.
- Negative values remain separate arguments: `--min -8`. Quote strings that
  contain spaces, semicolons, or shell metacharacters, for example
  `--paths '2,6;4,3'` and `--operators '+-*/^'`.
- `rules-approve` and `rules-activate` use a mandatory terminal dialogue.
  The other commands are non-interactive. No configuration file or
  environment variable selects a strategy. Input comes from arguments or
  explicitly named files. `-` is not an implemented standard-input placeholder.
- On successful domain processing, `stdout` contains a JSON object followed by
  a newline. Help is plain text; `rules-approve` and `rules-activate` write a
  plain-text dialogue, then JSON on success. The optional `mcp` command
  serves its local interface in the foreground and produces no domain JSON on
  stdout; terminate it with Ctrl+C. Argument errors are JSON on `stderr` with
  exit code 2; an unavailable MCP port exits nonzero without a JSON result.
- Exit code 0 means processing completed successfully. It does not mean a
  complete search or mathematical proof. Evaluate `INCOMPLETE`,
  `NO_CANDIDATE`, `REFUTED`, `UNDECIDED`, and ambiguity in the JSON. NanoCell
  `NO_MATCH` and `CONTEXT_REJECTED` are also ordinary responses.
- Without `--output`, no mathematical bundle or route report is exported. Mod
  installation/activation/deactivation update the explicitly selected `--mod-root`.
  Rule proposals and
  individual file approvals use positional arguments for their output file;
  `rules-activate` updates the named store. For mathematical run commands, a
  redirect such as `> result.json` saves JSON output rather than a replay bundle.
  For `route`, stdout and `--output` contain the same replayable JSON report.
- Exports must not already exist. `experience replace` does not overwrite a
  file either. Target directories must exist. The exception is
  `rules-activate`, which atomically replaces the named rule store and its
  journal after explicit approval. Mod lifecycle commands also update their
  private installation journal and recoverable registry snapshot.

## First complete research run

The [tutorial](tutorial.sh) creates its own sample data in a new directory,
runs research, comparison, grid, resume, selection, and experience correction,
and checks key results. From the project root:

```bash
bash docs/cli/tutorial.sh ./build/bruce
```

It does not delete existing files. The printed working directory contains JSON
results, inputs, and replayable bundles. A deliberately triggered budget
exhaustion demonstrates the difference between an error code and a partial
domain result.

## Application boundaries

**IMPLEMENTIERT UND BELEGT:** DAG-Scheduler V4 adds exactly
`bruce dag-scheduler-v4 execute SPEC --output BUNDLE`,
`bruce dag-scheduler-v4 inspect BUNDLE` and `bruce dag-scheduler-v4 replay BUNDLE`,
also with NanoCell/MCP OFF. Execute reuses the V3 Spec and fixed
`literal-v1`, `append-v1`, `join-v1` catalog over the unchanged serial V2 core.
It publishes a new canonical `BRUCE_DAG_SCHEDULER_RUN_V4` bundle atomically
and exclusively; an existing file or symlink is preserved. Bundles are at
most 5 MiB including header/digest. Both readers are read-only.
**Request- und Payload-Rohbytes** are embedded in the bundle: these are local
**vertrauliche Artefakte**. All three JSON reports show metadata only under
separate `dag-scheduler-v4-execute-v1`, `dag-scheduler-v4-inspect-v1` and
`dag-scheduler-v4-replay-v1` contracts, with the payload `bundle_digest` and
`scheduler_version=dag-scheduler-v2`, receipts and trace.
Inspect validates header, digest, decoding and **Kanonizität** without running
callbacks; it cannot establish that encoded receipts or trace were executed.
Replay reexecutes the embedded Spec through the fixed catalog and requires
full run equality and byte-identical packing. This establishes local
**Reproduzierbarkeit**, no mathematical proof, encryption, authorship or
authentication. There are no free callbacks, external stores, network,
MCP execution, parallelism or migration of V3 runs. The optional
[MCP Inspector](#mcp-inspector) checks V4 structure and metadata.
V2/V3 behavior stays unchanged.
See [commands, reports and proof boundary](commands.md#dag-scheduler-v4--kanonische-lokale-bundles-und-replay),
[bundle format and confidentiality](formats.md#dag-scheduler-v4-run-bundle),
[core contracts](../../tests/unit/dag_scheduler_v4_tests.cpp) and
[real-binary workflows](../../tests/integration/test_dag_scheduler_v4_cli.py).
The unchanged `verify_examples.py` does not execute V4; its generated
examples/validation are not V4 runtime evidence. The existing MCP
assumption remains an independent OFF-build documentation limitation.

**IMPLEMENTIERT UND BELEGT:** DAG-Scheduler V3 accepts
`bruce dag-scheduler-v3 execute SPEC` and `bruce dag-scheduler-v3 inspect SPEC`,
also with NanoCell/MCP OFF.
It is a bounded parser/CLI adapter over the unchanged V2 core, with no new
scheduler. The ASCII, LF-only `BRUCE_DAG_SCHEDULER_SPEC_V3` file is limited
to 65,536 bytes and selects only `literal-v1`, `append-v1` and `join-v1`.
Verified direct READY inputs remain producer-ID sorted; V2 serial order and
budgets apply. `dag-scheduler-v3-cli-v1` JSON exposes the raw-file SHA-256
`spec_digest`, truthful `scheduler_version=dag-scheduler-v2`, sizes, digests,
receipts and trace, without request/payload bytes or their source hex.
The read-only Inspector uses the same `parse_spec()` validation, starts no
callbacks and never calls `execute_dag_scheduler_v2`. Its deterministic
`dag-scheduler-v3-inspect-v1` report contains only Spec digest, IDs, callbacks,
dependencies, request sizes/digests, budgets and `planned_start_order`: the
static lexicographic Kahn order, with no requests, payloads, receipts or trace.
It cannot predict callback outcomes, payload sizes, receipt-budget failures
or start-budget cancellation. The successful execute report remains unchanged.
This rendering provides no confidentiality or access control. There are no
arbitrary callbacks, persistence, bundle/replay, parallelism, GPU/network,
MCP interface or general orchestration features. See the
[exact command, report and limits](commands.md#dag-scheduler-v3--begrenzter-spec-dateiadapter),
[canonical Spec grammar](formats.md#dag-scheduler-v3-spec-datei) and
[real-binary workflow](../../tests/integration/test_dag_scheduler_v3_cli.py).
The unchanged `verify_examples.py` does not execute V3 execute or inspect; generated
examples/validation are not V3 runtime evidence. Its existing MCP
assumption remains a separate OFF-build documentation limitation.

**IMPLEMENTIERT UND BELEGT:** DAG-Scheduler V2 offers only the fixed local
in-memory `bruce dag-scheduler-v2 demo flow`, also with NanoCell/MCP OFF.
The bounded serial C++20 core passes verified direct READY payloads in
producer-ID order and runs one callback at a time in lexicographic Kahn order.
The fixed four-node flow emits `dag-scheduler-v2-cli-v1` JSON with digests and
sizes; raw requests/payloads are hidden. Each request/successful payload is
limited to 65,536 bytes, and the receipt budget includes every input copy.
Exit 0 has a complete stdout report and empty stderr; invalid arguments have
only compact usage JSON on stderr and exit 2. Valid but different callback
returns across runs are not automatically detectable; callbacks are not
rerun for detection. See the
[exact command, JSON and limits](commands.md#dag-scheduler-v2--fester-direkter-payload-fluss)
and [real-binary workflow](../../tests/integration/test_dag_scheduler_v2_cli.py).
There are no free inputs or additional CLI scenarios. V2 bundle/persistence/
replay, external stores, Phase-DAG V3 adapters, parallelism, GPU/network and
general orchestration remain **DOKUMENTIERT / KONZEPT**. V1 and Phase-DAG
contracts remain unchanged. The unchanged `verify_examples.py` does not
execute V2; generated examples/validation are not V2 runtime evidence.
Its existing MCP assumption can fail the global documentation test with
NanoCell/MCP OFF independently of the V2 workflow.

**IMPLEMENTIERT UND BELEGT:** DAG-Scheduler V1 provides three fixed local
in-memory demos: `bruce dag-scheduler-v1 demo mixed`,
`bruce dag-scheduler-v1 demo budget` and `bruce dag-scheduler-v1 demo cancelled`.
They also run with NanoCell/MCP OFF. The independent bounded C++20 core
selects the smallest ready node ID through serial Kahn order and invokes one
callback at a time with sorted direct successful predecessor references.
`dag-scheduler-v1-cli-v1` JSON has sorted terminal receipts and a chronological
logical trace ending in `COMPLETED`. Failure/blocking, exhausted start budget
and controlled cancellation remain distinct terminal results, each with exit 0.
There are no free graph files, callbacks, budgets, extra parameters, threads,
GPU/network execution, persistence, bundle/replay or Phase-DAG V2/V3 extension.
See the [exact commands, reports and limits](commands.md#dag-scheduler-v1--feste-serielle-referenzdemos)
and [real-binary workflow](../../tests/integration/test_dag_scheduler_v1_cli.py).
The unchanged `verify_examples.py` does not execute these demos; generated
examples/validation cover its existing scenarios, while source diagnostics
are indexed. General runtime orchestration remains **DOKUMENTIERT / KONZEPT**.
The documentation generator still assumes MCP: its documentation CTest
is incompatible with NanoCell/MCP OFF, as reproduced on the pre-Task-2
baseline. This existing configuration limit does not prevent the Scheduler
workflow from passing in either build. See the detailed coverage caveat in
the command reference.


**IMPLEMENTIERT UND BELEGT:** Signed-Affine-Relation V1 offers one fixed local
in-memory bridge, `bruce signed-affine-relation-v1 demo merge`, also without
NanoCell and MCP. It reconstructs `m × s + c` from the executed V1 trace:
public source 2 gives `a-add=(1,2)` and `z-double=(2,0)`, both at 4. Under
`n=101,G=1,Q=2`, mapping `a=c mod n`, `b=m mod n` gives a Toy collision at 4
and independently verified `candidate_k=2` in `signed-affine-relation-v1-cli-v1`
JSON, exit 0. The candidate is the already public source. This is a local
non-cryptographic toy reference, with no search, secret recovery, DLP solver
or real curve cryptography. It accepts no files, formulas or group parameters
and adds no persistence, scheduler or Phase-DAG V2/V3 interface. Existing
Signed-Affine-DAG V1 and Toy-Relation V1 APIs, commands and reports remain
unchanged. See the [exact command, report and errors](commands.md#signed-affine-relation-v1--feste-referenzbrücke)
and [real-binary workflow](../../tests/integration/test_signed_affine_relation_v1_cli.py).
The unchanged `verify_examples.py` does not execute this demo; generated
examples/validation cover its existing scenarios, while source diagnostics
are indexed. The broader [Affine Phase-DAG roadmap](../concepts/affine-phase-dag.md)
remains **DOKUMENTIERT / KONZEPT**.

**IMPLEMENTIERT UND BELEGT:** Toy-Relation V1 provides exactly two fixed local
in-memory demos, `bruce toy-relation-v1 demo collision` and
`bruce toy-relation-v1 demo noninvertible`, independently of NanoCell and
MCP. The `toy-relation-v1-cli-v1` reports show a collision at 53 with
independently verified candidate 17, or a collision at 5 with
`NO_UNIQUE_RELATION` and no relation/candidate. Both return exit 0. The fixed
candidate is not secret; this is no discrete-logarithm solver, search or
real-world curve cryptography. There are no user group parameters, files,
persistence or Phase-DAG V2/V3/Signed-Affine extensions. See the
[exact commands, reports and errors](commands.md#toy-relation-v1--feste-referenzdemos)
and [real-binary workflows](../../tests/integration/test_toy_relation_v1_cli.py).
The unchanged `verify_examples.py` does not execute these demos: generated
examples/validation cover its existing scenarios, while source diagnostics
are indexed. The dedicated workflows establish the demo behavior.

**IMPLEMENTIERT UND BELEGT:** Signed-Affine-DAG V1 provides exactly two fixed
in-memory demos, `bruce signed-affine-dag-v1 demo merge` and
`bruce signed-affine-dag-v1 demo cancellation`, independently of the optional
NanoCell mod. Their `signed-affine-dag-v1-cli-v1` JSON reports retain both
paths to value 4 with balances 2 and 0, respectively, plus the source state
at value 2. They accept no graph files or free formulas, persist no runs,
and do not extend Phase-DAG V2/V3. See the
[commands, report fields, ordering and errors](commands.md#signed-affine-dag-v1--feste-referenzdemos)
and [real-binary workflow evidence](../../tests/integration/test_signed_affine_dag_v1_cli.py).
The broader [Affine Phase-DAG roadmap](../concepts/affine-phase-dag.md) is
**DOKUMENTIERT / KONZEPT**.

BRUCE investigates bounded built-in mathematical families. When the CMake
option `BRUCE_BUILD_MOD_LANGUAGE` is enabled,
`language interpret/select/execute/replay` provides an implemented,
deterministic Language mod: V1 remains limited to two fixed sentence patterns;
V2 adds several explicitly versioned German phrasings for the same rational
portfolio semantics. A general free-form dialogue language and a general
formula prover are not implemented. With `BRUCE_BUILD_MOD_NANOCELL=ON`, the
optional static [NanoCell mod](commands.md#nanocell--optionaler-statischer-nanocell-mod)
runs three fixed byte techniques with bound context, writes new provenance-bound
mod exports, and can only read historical direct V1 files. It is not part of
the general research session. The optional local MCP Inspector is described
below. **IMPLEMENTIERT UND BELEGT:** `phase-dag-v2 execute/inspect/replay`
adds serial, bounded CLI execution and confirmed read-only bundle views with
the fixed NanoCell and Evidence-Gate adapters; see the [three exact commands](commands.md#phase-dag-v2--begrenzte-nanocell-dag-ausführung),
[strict text Spec](formats.md#phase-dag-v2-spec-und-bundle) and
[partial outcomes](results-and-control.md#phase-dag-v2-ergebnis-und-teilbefunde).
`execute` requires exclusive `--output`; valid partial receipts still return
exit 0. The Phase-DAG V2 C++ library supports bounded execution and replay;
a general scheduler and broader runtime orchestration remain
**DOKUMENTIERT / KONZEPT**. By contrast, `compare` can
prove statements about two explicitly supplied rational functions over the
rational numbers; the relevant domain of definition is part of that statement.

**IMPLEMENTIERT UND BELEGT:** `phase-dag-v3 execute/inspect/replay` provides
separate portable byte-payload edges with the fixed `nanocell-phase-v3`/`v1`
and `payload-gate-v1`/`v1` adapters. Each payload is bounded to 65536 bytes;
the receipt budget also includes payload bytes. Both readers confirm actual
replay without original inputs; reports expose metadata without raw payloads.
V2 remains compatible; the MCP inspects existing V2 and V3 bundles
through separate read-only routes and explicit version selection.
The execute-only option `--max-parallel-nodes N` accepts 1..4 (default 1).
Independent adapters run in bounded physical waves; receipts and logical trace
retain the existing deterministic serial order. The same Spec produces
byte-identical bundles at each permitted limit, confirmed by serial replay.
`inspect` and `replay` accept no options. No performance improvement is claimed.
See [V3 commands](commands.md#phase-dag-v3--portable-payload-kanten),
[V3 Spec and bundle](formats.md#phase-dag-v3-spec-und-bundle), and
[V3 reports](results-and-control.md#phase-dag-v3-ergebnis-und-payloadgrenzen).

The [local mod commands](commands.md#mods-und-route--built-ins-sowie-installierte-data-logic-mods)
show static built-ins and build-state, inspect or verify signed Data/Logic
packages, and manage a durable user-owned installation without recompilation.
Installation is inactive; activation is explicit. Multiple versions remain
side by side, while `update` atomically installs and activates one version and
keeps older versions available for explicit rollback. `route` selects a
template deterministically; `--mod-details` opts into typed package ownership.
Only `mods run` executes an active Logic worker through the isolated V1 host.
There is no default installation path, implicit trusted key, BRUCE system
account, central service, or automatic network update.

## Local Mod Trust V2

**IMPLEMENTIERT UND BELEGT:** Trust rotation is an explicit local operation
for the invoking Linux user or administrator. BRUCE adds no system user,
daemon or multi-user service. Existing roots retain their V1 behavior until
an explicit import; ordinary install/update commands do not migrate them.

```sh
bruce mods trust status --mod-root ROOT
bruce mods trust import-v1 --mod-root ROOT
bruce mods trust prepare NEXT_TRUST.pub --mod-root ROOT
bruce mods trust verify --mod-root ROOT
bruce mods trust activate --mod-root ROOT [--deactivate-untrusted]
bruce mods trust rollback --mod-root ROOT [--deactivate-untrusted]
```

Square brackets in this synopsis mark an optional flag and are not arguments.
`import-v1` copies the exact already-bound root truststore into generation
`00000000000000000001`; it accepts no replacement truststore. A durable
migration fence and barriers in both registry journals exclude older V1
binaries. Once the fence is published, a current V2 host must complete recovery;
`rollback` changes the trust generation and does not undo migration.
Keep a current binary available before importing a root.
Before that fence, an interrupted import leaves the complete legacy state
usable through the current host. Repeat `import-v1` explicitly to finish it:
the host validates the initial import footprint, retains the immutable first
generation, and prepares fresh snapshots including intervening legacy changes.
An established V2 state or surviving migration barrier cannot use this retry
path to return to legacy authority.

`prepare` validates a canonical V1 public truststore in a regular explicit
file owned by the effective user, with no group/other permission bits, one
hardlink and no symlink components (use a private directory and
`chmod 600 NEXT_TRUST.pub`). It copies it into a new immutable 20-digit
generation and does not activate keys. Generation metadata binds exact trust bytes, SHA-256 and UTC
creation time; CLI time comes from the system clock. State names `active`,
`prepared` and `previous` references with digests. `status` returns only this
state under a shared nonblocking lock: it creates no root/lock and performs no
recovery. On pending recovery it fails with `RuntimeTrustRecoveryRequired`.
`verify` checks coordinator history, projections, generations and installed
package integrity, returning `active_incompatible` and `prepared_incompatible`
records, including inactive historical packages. Those lists are compatibility
findings, not automatic deactivation. Under its exclusive lock, `verify` can
remove a torn journal suffix only after validating the fully projected
committed prefix and packages. It declines more complex pending recovery
without modifying the root.

`activate` selects the prepared generation; `rollback` selects the previous
generation and retains the current generation as history. Both clear
`prepared`. If an active Data or Logic Mod would lose its exact signing key,
the safe default exits 2 with `RuntimeTrustActiveModsIncompatible` and an
`affected` list, without changing state. Each record exposes only kind,
Mod-ID, version and signing-key ID. With the explicit emergency flag
`--deactivate-untrusted`, the switch and disabling of precisely those active
Mods commit in one root transaction. Historical packages remain installed;
rollback does not automatically reactivate disabled packages.

The coordinator Commit is the durable switch point under `registry.lock`.
Recovery completes committed Data/Logic/state projections before normal
operations; an uncommitted switch does not become active. New installs,
updates, activations and Logic bindings resolve active V2 trust under the
same lock. A `--trust-store` supplied to legacy install/update must match the
root's active trust; it cannot replace or bypass V2 trust. Standalone
`mods verify` checks only its supplied file, not authorization to install.
Already-running workers are not terminated. `mods run` retains the lock
through worker completion, so a conflicting rotation cannot switch trust
until that execution ends. Later `mods run` calls resolve active trust before
binding. An already-issued library binding remains an authorization snapshot
of its pinned worker bytes; rotation does not retroactively revoke it.

Successful commands return `runtime-mod-trust-v2` JSON: import/prepare return
`result: "ok"`, status returns state, verify adds compatibility lists, and
activate/rollback add `deactivated`. Errors are symbolic JSON on stderr with
exit 2 and no input paths or key bytes. Representative errors include
`RuntimeTrustNotImported`, `RuntimeTrustAlreadyImported`,
`RuntimeTrustTransitionMissing`, `MissingRuntimeTrustAction`,
`UnknownRuntimeTrustAction`, `MissingRuntimeTrustStore`, `MissingOption`,
`MissingOptionValue`, `DuplicateOption` and `UnknownOption`.
The no-value emergency flag is accepted only on activate/rollback; duplicates,
unknown options and missing `--mod-root` are rejected before mutation.

Evidence: [CLI parser](../../apps/runtime_mod_cli.hpp),
[host](../../src/mod/installed_mod_host_v2.cpp),
[coordinator](../../src/mod/runtime_trust_store_v2.cpp),
[generation/recovery contracts](../../tests/unit/runtime_trust_store_v2_tests.cpp),
[host transition/concurrency tests](../../tests/unit/installed_mod_host_v2_tests.cpp)
and [real-binary workflows](../../tests/integration/test_runtime_mods.py).
The [generated examples](examples.json) use only public test keys and isolated
temporary roots; they do not prove a production key ceremony.

**DOKUMENTIERT / KONZEPT:** automatic migration, online key distribution,
central revocation and remote multi-user administration are outside this
local contract. Core release signing uses separate keys and a separate
[offline audited-TGZ workflow](../implementation/150-release-signing-contract.md).

The only CLI network interface is the optional loopback MCP Inspector.
Executable dynamic strategy plugins, GPU control, and a universal time/RAM
limit are absent. Grid and numerical Z compatibility still use bounded
integers. Exact BigInt/rational-function research is a separate path.

## MCP Inspector

**IMPLEMENTIERT UND BELEGT:** enable the optional static Mod with
`BRUCE_BUILD_MOD_MCP=ON`; it requires `BRUCE_BUILD_MOD_NANOCELL=ON`.
`bruce mcp [--port 1..65535]` defaults to 8080 and serves only `127.0.0.1`.
An unavailable requested port fails controlledly, without another port/address.
Open `http://127.0.0.1:8080/` in a browser for the default invocation.
Only `GET /`, `GET /api/v1/health`, `POST /api/v1/inspect`,
`POST /api/v3/inspect` and `POST /api/dag-scheduler-v4/inspect` are registered.

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
V3 additionally exposes `scheduler_explanation` with `canonical-logical-v1`,
`parallelism_recorded: false` and direct `blocking_predecessors`. Physical
parallelism is not recorded: no timing, wave, thread ID or parallel-limit
metadata is stored. This remains a replay-derived logical explanation.
Replay/digest establishes bundle consistency, not author identity or a general
mathematical proof. The 5-MiB limit does not override tighter core/catalog limits.

See [ports, errors and HTTP results](commands.md#mcp-inspector),
[bundle privacy and metadata](formats.md#mcp-inspector),
[V2 acceptance evidence](../superpowers/specs/2026-10-12-phase-dag-v2-cockpit-inspector-design.md)
and [HTTP/DOM contracts](../../tests/mods/mcp/test_mcp_inspector.py).

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


## Maintaining the reference

`commands.md`, `formats.md`, and `results-and-control.md` explain behavior.
`source-index.md` is derived directly from the CLI parser and error literals;
`examples.json` contains real invocations and their complete responses. On a
new core version, both text and executed examples must be checked again. The
[historical implementation packages](../README.md) remain available for
provenance and detailed contracts.
