# Local Mod Lifecycle V2

**IMPLEMENTIERT UND BELEGT:** This contract describes the local, single-user
mod host exposed by `bruce mods`, `bruce route`, and `bruce-mod-builder`.
It is not a daemon, account system, multi-user service, package repository, or
network update mechanism.

## Three mod classes

| Class | Origin | Build or installation state | Execution boundary |
| --- | --- | --- | --- |
| Language, NanoCell, MCP | built into `bruce` | `available` or `not_built` | fixed C++ implementation selected at configure time |
| Data Mod | signed local package | installed, then explicitly activated | declarative Tree routing data only |
| Logic Mod | signed local package | installed, then explicitly activated | one pinned native worker through the existing Logic-Mod V1 sandbox |

Built-in Mod IDs are immutable through the installed-mod lifecycle. Language,
NanoCell, and MCP always appear in `bruce-mod-catalog-v2`, even in an OFF
build, so callers can distinguish `not_built` from an unknown mod. MCP requires
NanoCell at configuration time. The `bruce mcp` loopback inspector and the
separate `tools/bruce_mcp.py` stdio protocol adapter are different interfaces.

## Build and package admission

`bruce-mod-builder data SOURCE_DIR ...` requires canonical
`manifest.json`, `tree.json`, and `golden-cases.json`. `bruce-mod-builder logic
SOURCE_DIR ...` additionally requires an embedded `data.brucemod`, canonical
`logic.json`, and exactly the declared worker. Both forms read a private 0600
signing file supplied by `--signing-env`, publish package and trust outputs
without replacement, and execute no compiler, shell, or generated program.

Both modern subcommands report every failure as exactly
`{"error":"LocalModBuildFailed"}` followed by LF on stderr, with empty stdout
and exit 2. This includes argument, path, signing, package, output-collision,
and internal builder failures. Paths, signing seeds, source inputs, and nested
diagnostics are never disclosed. The legacy five-option grammar
(`--manifest`, `--tree`, `--golden`, `--output`, `--trust-store-output`)
retains exactly `LocalModBuildFailed` followed by LF on stderr, empty stdout,
and exit 1. Success reports and `--help` remain unchanged.

`mods inspect` recognizes only the exact Data V1 and Logic V1 envelope magic
and reports metadata without establishing trust. `mods verify` additionally
requires an explicit trust store and verifies the complete selected package.
Package readers accept only bounded regular files, reject a final symlink, and
detect a file changing while it is read. Data and Logic envelopes are limited
to 1 MiB.

## Durable local state

`mods install` stores a verified version as `installed_inactive`. Data and
Logic use separate package/worker storage and journals below one explicit
owner-only mod root, but are projected through one installed host and one
canonical `bruce-mod-catalog-v2`. Mutations take a shared root lock. Journal
publication and recovery are fail-closed and atomically replace only the
affected state file. Extracted Logic workers are owner-only regular files with
mode 0500 and are reverified before binding.

Multiple versions of one mod remain installed side by side. `mods activate`
selects exactly one version; the previously active version becomes disabled.
`mods disable` leaves all versions installed. `mods update` is an atomic
install-and-activate operation for one Data or Logic package: the same package
is idempotent, a new version becomes active, and older versions remain
available for explicit rollback with `mods activate MOD_ID VERSION`. Reusing
the same ID and version with different package bytes is rejected.

There is no implicit mod root, global installation, privileged helper,
background service, automatic download, automatic activation, or special
BRUCE operating-system user. The invoking local user owns the files and must
name every root and initial trust store explicitly.

## Discovery, routing, and execution

`mods list` and `mods status` can describe the three optional built-in Mods without opening a mod
root. With `--mod-root`, their canonical V2 catalog also includes installed
Data and Logic versions with origin, kind, lifecycle state, package digest,
and capabilities.

The unchanged default `route` and `route replay` emit and verify
`route-report-v1`. Opting into `--mod-details` emits or verifies
`route-report-v2`, which adds a typed owner to every candidate and the selected
owner. This makes Data/Logic ownership visible without changing V1 bytes.
Routing selects a template; it does not execute it.

`mods run MOD_ID --template TEMPLATE --input TEXT --mod-root ROOT` accepts only
an active installed Logic Mod with `worker.execute`. It obtains a fresh binding,
pins the verified worker bytes before spawn, and uses the existing Logic-Mod V1
worker supervisor. Disable or update affects future bindings, not the bytes of
an already issued binding. Data Mods and built-in IDs are not executable
through this command. The current result contract is
`installed-logic-mod-run-v1`; accepted mathematical scope remains
`candidate_only` where the existing host validator applies.

## Evidence and exclusions

Evidence is provided by the package, catalog, registry, installed-host, update,
route, builder, and real-worker tests in `tests/unit/` and `tests/integration/`.
The public APIs do not expose the test publication failpoints.

**DOKUMENTIERT / KONZEPT:** remote package discovery, online updates,
multi-user policy, a central mod service, dependency solving, garbage
collection, arbitrary shared libraries, Windows/macOS worker isolation, and a
general executable-plugin ABI.
