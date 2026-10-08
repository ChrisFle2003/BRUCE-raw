# BRUCE: local CLI distribution and dependencies

The local package contains the CLI, build information, and a checksum list of
the included build, source, and test files. It contains no C++ SDK and bundles
no third-party library files.

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
and the associated primary evidence. For the optional Cockpit mod,
`cpp-httplib` v0.59.0 is pinned as a vendored header-only HTTP component under
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
