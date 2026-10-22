#!/bin/sh
# Optional user-local Debian dependencies; changes affect this child process only.
set -eu
if [ "$#" -eq 0 ]; then echo "Usage: with-host-deps.sh command [args...]" >&2; exit 2; fi
project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
deps_root="$project_root/build-host-deps/root"
case "$deps_root" in
  *:*|*[[:space:]]*) echo "Unsupported dependency path: $deps_root contains ':' or whitespace; use a native safe-path checkout or a temporary copy without either" >&2; exit 2 ;;
esac
triplet=$(g++ -print-multiarch)
test -x "$deps_root/usr/bin/pkgconf" || { echo "Missing local packages in $deps_root" >&2; exit 2; }
export CPLUS_INCLUDE_PATH="$deps_root/usr/include/$triplet${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}"
export PATH="$deps_root/usr/bin:$PATH"
export LD_LIBRARY_PATH="$deps_root/usr/lib/$triplet${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PKG_CONFIG_SYSROOT_DIR="$deps_root"
# Prefer the bundled package descriptions without masking a host pkgconf that
# cannot honour PKG_CONFIG_LIBDIR through a GVFS mount.
unset PKG_CONFIG_LIBDIR
export PKG_CONFIG_PATH="$deps_root/usr/lib/$triplet/pkgconfig:$deps_root/usr/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
exec "$@"
