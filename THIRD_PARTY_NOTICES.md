# BRUCE Core: system-library notices

This document records the runtime libraries used during the Linux acceptance
build. BRUCE Core does not distribute their binaries or source code. The target
machine provides them as system libraries.

| Component | Role in BRUCE Core | Version identified during acceptance build | Primary evidence on the build system |
| --- | --- | --- | --- |
| GMP / GMPXX | Exact integer and rational arithmetic | GMPXX 6.3 | `/usr/share/doc/libgmp-dev/copyright` |
| OpenSSL Crypto | Cryptographic checksum and digest functions | OpenSSL 3.5.7 | `/usr/share/doc/libssl-dev/copyright` |

The Debian package evidence identifies the GMP license framework as
`GPL-2+ or LGPL-3+` and OpenSSL as `Apache-2.0`. These data describe the system
packages used. They neither extend nor replace those packages' license files or
the obligation to check the specific type of distribution before publication.

The complete build identity, including the resolved library versions, is also
available in `share/bruce/build-info.json`. This document is not a substitute
for a complete provenance review of future build environments.

## Vendored Cockpit dependency

| Component | Version / source | License | SHA-256 |
| --- | --- | --- | --- |
| `third_party/cpp-httplib/httplib.h` | `yhirose/cpp-httplib`, tag `v0.59.0`, release commit `cf3693c` | MIT | `dc1e4de3e0ef3a18f3a5818fab563eb3fa45d5d3bb1db527a8d389077ed5ee6c` |
| `third_party/cpp-httplib/LICENSE` | same tag | MIT | `4b45cbe16d7b71b89ae6127e26e0d90a029198ca5e958ad8e3d0b8bbed364d8b` |

The files are taken unchanged from the official tag. The Cockpit build may use
them locally, but must not request a network source during configuration or
compilation.
