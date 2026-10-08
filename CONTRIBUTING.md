# Contributing to BRUCE_raw

Thank you for your interest in BRUCE_raw. The core is a C++20 research
project: code, tests, and reproducible runs are evidence; roadmaps, concepts,
and ADR designs are not implementation promises. Third-party contributions
require prior written agreement with Christopher Flegel; a pull request neither
transfers rights nor grants a license.

## Before your first contribution

1. Confirm the written contribution agreement with Christopher Flegel before
   creating a pull request.
2. Then read [README.md](README.md), the [documentation index](docs/README.md),
   and the [project manual](AGENTS.md).
3. Check whether the topic already appears in [concepts](docs/concepts/README.md),
   [open decisions](docs/implementation/03-open-decisions.md), or existing issues.
4. Open a structured bug report for defects and a feature request for new ideas.
   Do not report security vulnerabilities in public issues; see
   [SECURITY.md](SECURITY.md).

## Local environment

Install CMake 3.20 or later, Ninja, a C++20 compiler, `pkg-config`, and the
GMP/GMPXX, OpenSSL Crypto, and Python 3 development tools. The
[README build path](README.md#build-test-and-install) is the reference for a
normal local environment. In the supplied Bruce-AI environment,
`scripts/with-host-deps.sh` sets local package paths for its child process.

From a native project path without `:`:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-build
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build --output-on-failure
```

The wrapper deliberately refuses GVFS SFTP paths. Use a native SSH path or a
complete temporary copy outside GVFS instead.

## Expected checks

Before opening a pull request, run at least the affected tests. For changes to
the build, C++ code, or tests, the full verification path above is expected.
Additionally run:

```sh
git diff --check
```

CI also checks local Markdown links, reproduces generated CLI artifacts, and
runs a secret scan. The generated files `docs/cli/source-index.md`,
`docs/cli/examples.json`, and `docs/cli/validation.md` may only be updated or
checked through `docs/cli/verify_examples.py` in a temporary copy—never by hand.

## Submitting changes

- Keep pull requests small and focused.
- For behavior changes, add appropriate tests and name the commands actually
  run in the pull request.
- Update documentation, examples, and contracts with the implementation.
- Classify content precisely as **IMPLEMENTED AND VERIFIED**,
  **IMPLEMENTED, BUT NOT CURRENTLY VERIFIED**, **DOCUMENTED / CONCEPT**,
  **PROPOSAL**, or **OPEN DECISION**.
- Do not commit build output, raw benchmark data, local configuration files,
  secrets, or access tokens.
- Do not alter pre-existing working-copy changes made by others.
