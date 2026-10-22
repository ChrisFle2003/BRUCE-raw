#!/usr/bin/env bash
# Safe convenience entry point for the explicit local BRUCE assistant.
set -eu

usage() {
    cat <<'EOF'
Usage:
  bruce-local-ai.sh config-check CONFIG [PROFILE]
  bruce-local-ai.sh index-build CONFIG [PROFILE]
  bruce-local-ai.sh ask CONFIG QUESTION [PROFILE]
  bruce-local-ai.sh explain CONFIG RESULT [PROFILE]

CONFIG, RESULT and PROFILE are passed unchanged to tools/bruce_ai.py.
`ask` always preserves the assistant's preview and literal `run` confirmation.
No model download, index overwrite, shell execution or persistent mutation is
provided by this helper.
EOF
}

fail_usage() {
    usage >&2
    exit 64
}

if [ "$#" -eq 1 ] && [ "$1" = "--help" ]; then
    usage
    exit 0
fi

[ "$#" -ge 2 ] || fail_usage

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
assistant="$script_dir/../tools/bruce_ai.py"
operation=$1
config=$2
shift 2

[ -f "$assistant" ] || exit 70
command -v python3 >/dev/null 2>&1 || exit 69

profile_args=()
if [ "$operation" = "ask" ] || [ "$operation" = "explain" ]; then
    [ "$#" -ge 1 ] || fail_usage
    value=$1
    shift
else
    value=""
fi
if [ "$#" -eq 1 ]; then
    [ -n "$1" ] || fail_usage
    profile_args=(--profile "$1")
    shift
fi
[ "$#" -eq 0 ] || fail_usage

case "$operation" in
    config-check)
        exec python3 "$assistant" config check --config "$config" "${profile_args[@]}"
        ;;
    index-build)
        exec python3 "$assistant" index build --config "$config" "${profile_args[@]}"
        ;;
    ask)
        exec python3 "$assistant" ask --config "$config" --question "$value" "${profile_args[@]}"
        ;;
    explain)
        exec python3 "$assistant" explain --config "$config" --input "$value" "${profile_args[@]}"
        ;;
    *)
        fail_usage
        ;;
esac
