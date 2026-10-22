"""Guard mandatory CI command partitions using only the Python standard library.

This is a source contract, not evidence that GitHub jobs executed. The small
reader deliberately supports the workflow's block/folded run scalars only.
"""

from pathlib import Path
import io
import re
import shlex
import subprocess
import unittest


WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/ci.yml"


def jobs(source):
    matches = list(re.finditer(r"^  ([\w-]+):\n", source.split("\njobs:\n", 1)[1], re.M))
    body = source.split("\njobs:\n", 1)[1]
    return {match[1]: body[match.end():matches[i + 1].start() if i + 1 < len(matches) else len(body)]
            for i, match in enumerate(matches)}


def scripts(job):
    lines = job.splitlines()
    result = []
    for i, line in enumerate(lines):
        match = re.match(r"        run: (.*)", line)
        if not match:
            continue
        value = match[1]
        if value in ("|", ">-"):
            block = []
            for following in lines[i + 1:]:
                if following and not following.startswith("          "):
                    break
                block.append(following[10:])
            value = (" " if value == ">-" else "\n").join(block)
        result.append(value)
    return result


def commands(job, executable):
    found = []
    for script in scripts(job):
        for line in script.replace("\\\n", " ").splitlines():
            tokens = shlex.split(line)
            if tokens and tokens[0] == executable:
                found.append(tokens)
    return found


def option(command, flag):
    return command[command.index(flag) + 1] if flag in command else None


class WorkflowContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = WORKFLOW.read_text()
        cls.jobs = jobs(cls.source)

    def configured_job(self, build_type, sanitized=False):
        for name, job in self.jobs.items():
            for cmd in commands(job, "cmake"):
                if f"-DCMAKE_BUILD_TYPE={build_type}" in cmd and (
                    ("-DBRUCE_ENABLE_ASAN_UBSAN=ON" in cmd) == sanitized
                ):
                    return name, job, option(cmd, "-B")
        self.fail(f"Missing {'sanitized ' if sanitized else ''}{build_type} configure job")

    def assert_ctest(self, cmd):
        self.assertIn("--output-on-failure", cmd)
        self.assertIn("--no-tests=error", cmd)
        # Only the reviewed selectors below may partition execution. Reject
        # index/fixture selectors, dry runs, reruns and retry-until-pass modes.
        valued = {"--test-dir", "--parallel", "-R", "-E", "-L", "-LE"}
        switches = {"--output-on-failure", "--no-tests=error"}
        tokens = iter(cmd[1:])
        seen = set()
        for token in tokens:
            self.assertIn(token, valued | switches, f"Unreviewed CTest option: {token}")
            self.assertNotIn(token, seen, f"Repeated CTest option: {token}")
            seen.add(token)
            if token in valued:
                self.assertIsNotNone(next(tokens, None), f"Missing value for {token}")

    def test_release_keeps_explicit_matrix_and_complete_remaining_suite(self):
        _, job, build = self.configured_job("Release")
        runs = commands(job, "ctest")
        self.assertEqual(len(runs), 2)
        matrix = next(cmd for cmd in runs if "-R" in cmd)
        remaining = next(cmd for cmd in runs if "-E" in cmd)
        for cmd in runs:
            self.assert_ctest(cmd)
            self.assertEqual(option(cmd, "--test-dir"), build)
            self.assertNotIn("-L", cmd)
            self.assertNotIn("-LE", cmd)
        self.assertEqual(option(matrix, "-R"), "^builtin_mod_build_matrix$")
        self.assertEqual(option(remaining, "-E"), "^builtin_mod_build_matrix$")
        self.assertNotIn("-R", remaining)

    def test_debug_runs_full_suite_including_its_debug_matrix(self):
        _, job, build = self.configured_job("Debug")
        builds = [cmd for cmd in commands(job, "cmake") if "--build" in cmd]
        self.assertTrue(builds, "Debug must build before CTest")
        runs = commands(job, "ctest")
        self.assertEqual(len(runs), 2, "Debug must run exactly two disjoint exhaustive partitions")
        remaining = [cmd for cmd in runs if option(cmd, "-LE") == "^static-fixture-dependent$"]
        static = [cmd for cmd in runs if option(cmd, "-L") == "^static-fixture-dependent$"]
        self.assertEqual(len(remaining), 1, "Debug must include matrix/MCP outside the static partition")
        self.assertEqual(len(static), 1)
        for cmd in runs:
            self.assert_ctest(cmd)
            self.assertEqual(option(cmd, "--test-dir"), build)
        self.assertFalse(set(remaining[0]) & {"-R", "-E", "-L"})

    def test_release_verifies_new_local_package_archive(self):
        _, job, build = self.configured_job("Release")
        package = [cmd for cmd in commands(job, "python3") if "scripts/verify_install.py" in cmd]
        self.assertEqual(len(package), 1, "Release package verification missing")
        cmd = package[0]
        self.assertIn("--package", cmd)
        self.assertEqual(option(cmd, "--build-dir"), build)
        self.assertEqual(option(cmd, "--config"), "Release")
        for flag in ("--archive-out", "--report"):
            self.assertTrue((option(cmd, flag) or "").startswith("${RUNNER_TEMP}/"), flag)
        self.assertIn("--fixtures", cmd)
        self.assertIn("--version", cmd)

    def test_sanitizer_job_runs_entire_compatible_partition(self):
        _, job, build = self.configured_job("Debug", sanitized=True)
        runs = commands(job, "ctest")
        self.assertEqual(len(runs), 1)
        cmd = runs[0]
        self.assert_ctest(cmd)
        self.assertEqual(option(cmd, "--test-dir"), build)
        self.assertEqual(option(cmd, "-L"), "^sanitizer-compatible$")
        self.assertFalse(set(cmd) & {"-R", "-E", "-LE"}, "Do not trim the compatible partition")
        for variable in ("ASAN_OPTIONS", "UBSAN_OPTIONS"):
            values = re.findall(rf"^\s+{variable}: (.+)$", job, re.M)
            self.assertEqual(len(values), 1, f"Missing/overridden {variable}")
            settings = dict(setting.split("=", 1) for setting in values[0].strip("'\"").split(":"))
            self.assertEqual(settings.get("halt_on_error"), "1", f"{variable} must fail on errors")

    def test_static_fixture_partition_runs_unsanitized(self):
        _, debug, build = self.configured_job("Debug")
        runs = [cmd for cmd in commands(debug, "ctest") if option(cmd, "-L") == "^static-fixture-dependent$"]
        self.assertEqual(len(runs), 1, "Separate unsanitized static-fixture step missing")
        self.assert_ctest(runs[0])
        self.assertEqual(option(runs[0], "--test-dir"), build)
        self.assertFalse(set(runs[0]) & {"-R", "-E", "-LE"})

    def test_jobs_are_bounded_and_quality_failures_are_fatal(self):
        for name, job in self.jobs.items():
            with self.subTest(job=name):
                timeout = re.search(r"^    timeout-minutes: (\d+)$", job, re.M)
                self.assertIsNotNone(timeout, "Every job needs a timeout")
                self.assertTrue(1 <= int(timeout[1]) <= 60)
                nested = re.search(r"^      CMAKE_BUILD_PARALLEL_LEVEL: ['\"]?(\d+)['\"]?$", job, re.M)
                self.assertIsNotNone(nested, "Nested builds need a bounded default parallel level")
                self.assertTrue(1 <= int(nested[1]) <= 4)
                for cmd in commands(job, "cmake") + commands(job, "ctest"):
                    if "--build" in cmd or (cmd[0] == "ctest" and "-R" not in cmd):
                        self.assertIn("--parallel", cmd)
                        self.assertTrue(1 <= int(option(cmd, "--parallel")) <= 4)
        self.assertNotIn("continue-on-error", self.source)
        # Mandatory jobs and steps run on both workflow triggers. The sole
        # exception is cache statistics, which also run after earlier failure.
        for condition in re.findall(r"^\s+if: (.+)$", self.source, re.M):
            self.assertEqual(condition, "always()", "Mandatory CI lanes cannot be conditional")
        for job in self.jobs.values():
            self.assertNotRegex(job, r"(?m)^    if:", "Jobs must not bypass failure status")
            for step in re.split(r"^      - ", job, flags=re.M)[1:]:
                if re.search(r"(?m)^        if:", step):
                    self.assertEqual(scripts(step), ["ccache --show-stats"],
                                     "Only cache statistics may use an always condition")
        self.assertNotRegex(self.source, r"\|\|\s*(true|:)\b")
        self.assertRegex(self.source, r"(?m)^permissions:\n  contents: read\n")
        for action in re.findall(r"uses: (\S+)", self.source):
            self.assertRegex(action, r"^[\w/-]+@[0-9a-f]{40}$")
        for script in (script for job in self.jobs.values() for script in scripts(job)):
            checked = subprocess.run(["bash", "-n"], input=script, text=True, capture_output=True)
            self.assertEqual(checked.returncode, 0, checked.stderr)

    def test_existing_documentation_cache_and_secret_gates_remain(self):
        release = self.jobs["release-check"]
        self.assertIn("--offline", release)
        self.assertIn("fail: true", release)
        self.assertIn("ccache --show-stats", release)
        self.assertIn("gitleaks/gitleaks-action@", release)
        self.assertIn("docs/cli/verify_examples.py", release)
        self.assertIn("cmp", release)
        self.assertIn("source-index.md examples.json validation.md", release)


