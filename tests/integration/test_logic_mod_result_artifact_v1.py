"""Real signed-package output publication, paired options and fresh replay."""
import hashlib
import json
import os
import pathlib
import stat
import subprocess
import sys
import tempfile

bruce, fixture, worker = sys.argv[1:]
DIGEST = "31540b43fa20cb275095247f6784794cecee19c0117b76238fa153964a52af5b"


def field(value):
    value = value if isinstance(value, bytes) else str(value).encode()
    return str(len(value)).encode() + b":" + value


with tempfile.TemporaryDirectory(prefix="bruce-result-cli-") as tmp:
    root = pathlib.Path(tmp)
    subprocess.run([fixture, worker, tmp, "export"], check=True)
    trust, store = root / "trust.txt", root / "store"

    def session(package="output", actions="install,activate,run,inspect,replay", extra=(),
                text="3/4 hoch 2", code=0, timeout=10):
        p = subprocess.run([bruce, "logic-mod-plugin-v1", "session", str(root / (package + ".brucemod")),
                            "--trust-store", str(trust), "--actions", actions,
                            "--input", text, *map(str, extra)], capture_output=True, text=True, timeout=timeout)
        assert p.returncode == (2 if code == 1 else code), (p.returncode, p.stdout, p.stderr)
        assert str(root) not in p.stdout and str(root) not in p.stderr, "local path leaked"
        if code == 1:
            assert not p.stdout, "rejection emitted partial result"
            return p
        return json.loads(p.stdout)

    # Bound the inherited conflict-open regression independently of worker timeouts.
    fifo = store / "manifests/fifo-result.manifest"
    os.mkfifo(fifo, 0o600)
    objects_before = sorted(p.relative_to(store) for p in (store / "objects").rglob("*.blob"))
    for text in ("3/4 hoch 2", "timeout"):
        rejected = session(actions="install,activate,run", extra=("--output-manifest", "fifo-result", "--store", store),
                           text=text, code=1, timeout=2)
        assert "ArtifactManifestConflict" in rejected.stderr
        assert stat.S_ISFIFO(fifo.lstat().st_mode), "FIFO output target was replaced"
        assert sorted(p.relative_to(store) for p in (store / "objects").rglob("*.blob")) == objects_before
    fifo.unlink()

    pair = ("--output-manifest", "result", "--store", store)
    record = root / "result.record"
    good = session(extra=(*pair, "--record", record))
    run = good["runs"][0]
    assert run["accepted"] and run["result"] == "9/16" and good["replay_verified"]
    assert {key: run[key] for key in ("output_manifest_id", "output_object_index", "output_sha256",
                                     "output_size_bytes", "output_codec")} == {
        "output_manifest_id": "result", "output_object_index": 0, "output_sha256": DIGEST,
        "output_size_bytes": 4, "output_codec": "logic-mod-result-text-v1"}
    data = record.read_bytes()
    suffix = b"".join(map(field, ("output.v1", "1", "result", "0", DIGEST, "4", "logic-mod-result-text-v1")))
    assert data.endswith(suffix) and str(root).encode() not in data
    obj = store / "objects" / DIGEST[:2] / (DIGEST + ".blob")
    assert obj.read_bytes() == b"9/16", "published protocol frame instead of host result"
    replay = ("--replay-record", record, *pair)
    assert session(actions="install,activate,replay", extra=replay)["replay_verified"]
    # Idempotence and combined references use one shared Store singleton.
    assert session(extra=pair)["replay_verified"]
    assert session("combined", extra=("--input-artifact", "good:0", "--output-manifest", "combined",
                                      "--store", store))["replay_verified"]

    for extra in (("--output-manifest", "result"), ("--store", store),
                  (*pair, "--output-manifest", "second"), (*pair, "--store", store),
                  ("--output-manifest", "", "--store", store),
                  ("--output-manifest", "result", "--store", ""),
                  ("--input-artifact", "good:0", "--output-manifest", "result"),
                  ("--input-artifact", "good:0", *pair, "--store", store),
                  ("--input-artifact", "good:0", *pair, "--output-manifest", "second")):
        session(extra=extra, code=1)
    for manifest in ("../escape", "bad/id", "https://remote", "."):
        session(extra=("--output-manifest", manifest, "--store", store), code=1)
    invalid_store = root / "store-file"
    invalid_store.write_bytes(b"not a directory")
    invalid_store.chmod(0o600)
    session(extra=("--output-manifest", "store-error", "--store", invalid_store), code=1)
    for actions in ("install,run", "install,activate,disable,run", "install,activate,revoke,run"):
        session(actions=actions, extra=("--output-manifest", "denied", "--store", store), code=1)
        assert not (store / "manifests/denied.manifest").exists()
    session("legacy", extra=("--output-manifest", "denied", "--store", store), code=1)
    session("payload", extra=("--input-artifact", "good:0", "--output-manifest", "denied", "--store", store), code=1)
    assert not (store / "manifests/denied.manifest").exists()
    for package, extra in (("output", ()), ("combined", ("--input-artifact", "good:0"))):
        for text in ("wrong", "malformed", "timeout", "crash", "output", "filesystem", "network", "fork"):
            name = package + "-" + text
            failed = session(package, "install,activate,run", (*extra, "--output-manifest", name, "--store", store), text, 2)
            assert not failed["runs"][0]["accepted"]
            assert not any(k.startswith("output_") for k in failed["runs"][0])
            assert not (store / "manifests" / (name + ".manifest")).exists()
    # Reusing the input manifest must preserve its existing immutable bytes.
    existing = (store / "manifests/good.manifest").read_bytes()
    conflict = session(extra=("--output-manifest", "good", "--store", store), code=1)
    assert "ArtifactManifestConflict" in conflict.stderr
    assert (store / "manifests/good.manifest").read_bytes() == existing
    session(actions="install,activate,replay", extra=("--replay-record", record), code=1)
    session(actions="install,activate,replay", extra=("--replay-record", record, "--output-manifest", "combined", "--store", store), code=1)
    manifest = store / "manifests/result.manifest"
    original = manifest.read_bytes()
    for before, after in ((b"READY", b"FAILED"), (b"logic-mod-result-text-v1", b"other-result-codec-v1")):
        assert before in original
        manifest.write_bytes(original.replace(before, after))
        session(actions="install,activate,replay", extra=replay, code=1)
        manifest.write_bytes(original)
    obj.write_bytes(b"1/16")
    session(actions="install,activate,replay", extra=replay, code=1)
    obj.unlink()
    session(actions="install,activate,replay", extra=replay, code=1)
    obj.write_bytes(b"9/16")
    obj.chmod(0o600)
    manifest.unlink()
    session(actions="install,activate,replay", extra=replay, code=1)
    manifest.write_bytes(original)
    manifest.chmod(0o600)
    assert session(actions="install,activate,replay", extra=replay)["replay_verified"]
    # Output must leave input-free and input-only records and digest preimages unchanged.
    for package, extra in (("legacy", ()), ("payload", ("--input-artifact", "good:0", "--store", store))):
        path = root / (package + ".record")
        result = session(package, extra=(*extra, "--record", path))
        blob = path.read_bytes()
        assert b"output.v1" not in blob
        rest = blob.removeprefix(b"BRUCE_LOGIC_RUN_V1\n")
        values = []
        while rest:
            length, rest = rest.split(b":", 1)
            length = int(length)
            values.append(rest[:length])
            rest = rest[length:]
        assert len(values) == (11 if package == "legacy" else 17)
        tail = b"".join(map(field, values[11:]))
        assert values[8].decode() == hashlib.sha256(b"BRUCE_LOGIC_REQUEST_DIGEST_V1\n" +
                                                   b"".join(map(field, values[5:8])) + tail).hexdigest()
        assert result["replay_verified"]
        session(package, "install,activate,replay", (*extra, "--output-manifest", "result",
                *(("--store", store) if package == "legacy" else ()), "--replay-record", path), code=1)
print("logic result artifact CLI workflows passed")
