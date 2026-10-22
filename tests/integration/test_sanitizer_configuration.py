"""Exercise generated commands: lost instrumentation or coverage must fail.

Run under with-host-deps.sh where host development packages are unavailable.
No compilation is needed: Ninja's command graph is the observable contract.
"""

import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


STATIC_FIXTURES = {
    "bruce_logic_mod_worker_fixture",
    "bruce_logic_mod_exec_escape_fixture",
    "bruce_logic_mod_chain_worker_fixture",
    "bruce_logic_mod_dag_worker_fixture",
    "bruce_logic_mod_dag_v3_worker_fixture",
    "bruce_logic_mod_dag_v5_worker_fixture",
    "bruce_logic_mod_payload_worker_fixture",
}
CLASSES = {"sanitizer-compatible", "static-fixture-dependent", "sanitizer-excluded"}
# Includes indirect fixture dependencies embedded in host test compile definitions.
STATIC_TESTS = {
    "controller_observability_cli_v1_v6_contracts",
    "installed_logic_mod_run_workflows",
    "local_logic_builder_cli_contracts",
    "logic_mod_artifact_chain_preflight_v1_contracts",
    "logic_mod_artifact_chain_v1_contracts",
    "logic_mod_artifact_chain_v1_workflows",
    "logic_mod_artifact_dag_v2_contracts",
    "logic_mod_artifact_dag_v2_preflight_contracts",
    "logic_mod_artifact_dag_v2_workflows",
    "logic_mod_artifact_dag_v3_contracts",
    "logic_mod_artifact_dag_v3_workflows",
    "logic_mod_artifact_dag_v4_contracts",
    "logic_mod_artifact_dag_v4_workflows",
    "logic_mod_artifact_dag_v5_cli_workflows",
    "logic_mod_artifact_dag_v5_contracts",
    "logic_mod_artifact_dag_v6_cli_workflows",
    "logic_mod_artifact_dag_v6_contracts",
    "logic_mod_lifecycle_workflows",
    "logic_mod_payload_v1_workflows",
    "logic_mod_payload_worker_v1_contracts",
    "logic_mod_plugin_v1_workflows",
    "logic_mod_result_artifact_v1_workflows",
    "logic_mod_result_inspector_v1_workflows",
    "logic_mod_worker_attempt_v6_contracts",
    "logic_mod_worker_v1_contracts",
    "mod_package_inspection_workflows",
    "mod_update_workflows",
    "runtime_orchestrator_v1_cli_workflows",
}
REQUIRED_DYNAMIC = {
    "bruce_core", "bruce_mod_runtime", "bruce_mod_catalog", "bruce",
    "bruce-mod-builder", "bruce-mod-builder-report-fault", "bruce_core_tests",
    "bruce_mod_language", "bruce_mod_nanocell", "bruce_mod_mcp",
    "bruce_phase_dag_nanocell", "bruce_phase_dag_payload_v3_tests",
    "bruce_common_runtime_host_v2_acceptance_fixture",
    "bruce_common_runtime_host_v2_stage3c_fixture",
    "bruce_common_runtime_host_v2_stage3d_fixture",
}


def run(*command):
    result = subprocess.run(command, text=True, capture_output=True, timeout=120)
    assert result.returncode == 0, (command, result.stdout, result.stderr)
    return result.stdout


def inspect(build, enabled):
    cache = (build / "CMakeCache.txt").read_text()
    assert f"BRUCE_ENABLE_ASAN_UBSAN:BOOL={'ON' if enabled else 'OFF'}" in cache, \
        "Missing scoped BRUCE_ENABLE_ASAN_UBSAN option (default OFF)"
    for line in cache.splitlines():
        if re.match(r"CMAKE_(CXX|EXE_LINKER|SHARED_LINKER|MODULE_LINKER)_FLAGS.*=", line):
            assert "-fsanitize" not in line, f"Global instrumentation: {line}"

    commands = json.loads((build / "compile_commands.json").read_text())
    targets = {}
    for entry in commands:
        match = re.search(r"CMakeFiles/([^/]+)\.dir/", entry["command"])
        assert match, entry
        target = match[1]
        targets.setdefault(target, []).append(entry["command"])
        tokens = shlex.split(entry["command"])
        instrumented = enabled and target not in STATIC_FIXTURES
        assert ("-fsanitize=address,undefined" in tokens) == instrumented, \
            f"{target}: incorrect compile instrumentation: {entry['command']}"
        if instrumented:
            assert "-fno-omit-frame-pointer" in tokens, target
    assert REQUIRED_DYNAMIC | STATIC_FIXTURES <= targets.keys(), targets.keys()

    for target in targets:
        graph = run("ninja", "-C", str(build), "-t", "commands", target)
        link = graph.splitlines()[-1]
        # Static archives have no linker invocation; their objects were checked above.
        archive = bool(re.search(r"\bar\b.*\.(a|lib)\b", link))
        if not archive:
            tokens = shlex.split(link)
            assert ("-fsanitize=address,undefined" in tokens) == \
                (enabled and target not in STATIC_FIXTURES), f"{target}: {link}"
        if target in STATIC_FIXTURES:
            assert "-static" in shlex.split(link) and "-s" in shlex.split(link), link

    tests = json.loads(run("ctest", "--test-dir", str(build), "--show-only=json-v1"))["tests"]
    counts = dict.fromkeys(sorted(CLASSES), 0)
    for test in tests:
        properties = {prop["name"]: prop["value"] for prop in test["properties"]}
        labels = properties.get("LABELS", [])
        classified = CLASSES.intersection(labels)
        assert len(classified) == 1, f"{test['name']}: missing/multiple classes: {labels}"
        category = classified.pop()
        assert (category == "static-fixture-dependent") == (test["name"] in STATIC_TESTS), test
        counts[category] += 1
        reasons = [label for label in labels if label.startswith("sanitizer-reason:")]
        if category != "sanitizer-compatible":
            assert len(reasons) == 1 and len(reasons[0]) > 25, test
        command = " ".join(test.get("command", []))
        if any(fixture in command for fixture in STATIC_FIXTURES):
            assert category == "static-fixture-dependent", test
    assert all(counts.values()), counts
    print(f"{'ON' if enabled else 'default OFF'}: {len(targets)} targets, {len(tests)} tests: {counts}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--build-root", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="bruce-sanitizer-config-") as temporary:
        root = args.build_root or Path(temporary)
        for name, enabled in (("default", False), ("sanitized", True)):
            build = root / name
            command = ["cmake", "-G", "Ninja", "-S", str(args.source), "-B", str(build),
                       "-DBUILD_TESTING=ON", "-DCMAKE_BUILD_TYPE=Debug",
                       "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]
            if enabled:
                command.append("-DBRUCE_ENABLE_ASAN_UBSAN=ON")
            run(*command)
            inspect(build, enabled)


if __name__ == "__main__":
    main()
