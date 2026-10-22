"""V5 exposes host-only remote restore solely on DAG run commands."""
import json
import os
import pathlib
import socketserver
import ssl
import subprocess
import sys
import tempfile
import threading
import time


bruce, exporter, worker, compiler = map(os.path.abspath, sys.argv[1:])


def http_response(body=b"", status=200):
    return (f"HTTP/1.1 {status} Fixture\r\nContent-Length: {len(body)}\r\nConnection: close\r\n\r\n".encode() + body)


class TLSHandler(socketserver.BaseRequestHandler):
    def handle(self):
        try:
            with self.server.context.wrap_socket(self.request, server_side=True) as connection:
                request = bytearray()
                while b"\r\n\r\n" not in request:
                    block = connection.recv(4096)
                    if not block:
                        return
                    request.extend(block)
                header, body = bytes(request).split(b"\r\n\r\n", 1)
                method, path, _ = header.split(b"\r\n", 1)[0].decode().split(" ")
                fields = dict(line.split(b":", 1) for line in header.split(b"\r\n")[1:])
                length = int(fields.get(b"Content-Length", b"0"))
                while len(body) < length:
                    body += connection.recv(length - len(body))
                self.server.requests.append((method, path))
                if method == "PUT":
                    self.server.files[path] = body
                    connection.sendall(http_response(status=201))
                elif self.server.fail_get:
                    connection.sendall(http_response(status=500))
                elif path in self.server.files:
                    connection.sendall(http_response(self.server.files[path]))
                else:
                    connection.sendall(http_response(status=404))
        except (OSError, ssl.SSLError, ValueError):
            pass


class TLSServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, certificate, key):
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.load_cert_chain(certificate, key)
        self.files, self.requests, self.fail_get = {}, [], False
        super().__init__(("127.0.0.1", 0), TLSHandler)
        self.thread = threading.Thread(target=self.serve_forever, daemon=True)
        self.thread.start()

    def close(self):
        self.shutdown(); self.server_close(); self.thread.join()
