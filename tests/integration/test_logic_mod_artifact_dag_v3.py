"""Real signed workers exercise strict DAG JSON, preflight, replay and safe files."""
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
        p = subprocess.run([bruce, "logic-mod-artifact-dag-v3", *arguments], capture_output=True,
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
        assert result["contract"] == "logic-mod-artifact-dag-v3" and result["action"] == arguments[0]
        for n in result["nodes"]:
            assert list(n) == ["id", "result_artifact", "internal_input_artifact"]
            for artifact in (n["result_artifact"], n["internal_input_artifact"]):
                if artifact is not None:
                    assert list(artifact) == ["manifest_id", "sha256", "size_bytes", "codec"]
        return result

    def save(value):
        spec_file.write_text(json.dumps(value))

    save(spec)
    # RED before command implementation: this fails with UnknownCommand.
    good = invoke(args(), count=4)
    assert good["accepted"] and good["status"] == "ACCEPTED" and len(good["dag_id"]) == 64
    assert good["node_order"] == ["a", "b", "c", "d"] and len(good["spec_digest"]) == 64
    assert [n["id"] for n in good["nodes"]] == ["a", "b", "c", "d"]
    result_hash = "31540b43fa20cb275095247f6784794cecee19c0117b76238fa153964a52af5b"
    direct_parents = {"a": [], "b": ["a"], "c": ["a"], "d": ["b", "c"]}
    def field(value):
        value = value.encode()
        return str(len(value)).encode() + b":" + value
    for n in good["nodes"]:
        name = n["id"]
        assert n["result_artifact"] == dict(manifest_id=name + "-out", sha256=result_hash,
                                          size_bytes=4, codec="logic-mod-result-text-v1")
        parents = direct_parents[name]
        if not parents:
            assert n["internal_input_artifact"] is None
            continue
        payload = b"BRUCE_LOGIC_MOD_DAG_INPUT_V2\n" + field("2") + field(str(len(parents)))
        for parent in parents:
            for value in (parent, parent + "-out", result_hash, "4", "logic-mod-result-text-v1", "9/16"):
                payload += field(value)
        payload += b"\n"
        digest = hashlib.sha256(payload).hexdigest()
        assert n["internal_input_artifact"] == dict(manifest_id=name + "-input", sha256=digest,
                                                   size_bytes=len(payload), codec="logic-mod-dag-input-v2")
        assert (store / "objects" / digest[:2] / (digest + ".blob")).read_bytes() == payload
    # Scheduling is local: every supported limit preserves the default identity
    # and complete record bytes, including V2-compatible replay material.
    original = record.read_bytes()
    for limit in range(1, 5):
        target = root / ("parallel-" + str(limit) + ".record")
        assert invoke([*args(target=target), "--max-parallel-nodes", str(limit)], count=4) == good
        assert target.read_bytes() == original

    assert original.startswith(b"BRUCE_LOGIC_MOD_ARTIFACT_DAG_V2\n") and str(root).encode() not in original
    assert stat.S_IMODE(record.stat().st_mode) == 0o600
    def snapshot(path):
        return {str(p.relative_to(path)): (p.lstat().st_mode, p.lstat().st_size,
                p.read_bytes() if p.is_file() else None) for p in path.rglob("*")}
    before = snapshot(store)
    assert invoke(args("replay")) == {**good, "action": "replay"} and snapshot(store) == before
    # Canonical digest is independent of JSON key/node order and normalized text.
    canonical = copy.deepcopy(spec)
    canonical["nodes"].reverse()
    for n in canonical["nodes"]:
        n["input"] = " 3/4 HOCH 2 "
    for n in canonical["nodes"]:
        n["budgets"] = dict(wall_ms=250, memory_bytes=67108864, input_bytes=4096, output_bytes=16384)
    save(canonical)
    assert invoke(args("replay")) == {**good, "action": "replay"}
    save(spec)

    fifo = root / "fifo"; os.mkfifo(fifo, 0o600)
    base = args(source=fifo)
    malformed = [[], ["unknown"], base[:-1], [*base, "extra"], [*base, "--store", str(store)]]
    for i in range(len(base)):
        changed = base.copy(); changed[i] = ""; malformed.append(changed)
        changed = base.copy(); changed[i] = " \t"; malformed.append(changed)
    changed = base.copy(); changed[2:4], changed[4:6] = changed[4:6], changed[2:4]; malformed.append(changed)
    for arguments in malformed:
        invoke(arguments, 2, error="InvalidLogicModDagArguments", no_access=True)
    # Invalid local controls must fail before opening even the Spec FIFO.
    # Strict decimal spelling excludes signs, padding, whitespace and overflow.
    for value in ("0", "5", "4294967296", "-1", "+1", "01", "1.0", "1x", "", " ", " 1", "1 "):
        invoke([*base, "--max-parallel-nodes", value], 2,
               error="InvalidLogicModDagArguments", no_access=True)
    for tail in (["--max-parallel-nodes"], ["--max-parallel-nodes", "2", "extra"],
                 ["--max-parallel-nodes", "2", "--max-parallel-nodes", "3"],
                 ["--unknown", "2"]):
        invoke([*base, *tail], 2, error="InvalidLogicModDagArguments", no_access=True)
    for value in ("1", "2", "4"):
        invoke([*args("replay", source=fifo), "--max-parallel-nodes", value], 2,
               error="InvalidLogicModDagArguments", no_access=True)
    # Local controls never become accepted Spec fields.
    invalid_control_spec = copy.deepcopy(spec)
    invalid_control_spec["max_parallel_nodes"] = 2
    save(invalid_control_spec)
    invoke(args(), 2, error="LogicModDagFailed", no_access=True)
    # Structural errors must be rejected before attempting even a FIFO package.
    invalid_specs = []
    for change in (lambda s: s.update(extra=1), lambda s: s.update(schema_version=1),
                   lambda s: s.update(nodes=[]),
                   lambda s: s.update(nodes=[node("n" + str(i)) for i in range(17)]),
                   lambda s: s["nodes"][0].update(unknown="rejected"),
                   lambda s: s["nodes"][0].update(id="x" * 65), lambda s: s["nodes"].append(copy.deepcopy(s["nodes"][0])),
                   lambda s: s["nodes"][0].update(predecessors=["missing"]),
                   lambda s: s["nodes"][-1].update(predecessors=["d"], input_manifest="a-input"),
                   lambda s: s["nodes"][0].update(predecessors=["c", "b"]),
                   lambda s: s["nodes"][0].update(predecessors=["b", "b"]),
                   lambda s: s["nodes"][0].update(input_manifest="a-out"),
                   lambda s: s["nodes"][0].update(output_manifest="../escape"),
                   lambda s: s["nodes"][0].update(id="../escape"),
                   lambda s: s["nodes"][0].pop("input_manifest"),
                   lambda s: s["nodes"][-1].update(input_manifest="unexpected"),
                   lambda s: s["nodes"][0].update(budgets=dict(wall_ms=0,memory_bytes=67108864,input_bytes=4096,output_bytes=16384))):
        bad = copy.deepcopy(spec); change(bad)
        for n in bad["nodes"]: n["package"] = n["trust_store"] = str(fifo)
        invalid_specs.append(json.dumps(bad))
    valid_raw = json.dumps(spec)
    invalid_specs += [valid_raw + "x", valid_raw.replace('"schema_version": 2', '"schema_version": 2, "schema_version": 2'),
                     valid_raw.replace('"id": "d"', '"id": "d", "\\u0069d": "other"'),
                     valid_raw.replace('"id": "d"', '"id": "\\ud800"'),
                     valid_raw.replace('"schema_version": 2', '"schema_version": 2.0'), "[" * 10000, "x" * 65537]
    for raw in invalid_specs:
        spec_file.write_text(raw)
        rejected_store, rejected_record = root / "rejected-store", root / "rejected.record"
        invoke(args(local_store=rejected_store, target=rejected_record), 2, error="LogicModDagFailed", no_access=True)
        assert not rejected_store.exists() and not rejected_record.exists()
    for raw in (valid_raw.encode().replace(b'"id": "d"', b'"id": "\xff"'),
                valid_raw.encode().replace(b'"id": "d"', b'"id": "\xc0\xaf"')):
        spec_file.write_bytes(raw)
        invoke(args(local_store=root / "utf8-store", target=root / "utf8.record"), 2, error="LogicModDagFailed")
        assert not (root / "utf8-store").exists() and not (root / "utf8.record").exists()
    save(spec)

    # A syntactically valid signed late package with invalid ELF starts nobody.
    late = root / "late"; late.mkdir()
    invalid_worker = root / "invalid-worker"; invalid_worker.write_text("signed invalid ELF")
    subprocess.run([exporter, str(invalid_worker), str(late), "export"], check=True)
    for change in (lambda s: s["nodes"][0].update(package=str(late / "combined.brucemod")),
                   lambda s: s["nodes"][0].update(package=str(root / "output.brucemod")),
                   lambda s: s["nodes"][-1].update(package=str(root / "combined.brucemod")),
                   lambda s: s["nodes"][0].update(trust_store=str(invalid_worker))):
        bad = copy.deepcopy(spec); change(bad); save(bad)
        invoke(args(local_store=root / "preflight-store", target=root / "preflight.record"), 2, error="LogicModDagFailed")
        assert not (root / "preflight-store").exists() and not (root / "preflight.record").exists()

    for failed_id, count in (("a", 1), ("b", 2)):
        bad = copy.deepcopy(spec)
        next(n for n in bad["nodes"] if n["id"] == failed_id)["input"] = "fail"
        save(bad); failed_store = root / (failed_id + "-failed-store")
        result = invoke(args(local_store=failed_store, target=root / "failed.record"), 2, count)
        assert not result["accepted"] and result["status"] == "DAG_NODE_FAILED"
        assert not (root / "failed.record").exists() and not (failed_store / "manifests/d-out.manifest").exists()
        assert not (failed_store / "manifests/c-input.manifest").exists()
    save(spec)
    # Result and internal input identity/bytes are freshly verified with zero forks.
    for n in good["nodes"]:
        for artifact in (n["result_artifact"], n["internal_input_artifact"]):
            if artifact is None: continue
            manifest = store / "manifests" / (artifact["manifest_id"] + ".manifest")
            obj = store / "objects" / artifact["sha256"][:2] / (artifact["sha256"] + ".blob")
            for path in (manifest, obj):
                saved = path.read_bytes(); path.write_bytes(b"tampered")
                invoke(args("replay"), 2, error="DAG_REPLAY_MISMATCH")
                path.write_bytes(saved)
    for raw in (original + b"x", original.replace(good["spec_digest"].encode(), b"0" * 64), b"x" * (8*1024*1024+1)):
        record.write_bytes(raw); invoke(args("replay"), 2, error="DAG_REPLAY_MISMATCH")
    record.write_bytes(original)
    changed = copy.deepcopy(spec); changed["nodes"][0]["input"] = "1/2 hoch 2"; save(changed)
    invoke(args("replay"), 2, error="DAG_REPLAY_MISMATCH"); save(spec)
    missing = root / "missing-store"
    invoke(args("replay", local_store=missing), 2, error="DAG_REPLAY_MISMATCH")
    assert not missing.exists()

    link = root / "record-link"; link.symlink_to(record)
    directory = root / "nested"; directory.mkdir()
    parent_link = root / "parent-link"; parent_link.symlink_to(directory, target_is_directory=True)
    (directory / "record").write_bytes(original); (directory / "record").chmod(0o600)
    (directory / "spec").write_text(json.dumps(spec))
    for target in (fifo, link, root, pathlib.Path("/dev/null"), parent_link / "record", root / "nested/../dag.record",
                   str(record) + "/.", str(record) + "/"):
        invoke(args("replay", target=target), 2, error="DAG_REPLAY_MISMATCH")
    for source in (fifo, link, root, parent_link / "spec"):
        invoke(args(source=source), 2, error="LogicModDagFailed")
        if source == fifo:
            assert accesses.read_bytes() == b"access\n", "file-access probe was not active"
    for field_name, destination in (("package", root / "combined.brucemod"), ("trust_store", root / "trust.txt")):
        unsafe = root / (field_name + "-link"); unsafe.symlink_to(destination)
        bad = copy.deepcopy(spec); bad["nodes"][0][field_name] = str(unsafe); save(bad)
        invoke(args(local_store=root / "unsafe-store", target=root / "unsafe.record"), 2, error="LogicModDagFailed")
        assert not (root / "unsafe-store").exists() and not (root / "unsafe.record").exists()
    save(spec)
    hard = root / "hard-record"; os.link(record, hard)
    invoke(args("replay"), 2, error="DAG_REPLAY_MISMATCH"); hard.unlink()
    record.chmod(0o644); invoke(args("replay"), 2, error="DAG_REPLAY_MISMATCH"); record.chmod(0o600)
    assert invoke(args("replay")) == {**good, "action": "replay"}
    # No-replace publication preserves existing destinations and cleans temporaries.
    for target in (record, fifo, link, root, parent_link / "new-record", str(root / "new") + "/."):
        invoke(args(target=target), 2, 4, error="LogicModDagFailed")
        assert record.read_bytes() == original and link.is_symlink() and stat.S_ISFIFO(fifo.stat().st_mode)
    assert not list(root.rglob("*.tmp")) and not (root / "new").exists()
    # A second signed package contains timed real workers. Host start/reap
    # events prove concurrent lifetime, whole-wave barriers and complete drain.
    timed = root / "timed"; timed.mkdir()
    subprocess.run([exporter, timed_worker, str(timed), "export"], check=True)
    def timed_node(name, parents=(), text="3/4 hoch 2"):
        n = node(name, parents)
        n["package"] = str(timed / ("combined.brucemod" if parents else "output.brucemod"))
        n["input"] = text
        n["budgets"] = dict(wall_ms=1000, memory_bytes=67108864, input_bytes=4096, output_bytes=16384)
        return n
    def events():
        return [line.split() for line in starts.read_text().splitlines()]
    def drained(expected):
        rows = events()
        spawned = [pid for action, pid in rows if action == "start"]
        reaped = [pid for action, pid in rows if action == "reap"]
        assert len(spawned) == expected and sorted(spawned) == sorted(reaped), rows
        assert all(not pathlib.Path("/proc", pid).exists() for pid in spawned), rows
        return rows
    # The existing exporter keeps root output.brucemod bound to its plain
    # worker. Its combined package embeds the supplied timed worker.
    save(dict(schema_version=2, nodes=[timed_node("z", ["r"]), timed_node("b", ["a"]),
                                     timed_node("a", ["r"], text="1/2 hoch 2"), timed_node("r")]))
    timed_store, timed_record = root / "timed-store", root / "timed.record"
    timed_good = invoke([*args(local_store=timed_store, target=timed_record), "--max-parallel-nodes", "2"], count=4)
    rows = drained(4)
    assert [action for action, _ in rows] == ["start", "reap", "start", "start", "reap", "reap", "start", "reap"], rows
    assert timed_good["node_order"] == ["r", "a", "b", "z"]
    assert invoke(args("replay", local_store=timed_store, target=timed_record)) == {**timed_good, "action": "replay"}
    # The omitted control retains physical serial execution.
    invoke(args(local_store=root / "timed-serial-store", target=root / "timed-serial.record"), count=4)
    assert [action for action, _ in drained(4)] == ["start", "reap"] * 4
    save(dict(schema_version=2, nodes=[timed_node("d", ["a"]), timed_node("c", ["r"]),
                                     timed_node("b", ["r"]), timed_node("a", ["r"], text="2/3 hoch 2"),
                                     timed_node("r")]))
    failed_store, failed_record = root / "wave-failed-store", root / "wave-failed.record"
    failed = invoke([*args(local_store=failed_store, target=failed_record), "--max-parallel-nodes", "2"], 2, count=3)
    assert failed["status"] == "DAG_NODE_FAILED" and not failed["accepted"]
    rows = drained(3)
    assert [action for action, _ in rows] == ["start", "reap", "start", "start", "reap", "reap"], rows
    assert (failed_store / "manifests/b-out.manifest").is_file()
    assert not failed_record.exists() and not (failed_store / "manifests/c-out.manifest").exists()
    assert not (failed_store / "manifests/d-input.manifest").exists()
print("logic artifact DAG V3 CLI workflows passed")
