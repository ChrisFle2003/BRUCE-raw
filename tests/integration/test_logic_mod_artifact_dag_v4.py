"""Real signed workers prove V4 budgets, strict controls and V3 replay compatibility."""
import copy
import hashlib
import json
import os
import pathlib
import stat
import subprocess
import sys
import tempfile

bruce, exporter, worker, timed_worker, compiler = map(os.path.abspath, sys.argv[1:])
with tempfile.TemporaryDirectory(prefix="bruce-dag-cli-") as tmp:
    root = pathlib.Path(tmp)
    subprocess.run([exporter, worker, tmp, "export"], check=True)
    # Positive run calibrates observation of the host's real worker forks.
    source = root / "probe.cpp"
    source.write_text(r'''
#include <dlfcn.h>
#include <cstdio>
#include <fcntl.h>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <unistd.h>
extern "C" int openat(int dir, const char* path, int flags, ...) {
  mode_t mode = 0;
  if (flags & O_CREAT) { va_list args; va_start(args, flags); mode = va_arg(args, mode_t); va_end(args); }
  if (!std::strcmp(path, "fifo")) if (const char* p = std::getenv("BRUCE_TEST_ACCESSES")) {
    int fd = ::open(p, O_WRONLY|O_APPEND|O_CREAT, 0600);
    if (fd >= 0) { auto ignored=::write(fd,"access\n",7); (void)ignored; ::close(fd); }
  }
  return ::syscall(SYS_openat, dir, path, flags, mode);
}
void event(const char* action, pid_t pid) {
  if (const char* p = std::getenv("BRUCE_TEST_STARTS")) {
    int fd = ::open(p, O_WRONLY|O_APPEND|O_CREAT, 0600);
    if (fd >= 0) { char line[64]; const int size=std::snprintf(line,sizeof(line),"%s %ld\n",action,static_cast<long>(pid));
      auto ignored=::write(fd,line,size); (void)ignored; ::close(fd); }
  }
}
extern "C" pid_t fork() {
  const auto pid = reinterpret_cast<pid_t (*)()>(::dlsym(RTLD_NEXT,"fork"))();
  if (pid > 0) event("start",pid);
  return pid;
}
extern "C" pid_t waitpid(pid_t pid, int* status, int flags) {
  const auto result = reinterpret_cast<pid_t (*)(pid_t,int*,int)>(::dlsym(RTLD_NEXT,"waitpid"))(pid,status,flags);
  if (result > 0) event("reap",result);
  return result;
}
extern "C" pid_t wait4(pid_t pid, int* status, int flags, struct rusage* usage) {
  const auto result = reinterpret_cast<pid_t (*)(pid_t,int*,int,struct rusage*)>(::dlsym(RTLD_NEXT,"wait4"))(pid,status,flags,usage);
  if (result > 0) event("reap",result);
  return result;
}
''')
    library, starts, accesses = root / "probe.so", root / "starts", root / "accesses"
    subprocess.run([compiler, "-shared", "-fPIC", str(source), "-ldl", "-o", str(library)], check=True)
    env = {**os.environ, "LD_PRELOAD": str(library), "BRUCE_TEST_STARTS": str(starts), "BRUCE_TEST_ACCESSES": str(accesses)}
    spec_file, store, record = root / "dag.json", root / "dag-store", root / "dag.record"

    def node(name, parents=()):
        value = dict(id=name, package=str(root / ("combined.brucemod" if parents else "output.brucemod")),
                     trust_store=str(root / "trust.txt"), input="3/4 hoch 2", predecessors=list(parents),
                     output_manifest=name + "-out")
        if parents:
            value["input_manifest"] = name + "-input"
        return value

    spec = dict(schema_version=2, nodes=[node("d", ["b", "c"]), node("c", ["a"]), node("b", ["a"]), node("a")])

    def args(action="run", local_store=store, target=record, source=spec_file):
        return list(map(str, [action, source, "--store", local_store, "--record", target]))

    def invoke(arguments, code=0, count=0, error=None, no_access=False):
        starts.write_bytes(b"")
        accesses.write_bytes(b"")
        p = subprocess.run([bruce, "logic-mod-artifact-dag-v4", *arguments], capture_output=True,
                           text=True, timeout=5, env=env)
        assert p.returncode == code, (p.returncode, p.stdout, p.stderr)
        rows = [line.split() for line in starts.read_text().splitlines()]
        spawned = [pid for action, pid in rows if action == "start"]
        reaped = [pid for action, pid in rows if action == "reap"]
        assert len(spawned) == count and sorted(spawned) == sorted(reaped), (arguments, rows)
        if no_access:
            assert not accesses.read_bytes(), "invalid grammar/graph opened a forbidden package or Spec"
        assert str(root) not in p.stdout + p.stderr, "host path leaked"
        assert not any(s in p.stdout + p.stderr for s in ("3/4 hoch 2", "9/16", "BRUCE_LOGIC_RESULT")), "payload leaked"
        if error:
            assert not p.stdout and json.loads(p.stderr) == {"error": error}, (p.stdout, p.stderr)
            return
        assert not p.stderr and len(p.stdout.splitlines()) == 1, (p.stdout, p.stderr)
        result = json.loads(p.stdout)
        assert list(result) == ["contract", "action", "accepted", "status", "dag_id", "spec_digest", "node_order", "nodes"]
        assert result["contract"] == "logic-mod-artifact-dag-v4" and result["action"] == arguments[0]
        for n in result["nodes"]:
            assert list(n) == ["id", "result_artifact", "internal_input_artifact"]
            for artifact in (n["result_artifact"], n["internal_input_artifact"]):
                if artifact is not None:
                    assert list(artifact) == ["manifest_id", "sha256", "size_bytes", "codec"]
        return result

    def save(value):
        spec_file.write_text(json.dumps(value))

    save(spec)
    # Missing V4 dispatch fails here before any production implementation.
    good = invoke(args(), count=4)
    assert good["accepted"] and good["status"] == "ACCEPTED"
    assert good["node_order"] == ["a", "b", "c", "d"]
    original = record.read_bytes()
    assert original.startswith(b"BRUCE_LOGIC_MOD_ARTIFACT_DAG_V2\n")
    assert stat.S_IMODE(record.stat().st_mode) == 0o600

    controls = {"--max-parallel-nodes": (1, 4), "--max-total-wall-ms": (1, 60000),
                "--max-total-payload-bytes": (0, 8388608), "--max-started-workers": (1, 16),
                "--cancel-after-ms": (1, 60000)}
    generous = [item for flag, (_, maximum) in controls.items() for item in (flag, str(maximum))]
    for tail in (generous, list(reversed([generous[i:i+2] for i in range(0, len(generous), 2)]))):
        if tail and isinstance(tail[0], list):
            tail = [item for pair in tail for item in pair]
        target = root / ("controls-" + str(len(list(root.glob("controls-*")))) + ".record")
        assert invoke([*args(target=target), *tail], count=4) == good
        assert target.read_bytes() == original
    for limit in range(1, 5):
        target = root / ("parallel-" + str(limit) + ".record")
        assert invoke([*args(target=target), "--max-parallel-nodes", str(limit)], count=4) == good
        assert target.read_bytes() == original
    assert invoke(args("replay")) == {**good, "action": "replay"}
    # Record compatibility is exercised through the older real command too.
    older_record = root / "v3.record"
    older = subprocess.run([bruce, "logic-mod-artifact-dag-v3", *args(target=older_record)],
                           capture_output=True, text=True, timeout=5, env=env)
    assert older.returncode == 0 and older_record.read_bytes() == original, older.stderr
    assert invoke(args("replay", target=older_record)) == {**good, "action": "replay"}
    older_replay = subprocess.run([bruce, "logic-mod-artifact-dag-v3", *args("replay")],
                                  capture_output=True, text=True, timeout=5, env=env)
    assert older_replay.returncode == 0, older_replay.stderr

    # Strict options fail before opening even a FIFO Spec; no worker or Store.
    fifo = root / "fifo"; os.mkfifo(fifo, 0o600)
    base = args(source=fifo, local_store=root / "invalid-store", target=root / "invalid.record")
    malformed = [[], ["unknown"], base[:-1], [*base, "extra"], [*base, "--store", str(store)]]
    for i in range(len(base)):
        for value in ("", " \t"):
            changed = base.copy(); changed[i] = value; malformed.append(changed)
    changed = base.copy(); changed[2:4], changed[4:6] = changed[4:6], changed[2:4]; malformed.append(changed)
    for flag, (minimum, maximum) in controls.items():
        invalid = [str(maximum + 1), "18446744073709551616", "-1", "+1", "01", "00", "1.0", "1x", "", " ", " 1", "1 "]
        if minimum:
            invalid.append("0")
        for value in invalid:
            malformed.append([*base, flag, value])
        for tail in ([flag], [flag, str(maximum), "extra"],
                     [flag, str(maximum), flag, str(maximum)]):
            malformed.append([*base, *tail])
        for value in (minimum, maximum):
            replay_base = ["replay", *base[1:]]
            malformed.append([*replay_base, flag, str(value)])
    malformed.append([*base, "--unknown", "2"])
    for arguments in malformed:
        invoke(arguments, 2, error="InvalidLogicModDagArguments", no_access=True)
        assert not (root / "invalid-store").exists() and not (root / "invalid.record").exists()
    # Host controls remain outside the JSON Spec grammar.
    for name in (flag[2:].replace("-", "_") for flag in controls):
        bad = copy.deepcopy(spec); bad[name] = 1; save(bad)
        invoke(args(local_store=root / "bad-spec-store", target=root / "bad-spec.record"),
               2, error="LogicModDagFailed", no_access=True)
        assert not (root / "bad-spec-store").exists() and not (root / "bad-spec.record").exists()
    save(spec)

    def stopped(tag, tail, count, completed, status="DAG_GLOBAL_BUDGET_EXHAUSTED"):
        destination, target = root / (tag + "-store"), root / (tag + ".record")
        result = invoke([*args(local_store=destination, target=target), *tail], 2, count=count)
        assert not result["accepted"] and result["status"] == status and result["dag_id"] is None
        assert [n["id"] for n in result["nodes"]] == completed
        assert not target.exists()
        for name in completed:
            assert (destination / "manifests" / (name + "-out.manifest")).is_file()
        for n in spec["nodes"]:
            if n["id"] not in completed:
                assert not (destination / "manifests" / (n["id"] + "-out.manifest")).exists()
                assert not (destination / "manifests" / (n["id"] + "-input.manifest")).exists()
        return destination

    # Full waves are refused; completed outputs survive and future inputs never publish.
    stopped("zero-payload", ["--max-total-payload-bytes", "0"], 1, ["a"])
    stopped("starts", ["--max-started-workers", "1"], 1, ["a"])
    stopped("whole-start-wave", ["--max-parallel-nodes", "2", "--max-started-workers", "2"], 1, ["a"])
    stopped("whole-payload-wave", ["--max-parallel-nodes", "2", "--max-total-payload-bytes", "149"], 1, ["a"])
    # A one-parent input has exactly 149 bytes; the budget is cumulative.
    spec = dict(schema_version=2, nodes=[node("a"), node("b", ["a"]), node("c", ["b"])])
    save(spec)
    stopped("exact-payload", ["--max-total-payload-bytes", "149"], 2, ["a", "b"])
    spec = dict(schema_version=2, nodes=[node("a"), node("b", ["a"])])
    save(spec)
    exact = invoke([*args(target=root / "exact.record"), "--max-total-payload-bytes", "149", "--max-started-workers", "2"], count=2)
    assert exact["accepted"] and exact["nodes"][1]["internal_input_artifact"]["size_bytes"] == 149
    spec = dict(schema_version=2, nodes=[node("a")]); save(spec)
    assert invoke([*args(target=root / "zero-root.record"), "--max-total-payload-bytes", "0"], count=1)["accepted"]
    spec = dict(schema_version=2, nodes=[node("a"), node("z")]); save(spec)
    destination = stopped("initial-wave", ["--max-parallel-nodes", "2", "--max-started-workers", "1"], 0, [])
    assert not destination.exists()

    # Timed workers prove cancellation/wall limits drain a complete running wave.
    timed = root / "timed"; timed.mkdir()
    subprocess.run([exporter, timed_worker, str(timed), "export"], check=True)
    spec = dict(schema_version=2, nodes=[node("d", ["b", "c"]), node("c", ["a"]), node("b", ["a"]), node("a")])
    for n in spec["nodes"]:
        n["package"] = str(timed / ("combined.brucemod" if n["predecessors"] else "output.brucemod"))
        n["budgets"] = dict(wall_ms=1000, memory_bytes=67108864, input_bytes=4096, output_bytes=16384)
    save(spec)
    for flag, status in (("--max-total-wall-ms", "DAG_GLOBAL_BUDGET_EXHAUSTED"), ("--cancel-after-ms", "DAG_HOST_CANCELLED")):
        stopped(flag[2:], ["--max-parallel-nodes", "2", flag, "100"], 3, ["a", "b", "c"], status)
        rows = [line.split() for line in starts.read_text().splitlines()]
        assert [action for action, _ in rows] == ["start", "reap", "start", "start", "reap", "reap"], rows
        assert all(not pathlib.Path("/proc", pid).exists() for action, pid in rows if action == "start"), rows
print("logic artifact DAG V4 CLI workflows passed")
