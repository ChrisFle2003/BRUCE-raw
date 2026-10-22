"""Artifact Store V4 uses real CLI processes and private loopback TLS peers."""
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
ERROR = b'{"contract":"artifact-store-mirror-v4","error":"PROTOCOL_ERROR"}\n'


class TLSServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = False

    def __init__(self, cert, key):
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.load_cert_chain(cert, key)
        self.files = {}
        self.requests = []
        self.connections = 0
        self.reply = None
        self.stall = None
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
        try:
            with self.server.context.wrap_socket(self.request, server_side=True) as connection:
                data = bytearray()
                while b"\r\n\r\n" not in data and len(data) < 65536:
                    chunk = connection.recv(4096)
                    if not chunk:
                        return
                    data.extend(chunk)
                header, body = bytes(data).split(b"\r\n\r\n", 1)
                method, path, _ = header.split(b"\r\n", 1)[0].decode().split(" ")
                fields = dict(line.split(b":", 1) for line in header.split(b"\r\n")[1:])
                length = int(fields.get(b"Content-Length", b"0"))
                while len(body) < length:
                    chunk = connection.recv(min(65536, length - len(body)))
                    if not chunk:
                        return
                    body += chunk
                self.server.requests.append((method, path, header, body))
                if self.server.stall:
                    self.server.stop.wait(40)
                    return
                if self.server.reply is not None:
                    connection.sendall(self.server.reply)
                elif method == "PUT":
                    self.server.files[path] = body
                    connection.sendall(response(b"", 201))
                else:
                    connection.sendall(response(self.server.files[path]) if path in self.server.files
                                       else response(b"", 404))
        except (OSError, ssl.SSLError, ValueError):
            pass


def response(body, status=200, headers=b""):
    return (f"HTTP/1.1 {status} Fixture\r\nContent-Length: {len(body)}\r\n".encode() +
            headers + b"Connection: close\r\n\r\n" + body)


