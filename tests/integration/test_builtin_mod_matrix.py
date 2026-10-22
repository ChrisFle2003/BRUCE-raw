import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def run(*command: str, timeout: int = 300) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, capture_output=True, text=True, timeout=timeout)


def bounded_parallel(value: str) -> int:
    try:
        parsed = int(value, 10)
    except ValueError as error:
        raise argparse.ArgumentTypeError("expected an integer from 1 through 16") from error
    if not 1 <= parsed <= 16:
        raise argparse.ArgumentTypeError("expected an integer from 1 through 16")
    return parsed


parser = argparse.ArgumentParser()
parser.add_argument("--cmake", required=True)
parser.add_argument("--ctest", required=True)
parser.add_argument("--source", required=True)
parser.add_argument("--build-root", required=True)
parser.add_argument("--build-type", required=True, choices=("Release", "Debug"))
parser.add_argument("--parallel", type=bounded_parallel, default=2)
parser.add_argument("--compiler-launcher", action="append", default=[])
args = parser.parse_args()

source = Path(args.source).resolve()
build_root = Path(args.build_root).resolve()
help_test = source / "tests/integration/test_builtin_mod_help.py"
configure_profile = [f"-DCMAKE_BUILD_TYPE={args.build_type}"]
if args.compiler_launcher:
    configure_profile.append(
        "-DCMAKE_CXX_COMPILER_LAUNCHER=" + ";".join(args.compiler_launcher)
    )

valid = (
    (False, False, False),
    (True, False, False),
    (False, True, False),
    (True, True, False),
    (False, True, True),
    (True, True, True),
)

feature_tests = (
    ("language_contracts", "language_workflows"),
    ("nanocell_contracts", "nanocell_cli_workflows"),
    ("mcp_inspector_contracts", "mcp_inspector_workflows"),
)
feature_registrations = (
    ("language_contracts", "language_run_contracts", "language_workflows"),
    ("nanocell_contracts", "nanocell_mod_contracts", "phase_dag_nanocell_contracts",
     "nanocell_cli_workflows", "phase_dag_v2_cli_workflows", "phase_dag_v3_cli_workflows"),
    ("mcp_inspector_contracts", "mcp_inspector_workflows"),
)
feature_targets = (
    "bruce_language_tests",
    "bruce_nanocell_tests",
    "bruce_mcp_inspector_tests",
)


def assert_invalid_configuration_rejected(
    name: str,
    configured: subprocess.CompletedProcess[str],
) -> None:
    assert configured.returncode != 0, (name, configured.stdout, configured.stderr)
    assert "McpRequiresNanoCell" in configured.stderr, (
        name, configured.stdout, configured.stderr,
    )


for language, nanocell, mcp in valid:
    name = f"l{int(language)}-n{int(nanocell)}-m{int(mcp)}"
    build = build_root / name
    shutil.rmtree(build, ignore_errors=True)
    configured = run(
        args.cmake,
        "-G", "Ninja",
        "-S", str(source),
        "-B", str(build),
        "-DBUILD_TESTING=ON",
        *configure_profile,
        f"-DBRUCE_BUILD_MOD_LANGUAGE={'ON' if language else 'OFF'}",
        f"-DBRUCE_BUILD_MOD_NANOCELL={'ON' if nanocell else 'OFF'}",
        f"-DBRUCE_BUILD_MOD_MCP={'ON' if mcp else 'OFF'}",
    )
    assert configured.returncode == 0, (name, configured.stdout, configured.stderr)

    listed = run(args.ctest, "--test-dir", str(build), "--show-only=json-v1")
    assert listed.returncode == 0, (name, listed.stdout, listed.stderr)
    registered_tests = {
        test["name"]: test for test in json.loads(listed.stdout)["tests"]
    }
    registered = set(registered_tests)
    assert ("cli_documentation_determinism" in registered) is (language and nanocell and mcp), (name, registered)
    assert "mod_catalog_contracts" in registered, (name, registered)
    assert "builtin_mod_help_workflows" in registered, (name, registered)
    enabled = (language, nanocell, mcp)
    help_command = registered_tests["builtin_mod_help_workflows"]["command"]
    assert help_command[1:3] == [str(help_test), str(build / "bruce")], (name, help_command)
    assert help_command[3:] == [
        "--language", "on" if language else "off",
        "--nanocell", "on" if nanocell else "off",
        "--mcp", "on" if mcp else "off",
    ], (name, help_command)
    for active, tests in zip(enabled, feature_registrations):
        for test in tests:
            assert (test in registered) is active, (name, test, active)

    targets = ["bruce", "bruce_mod_catalog_tests"]
    targets.extend(target for active, target in zip(enabled, feature_targets) if active)
    built = run(
        args.cmake,
        "--build", str(build),
        "--target", *targets,
        "--parallel", str(args.parallel),
        timeout=600,
    )
    assert built.returncode == 0, (name, built.stdout, built.stderr)

    help_result = run(
        args.ctest, "--test-dir", str(build), "--output-on-failure",
        "-R", "^builtin_mod_help_workflows$",
    )
    assert help_result.returncode == 0, (name, help_result.stdout, help_result.stderr)

    catalog = run(str(build / "bruce_mod_catalog_tests"))
    assert catalog.returncode == 0, (name, catalog.stdout, catalog.stderr)

    focused = [test for active, tests in zip(enabled, feature_tests) if active for test in tests]
    if focused:
        pattern = "^(" + "|".join(re.escape(test) for test in focused) + ")$"
        contracts_and_workflows = run(
            args.ctest, "--test-dir", str(build), "--output-on-failure", "-R", pattern,
            timeout=600,
        )
        assert contracts_and_workflows.returncode == 0, (
            name, contracts_and_workflows.stdout, contracts_and_workflows.stderr,
        )
        assert f"100% tests passed, 0 tests failed out of {len(focused)}" in contracts_and_workflows.stdout, (
            name, contracts_and_workflows.stdout, contracts_and_workflows.stderr,
        )
    print(f"{name}: help, catalog, {len(focused)} feature tests passed", flush=True)

