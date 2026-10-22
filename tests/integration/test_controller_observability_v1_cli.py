import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile

BRUCE = str(Path(sys.argv[1]).resolve())
SPEC = b'''BRUCE_DAG_SCHEDULER_SPEC_V3
dag-id=observe-cli
max-started-nodes=1
max-total-receipt-bytes=4096

node=a
depends=
callback=literal-v1
request-hex=78
'''
ERROR = b'{"contract":"controller-observability-v1","ok":false,"error":"INVALID_EVIDENCE"}\n'
with tempfile.TemporaryDirectory(prefix="bruce-observe-cli-") as directory:
    root = Path(directory)
    spec, bundle = root / "spec", root / "bundle"
    spec.write_bytes(SPEC)
    executed = subprocess.run([BRUCE, "dag-scheduler-v5", "execute", str(spec), "--output", str(bundle)], capture_output=True, timeout=20)
    assert executed.returncode == 0, executed.stderr
    valid = subprocess.run([BRUCE, "controller-observability-v1", "v5", str(bundle)], capture_output=True, timeout=20)
    assert valid.returncode == 0 and valid.stderr == b"", valid.stderr
    public = json.loads(valid.stdout)
    assert public == {"schema":"controller-observability-crosswalk-v1", "source":2,
                      "id":"redacted-v5-worker-run-inspection-v1", "phase":2, "terminal":4,
                      "cancellation":0, "success":2, "publication":2, "durable_evidence":2,
                      "diagnostic":2}, public
    for bad in (root / "missing", root / "corrupt"):
        if bad.name == "corrupt": bad.write_bytes(b"not a v5 bundle")
        reply = subprocess.run([BRUCE, "controller-observability-v1", "v5", str(bad)], capture_output=True, timeout=20)
        assert reply.returncode == 2 and reply.stdout == b"" and reply.stderr == ERROR, reply
    fifo_request = bundle.with_name(bundle.name + ".run") / "inputs" / "a.bin"
    fifo_request.unlink()
    os.mkfifo(fifo_request)
    reply = subprocess.run([BRUCE, "controller-observability-v1", "v5", str(bundle)], capture_output=True, timeout=2)
    assert reply.returncode == 2 and reply.stdout == b"" and reply.stderr == ERROR, reply
    fifo_request.unlink()
    # Recreate a valid V5 bundle for the independent run-log FIFO regression.
    fresh = root / "fresh"
    executed = subprocess.run([BRUCE, "dag-scheduler-v5", "execute", str(spec), "--output", str(fresh)], capture_output=True, timeout=20)
    assert executed.returncode == 0, executed.stderr
    bundle = fresh
    fifo_log = bundle.with_name(bundle.name + ".run") / "control" / "run-log.jsonl"
    fifo_log.unlink()
    os.mkfifo(fifo_log)
    reply = subprocess.run([BRUCE, "controller-observability-v1", "v5", str(bundle)], capture_output=True, timeout=2)
    assert reply.returncode == 2 and reply.stdout == b"" and reply.stderr == ERROR, reply
    # A second complete bundle supplies a valid but differently bound sibling run.
    other = root / "other"
    executed = subprocess.run([BRUCE, "dag-scheduler-v5", "execute", str(spec), "--output", str(other)], capture_output=True, timeout=20)
    assert executed.returncode == 0, executed.stderr
    unbound = root / "unbound"
    unbound.write_bytes(other.read_bytes())
    shutil.copytree(str(other) + ".run", str(unbound) + ".run")
    reply = subprocess.run([BRUCE, "controller-observability-v1", "v5", str(unbound)], capture_output=True, timeout=20)
    assert reply.returncode == 2 and reply.stdout == b"" and reply.stderr == ERROR, reply
for args in ([], ["v5"], ["v5", "x", "extra"], ["offline", "x"], ["v6", "x", "--store", "y"]):
    reply = subprocess.run([BRUCE, "controller-observability-v1", *args], capture_output=True, timeout=20)
    assert reply.returncode == 2 and reply.stdout == b"" and reply.stderr == ERROR, (args, reply)