class ArtifactStoreV4WorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.certificates = tempfile.TemporaryDirectory(prefix="bruce-v4-test-ca-")
        cls.addClassCleanup(cls.certificates.cleanup)
        root = Path(cls.certificates.name)
        cls.trusted, cls.wrong_host, cls.untrusted = [root / name for name in ("trusted", "wrong", "untrusted")]
        for base, san in ((cls.trusted, "IP:127.0.0.1"), (cls.wrong_host, "DNS:wrong.invalid"),
                          (cls.untrusted, "IP:127.0.0.1")):
            subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                            "-subj", "/CN=private-v4-fixture", "-addext", "subjectAltName=" + san,
                            "-keyout", str(base) + ".key", "-out", str(base) + ".pem"],
                           check=True, capture_output=True, timeout=15)
        cls.ca = root / "trust.pem"
        cls.ca.write_bytes(Path(str(cls.trusted) + ".pem").read_bytes() +
                          Path(str(cls.wrong_host) + ".pem").read_bytes())
        cls.empty_ca_dir = root / "empty-ca-dir"
        cls.empty_ca_dir.mkdir()

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="bruce-v4-private-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "source-store"
        self.target = self.root / "restore-store"
        self.env = os.environ.copy()
        self.env.update(SSL_CERT_FILE=str(self.ca), SSL_CERT_DIR=str(self.empty_ca_dir))
        self.payload = b"PRIVATE-v4-payload\x00\xff"
        self.sha = hashlib.sha256(self.payload).hexdigest()
        self.manifest = ("BRUCE_ARTIFACT_MANIFEST_V1\nrun_id=run\nnode_id=node\n"
                         "attempt_id=attempt\nstatus=READY\nresult_code=OK\nobject_count=1\n"
                         f"object_0_sha256={self.sha}\nobject_0_size_bytes={len(self.payload)}\n"
                         "object_0_media_type=-\nobject_0_origin=-\n").encode()
        source_file = self.root / "payload"
        source_file.write_bytes(self.payload)
        source_file.chmod(0o600)
        result = self.run_cli("artifact-store-v1", "import", source_file, "--store", self.source)
        self.assertEqual(result.returncode, 0, result.stderr)
        manifest_file = self.source / "manifests" / "example.manifest"
        manifest_file.write_bytes(self.manifest)
        manifest_file.chmod(0o600)

    def serve(self, cert=None):
        base = cert or self.trusted
        server = TLSServer(str(base) + ".pem", str(base) + ".key")
        self.addCleanup(server.close)
        self.remote = f"https://127.0.0.1:{server.server_address[1]}/private-prefix"
        return server

    def run_cli(self, *args, timeout=40):
        return subprocess.run([BIN, *map(str, args)], capture_output=True, timeout=timeout, env=self.env)

    def args(self, action="mirror", store=None):
        return ["artifact-store-v4", action, "example", "--store", str(store or self.source),
                "--remote", self.remote]

    def transfer(self, action="mirror", store=None):
        return self.run_cli(*self.args(action, store))

    def assert_error(self, result):
        self.assertEqual((result.returncode, result.stdout, result.stderr), (2, b"", ERROR))

    def expected(self, action):
        return ('{"contract":"artifact-store-mirror-v4","action":"%s","status":"%s",'
                '"manifest_id":"example","manifest_sha256":"%s","manifest_size_bytes":%d,'
                '"object_count":1,"total_object_bytes":%d}\n' %
                (action, "MIRRORED" if action == "mirror" else "RESTORED",
                 hashlib.sha256(self.manifest).hexdigest(), len(self.manifest), len(self.payload))).encode()

    @staticmethod
    def snapshot(root):
        if not root.exists():
            return None
        out = []
        for path in [root, *sorted(root.rglob("*"))]:
            info = path.lstat()
            regular = stat.S_ISREG(info.st_mode)
            out.append((str(path.relative_to(root)), info.st_mode, info.st_ino, info.st_nlink,
                        info.st_mtime_ns if regular else None, path.read_bytes() if regular else None))
        return out

    def test_command_is_recognized_with_private_protocol_error(self):
        result = subprocess.run([BIN, "artifact-store-v4"], capture_output=True, timeout=3)
        self.assertEqual((result.returncode, result.stdout, result.stderr), (2, b"", ERROR))

    # A loose grammar would access the FIFO or open a network connection here.
    def test_closed_grammar_rejects_before_file_or_network_access(self):
        server = self.serve()
        fifo = self.root / "fifo"
        os.mkfifo(fifo, 0o600)
        valid = self.args(store=fifo)
        malformed = [valid[:n] for n in range(1, len(valid))]
        for index, values in {1: ("fetch", "delete", "--help", "MIRROR"),
                              2: ("", "../id", "-id", "a b", "x" * 129),
                              3: ("--remote", "--url"), 4: ("", "--option"),
                              5: ("--store", "--url"),
                              6: ("", "http://127.0.0.1", "https://user:secret@localhost",
                                  "https://127.0.0.1/../private", "https://127.0.0.1/?secret")}.items():
            for value in values:
                args = valid.copy()
                args[index] = value
                malformed.append(args)
        malformed += [valid + ["extra"], valid + ["--remote", self.remote],
                      valid[:3] + ["--remote", self.remote, "--store", str(fifo)]]
        for args in malformed:
            with self.subTest(args=args):
                self.assert_error(self.run_cli(*args, timeout=3))
        self.assertEqual(server.connections, 0)

    # Missing dispatch, wrong upload route/body/order or leaking private data fails this.
    def test_real_https_roundtrip_metadata_only_and_immutable_local_data(self):
        server = self.serve()
        before = self.snapshot(self.source)
        mirrored = self.transfer()
        self.assertEqual((mirrored.returncode, mirrored.stdout, mirrored.stderr),
                         (0, self.expected("mirror"), b""))
        self.assertEqual(self.snapshot(self.source), before)
        object_path = f"/private-prefix/objects/{self.sha[:2]}/{self.sha}.blob"
        manifest_path = "/private-prefix/manifests/example.mirror"
        envelope = (f"BRUCE_ARTIFACT_MIRROR_V4\nmanifest_id=example\n"
                    f"manifest_sha256={hashlib.sha256(self.manifest).hexdigest()}\n"
                    f"manifest_size_bytes={len(self.manifest)}\n").encode() + self.manifest
        self.assertEqual(server.files, {object_path: self.payload, manifest_path: envelope})
        restored = self.transfer("restore", self.target)
        self.assertEqual((restored.returncode, restored.stdout, restored.stderr),
                         (0, self.expected("restore"), b""))
        self.assertEqual((self.target / "manifests/example.manifest").read_bytes(), self.manifest)
        self.assertEqual((self.target / "objects" / self.sha[:2] / (self.sha + ".blob")).read_bytes(), self.payload)
        self.assertEqual([(r[0], r[1]) for r in server.requests],
                         [("PUT", object_path), ("PUT", manifest_path), ("GET", manifest_path), ("GET", object_path)])
        for method, _, headers, _ in server.requests:
            for forbidden in (b"authorization:", b"cookie:", b"proxy-authorization:"):
                self.assertNotIn(forbidden, headers.lower())
        for private in (self.payload, str(self.root).encode(), self.remote.encode(), b"127.0.0.1"):
            self.assertNotIn(private, mirrored.stdout + mirrored.stderr + restored.stdout + restored.stderr)
        snapshot = self.snapshot(self.target)
        count = server.connections
        self.assert_error(self.transfer("restore", self.target))
        self.assertEqual(self.snapshot(self.target), snapshot)
        self.assertEqual(server.connections, count)
        resolved = self.run_cli("artifact-store-v1", "resolve", "example", "--store", self.target)
        self.assertEqual(resolved.returncode, 0, resolved.stderr)
        self.assertEqual(json.loads(resolved.stdout)["objects"],
                         [{"sha256": self.sha, "size_bytes": len(self.payload), "origin": ""}])

    # PUT has its own response path; every documented success code must be honored.
    def test_put_accepts_200_201_204_and_bounded_acknowledgement(self):
        server = self.serve()
        before = self.snapshot(self.source)
        for code, body in ((200, b"accepted"), (201, b""), (204, b"")):
            with self.subTest(code=code):
                server.reply = response(body, code)
                result = self.transfer()
                self.assertEqual((result.returncode, result.stdout, result.stderr),
                                 (0, self.expected("mirror"), b""))
                self.assertEqual(self.snapshot(self.source), before)
        self.assertEqual(server.connections, 6)

    def test_put_accepts_204_without_content_length(self):
        server = self.serve()
        # A 204 response ends at the headers without body framing.
        server.reply = b"HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n"
        before = self.snapshot(self.source)
        result = self.transfer()
        self.assertEqual((result.returncode, result.stdout, result.stderr),
                         (0, self.expected("mirror"), b""))
        self.assertEqual(self.snapshot(self.source), before)
        self.assertEqual([(method, path) for method, path, _, _ in server.requests],
                         [("PUT", f"/private-prefix/objects/{self.sha[:2]}/{self.sha}.blob"),
                          ("PUT", "/private-prefix/manifests/example.mirror")])
        self.assertEqual(server.connections, 2)

    # Following a redirect or retrying a failed PUT changes these counters.
    def test_put_failure_redirect_and_malformed_response_preserve_source(self):
        redirect = self.serve()
        destination = self.remote
        server = self.serve()
        before = self.snapshot(self.source)
        replies = [response(b"", 404), response(b"", 500),
                   response(b"", 302, f"Location: {destination}/private\r\n".encode()),
                   response(b"", 307, f"Location: {destination}/private\r\n".encode()),
                   b"", b"NOT HTTP\r\n\r\n",
                   b"HTTP/1.1 201 OK\r\nContent-Length: nope\r\n\r\n",
                   response(b"", 201, b"Content-Length: 0\r\n"),
                   response(b"", 201, b"Content-Encoding: gzip\r\n"),
                   b"HTTP/1.1 201 OK\r\nContent-Length: 5\r\n\r\nx",
                   b"HTTP/1.1 201 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nx\r\n",
                   response(b"x" * 65537, 201),
                   b"HTTP/1.1 204 No Content\r\nContent-Length: 3\r\n\r\n",
                   response(b"", 204, b"Content-Length: 0\r\n"),
                   response(b"", 204, b"Content-Encoding: gzip\r\n"),
                   b"HTTP/1.1 204 No Content\r\nTransfer-Encoding: chunked\r\n\r\n"]
        for reply in replies:
            with self.subTest(reply=reply[:40]):
                server.reply = reply
                count = server.connections
                self.assert_error(self.transfer())
                self.assertEqual(server.connections, count + 1)
                self.assertEqual(self.snapshot(self.source), before)
        self.assertEqual(redirect.connections, 0)

    # Bypassing TLS identity checks would send HTTP and make this succeed.
    def test_untrusted_and_wrong_hostname_reject_put_and_get_before_http(self):
        for cert in (self.untrusted, self.wrong_host):
            with self.subTest(cert=cert.name):
                server = self.serve(cert)
                before = self.snapshot(self.source)
                self.assert_error(self.transfer())
                self.assert_error(self.transfer("restore", self.target))
                self.assertEqual(server.connections, 2)
                self.assertEqual(server.requests, [])
                self.assertFalse(self.target.exists())
                self.assertEqual(self.snapshot(self.source), before)

    # Publishing before all bytes are verified would leave a partial target.
    def test_corrupt_remote_envelope_and_object_never_publish(self):
        server = self.serve()
        self.assertEqual(self.transfer().returncode, 0)
        good = server.files.copy()
        manifest_path = "/private-prefix/manifests/example.mirror"
        object_path = f"/private-prefix/objects/{self.sha[:2]}/{self.sha}.blob"
        changes = [(manifest_path, good[manifest_path].replace(b"manifest_id=example", b"manifest_id=another")),
                   (manifest_path, good[manifest_path][:-1]),
                   (manifest_path, b"invalid envelope\n"),
                   (object_path, b"x" * len(self.payload)), (object_path, self.payload[:-1])]
        for path, data in changes:
            with self.subTest(path=path, data=data[:40]):
                server.files = good.copy()
                server.files[path] = data
                self.assert_error(self.transfer("restore", self.target))
                self.assertFalse(self.target.exists())
        server.files = good
        # A corrupt existing object is preserved, including its inode and bytes.
        existing = self.run_cli("artifact-store-v1", "import", self.root / "payload", "--store", self.target)
        self.assertEqual(existing.returncode, 0, existing.stderr)
        (self.target / "objects" / self.sha[:2] / (self.sha + ".blob")).write_bytes(b"bad")
        before = self.snapshot(self.target)
        self.assert_error(self.transfer("restore", self.target))
        self.assertEqual(self.snapshot(self.target), before)

    # Missing local validation must fail before the first upload.
    def test_invalid_local_ready_artifact_never_uploads(self):
        server = self.serve()
        manifest_file = self.source / "manifests/example.manifest"
        manifest_file.write_bytes(self.manifest.replace(b"status=READY", b"status=FAILED"))
        before = self.snapshot(self.source)
        self.assert_error(self.transfer())
        self.assertEqual(self.snapshot(self.source), before)
        manifest_file.write_bytes(self.manifest)
        (self.source / "objects" / self.sha[:2] / (self.sha + ".blob")).write_bytes(b"bad")
        before = self.snapshot(self.source)
        self.assert_error(self.transfer())
        self.assertEqual(self.snapshot(self.source), before)
        self.assertEqual(server.connections, 0)

    # A GET error must not create a target or follow the supplied destination.
    def test_restore_redirect_and_malformed_response_never_publish(self):
        redirect = self.serve()
        destination = self.remote
        server = self.serve()
        replies = [response(b"", 302, f"Location: {destination}/private\r\n".encode()),
                   response(b"", 404),
                   b"HTTP/1.1 200 OK\r\nContent-Length: nope\r\n\r\n",
                   response(b"", 200, b"Content-Length: 0\r\n"),
                   b"HTTP/1.1 200 OK\r\nContent-Length: 1048577\r\n\r\n"]
        for reply in replies:
            with self.subTest(reply=reply[:40]):
                server.reply = reply
                count = server.connections
                self.assert_error(self.transfer("restore", self.target))
                self.assertEqual(server.connections, count + 1)
                self.assertFalse(self.target.exists())
        self.assertEqual(redirect.connections, 0)

    # PUT must obey the same whole-request deadline as GET.
    def test_put_response_deadline_preserves_source(self):
        server = self.serve()
        server.stall = "response"
        before = self.snapshot(self.source)
        start = time.monotonic()
        self.assert_error(self.transfer())
        elapsed = time.monotonic() - start
        self.assertGreaterEqual(elapsed, 29)
        self.assertLess(elapsed, 35)
        self.assertEqual(server.connections, 1)
        self.assertEqual(len(server.requests), 1)
        self.assertEqual(self.snapshot(self.source), before)


if __name__ == "__main__":
    unittest.main()
