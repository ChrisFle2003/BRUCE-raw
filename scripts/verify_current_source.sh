#!/bin/sh
# Build and verify the current source tree without modifying local build artifacts.
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unsafe_path() {
    case "$1" in
        *:*|*[[:space:]]*) return 0 ;;
        *) return 1 ;;
    esac
}

if unsafe_path "$project_root"; then
    echo "Unsafe project path: use a native path without ':' or whitespace" >&2
    exit 2
fi

if [ "$#" -gt 1 ]; then
    echo "Usage: scripts/verify_current_source.sh [SAFE_BUILD_DIRECTORY]" >&2
    exit 2
fi
if [ "$#" -eq 1 ]; then
    build_dir=$1
    if unsafe_path "$build_dir"; then
        echo "Unsafe build path: use a path without ':' or whitespace" >&2
        exit 2
    fi
    mkdir -p "$build_dir"
else
    build_dir=$(mktemp -d /tmp/bruce-current-verify-XXXXXX)
fi

cleanup_dir=$(mktemp -d /tmp/bruce-cli-docs-XXXXXX)
trap 'rm -rf -- "$cleanup_dir"' EXIT HUP INT TERM

cd "$project_root"
scripts/with-host-deps.sh cmake -G Ninja -S . -B "$build_dir" -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build "$build_dir"
scripts/with-host-deps.sh ctest --test-dir "$build_dir" --output-on-failure

tar -C . \
    --exclude='./build*' \
    --exclude='./.git' \
    --exclude='./.agents' \
    --exclude='./.codex' \
    -cf - . | tar -C "$cleanup_dir" -xf -
python3 "$cleanup_dir/docs/cli/verify_examples.py" "$build_dir/bruce"
for artifact in source-index.md examples.json validation.md; do
    cmp "docs/cli/$artifact" "$cleanup_dir/docs/cli/$artifact"
done
python3 scripts/check_local_doc_links.py