class WorkflowMutationTests(unittest.TestCase):
    def assert_rejected(self, source):
        class CandidateWorkflow(WorkflowContract):
            @classmethod
            def setUpClass(cls):
                cls.source = source
                cls.jobs = jobs(source)

        suite = unittest.defaultTestLoader.loadTestsFromTestCase(CandidateWorkflow)
        result = unittest.TextTestRunner(stream=io.StringIO()).run(suite)
        self.assertFalse(result.errors, "Mutation must reach an assertion, not a parser error")
        self.assertTrue(result.failures, "Checker accepted an unsafe workflow mutation")

    def test_rejects_disabled_sanitizer_error_handling_independently(self):
        source = WORKFLOW.read_text()
        for variable in ("ASAN_OPTIONS", "UBSAN_OPTIONS"):
            with self.subTest(variable=variable):
                changed = re.sub(rf"({variable}: )([^\n]*)", lambda match: match[0].replace(
                    "halt_on_error=1", "halt_on_error=0"), source)
                self.assertNotEqual(changed, source)
                self.assert_rejected(changed)

    def test_rejects_skipped_mandatory_jobs_and_steps(self):
        source = WORKFLOW.read_text()
        for name in ("release-check", "debug-check", "sanitizer-check"):
            for condition in ("false", "github.event_name == 'push'"):
                with self.subTest(job=name, condition=condition):
                    marker = f"  {name}:\n"
                    self.assert_rejected(source.replace(marker, marker + f"    if: {condition}\n", 1))
        for marker in ("      - name: Run mandatory built-in Mod matrix\n",
                       "      - name: Run Debug tests outside static fixtures including Debug built-in Mod matrix\n",
                       "      - name: Run complete sanitizer-compatible partition including MCP loopback\n"):
            with self.subTest(step=marker.strip()):
                self.assertIn(marker, source)
                self.assert_rejected(source.replace(marker, marker + "        if: false\n", 1))

    def test_rejects_alternate_ctest_selection_or_nonexecution(self):
        source = WORKFLOW.read_text()
        invocations = list(re.finditer("ctest --test-dir", source))
        self.assertEqual(len(invocations), 5)
        for selector in ("-I 1,1", "--tests-information 1,1", "--tests-information=1,1",
                         "-N", "--show-only=json-v1", "--rerun-failed", "-U",
                         "-FA .*", "-FS .*", "-FC .*", "--fixture-exclude-any .*",
                         "--fixture-exclude-setup .*", "--fixture-exclude-cleanup .*",
                         "--repeat until-pass:3"):
            for index, match in enumerate(invocations):
                with self.subTest(selector=selector, invocation=index):
                    changed = source[:match.start()] + f"ctest {selector} --test-dir" + source[match.end():]
                    self.assert_rejected(changed)

    def test_rejects_unbounded_nested_builds(self):
        source = WORKFLOW.read_text()
        for value in (None, "0", "99"):
            with self.subTest(value=value):
                changed = re.sub(r'^      CMAKE_BUILD_PARALLEL_LEVEL: "2"\n',
                                 "" if value is None else f'      CMAKE_BUILD_PARALLEL_LEVEL: "{value}"\n',
                                 source, flags=re.M)
                self.assertNotEqual(changed, source)
                self.assert_rejected(changed)


if __name__ == "__main__":
    unittest.main(verbosity=2)
