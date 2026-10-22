# First local BRUCE data-mod: rational-power routing

**IMPLEMENTED AND VERIFIED:** `local_data_mod_example_workflows` builds these
inputs with a published test-only key in a fresh temporary directory, then
inspects, verifies, installs, activates, routes, replays and disables the
package through the real builder and runtime. It never reads the operator's
project `.env`.

This example adds the child `[1,1,5]` beneath the Core mathematics mount `[1,1]`.
Its controlled pattern is `<rational> hoch <integer>`. The input `3/4 hoch 2`
selects `math.rational.power.v1`; a zero denominator or the unregistered word
`potenz` returns `UNKNOWN`. ASCII case and whitespace normalization still apply.
The result identifies a template; routing does not execute a calculation or
return `9/16`. `candidate_only` does not establish a mathematical proof.

The builder grants exactly `dataset.read` and `tree.extend`, uses user scope,
and emits the supported V1 versions automatically. No executable code is included.

## Inputs and private signing file

- `manifest.json`: mod ID, version and public signing-key ID.
- `tree.json`: child address, controlled pattern, template and claim scope.
- `golden-cases.json`: expected matched and unknown routes, checked by the
  installation/activation lifecycle against the candidate Tree.

The JSON inputs are canonical UTF-8: sorted object keys, compact punctuation,
and **no final newline**. They contain no key material. See the
[V1 schema](../../../tests/fixtures/runtime_mods/README.md) for the supported
values and limits. Keep edited inputs in that format; ordinary pretty-printed
JSON is rejected. Never include secrets in the manifest, Tree or golden inputs.

The recommended builder grammar receives a signing file explicitly with
`--signing-env`. That file must belong to the current user, have mode `0600`,
and contain exactly one `BRUCE_MOD_ED25519_SEED_HEX` assignment with a locally
generated, private 32-byte Ed25519 seed encoded as 64 lowercase hexadecimal
characters (one final LF is allowed). It accepts neither symlinks nor hard
links. Keep an existing operator key; do not replace it to rerun this example.
Do not display or shell-source the file, copy it into the source directory, or
commit it. Published test keys are unsuitable for a personal installation.
The old five-option builder grammar remains byte-compatible and reads only
`.env` in the current directory; new workflows should use the explicit form.

Modern `data` and `logic` failures emit exactly
`{"error":"LocalModBuildFailed"}` plus LF on stderr, leave stdout empty, and
exit 2. This includes invalid arguments, unsafe paths or signing files,
malformed packages, existing outputs, and internal failures; no input paths,
seeds, source contents, or nested diagnostics are disclosed. The legacy
five-option grammar retains `LocalModBuildFailed` plus LF on stderr, empty
stdout, and exit 1. Successful JSON reports and help are unchanged.

## Build and use on this machine

Run from the project root on Linux with the existing C++20, CMake, GMP and
OpenSSL dependencies. Substitute a different build directory if one is already
available:

```sh
cmake -G Ninja -S . -B build-cli -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-cli --target bruce bruce-mod-builder -j2
```

Create the output directory privately. `runs/` is ignored by Git. Both package
and public truststore are written with mode `0600`; the directory must belong
to you and must not be writable by other users or groups. Use a new output
filename or directory on later builds: existing outputs are never replaced.

```sh
umask 077
mkdir -p runs/local-data-mods
./build-cli/bruce-mod-builder data \
  examples/local-data-mods/math-rational-power \
  --output runs/local-data-mods/math-rational-power-v1.brucemod \
  --trust-store-output runs/local-data-mods/local-math-v1.pub \
  --signing-env runs/local-data-mods/signing.env
```

The builder reports the public mod ID, version and package digest. The public
truststore contains `local-math-v1` and the public key derived from the local
seed. Review which key you trust before accepting packages from other authors.
Building alone does not install or activate anything. Verify first, then
install into an explicitly selected private registry:

```sh
./build-cli/bruce mods inspect runs/local-data-mods/math-rational-power-v1.brucemod
./build-cli/bruce mods verify runs/local-data-mods/math-rational-power-v1.brucemod \
  --trust-store runs/local-data-mods/local-math-v1.pub
./build-cli/bruce mods install runs/local-data-mods/math-rational-power-v1.brucemod \
  --mod-root runs/local-data-mods/mods \
  --trust-store runs/local-data-mods/local-math-v1.pub
./build-cli/bruce mods list --mod-root runs/local-data-mods/mods
./build-cli/bruce mods status user.local-math-rational-power \
  --mod-root runs/local-data-mods/mods
./build-cli/bruce route --text '3/4 hoch 2' --mod-root runs/local-data-mods/mods
```

The list shows `installed_inactive`; the route is `UNKNOWN` until activation.
Activation mounts the child and checks the golden cases. An existing mod using
the same address causes a conflict; use a fresh private registry for this first
example rather than replacing an existing child.

```sh
./build-cli/bruce mods activate user.local-math-rational-power 1.0.0 \
  --mod-root runs/local-data-mods/mods
./build-cli/bruce route --text '3/4 hoch 2' --mod-root runs/local-data-mods/mods \
  --output runs/local-data-mods/power.route.json --mod-details
./build-cli/bruce route replay runs/local-data-mods/power.route.json \
  --mod-root runs/local-data-mods/mods --mod-details
```

The route now returns `MATCHED`, candidate address `[1,1,5]`, template
`math.rational.power.v1`, and a Data-Mod owner. The saved V2 report binds the
active Tree and package digest. Replay returns the same report while that
snapshot remains active. Without `--mod-details`, the unchanged V1 report is
emitted instead.
To remove this child from the active Tree, retain its stored package and disable
it explicitly:

```sh
./build-cli/bruce mods disable user.local-math-rational-power \
  --mod-root runs/local-data-mods/mods
./build-cli/bruce route --text '3/4 hoch 2' --mod-root runs/local-data-mods/mods
```

The route returns `UNKNOWN` again. Replaying the old report after this change
fails with `RouteRegistryDigestMismatch` (exit code 2). Installation, activation,
and deactivation never recompile BRUCE.

For a later signed version, `mods update PACKAGE --mod-root ... --trust-store
...` installs and activates it atomically. The old version remains disabled and
can be selected again with `mods activate MOD_ID OLD_VERSION`; no package is
downloaded automatically.

## Reproducible check without an operator key

```sh
cmake --build build-cli -j2
ctest --test-dir build-cli -R 'local_builder|local_data_mod_example|runtime_mod|runtime_package|runtime_registry|tree_registry' --output-on-failure
```

These tests create their own temporary key files from published test material.
The example integration checks both new routing and preservation of the Core
addition route. The complete project suite remains `ctest --test-dir build-cli
--output-on-failure`.
