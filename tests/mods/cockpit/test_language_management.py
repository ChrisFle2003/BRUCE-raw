"""Cockpit language management is inspectable and never launches a job."""
import hashlib
import http.client
import json
import subprocess
import socket
import sys
import tempfile
import urllib.error
import urllib.request
from pathlib import Path

binary = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix="bruce-cockpit-language-") as workdir:
    server = subprocess.Popen([binary, "cockpit", "--port", "0"], cwd=workdir,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        startup = json.loads(server.stdout.readline())
        origin = f"http://127.0.0.1:{startup['port']}"
        headers = {"Authorization": "Bearer " + startup["token"], "Origin": origin,
                   "Content-Type": "application/json"}

        def request(path, body=None, status=200, extra=None):
            wire = body if isinstance(body, bytes) else (
                json.dumps(body).encode() if body is not None else None)
            req = urllib.request.Request(origin + "/api/v1/" + path, data=wire,
                                         headers=headers | (extra or {}))
            try:
                with urllib.request.urlopen(req, timeout=5) as response:
                    assert response.status == status, response.status
                    return json.loads(response.read())
            except urllib.error.HTTPError as error:
                payload = error.read()
                assert error.code == status, (error.code, status, payload)
                return json.loads(payload)

        def payload(text, aliases=("train", "holdout")):
            return {"text": text, "inputs": [
                {"alias": alias, "bytes": data,
                 "sha256": hashlib.sha256(data.encode()).hexdigest()}
                for alias, data in zip(aliases, ("0,0\n", "1,1\n"))]}

        def streamed_request(path, framing, prefix=b"", extra=None):
            # Send only a prefix of a declared large body. A response must arrive
            # before the rest of the body is sent, proving early admission/read limits.
            with socket.create_connection(("127.0.0.1", startup["port"]), timeout=2) as connection:
                raw_headers = headers | {"Host": "127.0.0.1", "Connection": "close"} | framing | (extra or {})
                wire = ("POST /api/v1/" + path + " HTTP/1.1\r\n" +
                        "".join(key + ": " + value + "\r\n" for key, value in raw_headers.items()) + "\r\n").encode()
                connection.sendall(wire + prefix)
                response = http.client.HTTPResponse(connection)
                response.begin()
                return response.status, json.loads(response.read())

        assert request("language/drafts") == []
        understood = request("language/drafts", payload(
            " Analysiere @train anhand @holdout auf rationale Beziehungen. "), 201)
        assert understood["status"] == "UNDERSTOOD"
        assert understood["normalized_text"] == "untersuche @train mit @holdout auf rationale formeln."
        assert understood["selected_id"] == ""
        assert understood["alternatives"] == [{"id": "rational-portfolio-v2",
            "rule": "rational-portfolio-v2 train=train holdout=holdout"}]
        assert all("bytes" not in item for item in understood["inputs"])
        assert request("language/drafts/" + understood["id"]) == understood
        ambiguous = request("language/drafts", payload(
            "Analysiere @left und @right auf rationale Formeln.", ("left", "right")), 201)
        assert ambiguous["status"] == "AMBIGUOUS" and ambiguous["selected_id"] == ""
        assert [item["id"] for item in ambiguous["alternatives"]] == [
            "train=left;holdout=right", "train=right;holdout=left"]
        select_path = "language/drafts/" + ambiguous["id"] + "/select"
        assert request(select_path, {"alternative": "unknown"}, 400) == {
            "error": "UnknownLanguageAlternative"}
        assert request("language/drafts/" + ambiguous["id"])["selected_id"] == ""
        selected = request(select_path, {"alternative": "train=right;holdout=left"})
        assert selected["selected_id"] == "train=right;holdout=left"
        assert selected["status"] == "AMBIGUOUS"
        assert request(select_path, {"alternative": selected["selected_id"]}, 400) == {
            "error": "InvalidLanguageSelection"}
        unsupported = request("language/drafts", payload("Finde die beste Formel."), 201)
        assert unsupported["status"] == "UNSUPPORTED"
        assert unsupported["alternatives"] == [] and unsupported["selected_id"] == ""
        assert request("language/drafts/" + unsupported["id"] + "/select",
                       {"alternative": "unknown"}, 400)["error"] == "UnsupportedLanguageDraft"
        assert request("language/drafts/language-999999", status=404) == {
            "error": "CockpitUnknownLanguageDraft"}
        assert request("language/drafts/language-999999/select", {"alternative": "x"}, 404) == {
            "error": "CockpitUnknownLanguageDraft"}
        # Selection resolves the ID before parsing; media admission precedes it.
        for body in (b"not-json", b'{"alternative":'):
            assert request("language/drafts/language-999999/select", body, 404) == {
                "error": "CockpitUnknownLanguageDraft"}
            assert request(select_path, body, 400) == {"error": "CockpitLanguageRequestMalformed"}
            assert request("language/drafts/language-999999/select", body, 400,
                           {"Content-Type": "text/plain"}) == {"error": "CockpitContentTypeRequired"}
        bad_digest = payload("Finde die beste Formel.")
        bad_digest["inputs"][0]["sha256"] = "0" * 64
        assert request("language/drafts", bad_digest, 400) == {"error": "LanguageInputDigestMismatch"}
        assert request("language/drafts", {"text": "x", "inputs": []}, 400) == {
            "error": "InvalidLanguageInputCount"}
        wrong_grammar = payload("Finde die beste Formel.") | {"grammar_version": "language-grammar-v1"}
        assert request("language/drafts", wrong_grammar, 400) == {"error": "UnsupportedLanguageGrammar"}
        duplicate_alias = payload("Finde die beste Formel.", ("train", "train"))
        assert request("language/drafts", duplicate_alias, 400) == {"error": "DuplicateLanguageAlias"}
        assert request("language/drafts", b"{}", 400,
                       {"Content-Type": "text/plain"}) == {"error": "CockpitContentTypeRequired"}
        for body in (b"{}", b'{"text":"x","text":"y","inputs":[]}',
                     b'{"text":"x","inputs":[],"execute":1}', b'{"text":true}',
                     b'{"text":"x",}', b'{"max_bits":01}', b'{"max_bits":-1}'):
            assert request("language/drafts", body, 400)["error"].startswith("CockpitLanguageRequest")
        assert request(select_path, b'{"alternative":"x","alternative":"y"}', 400) == {
            "error": "CockpitLanguageRequestMalformed"}
        assert request("language/drafts", b" " * (65536 + 1), 400) == {
            "error": "CockpitRequestTooLarge"}
        for path in ("language/drafts", select_path):
            assert streamed_request(path, {"Content-Length": "1048576"}) == (
                400, {"error": "CockpitRequestTooLarge"})
            assert streamed_request(path, {"Content-Length": "1048576"},
                extra={"Content-Type": "text/plain"}) == (400, {"error": "CockpitRequestTooLarge"})
            assert streamed_request(path, {"Transfer-Encoding": "chunked"},
                b"100001\r\nx",
                extra={"Content-Type": "text/plain"}) == (400, {"error": "CockpitContentTypeRequired"})
            # An incomplete first chunk must be rejected at 64 KiB rather than
            # buffered until the advertised chunk/body has finished arriving.
            assert streamed_request(path, {"Transfer-Encoding": "chunked"},
                b"100000\r\n" + b" " * 65537) == (400, {"error": "CockpitRequestTooLarge"})
            # HTTP requests without either framing header have no body. Stray
            # bytes are not buffered as an unknown-length language request.
            assert streamed_request(path, {}, b" " * 65537) == (
                400, {"error": "CockpitLanguageRequestMalformed"})
            assert streamed_request(path, {"Content-Length": "1048576"},
                extra={"Authorization": "Bearer invalid"}) == (401, {"error": "CockpitAuthRejected"})
            assert streamed_request(path, {"Content-Length": "1048576"},
                extra={"Origin": "http://foreign.invalid"}) == (403, {"error": "CockpitOriginRejected"})
        # A normally completed chunked body remains a supported JSON request,
        # and exactly 64 KiB reaches native selection validation.
        selection_wire = json.dumps({"alternative": selected["selected_id"]}).encode()
        selection_wire += b" " * (65536 - len(selection_wire))
        assert streamed_request(select_path, {"Transfer-Encoding": "chunked"},
            b"10000\r\n" + selection_wire + b"\r\n0\r\n\r\n") == (
                400, {"error": "InvalidLanguageSelection"})
        # Authentication and Origin remain ahead of all new route validation.
        assert request("language/drafts", b"bad", 401, {"Authorization": "Bearer invalid"}) == {
            "error": "CockpitAuthRejected"}
        assert request("language/drafts", b"bad", 403, {"Origin": "http://foreign.invalid"}) == {
            "error": "CockpitOriginRejected"}
        assert request("language/drafts") == [understood, selected, unsupported]
        # A newly requested dashboard can explicitly list and inspect existing
        # process-local snapshots without changing alternatives, jobs, or files.
        for _ in range(2):
            with urllib.request.urlopen(origin + "/?token=" + startup["token"], timeout=5) as dashboard:
                markup = dashboard.read()
                assert b"Load drafts" in markup and b"data-language-drafts" in markup
                assert b"const languageEnabled = true;" in markup
                assert b'await getJson("/api/v1/language/drafts")' in markup
            assert request("language/drafts") == [understood, selected, unsupported]
            assert request("language/drafts/" + selected["id"]) == selected
            assert request("language/drafts/" + understood["id"])["selected_id"] == ""
            assert request("jobs") == []
            assert request("imports") == []
        # The draft management UI still has no run route. Dispatch is separately optional.
        for action in (("run",) if len(sys.argv) > 2 and sys.argv[2] == "ON" else ("run", "dispatch")):
            req = urllib.request.Request(origin + "/api/v1/language/drafts/" +
                selected["id"] + "/" + action, data=b"{}", headers=headers)
            try:
                urllib.request.urlopen(req, timeout=5)
                raise AssertionError("unexpected language execution route")
            except urllib.error.HTTPError as error:
                assert error.code == 404
        for _ in range(125):
            request("language/drafts", payload("Finde die beste Formel."), 201)
        assert request("language/drafts", payload("Finde die beste Formel."), 409) == {
            "error": "CockpitLanguageDraftLimit"}
        assert len(request("language/drafts")) == 128
        assert request("language/drafts/" + selected["id"]) == selected
        assert request("jobs") == []
        assert not list((Path(workdir) / "runs" / "cockpit").glob("job-*"))
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait(timeout=5)
