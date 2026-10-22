import hashlib
import json
import os
import re
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())

# Captured with the pre-Task-2 real executable. Report serialization is checked
# byte-for-byte after replacing only nondeterministic wall time and peak RSS.
LEGACY_REPORT_DIGESTS = {
    13: "cb0ffb4ccfbede41404a239836e99761d5b118e15aa9fa91a0a04846e01fbe03",
    14: "0278d8ee67544578b7f983e6ee92ce2e6299dc8d1a8b7732de357fd64cab2f81",
}

SPEC = b"""BRUCE_DAG_SCHEDULER_SPEC_V3
dag-id=payload-route
max-started-nodes=1
max-total-receipt-bytes=4096

node=a
depends=
callback=literal-v1
request-hex=7061796c6f61642d726f757465
"""


class DagSchedulerV5PayloadWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Test-only interposition corrupts private stream writes or expires
        # deadlines at real reads. Production code has no fault switch/test hook.
        cls.fault_directory = tempfile.TemporaryDirectory(prefix="bruce-stream-fault-")
        cls.addClassCleanup(cls.fault_directory.cleanup)
        source = Path(cls.fault_directory.name) / "fault.c"
        cls.fault_library = source.with_suffix(".so")
        source.write_text(r'''#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
// Advance only the monotonic clock after a selected adopted-stream read.
// No wall-clock sleeps or production test hooks are needed for this regression.
static int expired = 0;
static unsigned adopted_reads = 0;
static void record(const char *event) {
    const char *marker = getenv("BRUCE_TEST_STREAM_MARKER");
    if (!marker) return;
    int fd = open(marker, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd >= 0) {
        ssize_t (*real_write)(int, const void *, size_t) = dlsym(RTLD_NEXT, "write");
        int (*real_close)(int) = dlsym(RTLD_NEXT, "close");
        real_write(fd, event, strlen(event));
        real_close(fd);
    }
}
static int deadline_target(int fd) {
    char descriptor[64], path[4096];
    snprintf(descriptor, sizeof descriptor, "/proc/self/fd/%d", fd);
    ssize_t n = readlink(descriptor, path, sizeof path - 1);
    if (n < 0) return 0;
    path[n] = 0;
    return strstr(path, "/input-1/stream-payload/chunk-") ||
           strstr(path, "/input-1/input.bin");
}
int clock_gettime(clockid_t clock, struct timespec *time) {
    static int (*real_clock_gettime)(clockid_t, struct timespec *);
    if (!real_clock_gettime) real_clock_gettime = dlsym(RTLD_NEXT, "clock_gettime");
    int result = real_clock_gettime(clock, time);
    if (result == 0 && clock == CLOCK_MONOTONIC && expired) time->tv_sec += 3600;
    return result;
}
ssize_t read(int fd, void *bytes, size_t size) {
    static ssize_t (*real_read)(int, void *, size_t);
    if (!real_read) real_read = dlsym(RTLD_NEXT, "read");
    const char *trigger = getenv("BRUCE_TEST_STREAM_EXPIRE_READ");
    int target = trigger && deadline_target(fd);
    if (target && expired) record("late-read\n");
    ssize_t result = real_read(fd, bytes, size);
    if (target && !expired && ++adopted_reads == strtoul(trigger, NULL, 10)) {
        expired = 1;
        record("expired-after-adopted-read\n");
    }
    return result;
}
ssize_t write(int fd, const void *bytes, size_t size) {
    static ssize_t (*real_write)(int, const void *, size_t);
    if (!real_write) real_write = dlsym(RTLD_NEXT, "write");
    const char *fault = getenv("BRUCE_TEST_STREAM_FAULT");
    if (expired && deadline_target(fd)) record("late-write\n");
    char descriptor[64], path[4096];
    snprintf(descriptor, sizeof descriptor, "/proc/self/fd/%d", fd);
    ssize_t n = readlink(descriptor, path, sizeof path - 1);
    if (n >= 0) path[n] = 0;
    if (fault && n >= 0 && size &&
        ((strstr(path, "/input-1/.stream-staging-") &&
          ((strcmp(fault, "seal") == 0 && strstr(path, "/seal.json")) ||
           (strcmp(fault, "digest") == 0 && strstr(path, "/chunk-")))) ||
         (strcmp(fault, "compatibility") == 0 && strstr(path, "/input-1/input.bin")))) {
        char *copy = malloc(size);
        if (!copy) return -1;
        memcpy(copy, bytes, size);
        copy[0] ^= 1;
        ssize_t result = real_write(fd, copy, size);
        free(copy);
        return result;
    }
    return real_write(fd, bytes, size);
}
int close(int fd) {
    static int (*real_close)(int);
    if (!real_close) real_close = dlsym(RTLD_NEXT, "close");
    const char *fault = getenv("BRUCE_TEST_STREAM_FAULT");
    char descriptor[64], path[4096];
    snprintf(descriptor, sizeof descriptor, "/proc/self/fd/%d", fd);
    ssize_t n = readlink(descriptor, path, sizeof path - 1);
    if (n >= 0) path[n] = 0;
    int delay = fault && strcmp(fault, "deadline-close") == 0 && n >= 0 &&
                strstr(path, "/input-1/source.tmp (deleted)");
    int result = real_close(fd);
    if (delay) {
        const char *marker = getenv("BRUCE_TEST_STREAM_MARKER");
        if (marker) {
            int out = open(marker, O_WRONLY | O_CREAT | O_EXCL, 0600);
            if (out >= 0) {
                write(out, "completed-source-close", 22);
                real_close(out);
            }
        }
        struct timespec remaining = {1, 200000000};
        while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {}
    }
    return result;
}
''')
        built = subprocess.run(["cc", "-shared", "-fPIC", str(source), "-o",
                                str(cls.fault_library), "-ldl"], capture_output=True, timeout=30)
        if built.returncode:
            raise RuntimeError(built.stderr.decode())

    def create_store(self, root):
        source = root / "source.bin"
        source.write_bytes(b"unused")
        source.chmod(0o600)
        store = root / "store"
        imported = subprocess.run(
            [BIN, "artifact-store-v1", "import", str(source), "--store", str(store)],
            capture_output=True, timeout=20)
        self.assertEqual(imported.returncode, 0, imported.stderr.decode())
        return store

    def run_payload(self, spec, bundle, store, *options, env=None):
        return subprocess.run(
            [BIN, "dag-scheduler-v5", "payload", str(spec), "--output", str(bundle),
             "--store", str(store), *map(str, options)],
            capture_output=True, timeout=20, env=env)

    def assert_metadata_only(self, root, bundle, result, secrets):
        run_root = bundle.with_name(bundle.name + ".run")
        # Bundle and run-log are metadata artifacts; private payload files are
        # deliberately inspected separately below.
        log_files = list((run_root / "control").glob("*.jsonl"))
        public = result.stdout + result.stderr + bundle.read_bytes()
        public += b"".join(p.read_bytes() for p in log_files)
        for private in [str(root).encode(), b"payload-inputs/", b"stream-payload",
                        b".stream-staging-", b"header.jsonl", b"seal.json", *secrets]:
            self.assertNotIn(private, public)

    def test_stream_two_node_success_and_legacy_report_bytes(self):
        payload = b"private-stream-payload-" * 100
        spec_bytes = (b"BRUCE_DAG_SCHEDULER_SPEC_V3\ndag-id=stream-flow\n"
                      b"max-started-nodes=2\nmax-total-receipt-bytes=16384\n\n"
                      b"node=a\ndepends=\ncallback=literal-v1\nrequest-hex=" +
                      payload.hex().encode() + b"\n\nnode=b\ndepends=a\n"
                      b"callback=append-v1\nrequest-hex=21\n")
        with tempfile.TemporaryDirectory(prefix="bruce-v5-stream-") as directory:
            root = Path(directory)
            spec = root / "flow.spec"
            spec.write_bytes(spec_bytes)
            sidecar = root / "empty-inputs.json"
            sidecar.write_bytes(b'{"contract":"bruce-dag-scheduler-v5-external-inputs-v14","bindings":[]}')
            sidecar.chmod(0o600)
            for version in (13, 14):
                for stream in (False, True):
                    with self.subTest(version=version, stream=stream):
                        case = root / f"case-{version}-{stream}"
                        case.mkdir()
                        store = self.create_store(case)
                        bundle = case / "bundle"
                        options = ["--external-inputs", sidecar] if version == 14 else []
                        if stream:
                            options.append("--stream-v15")
                        result = self.run_payload(spec, bundle, store, *options)
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        report = json.loads(result.stdout)
                        self.assertEqual(report["contract"], f"dag-scheduler-v5-payload-v{version}")
                        self.assertEqual([r["status"] for r in report["receipts"]], ["READY", "READY"])
                        self.assertEqual(bundle.read_bytes(), result.stdout)
                        run_root = bundle.with_name("bundle.run")
                        self.assertEqual((run_root / "attempts/a-2-1/output/result.bin").read_bytes(),
                                         payload + b"!")
                        self.assert_metadata_only(root, bundle, result, [payload, payload.hex().encode()])
                        self.assertFalse(list(run_root.rglob("source.tmp")))
                        self.assertFalse(list(run_root.rglob(".stream-staging-*")))
                        if stream:
                            streams = list((run_root / "payload-inputs").rglob("stream-payload"))
                            self.assertEqual(len(streams), 1)
                            self.assertEqual(streams[0].relative_to(run_root).as_posix(),
                                             "payload-inputs/a-2-1/input-0/stream-payload")
                            self.assertEqual((streams[0] / "chunk-00000000000000000000.bin").read_bytes(), payload)
                            compatibility = streams[0].parent / "input.bin"
                            self.assertEqual(compatibility.read_bytes(), payload)
                            self.assertEqual(compatibility.stat().st_size, len(payload))
                            self.assertEqual(hashlib.sha256(compatibility.read_bytes()).hexdigest(),
                                             report["receipts"][0]["payload_digest"])
                            self.assertEqual(compatibility.stat().st_mode & 0o777, 0o600)
                            for path in [streams[0], *streams[0].iterdir()]:
                                self.assertEqual(path.stat().st_mode & 0o777, 0o700 if path.is_dir() else 0o600)
                        else:
                            self.assertEqual((run_root / "payload-inputs/n1-a-2-1-0.bin").read_bytes(), payload)
                            self.assertFalse(list(run_root.rglob("stream-*")))
                            # Only measured wall time/RSS vary across runs. All
                            # other report bytes match the pre-V1.5 executable.
                            stable = re.sub(rb'"(wall_ms|peak_memory_bytes)":\d+',
                                            rb'"\1":0', result.stdout)
                            self.assertEqual(hashlib.sha256(stable).hexdigest(),
                                             LEGACY_REPORT_DIGESTS[version])

    def test_external_stream_integrity_and_final_deadline_failures_clean_all_inputs(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-stream-failure-") as directory:
            root = Path(directory)
            store = self.create_store(root)
            seed = root / "seed.spec"
            seed.write_bytes(b"BRUCE_DAG_SCHEDULER_SPEC_V3\ndag-id=stream-seed\n"
                             b"max-started-nodes=2\nmax-total-receipt-bytes=8192\n\n"
                             b"node=a\ndepends=\ncallback=literal-v1\nrequest-hex=414141\n\n"
                             b"node=b\ndepends=\ncallback=literal-v1\nrequest-hex=424242\n")
            seeded = self.run_payload(seed, root / "seed-bundle", store)
            self.assertEqual(seeded.returncode, 0, seeded.stderr.decode())
            inputs = []
            for manifest_path in sorted((store / "manifests").glob("*.manifest")):
                manifest = dict(line.split("=", 1) for line in manifest_path.read_text().splitlines()
                                if "=" in line)
                inputs.append({"manifest_id": manifest_path.stem, "object_index": 0,
                               "sha256": manifest["object_0_sha256"],
                               "size_bytes": int(manifest["object_0_size_bytes"]),
                               "source_run_id": manifest["run_id"], "source_node_id": manifest["node_id"],
                               "source_attempt_id": manifest["attempt_id"]})
            sidecar = root / "inputs.json"
            sidecar.write_text(json.dumps({"contract": "bruce-dag-scheduler-v5-external-inputs-v14",
                "bindings": [{"node_id": "a", "inputs": inputs}]}, separators=(",", ":")))
            sidecar.chmod(0o600)
            consumer = root / "consumer.spec"
            consumer.write_bytes(b"BRUCE_DAG_SCHEDULER_SPEC_V3\ndag-id=stream-consumer\n"
                                 b"max-started-nodes=1\nmax-total-receipt-bytes=8192\n\n"
                                 b"node=a\ndepends=\ncallback=join-v1\nrequest-hex=2d\n")
            snapshot = {p.relative_to(store): p.read_bytes() for p in store.rglob("*") if p.is_file()}
            for fault in ("digest", "seal", "compatibility", "deadline-close",
                          "deadline-verify-1", "deadline-verify-3",
                          "deadline-consume-5", "deadline-verify-7"):
                with self.subTest(fault=fault):
                    bundle = root / f"failed-{fault}"
                    marker = root / f"{fault}-marker"
                    env = dict(os.environ, LD_PRELOAD=str(self.fault_library),
                               BRUCE_TEST_STREAM_FAULT=fault, BRUCE_TEST_STREAM_MARKER=str(marker))
                    post_adoption_deadline = fault.startswith(("deadline-verify-", "deadline-consume-"))
                    if post_adoption_deadline:
                        # With the three-byte fixture, reads 1/3/7 are the first
                        # reads in preparation/pre-consumption/post-consumption
                        # verification; read 5 consumes, and read 6 probes EOF.
                        env["BRUCE_TEST_STREAM_EXPIRE_READ"] = fault.rsplit("-", 1)[1]
                    options = ["--max-wall-ms", "1000"] if fault == "deadline-close" else []
                    result = self.run_payload(consumer, bundle, store, "--stream-v15",
                                              "--external-inputs", sidecar, *options, env=env)
                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                    report = json.loads(result.stdout)
                    self.assertEqual(report["result_code"],
                                     "TIMEOUT" if fault == "deadline-close" else
                                     "EXTERNAL_INPUT_TIMEOUT" if post_adoption_deadline else "EXTERNAL_OBJECT_INVALID")
                    if post_adoption_deadline:
                        # Any later chunk read or compatibility write is a bug,
                        # even when a subsequent boundary eventually times out.
                        self.assertEqual(marker.read_bytes(), b"expired-after-adopted-read\n")
                    if fault == "deadline-close":
                        # The unlinked source is closed only after both streams,
                        # compatibility files and refs have been prepared.
                        self.assertEqual(marker.read_bytes(), b"completed-source-close")
                    self.assertEqual(report["attempts"], [])
                    run_root = bundle.with_name(bundle.name + ".run")
                    self.assertFalse((run_root / "attempts").exists())
                    self.assertEqual(list((run_root / "payload-inputs").iterdir()), [])
                    if fault == "deadline-close":
                        self.assertEqual(report["external_input_count"], 0)
                        self.assertEqual(report["external_input_bytes"], 0)
                    self.assert_metadata_only(root, bundle, result, [b"AAA", b"BBB", b"414141", b"424242"])
                    logs = b"".join(p.read_bytes() for p in (run_root / "control").glob("*.jsonl"))
                    self.assertNotIn(b"ATTEMPT_STARTED", logs)
                    self.assertEqual(snapshot, {p.relative_to(store): p.read_bytes()
                                               for p in store.rglob("*") if p.is_file()})
            bundle = root / "external-success"
            result = self.run_payload(consumer, bundle, store, "--stream-v15", "--external-inputs", sidecar)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            streams = list(bundle.with_name(bundle.name + ".run").rglob("stream-payload"))
            self.assertEqual(len(streams), 2)
            self.assertNotEqual(streams[0].stat().st_ino, streams[1].stat().st_ino)
            for stream in streams:
                self.assertEqual((stream.parent / "input.bin").read_bytes(),
                                 (stream / "chunk-00000000000000000000.bin").read_bytes())
            self.assert_metadata_only(root, bundle, result, [b"AAA", b"BBB"])

    def test_stream_mixed_direct_and_external_input_order(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-stream-mixed-") as directory:
            root = Path(directory)
            store = self.create_store(root)
            external = b"external-input"
            seed = root / "seed.spec"
            seed.write_bytes(SPEC.replace(b"7061796c6f61642d726f757465", external.hex().encode()))
            seeded = self.run_payload(seed, root / "seed-bundle", store)
            self.assertEqual(seeded.returncode, 0, seeded.stderr.decode())
            manifest_path = next((store / "manifests").glob("*.manifest"))
            manifest = dict(line.split("=", 1) for line in manifest_path.read_text().splitlines()
                            if "=" in line)
            sidecar = root / "inputs.json"
            sidecar.write_text(json.dumps({
                "contract": "bruce-dag-scheduler-v5-external-inputs-v14",
                "bindings": [{"node_id": "b", "inputs": [{
                    "manifest_id": manifest_path.stem, "object_index": 0,
                    "sha256": manifest["object_0_sha256"], "size_bytes": len(external),
                    "source_run_id": manifest["run_id"], "source_node_id": manifest["node_id"],
                    "source_attempt_id": manifest["attempt_id"],
                }]}],
            }, separators=(",", ":")))
            sidecar.chmod(0o600)
            spec = root / "mixed.spec"
            spec.write_bytes(b"BRUCE_DAG_SCHEDULER_SPEC_V3\ndag-id=stream-mixed\n"
                             b"max-started-nodes=3\nmax-total-receipt-bytes=8192\n\n"
                             b"node=a\ndepends=\ncallback=literal-v1\nrequest-hex=6469726563742d61\n\n"
                             b"node=b\ndepends=a,z\ncallback=join-v1\nrequest-hex=7c\n\n"
                             b"node=z\ndepends=\ncallback=literal-v1\nrequest-hex=6469726563742d7a\n")
            bundle = root / "mixed-bundle"
            result = self.run_payload(spec, bundle, store, "--stream-v15", "--external-inputs", sidecar)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            report = json.loads(result.stdout)
            self.assertEqual(report["contract"], "dag-scheduler-v5-payload-v14")
            consumer = next(r for r in report["receipts"] if r["node_id"] == "b")
            self.assertEqual(consumer["status"], "READY")
            self.assertEqual(consumer["external_input_count"], 1)
            self.assertEqual(consumer["external_input_bytes"], len(external))
            run_root = bundle.with_name(bundle.name + ".run")
            inputs = [b"direct-a", b"direct-z", external]
            self.assertEqual((run_root / "attempts/a-2-1/output/result.bin").read_bytes(), b"|".join(inputs))
            for index, expected in enumerate(inputs):
                private = run_root / "payload-inputs/a-2-1" / f"input-{index}"
                self.assertEqual((private / "input.bin").read_bytes(), expected)
                self.assertEqual((private / "stream-payload/chunk-00000000000000000000.bin").read_bytes(),
                                 expected)
                header = json.loads((private / "stream-payload/header.jsonl").read_bytes())
                self.assertEqual(header["sha256"], hashlib.sha256(expected).hexdigest())
            self.assertFalse(list(run_root.rglob("source.tmp")))
            self.assertFalse(list(run_root.rglob(".stream-staging-*")))
            self.assert_metadata_only(root, bundle, result, inputs)

    def test_explicit_payload_route_uses_existing_store_and_worker(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-payload-") as directory:
            root = Path(directory)
            source = root / "source.bin"
            source.write_bytes(b"store-input")
            source.chmod(0o600)
            store = root / "store"
            imported = subprocess.run(
                [BIN, "artifact-store-v1", "import", str(source), "--store", str(store)],
                capture_output=True, timeout=20,
            )
            self.assertEqual(imported.returncode, 0, imported.stderr.decode())
            spec = root / "payload.spec"
            bundle = root / "payload-bundle"
            spec.write_bytes(SPEC)
            result = subprocess.run(
                [BIN, "dag-scheduler-v5", "payload", str(spec), "--output", str(bundle),
                 "--store", str(store)],
                capture_output=True, timeout=20,
            )
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            report = json.loads(result.stdout)
            self.assertEqual(list(report)[0:2], ["contract", "action"])
            self.assertEqual(report["contract"], "dag-scheduler-v5-payload-v13")
            self.assertEqual(report["action"], "payload")
            self.assertNotIn(str(root).encode(), result.stdout)
            self.assertTrue(bundle.exists())

    def test_external_sidecar_binds_ready_manifest_and_keeps_report_metadata_only(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-payload-external-") as directory:
            root = Path(directory)
            source = root / "source.bin"
            source.write_bytes(b"unused")
            source.chmod(0o600)
            store = root / "store"
            imported = subprocess.run(
                [BIN, "artifact-store-v1", "import", str(source), "--store", str(store)],
                capture_output=True, timeout=20,
            )
            self.assertEqual(imported.returncode, 0, imported.stderr.decode())

            seed_spec = root / "seed.spec"
            seed_spec.write_bytes(b"""BRUCE_DAG_SCHEDULER_SPEC_V3
dag-id=payload-external-seed
max-started-nodes=1
max-total-receipt-bytes=4096

node=a
depends=
callback=literal-v1
request-hex=41
""")
            seed_bundle = root / "seed-bundle"
            seeded = subprocess.run(
                [BIN, "dag-scheduler-v5", "payload", str(seed_spec), "--output", str(seed_bundle),
                 "--store", str(store)], capture_output=True, timeout=20,
            )
            self.assertEqual(seeded.returncode, 0, seeded.stderr.decode())
            manifest_path = next((store / "manifests").glob("*.manifest"))
            manifest = {}
            for line in manifest_path.read_text().splitlines():
                if "=" in line:
                    key, value = line.split("=", 1)
                    manifest[key] = value

            spec = root / "consumer.spec"
            spec.write_bytes(b"""BRUCE_DAG_SCHEDULER_SPEC_V3
dag-id=payload-external-consumer
max-started-nodes=4
max-total-receipt-bytes=4096

node=a
depends=
callback=literal-v1
request-hex=41

node=b
depends=a,c
callback=join-v1
request-hex=2d

node=c
depends=
callback=literal-v1
request-hex=43
""")
            sidecar = root / "external-inputs.json"
            sidecar.write_text(json.dumps({
                "contract": "bruce-dag-scheduler-v5-external-inputs-v14",
                "bindings": [{
                    "node_id": "b",
                    "inputs": [{
                        "manifest_id": manifest_path.stem,
                        "object_index": 0,
                        "sha256": manifest["object_0_sha256"],
                        "size_bytes": int(manifest["object_0_size_bytes"]),
                        "source_run_id": manifest["run_id"],
                        "source_node_id": manifest["node_id"],
                        "source_attempt_id": manifest["attempt_id"],
                    }],
                }],
            }, separators=(",", ":")))
            sidecar.chmod(0o600)
            bundle = root / "consumer-bundle"
            result = subprocess.run(
                [BIN, "dag-scheduler-v5", "payload", str(spec), "--output", str(bundle),
                 "--store", str(store), "--external-inputs", str(sidecar)],
                capture_output=True, timeout=20,
            )
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            report = json.loads(result.stdout)
            self.assertEqual(report["contract"], "dag-scheduler-v5-payload-v14")
            self.assertIsNotNone(report["external_inputs_digest"])
            receipts = {receipt["node_id"]: receipt for receipt in report["receipts"]}
            self.assertEqual(receipts["b"]["external_input_count"], 1)
            self.assertEqual(receipts["b"]["external_input_bytes"], 1)
            outputs = list((bundle.with_name(bundle.name + ".run") / "attempts").glob(
                "a-2-1/output/result.bin"))
            self.assertEqual(len(outputs), 1)
            self.assertEqual(outputs[0].read_bytes(), b"A-C-A")
            self.assertNotIn(str(root).encode(), result.stdout)

    def test_external_sidecar_preflight_and_manifest_errors_are_separated(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-payload-external-errors-") as directory:
            root = Path(directory)
            source = root / "source.bin"
            source.write_bytes(b"unused")
            source.chmod(0o600)
            store = root / "store"
            imported = subprocess.run(
                [BIN, "artifact-store-v1", "import", str(source), "--store", str(store)],
                capture_output=True, timeout=20,
            )
            self.assertEqual(imported.returncode, 0, imported.stderr.decode())
            spec = root / "payload.spec"
            spec.write_bytes(b"""BRUCE_DAG_SCHEDULER_SPEC_V3
dag-id=payload-external-errors
max-started-nodes=1
max-total-receipt-bytes=4096

node=a
depends=
callback=append-v1
request-hex=41
""")
            malformed = root / "malformed.json"
            malformed.write_text("{\"contract\":\"wrong\"}")
            malformed.chmod(0o600)
            malformed_result = subprocess.run(
                [BIN, "dag-scheduler-v5", "payload", str(spec), "--output", str(root / "bad"),
                 "--store", str(store), "--external-inputs", str(malformed)],
                capture_output=True, timeout=20,
            )
            self.assertEqual(malformed_result.returncode, 2)
            self.assertEqual(malformed_result.stdout, b"")
            sidecar = root / "missing-manifest.json"
            sidecar.write_text(json.dumps({
                "contract": "bruce-dag-scheduler-v5-external-inputs-v14",
                "bindings": [{"node_id": "a", "inputs": [{
                    "manifest_id": "missing-manifest",
                    "object_index": 0,
                    "sha256": "a" * 64,
                    "size_bytes": 1,
                    "source_run_id": "run-source",
                    "source_node_id": "producer",
                    "source_attempt_id": "a-1",
                }]}],
            }, separators=(",", ":")))
            sidecar.chmod(0o600)
            result = subprocess.run(
                [BIN, "dag-scheduler-v5", "payload", str(spec), "--output", str(root / "failed"),
                 "--store", str(store), "--external-inputs", str(sidecar)],
                capture_output=True, timeout=20,
            )
            self.assertEqual(result.returncode, 1, result.stderr.decode())
            report = json.loads(result.stdout)
            self.assertEqual(report["contract"], "dag-scheduler-v5-payload-v14")
            self.assertEqual(report["result_code"], "EXTERNAL_MANIFEST_INVALID")

    def test_payload_route_publishes_each_ready_node_before_direct_successor(self):
        spec_bytes = b"""BRUCE_DAG_SCHEDULER_SPEC_V3
dag-id=payload-state
max-started-nodes=8
max-total-receipt-bytes=4096

node=a
depends=
callback=literal-v1
request-hex=41

node=b
depends=
callback=literal-v1
request-hex=42

node=c
depends=a,b
callback=join-v1
request-hex=2d
"""
        with tempfile.TemporaryDirectory(prefix="bruce-v5-payload-state-") as directory:
            root = Path(directory)
            source = root / "source.bin"
            source.write_bytes(b"unused")
            source.chmod(0o600)
            store = root / "store"
            imported = subprocess.run(
                [BIN, "artifact-store-v1", "import", str(source), "--store", str(store)],
                capture_output=True, timeout=20,
            )
            self.assertEqual(imported.returncode, 0, imported.stderr.decode())
            spec = root / "payload.spec"
            bundle = root / "payload-bundle"
            spec.write_bytes(spec_bytes)
            result = subprocess.run(
                [BIN, "dag-scheduler-v5", "payload", str(spec), "--output", str(bundle),
                 "--store", str(store), "--workers", "2"],
                capture_output=True, timeout=20,
            )
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            report = json.loads(result.stdout)
            self.assertEqual(report["status"], "READY")
            self.assertEqual([receipt["status"] for receipt in report["receipts"]],
                             ["READY", "READY", "READY"])
            self.assertEqual(len(list((store / "manifests").glob("*.manifest"))), 3)
            self.assertEqual((bundle.with_name(bundle.name + ".run") /
                              "attempts" / "a-3-1" / "output" / "result.bin").read_bytes(), b"A-B")
            for manifest in store.joinpath("manifests").glob("*.manifest"):
                resolved = subprocess.run(
                    [BIN, "artifact-store-v1", "resolve", manifest.stem, "--store", str(store)],
                    capture_output=True, timeout=20,
                )
                self.assertEqual(resolved.returncode, 0, resolved.stderr.decode())
                self.assertEqual(json.loads(resolved.stdout)["manifest_status"], "READY")

    def test_payload_input_budget_fails_node_and_blocks_transitive_successors(self):
        spec_bytes = b"""BRUCE_DAG_SCHEDULER_SPEC_V3
dag-id=payload-blocking
max-started-nodes=8
max-total-receipt-bytes=4096

node=a
depends=
callback=literal-v1
request-hex=41

node=b
depends=a
callback=append-v1
request-hex=42

node=c
depends=b
callback=append-v1
request-hex=43
"""
        with tempfile.TemporaryDirectory(prefix="bruce-v5-payload-blocked-") as directory:
            root = Path(directory)
            source = root / "source.bin"
            source.write_bytes(b"unused")
            source.chmod(0o600)
            store = root / "store"
            imported = subprocess.run(
                [BIN, "artifact-store-v1", "import", str(source), "--store", str(store)],
                capture_output=True, timeout=20,
            )
            self.assertEqual(imported.returncode, 0, imported.stderr.decode())
            spec = root / "payload.spec"
            bundle = root / "payload-bundle"
            spec.write_bytes(spec_bytes)
            result = subprocess.run(
                [BIN, "dag-scheduler-v5", "payload", str(spec), "--output", str(bundle),
                 "--store", str(store), "--max-input-bytes", "1"],
                capture_output=True, timeout=20,
            )
            self.assertEqual(result.returncode, 1, result.stderr.decode())
            report = json.loads(result.stdout)
            receipts = {receipt["node_id"]: receipt for receipt in report["receipts"]}
            self.assertEqual(receipts["a"]["status"], "READY")
            self.assertEqual(receipts["b"]["status"], "FAILED")
            self.assertEqual(receipts["b"]["result_code"], "INPUT_LIMIT_EXCEEDED")
            self.assertEqual(receipts["c"]["status"], "BLOCKED")
            self.assertEqual(len(list((store / "manifests").glob("*.manifest"))), 1)

    def test_payload_wall_budget_covers_worker_attempt(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-payload-wall-") as directory:
            root = Path(directory)
            source = root / "source.bin"
            source.write_bytes(b"unused")
            source.chmod(0o600)
            store = root / "store"
            imported = subprocess.run(
                [BIN, "artifact-store-v1", "import", str(source), "--store", str(store)],
                capture_output=True, timeout=20,
            )
            self.assertEqual(imported.returncode, 0, imported.stderr.decode())
            spec = root / "payload.spec"
            spec.write_bytes(SPEC)
            bundle = root / "payload-bundle"
            result = subprocess.run(
                [BIN, "dag-scheduler-v5", "payload", str(spec), "--output", str(bundle),
                 "--store", str(store), "--max-wall-ms", "1"],
                capture_output=True, timeout=20,
            )
            self.assertEqual(result.returncode, 1, result.stderr.decode())
            report = json.loads(result.stdout)
            self.assertEqual(report["status"], "FAILED")
            self.assertEqual(report["failure_code"], "TIMEOUT")
            self.assertFalse(list((store / "manifests").glob("*.manifest")))

    def test_payload_route_requires_existing_store_and_rejects_retry_misplacement(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-payload-") as directory:
            root = Path(directory)
            spec = root / "payload.spec"
            spec.write_bytes(SPEC)
            bundle = root / "payload-bundle"
            missing = subprocess.run(
                [BIN, "dag-scheduler-v5", "payload", str(spec), "--output", str(bundle),
                 "--store", str(root / "missing")], capture_output=True, timeout=20)
            self.assertEqual(missing.returncode, 2)
            self.assertEqual(missing.stdout, b"")
            self.assertNotIn(str(root).encode(), missing.stderr)
            malformed = subprocess.run(
                [BIN, "dag-scheduler-v5", "payload", str(spec), "--output", str(bundle),
                 "--store", str(root / "missing"), "retry", "2"],
                capture_output=True, timeout=20)
            self.assertEqual(malformed.returncode, 2)
            self.assertEqual(malformed.stdout, b"")

    def test_flow_v2_writes_lineage_sidecar_after_legacy_report(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-payload-flow-v2-") as directory:
            root = Path(directory)
            store = self.create_store(root)
            spec = root / "flow.spec"
            spec.write_bytes(b"""BRUCE_DAG_SCHEDULER_SPEC_V3
dag-id=payload-flow-v2
max-started-nodes=4
max-total-receipt-bytes=4096

node=a
depends=
callback=literal-v1
request-hex=41

node=b
depends=a
callback=append-v1
request-hex=42
""")
            bundle = root / "flow-bundle"
            result = self.run_payload(spec, bundle, store, "--flow-v2")
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            report = json.loads(result.stdout)
            self.assertEqual(report["contract"], "dag-scheduler-v5-payload-v13")
            self.assertEqual(bundle.read_bytes(), result.stdout)
            sidecar_path = Path(str(bundle) + ".flow-v2.json")
            self.assertTrue(sidecar_path.exists())
            sidecar = json.loads(sidecar_path.read_bytes())
            self.assertEqual(list(sidecar), ["contract", "run_id", "spec_digest",
                                             "report_digest", "stream_v15", "nodes", "attempts"])
            self.assertEqual(sidecar["contract"], "bruce-payload-flow-v2")
            self.assertFalse(sidecar["stream_v15"])
            self.assertEqual(sidecar["report_digest"], hashlib.sha256(bundle.read_bytes()).hexdigest())
            self.assertEqual([node["node_id"] for node in sidecar["nodes"]], ["a", "b"])
            self.assertEqual(sidecar["nodes"][1]["dependencies"], ["a"])
            self.assertEqual(len(sidecar["attempts"]), 2)
            self.assertEqual(sidecar["attempts"][0]["inputs"], [])
            binding = sidecar["attempts"][1]["inputs"][0]
            self.assertEqual(binding["input_index"], 0)
            self.assertEqual(binding["producer_node_id"], "a")
            self.assertEqual(binding["producer_attempt_id"], "a-1-1")
            self.assertEqual(binding["object_index"], 0)
            self.assertEqual(binding["size_bytes"], 1)
            self.assertEqual(binding["sha256"], hashlib.sha256(b"A").hexdigest())
            self.assertTrue((store / "manifests" / (binding["manifest_id"] + ".manifest")).exists())
            self.assertEqual(sidecar_path.stat().st_mode & 0o777, 0o600)

    def test_flow_v2_rejects_external_inputs_before_any_files_are_touched(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-payload-flow-v2-conflict-") as directory:
            root = Path(directory)
            spec = root / "missing.spec"
            bundle = root / "bundle"
            store = root / "missing-store"
            external = root / "missing-inputs.json"
            result = self.run_payload(spec, bundle, store, "--flow-v2",
                                      "--external-inputs", external)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, b"")
            self.assertEqual(result.stderr, b'{"error":"FLOW_V2_EXTERNAL_INPUTS_UNSUPPORTED"}\n')
            self.assertFalse(bundle.exists())
            self.assertFalse(Path(str(bundle) + ".run").exists())
            self.assertFalse(Path(str(bundle) + ".flow-v2.json").exists())
            self.assertFalse(store.exists())


if __name__ == "__main__":
    unittest.main()
