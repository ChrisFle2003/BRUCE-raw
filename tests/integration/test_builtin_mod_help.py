import argparse
import json
from pathlib import Path
import subprocess


def enabled(value: str) -> bool:
    if value not in {"on", "off"}:
        raise argparse.ArgumentTypeError("expected on or off")
    return value == "on"


parser = argparse.ArgumentParser()
parser.add_argument("binary", type=Path)
parser.add_argument("--language", type=enabled, required=True)
parser.add_argument("--nanocell", type=enabled, required=True)
parser.add_argument("--mcp", type=enabled, required=True)
args = parser.parse_args()


def run(*command: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, capture_output=True, text=True, timeout=10)


binary = str(args.binary.resolve())
help_result = run(binary, "--help")
assert help_result.returncode == 0, help_result.stderr

commands = {
    "language": (args.language, "bruce language interpret"),
    "nanocell": (args.nanocell, "bruce nanocell run"),
    "mcp": (args.mcp, "bruce mcp [--port"),
}

for command, (is_enabled, help_fragment) in commands.items():
    assert (help_fragment in help_result.stdout) is is_enabled, (
        command,
        is_enabled,
        help_result.stdout,
    )
    if not is_enabled:
        result = run(binary, command)
        assert result.returncode == 2, (command, result.returncode, result.stdout, result.stderr)
        assert result.stdout == "", (command, result.stdout)
        assert json.loads(result.stderr) == {"error": f"UnknownCommand: {command}"}, result.stderr

catalog_result = run(binary, "mods", "list")
assert catalog_result.returncode == 0, catalog_result.stderr
catalog = json.loads(catalog_result.stdout)
assert catalog["contract"] == "bruce-mod-catalog-v2", catalog
entries = {entry["id"]: entry for entry in catalog["entries"]}
assert list(entries) == ["language", "mcp", "nanocell"], entries
for command, (is_enabled, _) in commands.items():
    assert entries[command]["build_state"] == ("available" if is_enabled else "not_built"), entries[command]
    assert entries[command]["origin"] == "builtin", entries[command]
    assert entries[command]["kind"] == "builtin", entries[command]
    assert entries[command]["lifecycle_state"] is None, entries[command]
    assert entries[command]["package_digest"] is None, entries[command]
