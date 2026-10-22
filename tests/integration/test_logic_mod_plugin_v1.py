"""Real signed static worker, explicit in-process session and replay workflow."""
import json
import pathlib
import subprocess
import sys
import tempfile

bruce, fixture_tool, worker = sys.argv[1:]

def call(*args, ok=True):
    p = subprocess.run([bruce, "logic-mod-plugin-v1", *map(str, args)], text=True, capture_output=True, timeout=10)
    assert (p.returncode == 0) == ok, (p.returncode, p.stdout, p.stderr)
    return json.loads(p.stdout) if ok else p

with tempfile.TemporaryDirectory(prefix="bruce-logic-cli-") as tmp:
    root = pathlib.Path(tmp)
    subprocess.run([fixture_tool, worker, tmp, "export"], check=True)
    pkg, trust, record = root / "demo.brucemod", root / "trust.txt", root / "run.txt"
    def session(actions, *extra, ok=True):
        return call("session", pkg, "--trust-store", trust, "--actions", actions, *extra, ok=ok)
    inactive = session("install,inspect")
    assert inactive["state"] == "INSTALLED_INACTIVE" and inactive["runs"] == []
    active = session("install,activate,inspect")
    assert active["state"] == "ACTIVE" and active["runs"] == []
    good = session("install,activate,run,inspect,replay", "--input", "3/4 hoch 2", "--record", record)
    assert good["state"] == "ACTIVE" and good["replay_verified"]
    assert good["runs"][0]["result"] == "9/16" and good["runs"][0]["claim_scope"] == "candidate_only"
    assert record.read_bytes().startswith(b"BRUCE_LOGIC_RUN_V1\n")
    assert session("install,activate,replay", "--input", "3/4 hoch 2", "--replay-record", record)["replay_verified"]
    damaged = root / "damaged.txt"
    damaged.write_bytes(record.read_bytes().replace(b"candidate_only", b"proven"))
    session("install,activate,replay", "--input", "3/4 hoch 2", "--replay-record", damaged, ok=False)
    session("install,run", "--input", "3/4 hoch 2", ok=False)
    session("install,activate,disable,run", "--input", "3/4 hoch 2", ok=False)
    session("install,activate,revoke,run", "--input", "3/4 hoch 2", ok=False)
    session("install,activate,run", "--input", "timeout", ok=False)
    session("activate,run", "--input", "3/4 hoch 2", ok=False)
    session("install,activate,run", "--input", "3/4 hoch 2", "--claim-scope", "proven", ok=False)
    session("install,activate,run", "--input", "3/4 hoch 2", "--worker", "/bin/sh", ok=False)
    session("install,activate,run", "--input", "3/4 hoch 2", "--input", "x", ok=False)
    call("shell", ok=False)
    untrusted = root / "untrusted.txt"
    untrusted.write_text("BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n")
    call("session", pkg, "--trust-store", untrusted, "--actions", "install,activate,run", "--input", "3/4 hoch 2", ok=False)
print("logic mod plugin CLI workflows passed")
