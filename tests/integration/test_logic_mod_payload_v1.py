"""Signed worker + local store through the real session CLI and fresh replay."""
import json
import pathlib
import subprocess
import sys
import tempfile

bruce, fixture, worker = sys.argv[1:]
with tempfile.TemporaryDirectory(prefix="bruce-payload-cli-") as tmp:
    root = pathlib.Path(tmp)
    subprocess.run([fixture, worker, tmp, "export"], check=True)
    package, trust, store = root / "payload.brucemod", root / "trust.txt", root / "store"
    record = root / "run.record"

    def session(actions="install,activate,run,inspect,replay", extra=(), ok=True):
        p = subprocess.run([bruce, "logic-mod-plugin-v1", "session", str(package),
                            "--trust-store", str(trust), "--actions", actions,
                            "--input", "3/4 hoch 2", *map(str, extra)], capture_output=True, text=True, timeout=10)
        assert (p.returncode == 0) == ok, (p.returncode, p.stdout, p.stderr)
        if not ok:
            assert not p.stdout, "pre-spawn/replay rejection emitted partial result"
            return p
        return json.loads(p.stdout)

    pair = ("--input-artifact", "good:0", "--store", store)
    good = session(extra=(*pair, "--record", record))
    assert good["runs"][0]["result"] == "9/16" and good["replay_verified"]
    assert str(root).encode() not in record.read_bytes()
    assert session("install,activate,replay", (*pair, "--replay-record", record))["replay_verified"]
    assert session(extra=("--input-artifact", "limit:0", "--store", store))["runs"][0]["accepted"]
    for extra in (("--input-artifact", "good:0"), ("--store", store),
                  (*pair, "--input-artifact", "good:0"), (*pair, "--store", store),
                  ("--input-artifact", "", "--store", store), ("--input-artifact", "good:0", "--store", "")):
        session(extra=extra, ok=False)
    for reference in ("good", "good:", ":0", "good:-1", "good:+0", "good:00", "good:0:0", "good:18446744073709551616", "../good:0", "good:1", "missing:0", "oversize:0"):
        failed_record = root / "failed.record"
        session(extra=("--input-artifact", reference, "--store", store, "--record", failed_record), ok=False)
        assert not failed_record.exists()
    session("install,run", pair, ok=False)
    session("install,activate,disable,run", pair, ok=False)
    session("install,activate,revoke,run", pair, ok=False)
    session("install,activate,replay", ("--replay-record", record), ok=False)
    session("install,activate,replay", ("--input-artifact", "limit:0", "--store", store, "--replay-record", record), ok=False)
    object_path = store / "objects" / "ba" / "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad.blob"
    object_path.write_bytes(b"xyz")
    session("install,activate,replay", (*pair, "--replay-record", record), ok=False)
    object_path.unlink()
    session("install,activate,replay", (*pair, "--replay-record", record), ok=False)
print("logic payload CLI workflows passed")
