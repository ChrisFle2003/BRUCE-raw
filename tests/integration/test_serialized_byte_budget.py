import json
import subprocess
import sys
import tempfile
from pathlib import Path

BIN = str(Path(sys.argv[1]).resolve())


def call(*args, code=0):
    process = subprocess.run([BIN, *map(str, args)], capture_output=True, text=True)
    assert process.returncode == code, process.stderr
    return json.loads(process.stdout if code == 0 else process.stderr)


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    bundle = root / "bundle"
    args = ["grid-batch", "--paths", "2,6;4,3", "--verification", "replay"]
    baseline = call(*args, "--output", bundle)
    size = bundle.stat().st_size
    exact = call(*args, "--max-serialized-bytes", size)
    assert exact["result"] == baseline["result"]
    assert exact["execution"]["contract"] == "application-execution-v15"
    assert exact["execution"]["serialization_budget"] == {
        "max_serialized_bytes": size,
        "serialized_bytes": size,
    }
    failure = call(*args, "--max-serialized-bytes", size - 1, code=2)
    assert failure["execution"]["contract"] == "application-execution-failure-v3"
    assert failure["execution"]["failed_at"] == "SERIALIZING"
    assert failure["execution"]["serialization_budget"] == {
        "max_serialized_bytes": size - 1,
        "serialized_bytes": size,
    }
    assert not failure.get("result")
    assert call("grid", "--path", "2,6", "--max-serialized-bytes", 1, code=2) == {
        "error": "SerializationBudgetRequiresVerification"
    }
print("serialized byte budget: exact limit succeeds and serializing overflow fails before replay")
