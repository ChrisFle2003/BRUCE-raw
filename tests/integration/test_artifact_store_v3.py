"""Real executable, real loopback TLS, ephemeral test-only trust roots."""
import hashlib
import json
import os
from pathlib import Path
import socketserver
import ssl
import stat
import subprocess
import sys
import tempfile
import threading
import time
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())
ERROR = b'{"contract":"artifact-store-fetch-v3","error":"PROTOCOL_ERROR"}\n'
LIMIT = 32 * 1024 * 1024


class TLSServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = False

    def __init__(self, cert, key, response, stall=None):
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.load_cert_chain(cert, key)
        self.response = response
        self.stall = stall
        self.requests = []
        self.connections = 0
        self.stop = threading.Event()
        super().__init__(("127.0.0.1", 0), TLSHandler)
        self.thread = threading.Thread(target=self.serve_forever, kwargs={"poll_interval": 0.02})
        self.thread.start()

    def close(self):
        self.stop.set()
        self.shutdown()
        self.server_close()
        self.thread.join()


class TLSHandler(socketserver.BaseRequestHandler):
    def handle(self):
        self.server.connections += 1
        self.request.settimeout(40)
        if self.server.stall == "handshake":
            self.server.stop.wait(40)
            return
        try:
            with self.server.context.wrap_socket(self.request, server_side=True) as connection:
                request = bytearray()
                while b"\r\n\r\n" not in request and len(request) < 65536:
                    chunk = connection.recv(4096)
                    if not chunk:
                        return
                    request.extend(chunk)
                self.server.requests.append(bytes(request))
                if self.server.stall == "response":
                    # Keep bytes arriving below the inactivity timeout. Only a
                    # whole-request deadline can stop this response at 30 s.
                    connection.sendall(b"HTTP/1.1 200 OK\r\nContent-Length: 1000\r\n\r\n")
                    for _ in range(40):
                        connection.sendall(b"x")
                        if self.server.stop.wait(1):
                            break
                else:
                    connection.sendall(self.server.response)
        except (OSError, ssl.SSLError):
            # Expected when the client rejects the certificate, headers, body
            # or deadline. Counters above still expose retries/redirects.
            pass


