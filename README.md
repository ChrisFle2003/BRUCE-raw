# BRUCE_raw

BRUCE_raw is a bounded C++20 research core for local, reproducible
mathematical investigation. Its primary interface is the Linux command-line
program `bruce`. The current CMake project version is **Core 0.102.0**.

This repository is not a network service, a browser product, a general
formula prover, or a free-form dialogue system. It investigates explicitly
selected, bounded mathematical families; records results and replay data; and
keeps control, publication, and local-file boundaries explicit. A successful
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
experience records, explicit local publication, and signed runtime data-mod
routing. The [CLI manual](docs/cli/README.md) is the authoritative user-facing
reference for commands, formats, limits, and generated examples.

**IMPLEMENTED AND VERIFIED:** `Language` and `NanoCell` are optional static
mods built into the executable when their CMake switches are enabled. They are
not dynamically loaded executable plugins.

**DOCUMENTED / CONCEPT:** free-form dialogue, a general formula prover, GPU
execution, Phase-DAG execution, and a Cockpit build integration. In particular,
the current `CMakeLists.txt` defines no `BRUCE_BUILD_MOD_COCKPIT` option or
Cockpit target; unbuilt fragments or planning records do not establish an
implemented Cockpit feature.

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
dependency path is required:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-build
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build --output-on-failure
```

The wrapper must run from a native project path that contains no `:`. It
deliberately rejects a GVFS SFTP view because that view can split library
search paths. Use a native SSH path on the target host or a complete temporary
copy outside GVFS instead.

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

## Mods: two different mechanisms

The static build switches are:

```sh
cmake -G Ninja -S . -B build \
  -DBRUCE_BUILD_MOD_LANGUAGE=ON \
  -DBRUCE_BUILD_MOD_NANOCELL=ON
```

Setting either switch to `OFF` omits that mod and its associated CLI command.
There is no executable-plugin search or third-party binary loading.

Separately, the runtime data-mod facility accepts signed, schema-checked Tree
routing data through explicit `mods` and `route` commands. These packages add
data to a controlled registry; they do not add executable code and are not
active until explicitly activated. `bruce-mod-builder` builds a local signed
data-mod package and a trust store from canonical JSON inputs and a protected
local seed; it does not install or activate the package. See the
[local-data-mod example](examples/local-data-mods/math-rational-power/README.md).

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