with tempfile.TemporaryDirectory(prefix="bruce-dag-v5-cli-") as tmp:
    root = pathlib.Path(tmp)
    subprocess.run([exporter, worker, tmp, "export"], check=True)
    probe = root / "probe.cpp"
    probe.write_text(r'''
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>
extern "C" pid_t fork() {
  const auto pid = reinterpret_cast<pid_t (*)()>(::dlsym(RTLD_NEXT,"fork"))();
  if (pid > 0) if (const char* path = std::getenv("BRUCE_TEST_STARTS")) {
    const int fd = ::open(path, O_WRONLY | O_APPEND | O_CREAT, 0600);
    if (fd >= 0) { char line[64]; const int n = std::snprintf(line, sizeof(line), "start %ld\n", static_cast<long>(pid));
      (void)::write(fd, line, n); (void)::close(fd); }
    static unsigned children = 0;
    if (++children == 2 && std::getenv("BRUCE_TEST_PAUSE_SECOND")) (void)::kill(pid, SIGSTOP);
  }
  return pid;
}
''')
    library, starts = root / "probe.so", root / "starts"
    subprocess.run([compiler, "-shared", "-fPIC", str(probe), "-ldl", "-o", str(library)], check=True)
    env = {**os.environ, "LD_PRELOAD": str(library), "BRUCE_TEST_STARTS": str(starts)}
    spec = root / "dag.json"
    store, record = root / "store", root / "dag.record"
    spec.write_text(json.dumps({"schema_version": 2, "nodes": [{
        "id": "a", "package": str(root / "output.brucemod"), "trust_store": str(root / "trust.txt"),
        "input": "3/4 hoch 2", "predecessors": [], "output_manifest": "a-out"}]}))

    def args(action="run", local_store=store, target=record):
        return [action, str(spec), "--store", str(local_store), "--record", str(target)]

    def invoke(command, arguments, code=0):
        starts.write_bytes(b"")
        result = subprocess.run([bruce, command, *arguments], text=True, capture_output=True,
                                timeout=10, env=env)
        assert result.returncode == code, (result.returncode, result.stdout, result.stderr)
        return result

    # RED: before dispatch/header implementation V5 remains UnknownCommand.
    result = invoke("logic-mod-artifact-dag-v5", args(), 0)
    response = json.loads(result.stdout)
    assert response["contract"] == "logic-mod-artifact-dag-v5"
    assert response["action"] == "run" and response["accepted"] is True

    # A syntactically valid HTTPS origin reaches V5 host policy and leaves the
    # root-only run's behavior unchanged; no restore is needed for a root.
    result = invoke("logic-mod-artifact-dag-v5", [*args(local_store=root / "remote-store", target=root / "remote.record"),
                                                    "--remote-restore", "https://localhost:443"])
    assert json.loads(result.stdout)["contract"] == "logic-mod-artifact-dag-v5"

    # Bad origins are rejected before any worker fork, even with a valid Spec.
    result = invoke("logic-mod-artifact-dag-v5", [*args(), "--remote-restore", "http://localhost:443"], 2)
    assert json.loads(result.stderr) == {"error": "LogicModDagFailed"}
    assert not starts.read_bytes()

    # The host-only option is run-only and parser-invalid for replay, before
    # opening a FIFO Spec or starting a worker.
    fifo = root / "fifo"; os.mkfifo(fifo, 0o600)
    replay = ["replay", str(fifo), "--store", str(root / "replay-store"), "--record", str(root / "missing.record"),
              "--remote-restore", "https://localhost:443"]
    result = invoke("logic-mod-artifact-dag-v5", replay, 2)
    assert json.loads(result.stderr) == {"error": "InvalidLogicModDagArguments"}
    assert not starts.read_bytes()

    # Existing V4 grammar and contract remain untouched.
    v4 = invoke("logic-mod-artifact-dag-v4", args(local_store=root / "v4-store", target=root / "v4.record"))
    assert json.loads(v4.stdout)["contract"] == "logic-mod-artifact-dag-v4"

    help_result = subprocess.run([bruce, "--help"], text=True, capture_output=True, timeout=10)
    assert help_result.returncode == 0 and "logic-mod-artifact-dag-v5 run" in help_result.stdout

    # Real loopback TLS: seed exactly the expected a-out artifact remotely, then
    # remove its local manifest after a has completed but before slow root b can
    # unlock successor c. V5 must restore through the public CLI option first.
    certificate, key, authority = root / "tls.pem", root / "tls.key", root / "tls-ca.pem"
    subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                    "-subj", "/CN=bruce-v5-cli", "-addext", "subjectAltName=IP:127.0.0.1",
                    "-keyout", str(key), "-out", str(certificate)], check=True, capture_output=True, timeout=15)
    authority.write_bytes(certificate.read_bytes())
    tls_env = {**os.environ, "SSL_CERT_FILE": str(authority), "SSL_CERT_DIR": str(root / "empty-ca")}
    (root / "empty-ca").mkdir()
    server = TLSServer(certificate, key)
    remote = f"https://127.0.0.1:{server.server_address[1]}/remote"

    def dag_node(identifier, text, predecessors=()):
        node = {"id": identifier, "package": str(root / ("combined.brucemod" if predecessors else "output.brucemod")),
                "trust_store": str(root / "trust.txt"), "input": text, "predecessors": list(predecessors),
                "output_manifest": identifier + "-out"}
        if predecessors:
            node["input_manifest"] = identifier + "-input"
        return node

    def write_spec(path, nodes):
        path.write_text(json.dumps({"schema_version": 2, "nodes": nodes}))

    def seed_remote(tag):
        seed_spec, seed_store, seed_record = root / (tag + "-seed.json"), root / (tag + "-seed-store"), root / (tag + "-seed.record")
        write_spec(seed_spec, [dag_node("a", "3/4 hoch 2")])
        seeded = subprocess.run([bruce, "logic-mod-artifact-dag-v4", "run", str(seed_spec), "--store", str(seed_store),
                                 "--record", str(seed_record)], text=True, capture_output=True, timeout=10, env=tls_env)
        assert seeded.returncode == 0, seeded.stderr
        mirrored = subprocess.run([bruce, "artifact-store-v4", "mirror", "a-out", "--store", str(seed_store),
                                  "--remote", remote], text=True, capture_output=True, timeout=20, env=tls_env)
        assert mirrored.returncode == 0, mirrored.stderr

    def await_stopped(pid):
        """Do not mutate/continue until the probe's SIGSTOP has taken effect."""
        deadline = time.monotonic() + 2
        status = pathlib.Path("/proc") / str(pid) / "status"
        while time.monotonic() < deadline:
            try:
                state = next(line for line in status.read_text().splitlines() if line.startswith("State:"))
            except (FileNotFoundError, StopIteration):
                raise AssertionError(("paused root disappeared", pid))
            if "T" in state:
                return
            if "Z" in state:
                raise AssertionError(("paused root exited", pid, state))
            time.sleep(0.001)
        raise AssertionError(("second root never stopped", pid))

    def remote_run(tag, fail=False):
        seed_remote(tag)
        server.fail_get = fail
        remote_spec, remote_store, remote_record = root / (tag + ".json"), root / (tag + "-store"), root / (tag + ".record")
        write_spec(remote_spec, [dag_node("a", "3/4 hoch 2"), dag_node("b", "3/4 hoch 2"),
                                 dag_node("c", "3/4 hoch 2", ("a", "b"))])
        starts.write_bytes(b"")
        controlled_env = {**tls_env, "LD_PRELOAD": str(library), "BRUCE_TEST_STARTS": str(starts), "BRUCE_TEST_PAUSE_SECOND": "1"}
        process = subprocess.Popen([bruce, "logic-mod-artifact-dag-v5", "run", str(remote_spec), "--store", str(remote_store),
                                    "--record", str(remote_record), "--remote-restore", remote], text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=controlled_env)
        manifest = remote_store / "manifests" / "a-out.manifest"
        deadline = time.monotonic() + 5
        while not manifest.exists() and time.monotonic() < deadline:
            if process.poll() is not None:
                stdout, stderr = process.communicate()
                raise AssertionError(("root a exited before TLS mutation", process.returncode, stdout, stderr))
            time.sleep(0.005)
        assert manifest.exists(), "root a did not publish before TLS mutation"
        while time.monotonic() < deadline:
            rows = starts.read_text().splitlines()
            if len(rows) >= 2:
                break
            if process.poll() is not None:
                stdout, stderr = process.communicate()
                raise AssertionError(("second root exited before TLS mutation", process.returncode, stdout, stderr))
            time.sleep(0.001)
        assert len(rows) >= 2, rows
        second_root = int(rows[1].split()[1])
        await_stopped(second_root)
        manifest.unlink()
        os.kill(second_root, 18)  # SIGCONT: the confirmed paused independent root b
        stdout, stderr = process.communicate(timeout=20)
        return process.returncode, stdout, stderr, remote_store

    code, stdout, stderr, restored_store = remote_run("tls-success")
    assert code == 0 and not stderr, (code, stdout, stderr)
    assert json.loads(stdout)["contract"] == "logic-mod-artifact-dag-v5"
    assert (restored_store / "manifests" / "a-out.manifest").is_file()
    assert (restored_store / "manifests" / "c-out.manifest").is_file()
    assert [method for method, _ in server.requests].count("GET") >= 2

    code, stdout, stderr, rejected_store = remote_run("tls-failure", fail=True)
    assert (code, stdout, json.loads(stderr)) == (2, "", {"error": "LogicModDagFailed"})
    assert not (rejected_store / "manifests" / "c-input.manifest").exists()
    assert not (rejected_store / "manifests" / "c-out.manifest").exists()
    server.close()
print("logic artifact DAG V5 CLI workflows passed")