class ArtifactStoreV3WorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.certificates = tempfile.TemporaryDirectory(prefix="bruce-v3-test-ca-")
        cls.addClassCleanup(cls.certificates.cleanup)
        root = Path(cls.certificates.name)
        cls.trusted, cls.wrong_host, cls.untrusted = [root / name for name in ("trusted", "wrong-host", "untrusted")]
        for base, san in ((cls.trusted, "IP:127.0.0.1"), (cls.wrong_host, "DNS:wrong-host.invalid"),
                          (cls.untrusted, "IP:127.0.0.1")):
            subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                            "-subj", "/CN=private-tls-fixture", "-addext", "subjectAltName=" + san,
                            "-keyout", str(base) + ".key", "-out", str(base) + ".pem"],
                           check=True, capture_output=True, timeout=15)
        cls.ca = root / "trust.pem"
        cls.ca.write_bytes(Path(str(cls.trusted) + ".pem").read_bytes() +
                           Path(str(cls.wrong_host) + ".pem").read_bytes())
        cls.empty_ca_dir = root / "empty-ca-dir"
        cls.empty_ca_dir.mkdir()

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="bruce-v3-private-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.store = self.root / "private-store"
        self.config = self.root / "private-providers.json"
        self.payload = b"PRIVATE-PAYLOAD-v3\x00\xff"
        self.sha = hashlib.sha256(self.payload).hexdigest()
        self.env = os.environ.copy()
        self.env.update(SSL_CERT_FILE=str(self.ca), SSL_CERT_DIR=str(self.empty_ca_dir))
        self.server = None

    @staticmethod
    def response(payload, status=200, headers=b""):
        return (f"HTTP/1.1 {status} Fixture\r\nContent-Length: {len(payload)}\r\n".encode() +
                headers + b"Connection: close\r\n\r\n" + payload)

    def serve(self, response=None, cert=None, stall=None):
        if self.server:
            self.server.close()
        base = cert or self.trusted
        self.server = TLSServer(str(base) + ".pem", str(base) + ".key",
                                self.response(self.payload) if response is None else response, stall)
        server = self.server
        # Cleanup handles only a server still active at test end.
        self.addCleanup(lambda: server.close() if self.server is server else None)
        self.origin = f"https://127.0.0.1:{server.server_address[1]}/private-prefix"
        self.write_config(self.origin)
        return server

    def write_config(self, origin):
        self.config.write_text(json.dumps({"contract": "bruce-artifact-store-providers-v3",
            "providers": [{"id": "https-a", "origin": origin}]}, separators=(",", ":")) + "\n")
        self.config.chmod(0o600)

    def run_cli(self, *args, timeout=40):
        return subprocess.run([BIN, *map(str, args)], capture_output=True, timeout=timeout, env=self.env)

    def args(self, sha=None, size=None, provider="https-a"):
        return ["artifact-store-v3", "fetch", self.sha if sha is None else sha,
                str(len(self.payload) if size is None else size), "--store", str(self.store),
                "--provider", provider, "--providers", str(self.config)]

    def fetch(self, **kwargs):
        return self.run_cli(*self.args(**kwargs))

    def assert_error(self, result):
        self.assertEqual((result.returncode, result.stdout, result.stderr), (2, b"", ERROR))

    def expected(self, sha, size):
        return ('{"contract":"artifact-store-fetch-v3","action":"fetch","status":"FETCHED",'
                '"provider":"https-a","sha256":"%s","size_bytes":%d}\n' % (sha, size)).encode()

    @staticmethod
    def snapshot(root):
        if not root.exists():
            return None
        result = []
        for path in [root, *sorted(root.rglob("*"))]:
            info = path.lstat()
            regular = stat.S_ISREG(info.st_mode)
            result.append((str(path.relative_to(root)), info.st_mode, info.st_ino, info.st_nlink,
                           info.st_size if regular else None, info.st_mtime_ns if regular else None,
                           hashlib.sha256(path.read_bytes()).hexdigest() if regular else None))
        return result

    def test_closed_grammar_rejects_before_file_or_network_access(self):
        os.mkfifo(self.config, 0o600)
        valid = self.args()
        malformed = [["artifact-store-v3"], ["artifact-store-v3", "--help"]]
        malformed += [valid[:count] for count in range(2, len(valid))]
        for index, values in {
            1: ("import", "resolve", "publish", "delete"),
            2: ("A" * 64, "0" * 63, "../digest"),
            3: ("-1", "+1", "01", "1.0", "33554433", "18446744073709551616"),
            4: ("--provider", "--url"), 5: ("", "--option"),
            6: ("--root", "--store"), 7: ("../mirror", "https://mirror", "", "-a", "a b"),
            8: ("--provider", "--url"), 9: ("", "--option"),
        }.items():
            for value in values:
                args = valid.copy()
                args[index] = value
                malformed.append(args)
        malformed += [valid + ["extra"], valid + ["--provider", "https-b"],
                      valid[:4] + ["--provider", "https-a", "--store", str(self.store),
                                   "--providers", str(self.config)]]
        for args in malformed:
            with self.subTest(args=args):
                self.assert_error(self.run_cli(*args, timeout=3))
                self.assertFalse(self.store.exists())

    def test_trusted_success_exact_route_private_output_and_idempotence(self):
        server = self.serve()
        result = self.fetch()
        self.assertEqual((result.returncode, result.stdout, result.stderr),
                         (0, self.expected(self.sha, len(self.payload)), b""))
        published = self.store / "objects" / self.sha[:2] / (self.sha + ".blob")
        self.assertEqual(published.read_bytes(), self.payload)
        before = self.snapshot(self.store)
        repeat = self.fetch()
        self.assertEqual((repeat.returncode, repeat.stdout, repeat.stderr), (0, result.stdout, b""))
        self.assertEqual(self.snapshot(self.store), before)
        self.assertEqual(server.connections, 2)
        self.assertEqual(len(server.requests), 2)
        for request in server.requests:
            self.assertEqual(request.split(b"\r\n")[0],
                f"GET /private-prefix/objects/{self.sha[:2]}/{self.sha}.blob HTTP/1.1".encode())
            for forbidden in (b"authorization:", b"cookie:", b"proxy-authorization:"):
                self.assertNotIn(forbidden, request.lower())
        for secret in (self.payload, str(self.root).encode(), self.origin.encode(), b"127.0.0.1"):
            self.assertNotIn(secret, result.stdout + result.stderr)

    def test_empty_and_maximum_objects(self):
        for payload in (b"", b"x" * LIMIT):
            with self.subTest(size=len(payload)):
                self.serve(self.response(payload))
                sha = hashlib.sha256(payload).hexdigest()
                result = self.fetch(sha=sha, size=len(payload))
                self.assertEqual((result.returncode, result.stdout, result.stderr),
                                 (0, self.expected(sha, len(payload)), b""))
                self.assertEqual((self.store / "objects" / sha[:2] / (sha + ".blob")).read_bytes(), payload)

    def test_untrusted_and_wrong_hostname_fail_before_http(self):
        for cert in (self.untrusted, self.wrong_host):
            with self.subTest(cert=cert.name):
                server = self.serve(cert=cert)
                self.assert_error(self.fetch())
                self.assertEqual(server.connections, 1)
                self.assertEqual(server.requests, [])
                self.assertFalse(self.store.exists())

    def test_non200_redirect_and_peer_close_never_retry(self):
        redirect = self.serve()
        target_origin = self.origin
        # Keep a distinct target alive to prove redirects are not followed.
        self.server = None
        self.addCleanup(redirect.close)
        for response in (self.response(self.payload, status=404),
                         self.response(b"", status=302, headers=f"Location: {target_origin}/secret\r\n".encode()),
                         self.response(b"", status=307, headers=b"Location: http://127.0.0.1/secret\r\n"),
                         self.response(b"", status=204), b""):
            with self.subTest(response=response[:30]):
                server = self.serve(response)
                self.assert_error(self.fetch())
                self.assertEqual(server.connections, 1)
                self.assertEqual(len(server.requests), 1)
                self.assertEqual(redirect.connections, 0)
                self.assertFalse(self.store.exists())

    def test_headers_framing_digest_and_size_failures_preserve_store(self):
        source = self.root / "unrelated"
        source.write_bytes(b"unrelated existing object")
        source.chmod(0o600)
        imported = self.run_cli("artifact-store-v1", "import", source, "--store", self.store)
        self.assertEqual(imported.returncode, 0, imported.stderr)
        before = self.snapshot(self.store)
        cases = [
            (self.response(b"X" + self.payload[1:]), len(self.payload)),
            (self.response(self.payload), len(self.payload) - 1),
            (self.response(self.payload), len(self.payload) + 1),
            (b"HTTP/1.1 200 OK\r\nContent-Length: 33554433\r\n\r\n", len(self.payload)),
            (b"HTTP/1.1 200 OK\r\nContent-Length: nope\r\n\r\n", len(self.payload)),
            # Carry a valid payload so ignoring metadata bounds would succeed.
            (self.response(self.payload, headers=b"X-Private: " + b"p" * 40000 + b"\r\n"), len(self.payload)),
            (self.response(self.payload, headers=b"X-Private: p\r\n" * 110), len(self.payload)),
            (self.response(self.payload, headers=f"Content-Length: {len(self.payload)}\r\n".encode()), len(self.payload)),
            (b"HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\n" + self.payload, len(self.payload)),
            # All expected bytes arrive, but terminal chunk framing does not.
            (b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n" +
             f"{len(self.payload):x}\r\n".encode() + self.payload + b"\r\n", len(self.payload)),
            (b"HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n" + self.payload + b"extra", len(self.payload)),
        ]
        for response, size in cases:
            with self.subTest(prefix=response[:50], size=size):
                server = self.serve(response)
                self.assert_error(self.fetch(size=size))
                self.assertEqual(self.snapshot(self.store), before)
                self.assertEqual(server.connections, 1)
                self.assertEqual(len(server.requests), 1)

    def test_invalid_allowlist_never_connects_or_creates_store(self):
        server = self.serve()
        canonical = self.config.read_bytes()
        self.assert_error(self.fetch(provider="unknown"))
        for content in (canonical.rstrip(b"\n"), canonical + b"\n", b"{}\n",
                        canonical.replace(b'"origin":', b'"extra":0,"origin":'),
                        canonical.replace(b"https://", b"http://"),
                        canonical.replace(b"/private-prefix", b"/../private"),
                        canonical.replace(b"/private-prefix", b"/private?secret"),
                        canonical.replace(b"127.0.0.1", b"user:secret@127.0.0.1")):
            self.config.write_bytes(content)
            self.assert_error(self.fetch())
        self.config.write_bytes(canonical)
        self.config.chmod(0o644)
        self.assert_error(self.fetch())
        self.config.chmod(0o600)
        target = self.root / "config-real"
        self.config.rename(target)
        self.config.symlink_to(target)
        self.assert_error(self.fetch())
        self.assertEqual(server.connections, 0)
        self.assertFalse(self.store.exists())

    def test_connect_and_whole_request_deadlines(self):
        for stage, minimum, maximum in (("handshake", 4.5, 9), ("response", 29, 35)):
            with self.subTest(stage=stage):
                server = self.serve(stall=stage)
                start = time.monotonic()
                self.assert_error(self.fetch(size=1000))
                elapsed = time.monotonic() - start
                self.assertGreaterEqual(elapsed, minimum)
                self.assertLess(elapsed, maximum)
                self.assertEqual(server.connections, 1)
                self.assertEqual(len(server.requests), 0 if stage == "handshake" else 1)
                self.assertFalse(self.store.exists())

    def test_v14_consumes_fetched_bytes_with_server_offline(self):
        server = self.serve()
        self.assertEqual(self.fetch().returncode, 0)
        server.close()
        self.server = None
        manifest = self.store / "manifests" / "fetched.manifest"
        manifest.write_text("BRUCE_ARTIFACT_MANIFEST_V1\nrun_id=run-1\nnode_id=node-1\n"
                            "attempt_id=attempt-1\nstatus=READY\nresult_code=OK\nobject_count=1\n"
                            f"object_0_sha256={self.sha}\nobject_0_size_bytes={len(self.payload)}\n"
                            "object_0_media_type=-\nobject_0_origin=-\n")
        manifest.chmod(0o600)
        resolved = self.run_cli("artifact-store-v1", "resolve", "fetched", "--store", self.store)
        self.assertEqual(resolved.returncode, 0, resolved.stderr)
        self.assertEqual(json.loads(resolved.stdout)["objects"],
                         [{"sha256": self.sha, "size_bytes": len(self.payload), "origin": ""}])
        spec = self.root / "consumer.spec"
        spec.write_bytes(b"BRUCE_DAG_SCHEDULER_SPEC_V3\ndag-id=fetched-consumer\n"
                         b"max-started-nodes=1\nmax-total-receipt-bytes=4096\n\n"
                         b"node=a\ndepends=\ncallback=append-v1\nrequest-hex=21\n")
        sidecar = self.root / "external-inputs.json"
        sidecar.write_text(json.dumps({
            "contract": "bruce-dag-scheduler-v5-external-inputs-v14",
            "bindings": [{"node_id": "a", "inputs": [{"manifest_id": "fetched", "object_index": 0,
                "sha256": self.sha, "size_bytes": len(self.payload), "source_run_id": "run-1",
                "source_node_id": "node-1", "source_attempt_id": "attempt-1"}]}],
        }, separators=(",", ":")))
        sidecar.chmod(0o600)
        bundle = self.root / "consumer-bundle"
        result = self.run_cli("dag-scheduler-v5", "payload", spec, "--output", bundle,
                              "--store", self.store, "--external-inputs", sidecar)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["contract"], "dag-scheduler-v5-payload-v14")
        self.assertEqual(report["external_input_count"], 1)
        self.assertEqual(report["external_input_bytes"], len(self.payload))
        run_root = bundle.with_name(bundle.name + ".run")
        self.assertEqual((run_root / "attempts/a-1/output/result.bin").read_bytes(), self.payload + b"!")
        self.assertEqual(server.connections, 1)
        for secret in (self.payload, str(self.root).encode(), self.origin.encode()):
            self.assertNotIn(secret, result.stdout + result.stderr + resolved.stdout)

    def test_v1_v12_v2_byte_compatibility_before_and_after_v3(self):
        self.serve()
        source = self.root / "legacy-source"
        source.write_bytes(b"abc")
        source.chmod(0o600)
        sha = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        mirror = self.root / "mirror"
        mirror_object = mirror / "objects" / sha[:2] / (sha + ".blob")
        mirror_object.parent.mkdir(parents=True, mode=0o700)
        mirror.chmod(0o700)
        mirror_object.parent.parent.chmod(0o700)
        mirror_object.write_bytes(b"abc")
        mirror_object.chmod(0o600)
        v2_config = self.root / "v2.json"
        v2_config.write_text(json.dumps({"contract": "bruce-artifact-store-providers-v2",
            "providers": [{"id": "mirror-a", "root": str(mirror)}]}, separators=(",", ":")) + "\n")
        v2_config.chmod(0o600)
        for round_number in range(2):
            for action, value, status in (("import", source, "IMPORTED"), ("inspect", sha, "INSPECTED"),
                                          ("verify", sha, "VERIFIED")):
                result = self.run_cli("artifact-store-v1", action, value, "--store", self.store)
                expected = ('{"contract":"artifact-store-v1","action":"%s","status":"%s",'
                            '"sha256":"%s","size_bytes":3}\n' % (action, status, sha)).encode()
                self.assertEqual((result.returncode, result.stdout, result.stderr), (0, expected, b""))
            manifest = self.store / "manifests" / "legacy.manifest"
            manifest.write_text("BRUCE_ARTIFACT_MANIFEST_V1\nrun_id=run-1\nnode_id=node-1\n"
                                "attempt_id=attempt-1\nstatus=READY\nresult_code=OK\nobject_count=1\n"
                                f"object_0_sha256={sha}\nobject_0_size_bytes=3\n"
                                "object_0_media_type=-\nobject_0_origin=-\n")
            manifest.chmod(0o600)
            resolved = self.run_cli("artifact-store-v1", "resolve", "legacy", "--store", self.store)
            expected = ('{"contract":"artifact-store-resolve-v1","action":"resolve","status":"RESOLVED",'
                        '"manifest_id":"legacy","run_id":"run-1","node_id":"node-1",'
                        '"attempt_id":"attempt-1","manifest_status":"READY","result_code":"OK",'
                        '"objects":[{"sha256":"%s","size_bytes":3,"origin":""}]}\n' % sha).encode()
            self.assertEqual((resolved.returncode, resolved.stdout, resolved.stderr), (0, expected, b""))
            v2 = self.run_cli("artifact-store-v2", "fetch", sha, "3", "--store", self.store,
                              "--provider", "mirror-a", "--providers", v2_config)
            expected = ('{"contract":"artifact-store-fetch-v2","action":"fetch","status":"FETCHED",'
                        '"provider":"mirror-a","sha256":"%s","size_bytes":3}\n' % sha).encode()
            self.assertEqual((v2.returncode, v2.stdout, v2.stderr), (0, expected, b""))
            for command, action, contract in (("artifact-store-v1", "verify", "artifact-store-v1"),
                ("artifact-store-v1", "resolve", "artifact-store-resolve-v1"),
                ("artifact-store-v2", "fetch", "artifact-store-fetch-v2")):
                failure = self.run_cli(command, action, "../bad", "--store", self.store)
                self.assertEqual((failure.returncode, failure.stdout, failure.stderr),
                                 (2, b"", ('{"contract":"%s","error":"PROTOCOL_ERROR"}\n' % contract).encode()))
            if round_number == 0:
                self.assertEqual(self.fetch().returncode, 0)


if __name__ == "__main__":
    unittest.main()
