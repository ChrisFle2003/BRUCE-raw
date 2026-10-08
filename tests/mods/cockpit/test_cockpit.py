import json
import hashlib
import http.client
import os
import errno
import select
import socket
import ssl
import subprocess
import sys
import tempfile
import time
import urllib.request
import urllib.error
import urllib.parse
from pathlib import Path


binary = str(Path(sys.argv[1]).resolve())
mtls_enabled = len(sys.argv) > 2 and sys.argv[2] == "--mtls-enabled"


def run(*args, cwd=None):
    return subprocess.run([binary, *args], capture_output=True, text=True, timeout=30,
                          cwd=cwd)


help_result = run("cockpit", "--help")
assert help_result.returncode == 0, help_result.stderr
assert help_result.stdout == ("bruce cockpit [--port N] [--listen ADDRESS "
                              "--tls-cert CERT.pem --tls-key KEY.pem] "
                              + ("[--mtls-ca-bundle CA.pem --mtls-truststore STORE "
                                 "--mtls-policy-version VERSION] " if mtls_enabled else "")
                              + "[--origin ORIGIN] [--public-url URL] "
                              "[--state-backend local|redis-metadata|redis-full] "
                              "[--redis-url redis://HOST[:PORT][/DB]]\n")

invalid_port = run("cockpit", "--port", "65536")
assert invalid_port.returncode == 2
assert json.loads(invalid_port.stderr)["error"] == "InvalidCockpitPort"

missing_tls = run("cockpit", "--listen", "0.0.0.0", "--port", "0")
assert missing_tls.returncode == 2
assert json.loads(missing_tls.stderr)["error"] == "CockpitTlsConfigurationRequired"

invalid_tls = run("cockpit", "--listen", "0.0.0.0", "--port", "0",
                  "--tls-cert", "/tmp/bruce-missing-cert.pem",
                  "--tls-key", "/tmp/bruce-missing-key.pem")
assert invalid_tls.returncode == 2
assert json.loads(invalid_tls.stderr)["error"] == "CockpitTlsConfigurationInvalid"

