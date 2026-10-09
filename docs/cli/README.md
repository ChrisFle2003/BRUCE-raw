# BRUCE Core 0.102.0 – CLI manual

Current as of 9 October 2026. This reference describes the implemented Linux
CLI in `apps/bruce_cli.cpp`, `apps/runtime_mod_cli.hpp`, `apps/nanocell_cli.hpp`, and
`apps/rule_review.hpp`, together with the parsers and executors they invoke.
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
- Each option has exactly one following value: `--max-bits 4096`.
  `--max-bits=4096`, short options, combined flags, and a general `--`
  delimiter are unsupported.
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
  plain-text dialogue, then JSON on success. Errors are JSON on `stderr` and
  use exit code 2.
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
the general research session. With `BRUCE_BUILD_MOD_COCKPIT=ON`, the local
[Cockpit](commands.md#cockpit--optionaler-statischer-cockpit-mod) provides only the
separate management contracts; it performs no independent domain research. A
holdout confirms additional concrete number pairs. By contrast, `compare` can
prove statements about two explicitly supplied rational functions over the
rational numbers; the relevant domain of definition is part of that statement.

The [runtime data-mod commands](commands.md#mods-und-route--signierte-daten-mods-zur-laufzeit)
install signed user-scoped Tree routing data without recompilation and remain
inactive until explicit activation. `route` selects a template deterministically;
it does not run the selected template. Reports bind replay to the active Tree
and package versions. There is no default installation path or implicit trusted
key; every installation path and initial trust store is explicit.

The CLI has no network protocol, executable dynamic strategy plugins, GPU control, or
universal time/RAM limit. Grid and numerical Z compatibility still use bounded
integers. Exact BigInt/rational-function research is a separate path.

## Maintaining the reference

`commands.md`, `formats.md`, and `results-and-control.md` explain behavior.
`source-index.md` is derived directly from the CLI parser and error literals;
`examples.json` contains real invocations and their complete responses. On a
new core version, both text and executed examples must be checked again. The
[historical implementation packages](../README.md) remain available for
provenance and detailed contracts.
