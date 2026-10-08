# BRUCE_raw

BRUCE_raw is a bounded C++20 research core for verifiable mathematical
candidates, explanations, and replay. The current documented core state is
Core 0.102; its actual claim boundaries are set out in the
[Blueprint](docs/architecture/03-blueprint-consolidation.md).

The original BRUCE source code is proprietary and licensed under the
[BRUCE Proprietary License](LICENSE). All rights are held by Christopher
Flegel; sale or resale requires prior written agreement. This is a local
Linux CLI, not a network service. Concept papers and roadmaps do not describe
guaranteed runtime capabilities.

## Getting started

- [Documentation index](docs/README.md) – architecture, contracts, implementation, and CLI.
- [CLI manual](docs/cli/README.md) – commands, data formats, and examples.
- [Project manual for contributors and agents](AGENTS.md) – working rules and checks.
- [Concepts](docs/concepts/README.md) – documented ideas; do not treat them as implementation.
- [Historical root entry point](HISTORY.md) – development to date and earlier states.

## CLI

The local C++ CLI is named `bruce`.

```sh
./build/bruce --help
```

The complete code-based reference, including claim scope, build variants, data
flow, and verification state, is available at
[docs/cli/CLI_REFERENCE.md](docs/cli/CLI_REFERENCE.md).

## Prerequisites

The documented, CI-tested path is Ubuntu 24.04 with:

- a C++20 compiler (CI: `g++`), CMake 3.20 or later, and Ninja;
- `pkg-config`, GMP/GMPXX, and OpenSSL Crypto, including development files;
- Python 3 for integration tests and the CLI tutorial.

On Debian/Ubuntu, install the required packages with:

```sh
sudo apt-get update
sudo apt-get install -y cmake ninja-build g++ pkg-config libgmp-dev libssl-dev python3 python3-dev
```

## Build, test, and install

With the dependencies above installed, this sequence creates a release build
and runs the complete test suite:

```sh
cmake -G Ninja -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

For a local installation without administrator privileges:

```sh
cmake --install build --prefix "$HOME/.local"
"$HOME/.local/bin/bruce" --version
```

The installed command is `bruce`. The [CLI manual](docs/cli/README.md)
describes installed files, dependencies, and third-party notices.

## Local use and configuration

```sh
./build/bruce --version
./build/bruce --help
bash docs/cli/tutorial.sh ./build/bruce
```

The runtime uses neither a hidden configuration file nor an environment
variable to choose a strategy. Inputs are supplied through CLI arguments and
explicitly named files. The three built-in static mods can be enabled or
disabled selectively at configuration time:

```sh
cmake -G Ninja -S . -B build \
  -DBRUCE_BUILD_MOD_LANGUAGE=ON \
  -DBRUCE_BUILD_MOD_NANOCELL=ON \
  -DBRUCE_BUILD_MOD_COCKPIT=ON
```

With `=OFF`, the respective mod is not built and its CLI command is not
registered. Formats, limits, and complete examples are in the
[CLI manual](docs/cli/README.md).

## Provided Bruce-AI dependencies

The supplied dependency environment is not modified. For a fresh release
build outside the repository:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-build
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build --output-on-failure
```

The wrapper requires a native project path without `:`. It deliberately
refuses to run from a GVFS SFTP view such as `sftp:host=…`: search paths would
otherwise be split into fragments and silently fall back to host dependencies.
In that case, use the native SSH path on the target machine or a complete
temporary copy outside GVFS.

`BRUCE_BUILD_MOD_LANGUAGE=ON`, `BRUCE_BUILD_MOD_NANOCELL=ON`, and
`BRUCE_BUILD_MOD_COCKPIT=ON` enable the static mods that are built by default.
With the respective `=OFF`, the core is built without that mod's sources and
the associated CLI command is unavailable. There is no dynamic plugin search
or loading of third-party binaries.

## Contributing

Before opening a pull request, run at least the tests relevant to the change
and `git diff --check`. [CONTRIBUTING.md](CONTRIBUTING.md) explains the full
workflow, documentation rules, and handling of generated CLI artifacts.
Security vulnerabilities do not belong in public issues; see
[SECURITY.md](SECURITY.md).

## Mod status

- **IMPLEMENTED AND VERIFIED:** `bruce_mod_runtime`, the fixed build catalog,
  the optional deterministic Language mod, the optional NanoCell mod, and the
  local Cockpit as a separate static mod. New language and NanoCell exports
  use `BRUCE_MOD_RUN_V1`; direct historic `BRUCE_LANGUAGE_RUN_V1` and
  `BRUCE_NANOCELL_RUN_V1` files remain replayable read-only.
- **IMPLEMENTED AND VERIFIED:** the bounded Phase-DAG V1 core with its own
  CLI/replay integration remains a separate package.
- **IMPLEMENTED AND VERIFIED:** [explicit Cockpit backend execution of stored
  LanguageDraftV2 via CORE](docs/implementation/210-cockpit-language-core-dispatch-usage.md)
  and the static/HTTP contract for [dashboard execution](docs/implementation/211-cockpit-language-dispatch-ui-usage.md),
  when the Language/Dispatch bridge is enabled, without jobs or persistence. Browser interaction is **IMPLEMENTED, BUT NOT CURRENTLY VERIFIED**.
- **IMPLEMENTED AND VERIFIED:** [Cockpit backend export and separate replay of
  caller-held Language→CORE dispatch bundles](docs/implementation/212-cockpit-language-dispatch-bundle-replay-usage.md),
  with bounded receipt and without server persistence or transport UI.
- **DOCUMENTED / CONCEPT:** free-form dialogue, NanoCell as a general
  `ApplicationTask` run, and GPU execution.

## Interpreting the repository state

- **IMPLEMENTED AND VERIFIED:** claims supported by current source code,
  tests, or reproducible runs only.
- **IMPLEMENTED, BUT NOT CURRENTLY VERIFIED:** existing code without a fresh
  check in the relevant environment.
- **DOCUMENTED / CONCEPT:** designs, roadmaps, and architecture goals.
- **PROPOSAL / OPEN DECISION:** work that has not yet been decided or
  implemented.

Local output belongs in `runs/`; build directories and local worktrees remain
ignored. Compact benchmark summaries are versioned under `benchmarks/`; raw
data remains local.
