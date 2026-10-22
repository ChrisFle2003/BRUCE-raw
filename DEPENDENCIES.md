# BRUCE: local CLI distribution and dependencies

The local package contains the CLI, build information, and a checksum list of
the included build, source, and test files. It contains no C++ SDK and bundles
no third-party library binaries. The source tree includes the optional MCP
HTTP header dependency documented below.

## System requirements

Current acceptance applies to Linux on the architecture named in the build
report. The CLI uses the C++ runtime, GMP/GMPXX, and OpenSSL Crypto as system
libraries. A different machine needs compatible libraries; the archive is not
a statically linked universal package.

Building requires a C++20 compiler, CMake >=3.20, Ninja or another suitable
CMake generator, pkg-config, and development files for GMP/GMPXX and OpenSSL.
Tests and installation verification require Python 3; the archive is created
with CPack/TGZ.

## License status

Christopher Flegel holds the rights to BRUCE source code. The source code is
licensed under the [BRUCE Proprietary License](LICENSE). No rights to use,
copy, distribute, or sell are granted; in particular, sale and resale require
prior written agreement with Christopher Flegel. Notices in [NOTICE](NOTICE)
and the dependency licenses must be observed for every redistribution.

GMP/GMPXX, OpenSSL, compilers, and the C++ runtime are each governed by their
own licenses. [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) records the
runtime libraries identified during the acceptance build, their provenance,
and the associated primary evidence. For the optional MCP mod,
`cpp-httplib` v0.58.0 is pinned as a vendored header-only HTTP component under
`third_party/cpp-httplib/`; its exact file checksums and MIT notice are also
recorded there. The license and copyright files of the actually installed
system packages remain authoritative. Before public distribution, separately
check the library versions in use and their license obligations. Build library
versions are in `share/bruce/build-info.json`.

## Identity and limits

`bruce --version` returns the CMake project version. `source-files.sha256`
lists input files and its SHA-256 is in the build report. Binary and archive
checksums are in the separately generated installation verification report.
These data are integrity/identity evidence, not signatures or proof of
bit-identical builds.

## Vendored MCP HTTP header

**IMPLEMENTIERT UND BELEGT:** the static MCP boundary uses unchanged local
files from [official cpp-httplib tag v0.58.0](https://github.com/yhirose/cpp-httplib/tree/v0.58.0),
release commit `4f3f9ef19be83ae97a5d9a059432dc00e445b7ab`, under the MIT license.
## MCP Inspector

**IMPLEMENTIERT UND BELEGT:** the optional read-only Phase-DAG V2 and Phase-DAG V3
inspector is built with `BRUCE_BUILD_MOD_MCP=ON` and requires
`BRUCE_BUILD_MOD_NANOCELL=ON`. It binds only `127.0.0.1`.
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
See the [current command contract](docs/cli/commands.md#mcp-inspector) and
[C++ contracts](tests/mods/mcp/mcp_inspector_tests.cpp).

| Local file | Official source | Locally calculated SHA-256 |
| --- | --- | --- |
| [httplib.h](third_party/cpp-httplib/httplib.h) | [tag header](https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.58.0/httplib.h) | `aa14e7e7bd2703694e0a6b6855af3b8c406102ab1fc56ac905fe33619b31faa5` |
| [LICENSE](third_party/cpp-httplib/LICENSE) | [tag MIT license](https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.58.0/LICENSE) | `4b45cbe16d7b71b89ae6127e26e0d90a029198ca5e958ad8e3d0b8bbed364d8b` |

Configuration and builds use only these local files; no FetchContent, package
installation, or network retrieval is performed.

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
