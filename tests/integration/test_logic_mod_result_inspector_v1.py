"""Inspect real run records without a registry, worker, repair, or disclosure."""
import hashlib
import json
import os
import pathlib
import stat
import subprocess
import sys
import tempfile

bruce, fixture, worker = map(os.path.abspath, sys.argv[1:])
KEYS = ["contract", "action", "status", "accepted", "input_present", "output_present",
        "binding_identity", "output_manifest_id", "output_sha256", "output_size_bytes",
        "output_codec", "replay_readiness"]
DIGEST = "31540b43fa20cb275095247f6784794cecee19c0117b76238fa153964a52af5b"


def snapshot(root):
    # atime is allowed to change when reading; contents, inode, mode and mtime are not.
    result = {}
    for path in [root, *sorted(root.rglob("*"))]:
        info = path.lstat()
        result[str(path.relative_to(root))] = (info.st_mode, info.st_ino, info.st_nlink,
            info.st_size, info.st_mtime_ns,
            path.read_bytes() if stat.S_ISREG(info.st_mode) else
            os.readlink(path) if stat.S_ISLNK(info.st_mode) else None)
    return result


with tempfile.TemporaryDirectory(prefix="bruce-result-inspect-cli-") as tmp:
    root = pathlib.Path(tmp)
    subprocess.run([fixture, worker, tmp, "export"], check=True)
    store = root / "store"

    def session(package, record, extra=(), text="3/4 hoch 2"):
        p = subprocess.run([bruce, "logic-mod-plugin-v1", "session", str(root / (package + ".brucemod")),
            "--trust-store", str(root / "trust.txt"), "--actions", "install,activate,run",
            "--input", text, "--record", str(record), *map(str, extra)],
            capture_output=True, text=True, timeout=10)
        assert p.returncode in (0, 2) and p.stdout, (p.returncode, p.stdout, p.stderr)
        return json.loads(p.stdout)["runs"][0]

    record = root / "result.record"
    run = session("output", record, ("--output-manifest", "result", "--store", store))
    assert run["accepted"] and run["result"] == "9/16"
    legacy = root / "legacy.record"
    session("legacy", legacy)
    failed = root / "failed.record"
    session("legacy", failed, text="wrong")
    input_only = root / "input.record"
    session("payload", input_only, ("--input-artifact", "good:0", "--store", store))
    combined = root / "combined.record"
    session("combined", combined, ("--input-artifact", "good:0", "--output-manifest", "combined", "--store", store))

    # Inspection has no package/trust/worker argument. Hide those fixtures and use
    # an empty executable search path, so inspection cannot depend on a session.
    hidden = root / "hidden-packages"
    hidden.mkdir()
    for path in list(root.glob("*.brucemod")) + [root / "trust.txt"]:
        path.rename(hidden / path.name)
    empty = root / "empty-working-directory"
    empty.mkdir()

    def inspect(path=record, local_store=store, status="READY"):
        before = snapshot(root)
        p = subprocess.run([bruce, "logic-mod-result-inspect", str(path), "--store", str(local_store)],
            capture_output=True, text=True, timeout=3, cwd=empty, env={**os.environ, "PATH": ""})
        assert snapshot(root) == before, "inspection wrote or repaired fixture contents"
        assert p.returncode == 0 and not p.stderr, (p.returncode, p.stdout, p.stderr)
        assert p.stdout.endswith("\n") and len(p.stdout.splitlines()) == 1, "expected one LF-terminated JSON object"
        value = json.loads(p.stdout)
        assert list(value) == KEYS and value["status"] == status, value
        assert value["contract"] == "logic-mod-result-inspect-v1" and value["action"] == "inspect"
        assert str(root) not in p.stdout and "9/16" not in p.stdout and "3/4 hoch 2" not in p.stdout
        assert "BRUCE_LOGIC_RESPONSE" not in p.stdout and "CannotReadFile" not in p.stdout
        assert value["replay_readiness"] == ("READY" if status == "READY" else "NO_OUTPUT" if status == "NO_OUTPUT" else "NOT_READY")
        return value

    ready = inspect()
    assert ready["accepted"] and not ready["input_present"] and ready["output_present"]
    assert ready["output_manifest_id"] == "result" and ready["output_sha256"] == DIGEST
    assert ready["output_size_bytes"] == 4 and ready["output_codec"] == "logic-mod-result-text-v1"
    assert inspect(combined)["input_present"]
    for path, accepted, has_input in ((legacy, True, False), (failed, False, False), (input_only, True, True)):
        value = inspect(path, root / "nonexistent-store", "NO_OUTPUT")
        assert value["accepted"] == accepted and value["input_present"] == has_input and not value["output_present"]
        assert all(value[key] is None for key in KEYS[7:11])
    inspect(local_store=root / "nonexistent-store", status="STORE_ERROR")
    invalid_store = root / "store-file"
    invalid_store.write_bytes(b"private Store diagnostics")
    inspect(local_store=invalid_store, status="STORE_ERROR")
    malformed = root / "malformed.record"
    malformed.write_text("private malformed record " + str(root))
    assert not inspect(malformed, status="INVALID_RECORD")["accepted"]
    inspect(root / "missing.record", status="INVALID_RECORD")
    # The legacy parser leaves binding identity unconstrained. Its public
    # representation must be a digest, never a record-controlled path/payload.
    def field(value):
        return str(len(value)).encode() + b":" + value

    rest = legacy.read_bytes().removeprefix(b"BRUCE_LOGIC_RUN_V1\n")
    fields = []
    while rest:
        length, rest = rest.split(b":", 1)
        length = int(length)
        fields.append(rest[:length])
        rest = rest[length:]
    fields[5] = str(root / "private-binding-path").encode() + b" private-payload-marker"
    malicious = root / "malicious.record"
    malicious.write_bytes(b"BRUCE_LOGIC_RUN_V1\n" + b"".join(map(field, fields)))
    malicious.chmod(0o600)
    value = inspect(malicious, status="NO_OUTPUT")
    assert value["binding_identity"] == hashlib.sha256(fields[5]).hexdigest()
    assert "private-payload-marker" not in json.dumps(value)
    manifest = store / "manifests/result.manifest"
    original = manifest.read_bytes()
    saved = root / "saved.manifest"
    manifest.rename(saved)
    inspect(status="MISSING")
    saved.rename(manifest)
    for before, after, status in ((b"status=READY", b"status=FAILED", "NOT_READY"),
            (b"node_id=result", b"node_id=other", "IDENTITY_MISMATCH"),
            (b"logic-mod-result-text-v1", b"other-codec", "IDENTITY_MISMATCH")):
        manifest.write_bytes(original.replace(before, after))
        inspect(status=status)
    manifest.write_bytes(b"malformed private manifest " + str(root).encode())
    inspect(status="STORE_ERROR")
    manifest.write_bytes(original)
    obj = store / "objects" / DIGEST[:2] / (DIGEST + ".blob")
    obj.write_bytes(b"1/16")
    inspect(status="IDENTITY_MISMATCH")
    obj.rename(root / "saved.object")
    inspect(status="MISSING")
    (root / "saved.object").rename(obj)
    obj.write_bytes(b"9/16")
    # Symlink and FIFO paths remain bounded by existing read-only Store checks.
    manifest.rename(saved)
    manifest.symlink_to(saved)
    inspect(status="STORE_ERROR")
    manifest.unlink()
    os.mkfifo(manifest, 0o600)
    inspect(status="STORE_ERROR")
    manifest.unlink()
    saved.rename(manifest)
    inspect()

    # A FIFO run record would block if malformed options reached file reading.
    fifo_record = root / "do-not-read.record"
    os.mkfifo(fifo_record, 0o600)
    # Valid grammar must reject unsafe record inodes without opening in blocking
    # mode or following either final or intermediate symlink components.
    fifo_link = root / "fifo-link.record"
    fifo_link.symlink_to(fifo_record)
    regular_link = root / "regular-link.record"
    regular_link.symlink_to(legacy)
    device_link = root / "device-link.record"
    device_link.symlink_to("/dev/null")
    parent_link = root / "linked-parent"
    parent_link.symlink_to(empty, target_is_directory=True)
    private_record = empty / "private.record"
    private_record.write_bytes(legacy.read_bytes())
    private_record.chmod(0o600)
    for unsafe in (fifo_record, fifo_link, regular_link, device_link,
                   root, pathlib.Path("/dev/null"), pathlib.Path("/dev/zero"),
                   parent_link / private_record.name):
        inspect(unsafe, status="INVALID_RECORD")
    hardlink = root / "hardlink.record"
    os.link(private_record, hardlink)
    inspect(hardlink, status="INVALID_RECORD")
    hardlink.unlink()
    private_record.chmod(0o644)
    inspect(private_record, status="INVALID_RECORD")
    private_record.chmod(0o600)
    assert inspect(private_record, status="NO_OUTPUT")["binding_identity"]
    for args in ((), (str(fifo_record),), (str(fifo_record), "--store"),
            (str(fifo_record), "--store", ""), ("", "--store", str(store)),
            (" \t", "--store", str(store)), (str(fifo_record), "--store", " \n"),
            ("--store", str(store), str(fifo_record)),
            (str(fifo_record), "--store", str(store), "--store", str(store)),
            (str(fifo_record), "--store", str(store), "extra"),
            (str(fifo_record), "--worker", str(store))):
        before = snapshot(root)
        p = subprocess.run([bruce, "logic-mod-result-inspect", *args], capture_output=True, text=True, timeout=2)
        assert p.returncode == 2 and not p.stdout and str(root) not in p.stderr, (args, p.stdout, p.stderr)
        assert snapshot(root) == before
print("logic result inspector CLI workflows passed")