with tempfile.TemporaryDirectory(prefix="bruce-cockpit-tls-") as tls_workdir:
    tls_root = Path(tls_workdir)
    cert = tls_root / "server-cert.pem"
    key = tls_root / "server-key.pem"
    generated = subprocess.run(
        ["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
         "-keyout", str(key), "-out", str(cert), "-subj", "/CN=127.0.0.1",
         "-days", "1"], capture_output=True, text=True, timeout=30)
    assert generated.returncode == 0, generated.stderr
    missing_origin = run("cockpit", "--listen", "0.0.0.0", "--port", "0",
                         "--tls-cert", str(cert), "--tls-key", str(key))
    assert missing_origin.returncode == 2
    assert json.loads(missing_origin.stderr)["error"] == "CockpitOriginConfigurationRequired"
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        tls_port = reservation.getsockname()[1]
    lan_origin = f"https://cockpit.example:{tls_port}"
    public_url = f"https://dashboard.example:{tls_port}"
    for endpoint_args in (("--origin", lan_origin, "--port", "0"),
                          ("--origin", "https://0.0.0.0", "--port", str(tls_port)),
                          ("--origin", "http://cockpit.example", "--port", str(tls_port)),
                          ("--origin", "https://cockpit.example:1", "--port", str(tls_port))):
        invalid_origin = run("cockpit", "--listen", "0.0.0.0",
                             "--tls-cert", str(cert), "--tls-key", str(key), *endpoint_args)
        assert invalid_origin.returncode == 2
        assert json.loads(invalid_origin.stderr)["error"] == "CockpitOriginConfigurationInvalid"
    for invalid_url in ("https://0.0.0.0", "https://[::]", "http://dashboard.example",
                        "https://dashboard.example:1", "https://dashboard.example/path",
                        "https://dashboard.example?token=other", "https://user@dashboard.example"):
        invalid_public_url = run(
            "cockpit", "--listen", "0.0.0.0", "--port", str(tls_port),
            "--tls-cert", str(cert), "--tls-key", str(key),
            "--origin", lan_origin, "--public-url", invalid_url)
        assert invalid_public_url.returncode == 2, invalid_url
        assert json.loads(invalid_public_url.stderr)["error"] == "CockpitOriginConfigurationInvalid"
    # TLS admission keeps priority even when both browser endpoints are invalid.
    for tls_args, expected in (((), "CockpitTlsConfigurationRequired"),
                              (("--tls-cert", str(cert)), "CockpitTlsConfigurationRequired"),
                              (("--tls-cert", str(cert), "--tls-key", str(cert)),
                               "CockpitTlsConfigurationInvalid")):
        rejected_config = run(
            "cockpit", "--listen", "0.0.0.0", "--port", str(tls_port),
            "--origin", "https://0.0.0.0", "--public-url", "https://[::]", *tls_args)
        assert rejected_config.returncode == 2
        assert json.loads(rejected_config.stderr)["error"] == expected
    tls_server = subprocess.Popen(
        [binary, "cockpit", "--listen", "0.0.0.0", "--port", str(tls_port),
         "--origin", lan_origin, "--public-url", public_url,
         "--tls-cert", str(cert), "--tls-key", str(key)],
        cwd=tls_workdir, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True)
    try:
        tls_startup = json.loads(tls_server.stdout.readline())
        assert tls_startup["event"] == "cockpit_started"
        assert tls_startup["host"] == "0.0.0.0"
        assert isinstance(tls_startup["port"], int) and tls_startup["port"] > 0
        assert tls_startup["url"].startswith(public_url + "/")
        assert "0.0.0.0" not in tls_startup["url"]
        assert tls_startup["url"].endswith("/?token=" + tls_startup["token"])
        tls_base = f"https://127.0.0.1:{tls_startup['port']}"
        tls_context = ssl._create_unverified_context()
        try:
            urllib.request.urlopen(tls_base.replace("https://", "http://") +
                                   "/api/v1/health", timeout=5)
            raise AssertionError("plaintext client unexpectedly reached TLS server")
        except (urllib.error.URLError, OSError):
            pass
        tls_health = urllib.request.urlopen(
            urllib.request.Request(
                tls_base + "/api/v1/health",
                headers={"Authorization": "Bearer " + tls_startup["token"]}),
            context=tls_context, timeout=5)
        assert tls_health.status == 200
        assert tls_health.read() == b'{"status":"ok","service":"cockpit","version":1}'
        for candidate, expected in ((lan_origin, 200), (public_url, 403)):
            request = urllib.request.Request(
                tls_base + "/api/v1/health",
                headers={"Authorization": "Bearer " + tls_startup["token"],
                         "Origin": candidate})
            try:
                with urllib.request.urlopen(request, context=tls_context, timeout=5) as response:
                    assert response.status == expected
            except urllib.error.HTTPError as error:
                assert error.code == expected
                assert json.loads(error.read()) == {"error": "CockpitOriginRejected"}
        # Host is routing metadata, never the source of the permitted Origin.
        for candidate, host, expected in ((lan_origin, "foreign.example", 200),
                                         (public_url, f"dashboard.example:{tls_port}", 403)):
            request = urllib.request.Request(
                tls_base + "/api/v1/health",
                headers={"Authorization": "Bearer " + tls_startup["token"],
                         "Origin": candidate, "Host": host})
            try:
                with urllib.request.urlopen(request, context=tls_context, timeout=5) as response:
                    assert response.status == expected
            except urllib.error.HTTPError as error:
                assert error.code == expected
                assert json.loads(error.read()) == {"error": "CockpitOriginRejected"}
        # A malformed job body makes admission observable without creating a job.
        for authorization, candidate, expected_status, expected_error in (
                (None, public_url, 401, "CockpitAuthRequired"),
                ("Bearer invalid", public_url, 401, "CockpitAuthRejected"),
                ("Bearer " + tls_startup["token"], None, 403, "CockpitOriginRejected"),
                ("Bearer " + tls_startup["token"], public_url, 403, "CockpitOriginRejected"),
                ("Bearer " + tls_startup["token"], lan_origin, 400, "CockpitRequestMalformed")):
            headers = {"Content-Type": "application/json"}
            if authorization is not None:
                headers["Authorization"] = authorization
            if candidate is not None:
                headers["Origin"] = candidate
            request = urllib.request.Request(tls_base + "/api/v1/jobs", data=b"not-json",
                                             headers=headers, method="POST")
            try:
                urllib.request.urlopen(request, context=tls_context, timeout=5)
                raise AssertionError("invalid LAN POST unexpectedly succeeded")
            except urllib.error.HTTPError as error:
                assert error.code == expected_status
                assert json.loads(error.read()) == {"error": expected_error}
        with urllib.request.urlopen(
                urllib.request.Request(tls_base + "/api/v1/jobs",
                    headers={"Authorization": "Bearer " + tls_startup["token"]}),
                context=tls_context, timeout=5) as response:
            assert json.loads(response.read()) == []
        # Use an executable request to prove TLS write admission independently
        # of malformed-body rejection. Each denied write leaves the store empty.
        tls_argv = ["calculate", "--op", "add", "--left", "1", "--right", "1"]
        job_body = json.dumps({"argv": tls_argv}).encode("utf-8")
        for authorization, candidate, expected_error in (
                (None, public_url, "CockpitAuthRequired"),
                ("Bearer invalid", public_url, "CockpitAuthRejected"),
                ("Bearer " + tls_startup["token"], None, "CockpitOriginRejected"),
                ("Bearer " + tls_startup["token"], public_url, "CockpitOriginRejected")):
            headers = {"Content-Type": "application/json"}
            if authorization is not None:
                headers["Authorization"] = authorization
            if candidate is not None:
                headers["Origin"] = candidate
            request = urllib.request.Request(tls_base + "/api/v1/jobs", data=job_body,
                                             headers=headers, method="POST")
            try:
                urllib.request.urlopen(request, context=tls_context, timeout=5)
                raise AssertionError("unauthorized valid TLS write unexpectedly succeeded")
            except urllib.error.HTTPError as error:
                assert error.code == (403 if expected_error == "CockpitOriginRejected" else 401)
                assert json.loads(error.read()) == {"error": expected_error}
            with urllib.request.urlopen(
                    urllib.request.Request(tls_base + "/api/v1/jobs",
                        headers={"Authorization": "Bearer " + tls_startup["token"]}),
                    context=tls_context, timeout=5) as response:
                assert json.loads(response.read()) == []
        with urllib.request.urlopen(
                urllib.request.Request(tls_base + "/api/v1/jobs", data=job_body,
                    headers={"Authorization": "Bearer " + tls_startup["token"],
                             "Origin": lan_origin, "Content-Type": "application/json"},
                    method="POST"), context=tls_context, timeout=5) as response:
            assert response.status == 201
            tls_job = json.loads(response.read())
            assert tls_job["argv"] == tls_argv
        deadline = time.monotonic() + 5
        while True:
            with urllib.request.urlopen(
                    urllib.request.Request(tls_base + "/api/v1/jobs",
                        headers={"Authorization": "Bearer " + tls_startup["token"]}),
                    context=tls_context, timeout=5) as response:
                tls_jobs = json.loads(response.read())
            assert len(tls_jobs) == 1 and tls_jobs[0]["id"] == tls_job["id"]
            if tls_jobs[0]["state"] not in {"RUNNING", "CANCEL_REQUESTED"}:
                assert tls_jobs[0]["state"] == "SUCCEEDED"
                assert tls_jobs[0]["return_code"] == 0
                break
            assert time.monotonic() < deadline, "TLS calculation job did not finish"
            time.sleep(0.02)
        with urllib.request.urlopen(
                urllib.request.Request(tls_base + "/api/v1/jobs/" + tls_job["id"] + "/logs/stdout",
                    headers={"Authorization": "Bearer " + tls_startup["token"]}),
                context=tls_context, timeout=5) as response:
            assert json.loads(response.read()) == {
                "domain": "bigint", "operation": "add", "max_bits": 4096, "value": "2"}
    finally:
        tls_server.terminate()
        try:
            tls_server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            tls_server.kill()
            tls_server.wait(timeout=5)

    derived_server = subprocess.Popen(
        [binary, "cockpit", "--listen", "0.0.0.0", "--port", str(tls_port),
         "--origin", "https://cockpit.example",
         "--tls-cert", str(cert), "--tls-key", str(key)],
        cwd=tls_workdir, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        derived_startup = json.loads(derived_server.stdout.readline())
        assert derived_startup["url"] == lan_origin + "/?token=" + derived_startup["token"]
        assert "0.0.0.0" not in derived_startup["url"]
    finally:
        derived_server.terminate()
        try:
            derived_server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            derived_server.kill()
            derived_server.wait(timeout=5)

with tempfile.TemporaryDirectory(prefix="bruce-cockpit-http-") as workdir:
    server = subprocess.Popen([binary, "cockpit", "--port", "0"], cwd=workdir,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              text=True)
    try:
        startup = json.loads(server.stdout.readline())
        assert startup["event"] == "cockpit_started"
        assert startup["host"] == "127.0.0.1"
        assert isinstance(startup["port"], int) and startup["port"] > 0
        assert isinstance(startup["token"], str) and len(startup["token"]) == 64
        assert startup["url"] == ("http://127.0.0.1:" + str(startup["port"]) +
                                   "/?token=" + startup["token"])
        ready, _, _ = select.select([server.stdout], [], [], 0.2)
        assert not ready, "Cockpit emitted more than one startup record"
        base = f"http://127.0.0.1:{startup['port']}"
        plain_urlopen = urllib.request.urlopen
        try:
            plain_urlopen(base + "/", timeout=5)
            raise AssertionError("unauthenticated dashboard unexpectedly succeeded")
        except urllib.error.HTTPError as error:
            assert error.code == 401
            assert json.loads(error.read()) == {"error": "CockpitAuthRequired"}
        root = plain_urlopen(base + "/?token=" + startup["token"], timeout=5)
        assert root.status == 200
        html = root.read().decode("utf-8")
        assert "BRUCE Cockpit" in html
        assert startup["token"] in html
        for marker in ("/api/v1/health", "/api/v1/jobs", 'id="health"',
                       'id="jobs"', 'id="job-detail"',
                       'id="job-form"', 'id="job-command"', 'id="job-args"',
                       'id="import-form"', '/api/v1/imports',
                       'data-action="inspect"', 'data-action="assess"',
                       'data-action="replay"', 'data-action="gate"',
                       'data-action="cancel"'):
            assert marker in html, marker
        for marker in ('Natural language → draft', 'data-language-form',
                       'data-language-text', 'data-language-alias="1"',
                       'data-language-alias="2"', 'data-language-bytes="1"',
                       'data-language-bytes="2"', 'data-language-result',
                       'data-language-alternatives', '/api/v1/language/drafts',
                       'crypto.subtle.digest("SHA-256"', 'new TextEncoder()'):
            assert marker in html, marker
        markup = html.split("<script>", 1)[0]
        assert markup.count('data-language-alias="') == 2
        assert markup.count('data-language-bytes="') == 2
        language = html.split('const languageJson = async', 1)[1].split('const selectJob = async', 1)[0]
        for marker in ('data-language-dispatch-availability', 'data-language-dispatch-actions'):
            assert marker in markup, marker
        assert '__BRUCE_LANGUAGE_' not in html
        dispatch_enabled = 'const languageDispatchEnabled = true;' in language
        assert dispatch_enabled or 'const languageDispatchEnabled = false;' in language
        assert ('CORE execution is available.' if dispatch_enabled else
                'CORE execution is unavailable in this build.') in markup
        if dispatch_enabled:
            assert 'const languageEnabled = true;' in html
        if 'const languageEnabled = false;' in html:
            assert '<fieldset disabled>' in markup and not dispatch_enabled
        for marker in ('languageDispatchEnabled && (draft.status === "UNDERSTOOD" ||',
                       '(draft.status === "AMBIGUOUS" && Boolean(draft.selected_id)))',
                       'button.dataset.languageDispatch = draft.id',
                       'button.textContent = "Run CORE"',
                       'dispatchLanguageDraft(button.dataset.languageDispatch)'):
            assert marker in language, marker
        dispatch = language.split('const dispatchLanguageDraft = async', 1)[1].split(
            'const selectLanguageAlternative = async', 1)[0]
        for marker in ('if (!languageDispatchEnabled || languagePending) return',
                       'encodeURIComponent(id) + "/dispatch"', 'method:"POST"',
                       'authHeaders()', '"Content-Type":"application/json"', 'body:""',
                       'textContent = JSON.stringify(data, null, 2)', 'showError(error.message)'):
            assert marker in dispatch, marker
        for forbidden in ('/api/v1/jobs', 'selectJob(', 'createJob(', 'refresh(', 'state.job',
                          'innerHTML', 'data-action', '/run'):
            assert forbidden not in language, forbidden

        try:
            plain_urlopen(base + "/api/v1/health", timeout=5)
            raise AssertionError("unauthenticated health unexpectedly succeeded")
        except urllib.error.HTTPError as error:
            assert error.code == 401
            assert json.loads(error.read()) == {"error": "CockpitAuthRequired"}
        unauthorized_post = urllib.request.Request(
            base + "/api/v1/jobs", data=b'{"argv":["calculate"]}',
            headers={"Authorization": "Bearer " + startup["token"],
                     "Content-Type": "application/json"}, method="POST")
        try:
            plain_urlopen(unauthorized_post, timeout=5)
            raise AssertionError("cross-origin POST unexpectedly succeeded")
        except urllib.error.HTTPError as error:
            assert error.code == 403
            assert json.loads(error.read()) == {"error": "CockpitOriginRejected"}

        origin = base

        def authenticated_urlopen(target, *args, **kwargs):
            request = target if isinstance(target, urllib.request.Request) else urllib.request.Request(target)
            request.add_header("Authorization", "Bearer " + startup["token"])
            if request.get_method() == "POST":
                request.add_header("Origin", origin)
            return plain_urlopen(request, *args, **kwargs)

        urllib.request.urlopen = authenticated_urlopen
        health = urllib.request.urlopen(base + "/api/v1/health", timeout=5)
        assert health.status == 200
        assert health.headers["Content-Type"] == "application/json"
        assert health.read() == b'{"status":"ok","service":"cockpit","version":1}'
        jobs = urllib.request.urlopen(base + "/api/v1/jobs", timeout=5)
        assert jobs.status == 200
        assert json.loads(jobs.read().decode("utf-8")) == []

        imports = urllib.request.urlopen(base + "/api/v1/imports", timeout=5)
        assert imports.status == 200
        assert json.loads(imports.read().decode("utf-8")) == []

        # Application dispatch has no generic Cockpit route. Admission still
        # precedes unknown-route handling, and neither request starts a job.
        for headers, expected_status, expected_body in (
                ({"Origin": origin}, 401, {"error": "CockpitAuthRequired"}),
                ({"Authorization": "Bearer " + startup["token"], "Origin": origin},
                 404, None)):
            request = urllib.request.Request(
                base + "/api/v1/dispatch", data=b"", headers=headers, method="POST")
            try:
                plain_urlopen(request, timeout=5)
                raise AssertionError("generic application dispatch unexpectedly succeeded")
            except urllib.error.HTTPError as error:
                assert error.code == expected_status
                if expected_body is not None:
                    assert json.loads(error.read()) == expected_body
        assert json.loads(urllib.request.urlopen(base + "/api/v1/jobs", timeout=5).read()) == []
        assert json.loads(urllib.request.urlopen(base + "/api/v1/imports", timeout=5).read()) == []

        # Oversize admission must respond before the caller sends its body.
        # For chunked transfer, abort on the first byte beyond 8 MiB without
        # waiting for the remaining declared chunk or a terminating chunk.
        def streamed_import(framing, prefix=b"", extra=None, truncate=False):
            with socket.create_connection(("127.0.0.1", startup["port"]), timeout=5) as connection:
                headers = {"Host": "127.0.0.1", "Connection": "close",
                           "Authorization": "Bearer " + startup["token"], "Origin": base,
                           "Content-Type": "application/octet-stream",
                           "X-Bruce-Import-Name": "stream.bruce"} | framing | (extra or {})
                wire = ("POST /api/v1/imports HTTP/1.1\r\n" +
                        "".join(key + ": " + value + "\r\n" for key, value in headers.items()) + "\r\n").encode()
                connection.sendall(wire + prefix)
                if truncate:
                    connection.shutdown(socket.SHUT_WR)
                response = http.client.HTTPResponse(connection)
                response.begin()
                body = response.read()
                return response.status, json.loads(body) if body else b""

        import_limit = 8 * 1024 * 1024
        oversized_framing = {"Content-Length": str(import_limit + 1)}
        assert streamed_import(oversized_framing) == (413, {"error": "CockpitImportTooLarge"})
        assert streamed_import(oversized_framing, extra={"Content-Type": "text/plain",
                                                        "X-Bruce-Import-Name": ""}) == (
            413, {"error": "CockpitImportTooLarge"})
        assert streamed_import(oversized_framing,
                               extra={"Content-Type": "application/x-www-form-urlencoded"}) == (
            413, {"error": "CockpitImportTooLarge"})
        for extra, expected in (({"Authorization": ""}, (401, "CockpitAuthRequired")),
                                ({"Authorization": "Bearer wrong", "Origin": "http://foreign.example"},
                                 (401, "CockpitAuthRejected")),
                                ({"Origin": "http://foreign.example"}, (403, "CockpitOriginRejected")),
                                ({"Origin": ""}, (403, "CockpitOriginRejected"))):
            assert streamed_import(oversized_framing, extra=extra) == (expected[0], {"error": expected[1]})
            assert streamed_import({"Transfer-Encoding": "chunked"}, extra=extra) == (
                expected[0], {"error": expected[1]})
        chunk_prefix = b"800000\r\n" + b"BRUCE_" + b"x" * (import_limit - 6) + b"\r\n100000\r\nx"
        assert streamed_import({"Transfer-Encoding": "chunked"}, chunk_prefix) == (
            413, {"error": "CockpitImportTooLarge"})
        assert streamed_import({"Transfer-Encoding": "chunked"}, chunk_prefix,
                               {"Content-Type": "text/plain", "X-Bruce-Import-Name": ""}) == (
            413, {"error": "CockpitImportTooLarge"})
        assert streamed_import({"Transfer-Encoding": "chunked"}, chunk_prefix,
                               {"Content-Type": "application/x-www-form-urlencoded"}) == (
            413, {"error": "CockpitImportTooLarge"})
        assert streamed_import({"Content-Length": "20"}, b"BRUCE_", truncate=True) == (400, b"")
        assert streamed_import({"Content-Length": "6"}, b"BRUCE_",
                               {"Content-Encoding": "gzip"}) == (415, b"")
        for extra, expected in (({"Authorization": ""}, (401, "CockpitAuthRequired")),
                                ({"Authorization": "Bearer wrong"}, (401, "CockpitAuthRejected")),
                                ({"Origin": "http://foreign.example"}, (403, "CockpitOriginRejected"))):
            assert streamed_import({"Content-Length": "6"},
                                   extra={"Content-Encoding": "gzip"} | extra) == (
                expected[0], {"error": expected[1]})
        assert json.loads(urllib.request.urlopen(base + "/api/v1/imports", timeout=5).read()) == []
        assert list((Path(workdir) / "runs" / "cockpit" / "imports").iterdir()) == []
        assert json.loads(urllib.request.urlopen(base + "/api/v1/jobs", timeout=5).read()) == []

        wrong_import_type = urllib.request.Request(
            base + "/api/v1/imports", data=b"BRUCE_TEST_IMPORT_V1\n",
            headers={"Content-Type": "text/plain",
                     "X-Bruce-Import-Name": "demo.bruce"}, method="POST")
        try:
            urllib.request.urlopen(wrong_import_type, timeout=5)
            raise AssertionError("wrong import content type unexpectedly succeeded")
        except urllib.error.HTTPError as error:
            assert error.code == 400
            assert json.loads(error.read()) == {"error": "CockpitImportContentTypeRequired"}

        multipart_bytes = (b"--test\r\nContent-Disposition: form-data; name=\"file\"; filename=\"demo.bruce\"\r\n"
                           b"Content-Type: application/octet-stream\r\n\r\nBRUCE_TEST_IMPORT_V1\n\r\n--test--\r\n")
        assert streamed_import({"Content-Length": str(len(multipart_bytes))}, multipart_bytes,
                               {"Content-Type": "multipart/form-data; boundary=test"}) == (
            400, {"error": "CockpitImportContentTypeRequired"})
        assert streamed_import({"Content-Length": "6"}, b"broken",
                               {"Content-Type": "multipart/form-data; boundary=test"}) == (400, b"")
        form_body = b"x=" + b"f" * 8191
        for framing, body in (({"Content-Length": str(len(form_body))}, form_body),
                              ({"Transfer-Encoding": "chunked"},
                               b"2001\r\n" + form_body + b"\r\n0\r\n\r\n")):
            assert streamed_import(framing, body,
                                   {"Content-Type": "application/x-www-form-urlencoded; charset=UTF-8"}) == (
                413, b"")
        assert streamed_import({"Content-Length": "8192"}, form_body[:8192],
                               {"Content-Type": "application/x-www-form-urlencoded"}) == (
            400, {"error": "CockpitImportContentTypeRequired"})
        assert json.loads(urllib.request.urlopen(base + "/api/v1/imports", timeout=5).read()) == []
        assert list((Path(workdir) / "runs" / "cockpit" / "imports").iterdir()) == []

        missing_import_name = urllib.request.Request(
            base + "/api/v1/imports", data=b"BRUCE_TEST_IMPORT_V1\n",
            headers={"Content-Type": "application/octet-stream"}, method="POST")
        try:
            urllib.request.urlopen(missing_import_name, timeout=5)
            raise AssertionError("nameless import unexpectedly succeeded")
        except urllib.error.HTTPError as error:
            assert error.code == 400
            assert json.loads(error.read()) == {"error": "CockpitImportNameRequired"}

        rejected_import_name = urllib.request.Request(
            base + "/api/v1/imports", data=b"BRUCE_TEST_IMPORT_V1\n",
            headers={"Content-Type": "application/octet-stream",
                     "X-Bruce-Import-Name": "../escape.bruce"}, method="POST")
        try:
            urllib.request.urlopen(rejected_import_name, timeout=5)
            raise AssertionError("traversing import name unexpectedly succeeded")
        except urllib.error.HTTPError as error:
            assert error.code == 400
            assert json.loads(error.read()) == {"error": "CockpitImportNameRejected"}

        invalid_import = urllib.request.Request(
            base + "/api/v1/imports", data=b"not-a-bruce-bundle",
            headers={"Content-Type": "application/octet-stream",
                     "X-Bruce-Import-Name": "invalid.bruce"}, method="POST")
        try:
            urllib.request.urlopen(invalid_import, timeout=5)
            raise AssertionError("invalid import unexpectedly succeeded")
        except urllib.error.HTTPError as error:
            assert error.code == 400
            assert json.loads(error.read()) == {"error": "CockpitImportFormatRejected"}

        import_bytes = b"BRUCE_TEST_IMPORT_V1\nimported bytes\x00\r\n"
        valid_import = urllib.request.Request(
            base + "/api/v1/imports", data=import_bytes,
            headers={"Content-Type": "application/octet-stream",
                     "X-Bruce-Import-Name": "demo.bruce"}, method="POST")
        imported = urllib.request.urlopen(valid_import, timeout=5)
        assert imported.status == 201
        imported_json = json.loads(imported.read().decode("utf-8"))
        assert imported_json["id"].startswith("import-")
        assert imported_json["name"] == "demo.bruce"
        assert imported_json["bytes"] == len(import_bytes)
        assert imported_json["sha256"] == hashlib.sha256(import_bytes).hexdigest()
        imported_path = Path(imported_json["path"])
        assert imported_path.read_bytes() == import_bytes
        imported_detail = json.loads(urllib.request.urlopen(
            base + "/api/v1/imports/" + imported_json["id"], timeout=5).read())
        assert imported_detail == imported_json
        imported_listing = json.loads(urllib.request.urlopen(
            base + "/api/v1/imports", timeout=5).read())
        assert imported_listing == [imported_json]
        assert json.loads(urllib.request.urlopen(
            base + "/api/v1/jobs", timeout=5).read()) == []

        try:
            urllib.request.urlopen(base + "/api/v1/imports/import-999999", timeout=5)
            raise AssertionError("unknown import unexpectedly succeeded")
        except urllib.error.HTTPError as error:
            assert error.code == 404
            assert json.loads(error.read()) == {"error": "CockpitUnknownImport"}

        # A real allowed child blocks opening the FIFO, establishing RUNNING.
        # Where /proc is available, release it and observe exit without polling
        # the store, so management reads must preserve stale RUNNING metadata.
        if hasattr(os, "mkfifo"):
            fifo = Path(workdir) / "inspect_fifo.bruce"
            try:
                os.mkfifo(fifo)
            except NotImplementedError:
                pass
            else:
                blocked_request = urllib.request.Request(
                    base + "/api/v1/jobs",
                    data=json.dumps({"argv": ["inspect-run", str(fifo)]}).encode("utf-8"),
                    headers={"Content-Type": "application/json"}, method="POST")
                blocked_response = urllib.request.urlopen(blocked_request, timeout=5)
                assert blocked_response.status == 201
                blocked_job = json.loads(blocked_response.read())
                blocked_url = base + "/api/v1/jobs/" + blocked_job["id"]
                blocked_directory = Path(blocked_job["directory"])
                assert blocked_job["state"] == "RUNNING"
                saved_files = {name: (blocked_directory / name).read_bytes()
                               for name in ("job.json", "stdout.log", "stderr.log")}
                released = False
                try:
                    tasks_path = Path(f"/proc/{server.pid}/task")
                    if tasks_path.exists():
                        child_ids = [child for task in tasks_path.iterdir()
                                     for child in (task / "children").read_text().split()]
                        assert len(child_ids) == 1, child_ids
                        child_stat = Path("/proc") / child_ids[0] / "stat"
                        deadline = time.monotonic() + 5
                        while True:
                            try:
                                writer = os.open(fifo, os.O_WRONLY | os.O_NONBLOCK)
                                break
                            except OSError as error:
                                if error.errno != errno.ENXIO:
                                    raise
                                assert time.monotonic() < deadline, "FIFO child did not open input"
                                time.sleep(0.02)
                        try:
                            os.write(writer, b"malformed cockpit fixture\n")
                        finally:
                            os.close(writer)
                            released = True
                        # Logs are collected only by poll_children, so observe
                        # kernel exit state rather than asking a mutating API.
                        deadline = time.monotonic() + 5
                        while child_stat.read_text().rsplit(") ", 1)[1].split()[0] != "Z":
                            assert time.monotonic() < deadline, "FIFO child did not exit"
                            time.sleep(0.02)
                    for route in ("/api/v1/health",
                                  "/api/v1/imports",
                                  "/api/v1/imports/" + imported_json["id"],
                                  "/api/v1/jobs/" + blocked_job["id"] + "/artifacts",
                                  "/api/v1/jobs/" + blocked_job["id"] + "/logs/stdout",
                                  "/api/v1/jobs/" + blocked_job["id"] + "/logs/stderr"):
                        response = urllib.request.urlopen(base + route, timeout=5)
                        assert response.status == 200
                        response.read()
                        for name, before in saved_files.items():
                            assert (blocked_directory / name).read_bytes() == before
                        assert json.loads((blocked_directory / "job.json").read_bytes())[
                            "state"] == "RUNNING"
                finally:
                    if released:
                        reaped = json.loads(urllib.request.urlopen(blocked_url, timeout=5).read())
                        assert reaped["state"] == "FAILED"
                        assert reaped["return_code"] != 0
                    else:
                        cancel = urllib.request.Request(blocked_url + "/cancel",
                                                        data=b"", method="POST")
                        cancelled = json.loads(urllib.request.urlopen(cancel, timeout=5).read())
                        deadline = time.monotonic() + 5
                        while cancelled["state"] in {"RUNNING", "CANCEL_REQUESTED"}:
                            assert time.monotonic() < deadline, "FIFO fixture did not cancel"
                            time.sleep(0.02)
                            cancelled = json.loads(urllib.request.urlopen(
                                blocked_url, timeout=5).read())
                        assert cancelled["state"] == "CANCELLED"
                    fifo.unlink()

        # Local imports retain their manifest metadata until reload. A changed
        # path is rejected before job creation instead of becoming a new
        # action baseline.
        changed_import_bytes = b"BRUCE_TEST_IMPORT_V1\nchanged before action\x00\r\n"
        imported_path.write_bytes(changed_import_bytes)
        changed_digest = hashlib.sha256(changed_import_bytes).hexdigest()
        assert changed_digest != imported_json["sha256"]
        assert json.loads(urllib.request.urlopen(
            base + "/api/v1/imports/" + imported_json["id"], timeout=5).read()) == imported_json

        # Import actions reject a manifest mismatch without creating a job.
        existing_job_ids = {item["id"] for item in json.loads(urllib.request.urlopen(
            base + "/api/v1/jobs", timeout=5).read())}
        for action in ("inspect", "assess", "replay", "gate"):
            mismatch_request = urllib.request.Request(
                base + "/api/v1/imports/" + imported_json["id"] + "/" + action,
                data=b"", headers={"Content-Type": "application/json"}, method="POST")
            try:
                urllib.request.urlopen(mismatch_request, timeout=5)
                raise AssertionError("manifest mismatch unexpectedly created a job")
            except urllib.error.HTTPError as error:
                assert error.code == 409
                assert json.loads(error.read()) == {"error": "CockpitImportIntegrityMismatch"}
        assert {item["id"] for item in json.loads(urllib.request.urlopen(
            base + "/api/v1/jobs", timeout=5).read())} == existing_job_ids

        # Restoring the original bytes permits all four actions. Each job uses
        # an atomically created, job-owned snapshot rather than the import path.
        imported_path.write_bytes(import_bytes)
        import_action_ids = set()
        for action, command in (("inspect", "inspect-run"),
                                ("assess", "assess-run"),
                                ("replay", "replay"),
                                ("gate", "gate-run")):
            action_url = base + "/api/v1/imports/" + imported_json["id"] + "/" + action
            # Admission must win over both malformed body and unknown import ID.
            for headers, expected_status, error_name in (
                    ({"Origin": "http://foreign.example"}, 401, "CockpitAuthRequired"),
                    ({"Authorization": "Bearer " + startup["token"],
                      "Origin": "http://foreign.example"}, 403, "CockpitOriginRejected")):
                priority_request = urllib.request.Request(
                    base + "/api/v1/imports/import-999999/" + action,
                    data=b"{}", headers={"Content-Type": "text/plain"} | headers,
                    method="POST")
                try:
                    plain_urlopen(priority_request, timeout=5)
                    raise AssertionError("import action bypassed admission")
                except urllib.error.HTTPError as error:
                    assert error.code == expected_status
                    assert json.loads(error.read()) == {"error": error_name}
            action_request = urllib.request.Request(
                action_url, data=b"", headers={"Content-Type": "application/json"},
                method="POST")
            action_response = urllib.request.urlopen(action_request, timeout=5)
            assert action_response.status == 201
            action_job = json.loads(action_response.read())
            assert action_job["id"].startswith("job-")
            assert action_job["id"] not in existing_job_ids
            assert action_job["id"] not in import_action_ids
            import_action_ids.add(action_job["id"])
            assert action_job["argv"][0] == command
            assert action_job["argv"][1] != str(imported_path)
            assert ".materialized-actions" in action_job["argv"][1]
            assert action_job["inputs"] == [{"path": action_job["argv"][1],
                                               "sha256": imported_json["sha256"]}]
            persisted = json.loads((Path(action_job["directory"]) / "job.json").read_bytes())
            assert persisted["inputs"] == action_job["inputs"]
            assert imported_path.read_bytes() == import_bytes

            # Content validation precedes import lookup; a body cannot override
            # the import selector with an arbitrary absolute filesystem path.
            arbitrary_path = Path(workdir) / "arbitrary.bruce"
            arbitrary_path.write_bytes(import_bytes)
            for selected_id, body, content_type, status, error_name in (
                    (imported_json["id"], b"{}", "application/json", 400,
                     "CockpitRequestBodyRejected"),
                    (imported_json["id"], b" ", "application/json", 400,
                     "CockpitRequestBodyRejected"),
                    (imported_json["id"], json.dumps({"path": str(arbitrary_path)}).encode(),
                     "application/json", 400, "CockpitRequestBodyRejected"),
                    ("import-999999", b"", "application/json", 404, "CockpitUnknownImport"),
                    ("import-999999", b"{}", "application/json", 400,
                     "CockpitRequestBodyRejected"),
                    (imported_json["id"], b"", "text/plain", 400, "CockpitContentTypeRequired"),
                    ("import-999999", b"{}", "text/plain", 400, "CockpitContentTypeRequired")):
                rejected = urllib.request.Request(
                    base + "/api/v1/imports/" + selected_id + "/" + action,
                    data=body, headers={"Content-Type": content_type}, method="POST")
                try:
                    urllib.request.urlopen(rejected, timeout=5)
                    raise AssertionError("invalid import action unexpectedly succeeded")
                except urllib.error.HTTPError as error:
                    assert error.code == status
                    assert json.loads(error.read()) == {"error": error_name}
            absolute_selector = urllib.request.Request(
                base + "/api/v1/imports/" + urllib.parse.quote(str(arbitrary_path), safe="") +
                "/" + action, data=b"", headers={"Content-Type": "application/json"},
                method="POST")
            try:
                urllib.request.urlopen(absolute_selector, timeout=5)
                raise AssertionError("absolute path import selector unexpectedly succeeded")
            except urllib.error.HTTPError as error:
                assert error.code == 404
            assert imported_path.read_bytes() == import_bytes
        final_job_ids = {item["id"] for item in json.loads(urllib.request.urlopen(
            base + "/api/v1/jobs", timeout=5).read())}
        assert final_job_ids == existing_job_ids | import_action_ids

        missing_import_request = urllib.request.Request(
            base + "/api/v1/imports", data=import_bytes,
            headers={"Content-Type": "application/octet-stream",
                     "X-Bruce-Import-Name": "missing.bruce"}, method="POST")
        missing_import_response = urllib.request.urlopen(missing_import_request, timeout=5)
        assert missing_import_response.status == 201
        missing_import_json = json.loads(missing_import_response.read())
        Path(missing_import_json["path"]).unlink()
        for action in ("inspect", "assess", "replay", "gate"):
            missing_import_action = urllib.request.Request(
                base + "/api/v1/imports/" + missing_import_json["id"] + "/" + action,
                data=b"", headers={"Content-Type": "application/json"}, method="POST")
            try:
                urllib.request.urlopen(missing_import_action, timeout=5)
                raise AssertionError("action on deleted import unexpectedly succeeded")
            except urllib.error.HTTPError as error:
                assert error.code == 400
                assert json.loads(error.read()) == {"error": "CockpitInputNotReadable"}
            assert {item["id"] for item in json.loads(urllib.request.urlopen(
                base + "/api/v1/jobs", timeout=5).read())} == final_job_ids

        request = urllib.request.Request(
            base + "/api/v1/jobs",
            data=json.dumps({"argv": ["research", "--help"]}).encode("utf-8"),
            headers={"Content-Type": "application/json"}, method="POST")
        created = urllib.request.urlopen(request, timeout=5)
        assert created.status == 201
        job = json.loads(created.read().decode("utf-8"))
        assert job["id"].startswith("job-")
        assert job["argv"] == ["research", "--help"]
        assert job["directory"].startswith(str(Path(workdir) / "runs" / "cockpit"))
        detail = json.loads(urllib.request.urlopen(
            base + "/api/v1/jobs/" + job["id"], timeout=5).read().decode("utf-8"))
        assert detail["id"] == job["id"]
        listing = json.loads(urllib.request.urlopen(
            base + "/api/v1/jobs", timeout=5).read().decode("utf-8"))
        assert any(item["id"] == job["id"] for item in listing)

        # Follow-up checks are explicit, separate local jobs.  The source is
        # always recorded as a read-only input and is never modified.
        while detail["state"] in {"QUEUED", "RUNNING", "CANCEL_REQUESTED"}:
            time.sleep(0.02)
            detail = json.loads(urllib.request.urlopen(
                base + "/api/v1/jobs/" + job["id"], timeout=5).read().decode("utf-8"))
        # Management reads must neither escape artifacts/ nor rewrite metadata
        # or transform raw log bytes (including CRLF, NUL and UTF-8).
        job_url = base + "/api/v1/jobs/" + job["id"]
        job_directory = Path(job["directory"])
        artifacts = Path(job["artifacts_directory"])
        empty_artifacts = urllib.request.urlopen(job_url + "/artifacts", timeout=5)
        assert empty_artifacts.status == 200
        assert empty_artifacts.headers["Content-Type"] == "application/json"
        assert json.loads(empty_artifacts.read()) == {"job_id": job["id"], "artifacts": []}
        (artifacts / "result.bruce").write_bytes(b"artifact\x00\r\n")
        (artifacts / "nested").mkdir()
        (artifacts / "nested" / "hidden.bruce").write_bytes(b"nested")
        outside = Path(workdir) / "outside.bruce"
        outside.write_bytes(b"outside")
        try:
            (artifacts / "escape.bruce").symlink_to(outside)
        except (OSError, NotImplementedError):
            pass
        metadata_before = (job_directory / "job.json").read_bytes()
        listed = json.loads(urllib.request.urlopen(
            job_url + "/artifacts", timeout=5).read())
        assert listed == {"job_id": job["id"],
                          "artifacts": [{"path": "result.bruce", "bytes": 11}]}
        for stream, content in (("stdout", b"raw stdout\r\n\x00\xc3\xa4"),
                                ("stderr", b"raw stderr\r\n\x00\xc3\xb6")):
            log_path = job_directory / (stream + ".log")
            original_log = log_path.read_bytes()
            log_path.unlink()
            missing = urllib.request.urlopen(job_url + "/logs/" + stream, timeout=5)
            assert missing.status == 200
            assert missing.headers["Content-Type"] == "text/plain; charset=utf-8"
            assert missing.read() == b""
            assert not log_path.exists(), "reading a missing log created it"
            log_path.write_bytes(content)
            raw = urllib.request.urlopen(job_url + "/logs/" + stream, timeout=5)
            assert raw.status == 200
            assert raw.headers["Content-Type"] == "text/plain; charset=utf-8"
            assert raw.read() == content
            assert log_path.read_bytes() == content
            log_path.write_bytes(original_log)
        assert (job_directory / "job.json").read_bytes() == metadata_before
        for route in ("artifacts", "logs/stdout", "logs/stderr"):
            try:
                urllib.request.urlopen(base + "/api/v1/jobs/job-999999/" + route, timeout=5)
                raise AssertionError("unknown management job unexpectedly succeeded")
            except urllib.error.HTTPError as error:
                assert error.code == 404
                assert error.headers["Content-Type"] == "application/json"
                assert json.loads(error.read()) == {"error": "CockpitUnknownJob"}
        source = Path(workdir) / "external.bruce"
        source_bytes = b"external cockpit source\n"
        source.write_bytes(source_bytes)
        source_digest = hashlib.sha256(source_bytes).hexdigest()
        source_before = source.read_bytes()
        action_ids = set()
        for action, command in (("inspect", "inspect-run"),
                                ("assess", "assess-run"),
                                ("replay", "replay"),
                                ("gate", "gate-run")):
            action_request = urllib.request.Request(
                base + "/api/v1/jobs/" + job["id"] + "/" + action,
                data=json.dumps({"path": str(source)}).encode("utf-8"),
                headers={"Content-Type": "application/json"}, method="POST")
            action_response = urllib.request.urlopen(action_request, timeout=5)
            assert action_response.status == 201
            action_job = json.loads(action_response.read().decode("utf-8"))
            assert action_job["id"] != job["id"]
            assert action_job["id"] not in action_ids
            action_ids.add(action_job["id"])
            assert action_job["argv"] == [command, str(source.resolve())]
            assert action_job["inputs"] == [{"path": str(source.resolve()),
                                              "sha256": source_digest}]
            assert source.read_bytes() == source_before
            if action == "replay":
                # The malformed source triggers the same Core failure through
                # HTTP as a direct CLI call; management must preserve it.
                direct = run(command, str(source.resolve()))
                while action_job["state"] in {"QUEUED", "RUNNING", "CANCEL_REQUESTED"}:
                    time.sleep(0.02)
                    action_job = json.loads(urllib.request.urlopen(
                        base + "/api/v1/jobs/" + action_job["id"], timeout=5).read())
                assert direct.returncode != 0
                assert action_job["state"] == "FAILED"
                assert action_job["return_code"] == direct.returncode
                assert action_job["stdout"] == direct.stdout
                assert action_job["stderr"] == direct.stderr
                action_url = base + "/api/v1/jobs/" + action_job["id"]
                for stream in ("stdout", "stderr"):
                    assert urllib.request.urlopen(
                        action_url + "/logs/" + stream, timeout=5).read() == getattr(
                            direct, stream).encode("utf-8")
                assert json.loads(urllib.request.urlopen(
                    action_url + "/artifacts", timeout=5).read()) == {
                        "job_id": action_job["id"], "artifacts": []}
                after_reads = json.loads(urllib.request.urlopen(action_url, timeout=5).read())
                assert after_reads == action_job

        missing_body = urllib.request.Request(
            base + "/api/v1/jobs/" + job["id"] + "/replay",
            data=b"", headers={"Content-Type": "application/json"}, method="POST")
        try:
            urllib.request.urlopen(missing_body, timeout=5)
            raise AssertionError("action without explicit source unexpectedly succeeded")
        except urllib.error.HTTPError as error:
            assert error.code == 400
            assert json.loads(error.read().decode("utf-8"))["error"] == "CockpitActionSourceRequired"

        relative_escape = urllib.request.Request(
            base + "/api/v1/jobs/" + job["id"] + "/replay",
            data=json.dumps({"path": "../outside.bruce"}).encode("utf-8"),
            headers={"Content-Type": "application/json"}, method="POST")
        try:
            urllib.request.urlopen(relative_escape, timeout=5)
            raise AssertionError("action outside artifacts unexpectedly succeeded")
        except urllib.error.HTTPError as error:
            assert error.code == 400
            assert json.loads(error.read().decode("utf-8"))["error"] == "CockpitActionSourceRejected"

        missing_source = urllib.request.Request(
            base + "/api/v1/jobs/" + job["id"] + "/inspect",
            data=json.dumps({"path": str(Path(workdir) / "missing.bruce")}).encode("utf-8"),
            headers={"Content-Type": "application/json"}, method="POST")
        try:
            urllib.request.urlopen(missing_source, timeout=5)
            raise AssertionError("missing action source unexpectedly succeeded")
        except urllib.error.HTTPError as error:
            assert error.code == 409
            assert json.loads(error.read().decode("utf-8"))["error"] == "CockpitActionSourceUnavailable"

        for body in ({"argv": ["research", ";"]},
                     {"argv": ["research"], "unknown": True}):
            bad = urllib.request.Request(
                base + "/api/v1/jobs", data=json.dumps(body).encode("utf-8"),
                headers={"Content-Type": "application/json"}, method="POST")
            try:
                urllib.request.urlopen(bad, timeout=5)
                raise AssertionError("invalid job request unexpectedly succeeded")
            except urllib.error.HTTPError as error:
                assert error.code == 400
                assert json.loads(error.read().decode("utf-8"))["error"].startswith("Cockpit")

        unknown = urllib.request.Request(base + "/api/v1/jobs/job-999999/cancel",
                                          data=b"", method="POST")
        try:
            urllib.request.urlopen(unknown, timeout=5)
            raise AssertionError("unknown cancellation unexpectedly succeeded")
        except urllib.error.HTTPError as error:
            assert error.code == 404
            assert json.loads(error.read().decode("utf-8"))["error"] == "CockpitUnknownJob"

        # ADR-040: identical invalid JSON reaches different first failures:
        # action routes resolve the parent before parsing, cancel rejects bytes
        # before resolving it, and action media/size admission precedes both.
        priority_job_ids = {item["id"] for item in json.loads(urllib.request.urlopen(
            base + "/api/v1/jobs", timeout=5).read())}
        for action in ("inspect", "assess", "replay", "gate"):
            for selected_id, body, content_type, status, error_name in (
                    ("job-999999", b"not-json", "application/json", 404, "CockpitUnknownJob"),
                    (job["id"], b"not-json", "application/json", 400, "CockpitActionRequestMalformed"),
                    ("job-999999", b"not-json", "text/plain", 400, "CockpitContentTypeRequired"),
                    ("job-999999", b"x" * 65537, "text/plain", 400, "CockpitRequestTooLarge")):
                rejected = urllib.request.Request(
                    base + "/api/v1/jobs/" + selected_id + "/" + action, data=body,
                    headers={"Content-Type": content_type}, method="POST")
                try:
                    urllib.request.urlopen(rejected, timeout=5)
                    raise AssertionError("combined action errors unexpectedly succeeded")
                except urllib.error.HTTPError as error:
                    assert error.code == status
                    assert json.loads(error.read()) == {"error": error_name}
        for content_type in ("application/json", "text/plain"):
            rejected = urllib.request.Request(base + "/api/v1/jobs/job-999999/cancel",
                data=b"not-json", headers={"Content-Type": content_type}, method="POST")
            try:
                urllib.request.urlopen(rejected, timeout=5)
                raise AssertionError("nonempty unknown cancellation unexpectedly succeeded")
            except urllib.error.HTTPError as error:
                assert error.code == 400
                assert json.loads(error.read()) == {"error": "CockpitRequestBodyRejected"}
        assert {item["id"] for item in json.loads(urllib.request.urlopen(
            base + "/api/v1/jobs", timeout=5).read())} == priority_job_ids

        collision = run("cockpit", "--port", str(startup["port"]))
        assert collision.returncode == 2
        assert json.loads(collision.stderr)["error"] == "CockpitPortUnavailable"

        # The upper boundary remains inclusive and preserves all byte/digest
        # bindings for both fixed-length and chunked uploads.
        boundary_bytes = b"BRUCE_" + b"b" * (import_limit - 6)
        for chunked in (False, True):
            connection = http.client.HTTPConnection("127.0.0.1", startup["port"], timeout=10)
            try:
                connection.request("POST", "/api/v1/imports",
                                   body=iter([boundary_bytes]) if chunked else boundary_bytes,
                                   headers={"Authorization": "Bearer " + startup["token"],
                                            "Origin": base, "Content-Type": "application/octet-stream",
                                            "X-Bruce-Import-Name": "boundary.bruce"},
                                   encode_chunked=chunked)
                response = connection.getresponse()
                boundary = json.loads(response.read())
                assert response.status == 201, boundary
                assert boundary["bytes"] == import_limit
                assert boundary["sha256"] == hashlib.sha256(boundary_bytes).hexdigest()
                assert Path(boundary["path"]).read_bytes() == boundary_bytes
            finally:
                connection.close()
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait(timeout=5)


if mtls_enabled:
    def openssl(*args):
        completed = subprocess.run(["openssl", *map(str, args)], capture_output=True,
                                   text=True, timeout=30)
        assert completed.returncode == 0, completed.stderr
        return completed.stdout

    def mtls_store(path, fingerprint, status="active", entry_policy="policy-1"):
        fields = ["operator-truststore-v1", "store-1", "policy-1",
                  "0" if status is None else "1"]
        if status is not None:
            fields.extend([fingerprint, "operator-1", entry_policy, status])
        body = b"".join(str(len(field.encode())).encode() + b":" + field.encode()
                        for field in fields)
        path.write_bytes(b"BRUCE_MTLS_TRUSTSTORE_V1\n" +
                         hashlib.sha256(body).hexdigest().encode() + b"\n" + body)

    with tempfile.TemporaryDirectory(prefix="bruce-cockpit-mtls-") as directory:
        root = Path(directory)
        ca_key, ca_cert = root / "ca.key", root / "ca.pem"
        server_key, server_cert = root / "server.key", root / "server.pem"
        client_key, client_csr, client_cert = (root / "client.key", root / "client.csr",
                                                root / "client.pem")
        rogue_key, rogue_cert = root / "rogue.key", root / "rogue.pem"
        store = root / "store.bruce"
        openssl("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                "-keyout", ca_key, "-out", ca_cert, "-subj", "/CN=Bruce Test CA",
                "-addext", "basicConstraints=critical,CA:TRUE",
                "-addext", "keyUsage=critical,keyCertSign,cRLSign")
        openssl("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                "-keyout", server_key, "-out", server_cert, "-subj", "/CN=127.0.0.1")
        openssl("req", "-newkey", "rsa:2048", "-nodes", "-keyout", client_key,
                "-out", client_csr, "-subj", "/CN=Bruce Operator")
        extension = root / "client.ext"
        extension.write_text("basicConstraints=critical,CA:FALSE\n"
                             "keyUsage=critical,digitalSignature\n"
                             "extendedKeyUsage=clientAuth\n")
        openssl("x509", "-req", "-in", client_csr, "-CA", ca_cert, "-CAkey", ca_key,
                "-CAcreateserial", "-out", client_cert, "-days", "1", "-extfile", extension)
        openssl("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                "-keyout", rogue_key, "-out", rogue_cert, "-subj", "/CN=Rogue")
        der = subprocess.run(["openssl", "x509", "-in", str(client_cert), "-outform", "DER"],
                             capture_output=True, timeout=30)
        assert der.returncode == 0
        fingerprint = hashlib.sha256(der.stdout).hexdigest()
        secret_files = (ca_key, ca_cert, server_key, server_cert,
                        client_key, client_cert, rogue_key, rogue_cert)
        secret_material = [path.read_bytes() for path in secret_files]
        secret_material.append(der.stdout)
        for pem in secret_material[:-1]:
            secret_material.extend(line for line in pem.splitlines() if len(line) >= 32)
        secret_markers = (b"-----BEGIN", b"-----END", b"PRIVATE KEY")

        def assert_no_tls_secret(label, payload):
            assert isinstance(payload, bytes), label
            for marker in secret_markers:
                assert marker not in payload, f"{label} contains TLS PEM/key marker"
            for material in secret_material:
                assert material not in payload, f"{label} contains TLS certificate/key bytes"

        # The scanner must reject both a complete PEM block and its body.
        for poison in (ca_cert.read_bytes(), client_key.read_bytes().splitlines()[1]):
            try:
                assert_no_tls_secret("poisoned test payload", poison)
                raise AssertionError("TLS secret scanner accepted a poisoned payload")
            except AssertionError as error:
                assert "contains TLS" in str(error), error
        mtls_store(store, fingerprint)
        invalid_store = root / "invalid-store.bruce"
        invalid_store.write_bytes(b"not a canonical truststore")
        tls_args = ("--tls-cert", str(server_cert), "--tls-key", str(server_key))
        mtls_args = ("--mtls-ca-bundle", str(ca_cert), "--mtls-truststore", str(store),
                     "--mtls-policy-version", "policy-1")
        for extra, expected in (
                (("--mtls-ca-bundle", str(ca_cert)), "CockpitMtlsConfigurationRequired"),
                (("--mtls-truststore", str(store)), "CockpitMtlsConfigurationRequired"),
                (("--mtls-policy-version", "policy-1"), "CockpitMtlsConfigurationRequired"),
                (("--mtls-ca-bundle", str(root / "missing-ca"),
                  "--mtls-truststore", str(store), "--mtls-policy-version", "policy-1"),
                 "CockpitMtlsCaBundleUnreadable"),
                (("--mtls-ca-bundle", str(ca_cert),
                  "--mtls-truststore", str(root / "missing-store"),
                  "--mtls-policy-version", "policy-1"), "CockpitMtlsTruststoreUnreadable"),
                (("--mtls-ca-bundle", str(ca_cert), "--mtls-truststore", str(store),
                  "--mtls-policy-version", "wrong-policy"),
                 "CockpitMtlsTruststorePolicyMismatch"),
                (("--mtls-ca-bundle", str(ca_cert),
                  "--mtls-truststore", str(invalid_store),
                  "--mtls-policy-version", "policy-1"),
                 "CockpitMtlsTruststoreInvalid")):
            result = run("cockpit", *tls_args, *extra)
            assert result.returncode == 2, result.stderr
            assert json.loads(result.stderr)["error"] == expected, (extra, result.stderr)
        priority = run("cockpit", "--tls-cert", str(server_cert),
                       "--tls-key", str(server_cert), "--mtls-ca-bundle", str(ca_cert))
        assert json.loads(priority.stderr)["error"] == "CockpitTlsConfigurationInvalid"

        def call(url, context, token=None, origin=None, body=None):
            headers = {"Content-Type": "application/json"} if body is not None else {}
            if token is not None:
                headers["Authorization"] = "Bearer " + token
            if origin is not None:
                headers["Origin"] = origin
            request = urllib.request.Request(url + ("/api/v1/jobs" if body is not None
                                                  else "/api/v1/health"),
                                             data=body, headers=headers,
                                             method="POST" if body is not None else "GET")
            try:
                with plain_urlopen(request, context=context, timeout=5) as response:
                    return response.status, response.read()
            except urllib.error.HTTPError as error:
                return error.code, error.read()

        for case_index, (status, entry_policy) in enumerate(
                (("active", "policy-1"), (None, "policy-1"),
                 ("expired", "policy-1"), ("revoked", "policy-1"),
                 ("active", "other-policy"),
                 ("active", "policy-1"))):
            mtls_store(store, fingerprint, status, entry_policy)
            case_root = root / ("case-" + str(case_index))
            case_root.mkdir()
            jobs_root = case_root / "runs" / "cockpit"
            existing_jobs = set(jobs_root.glob("job-*"))
            server = subprocess.Popen([binary, "cockpit", *tls_args, *mtls_args],
                                      cwd=case_root, stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE, text=True)
            try:
                startup = json.loads(server.stdout.readline())
                assert startup["event"] == "cockpit_started"
                assert_no_tls_secret("startup JSON", json.dumps(startup).encode())
                url = f"https://127.0.0.1:{startup['port']}"
                trusted = ssl._create_unverified_context()
                trusted.load_cert_chain(client_cert, client_key)
                if status == "active" and entry_policy == "policy-1":
                    assert call(url, trusted, startup["token"])[0] == 200
                    assert call(url, trusted)[0] == 401
                    for token, origin, expected, error in (
                            (None, url, 401, "CockpitAuthRequired"),
                            ("wrong", url, 401, "CockpitAuthRejected"),
                            (startup["token"], None, 403, "CockpitOriginRejected"),
                            (startup["token"], "https://foreign.example", 403,
                             "CockpitOriginRejected"),
                            (startup["token"], url, 400, "CockpitRequestMalformed")):
                        code, content = call(url, trusted, token, origin, b"not-json")
                        response_error = json.loads(content)["error"]
                        assert (code, response_error) == (expected, error), (
                            "bearer-present" if token else "bearer-absent",
                            origin, code, expected, error)
                        assert_no_tls_secret("rejected HTTP JSON", content)
                    no_certificate = ssl._create_unverified_context()
                    try:
                        call(url, no_certificate, startup["token"], url,
                             b'{"argv":["calculate"]}')
                        raise AssertionError("missing client certificate reached HTTP")
                    except (urllib.error.URLError, OSError, http.client.HTTPException):
                        pass
                    rogue = ssl._create_unverified_context()
                    rogue.load_cert_chain(rogue_cert, rogue_key)
                    try:
                        call(url, rogue, startup["token"], url,
                             b'{"argv":["calculate"]}')
                        raise AssertionError("wrong CA reached HTTP")
                    except (urllib.error.URLError, OSError, http.client.HTTPException):
                        pass
                else:
                    try:
                        call(url, trusted, startup["token"], url,
                             b'{"argv":["calculate"]}')
                        raise AssertionError(f"{status}/{entry_policy} reached HTTP")
                    except (urllib.error.URLError, OSError, http.client.HTTPException):
                        pass
                    assert set(jobs_root.glob("job-*")) == existing_jobs, (
                        status, entry_policy, "denied handshake created a job")
                jobs_request = urllib.request.Request(
                    url + "/api/v1/jobs",
                    headers={"Authorization": "Bearer " + startup["token"]})
                if status == "active" and entry_policy == "policy-1":
                    with plain_urlopen(jobs_request, context=trusted,
                                       timeout=5) as response:
                        listing = response.read()
                    assert_no_tls_secret("job list after denied requests", listing)
                    assert json.loads(listing) == []
                    job_request = urllib.request.Request(
                        url + "/api/v1/jobs",
                        data=b'{"argv":["calculate","--op","add","--left","1","--right","1"]}',
                        headers={"Authorization": "Bearer " + startup["token"],
                                 "Origin": url, "Content-Type": "application/json"},
                        method="POST")
                    with plain_urlopen(job_request, context=trusted, timeout=5) as response:
                        assert response.status == 201
                        created = response.read()
                    assert_no_tls_secret("created job JSON", created)
                    job = json.loads(created)
                    job_url = url + "/api/v1/jobs/" + job["id"]
                    deadline = time.monotonic() + 10
                    while True:
                        with plain_urlopen(
                                urllib.request.Request(job_url, headers={
                                    "Authorization": "Bearer " + startup["token"]}),
                                context=trusted, timeout=5) as response:
                            detail = response.read()
                        assert_no_tls_secret("job detail JSON", detail)
                        job = json.loads(detail)
                        if job["state"] not in {"QUEUED", "RUNNING", "CANCEL_REQUESTED"}:
                            break
                        assert time.monotonic() < deadline, "mTLS job did not finish"
                        time.sleep(0.02)
                    assert job["state"] == "SUCCEEDED", job
                    job_directory = Path(job["directory"])
                    for filename in ("job.json", "stdout.log", "stderr.log"):
                        assert_no_tls_secret(filename, (job_directory / filename).read_bytes())
                    for endpoint in ("/logs/stdout", "/logs/stderr", "/artifacts"):
                        request = urllib.request.Request(job_url + endpoint, headers={
                            "Authorization": "Bearer " + startup["token"]})
                        with plain_urlopen(request, context=trusted, timeout=5) as response:
                            assert_no_tls_secret(endpoint + " HTTP response", response.read())
                    replay_source = root / "invalid-replay.bruce"
                    replay_source.write_bytes(b"not a replay bundle\n")
                    replay_request = urllib.request.Request(
                        job_url + "/replay",
                        data=json.dumps({"path": str(replay_source)}).encode(),
                        headers={"Authorization": "Bearer " + startup["token"],
                                 "Origin": url, "Content-Type": "application/json"},
                        method="POST")
                    with plain_urlopen(replay_request, context=trusted,
                                       timeout=5) as response:
                        assert response.status == 201
                        replay_created = response.read()
                    assert_no_tls_secret("replay job JSON", replay_created)
                    replay_job = json.loads(replay_created)
                    replay_url = url + "/api/v1/jobs/" + replay_job["id"]
                    replay_deadline = time.monotonic() + 10
                    while replay_job["state"] in {"QUEUED", "RUNNING", "CANCEL_REQUESTED"}:
                        assert time.monotonic() < replay_deadline, "mTLS replay did not finish"
                        time.sleep(0.02)
                        request = urllib.request.Request(replay_url, headers={
                            "Authorization": "Bearer " + startup["token"]})
                        with plain_urlopen(request, context=trusted, timeout=5) as response:
                            replay_detail = response.read()
                        assert_no_tls_secret("replay detail JSON", replay_detail)
                        replay_job = json.loads(replay_detail)
                    assert replay_job["state"] == "FAILED", replay_job
                    replay_directory = Path(replay_job["directory"])
                    for filename in ("job.json", "stdout.log", "stderr.log"):
                        assert_no_tls_secret("replay " + filename,
                                             (replay_directory / filename).read_bytes())
                    for endpoint in ("/logs/stdout", "/logs/stderr", "/artifacts"):
                        request = urllib.request.Request(replay_url + endpoint, headers={
                            "Authorization": "Bearer " + startup["token"]})
                        with plain_urlopen(request, context=trusted, timeout=5) as response:
                            assert_no_tls_secret("replay " + endpoint + " response",
                                                 response.read())
                    with plain_urlopen(jobs_request, context=trusted, timeout=5) as response:
                        completed_jobs = response.read()
                    assert_no_tls_secret("completed job list JSON", completed_jobs)
                    assert {item["id"] for item in json.loads(completed_jobs)} == {
                        job["id"], replay_job["id"]}
            finally:
                server.terminate()
                try:
                    server.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait(timeout=5)