for language in (False, True):
    name = f"invalid-l{int(language)}-n0-m1"
    build = build_root / name
    shutil.rmtree(build, ignore_errors=True)
    configured = run(
        args.cmake,
        "-G", "Ninja",
        "-S", str(source),
        "-B", str(build),
        "-DBUILD_TESTING=OFF",
        *configure_profile,
        f"-DBRUCE_BUILD_MOD_LANGUAGE={'ON' if language else 'OFF'}",
        "-DBRUCE_BUILD_MOD_NANOCELL=OFF",
        "-DBRUCE_BUILD_MOD_MCP=ON",
    )
    assert_invalid_configuration_rejected(name, configured)
    print(f"{name}: McpRequiresNanoCell rejected", flush=True)


def ignored_copy_entries(directory: str, entries: list[str]) -> set[str]:
    base = Path(directory)
    return {
        entry for entry in entries
        if entry in {".git", ".agents", ".codex", "runs"}
        or (entry.startswith("build") and (base / entry).is_dir())
    }


# Mutation contract: a disposable full source copy with the MCP dependency guard
# disabled must be rejected by the same invalid-configuration assertion above.
with tempfile.TemporaryDirectory(prefix="bruce-builtin-mod-mutation-") as temporary:
    mutated_source = Path(temporary) / "source"
    shutil.copytree(source, mutated_source, ignore=ignored_copy_entries)
    mutated_cmake = mutated_source / "CMakeLists.txt"
    original = "if(BRUCE_BUILD_MOD_MCP AND NOT BRUCE_BUILD_MOD_NANOCELL)"
    replacement = "if(FALSE) # matrix mutation: dependency guard intentionally disabled"
    cmake_text = mutated_cmake.read_text(encoding="utf-8")
    assert cmake_text.count(original) == 1, "MCP dependency guard changed unexpectedly"
    mutated_cmake.write_text(cmake_text.replace(original, replacement), encoding="utf-8")

    mutation_build = Path(temporary) / "build"
    mutation_result = run(
        args.cmake,
        "-G", "Ninja",
        "-S", str(mutated_source),
        "-B", str(mutation_build),
        "-DBUILD_TESTING=OFF",
        *configure_profile,
        "-DBRUCE_BUILD_MOD_LANGUAGE=OFF",
        "-DBRUCE_BUILD_MOD_NANOCELL=OFF",
        "-DBRUCE_BUILD_MOD_MCP=ON",
    )
    assert mutation_result.returncode == 0, (
        "mutation setup did not disable the dependency guard",
        mutation_result.stdout,
        mutation_result.stderr,
    )
    try:
        assert_invalid_configuration_rejected("mutation-l0-n0-m1", mutation_result)
    except AssertionError:
        print("mutation contract: broken MCP dependency state detected", flush=True)
    else:
        raise AssertionError("matrix mutation was not detected")
