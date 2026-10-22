"""Exercise the strict two-package CLI against real sealed workers and Store files."""
import json
import os
import pathlib
import stat
import subprocess
import sys
import tempfile

bruce, exporter, worker, compiler = map(os.path.abspath, sys.argv[1:])
A_HASH = "31540b43fa20cb275095247f6784794cecee19c0117b76238fa153964a52af5b"
B_HASH = "f70b94aeb67de2a5eb4bd8c2ea85128f13776cb545a661abe422b378a6cf3099"


def snapshot(root):
    result = {}
    for path in [root, *sorted(root.rglob("*"))]:
        info = path.lstat()
        result[str(path.relative_to(root))] = (info.st_mode, info.st_ino, info.st_nlink,
            info.st_size, info.st_mtime_ns,
            path.read_bytes() if stat.S_ISREG(info.st_mode) else
            os.readlink(path) if stat.S_ISLNK(info.st_mode) else None)
    return result


with tempfile.TemporaryDirectory(prefix="bruce-chain-cli-") as tmp:
    root = pathlib.Path(tmp)
    subprocess.run([exporter, worker, tmp, "export"], check=True)
    # Observe real worker starts in the CLI process. A positive two-start run
    # establishes the probe; negative cases require exactly zero or one start.
    # Static worker executables do not inherit this dynamic-loader fixture.
    probe = root / "fork-probe.cpp"
    probe.write_text(r'''
#include <dlfcn.h>
#include <fcntl.h>
#include <cstdlib>
#include <unistd.h>
extern "C" pid_t fork() {
    if (const char* path = std::getenv("BRUCE_TEST_STARTS")) {
        const int fd = ::open(path, O_WRONLY | O_APPEND | O_CREAT, 0600);
        if (fd >= 0) { const auto ignored = ::write(fd, "start\n", 6); (void)ignored; ::close(fd); }
    }
    return reinterpret_cast<pid_t (*)()>(::dlsym(RTLD_NEXT, "fork"))();
}
''')
    library, starts = root / "fork-probe.so", root / "starts"
    subprocess.run([compiler, "-shared", "-fPIC", str(probe), "-ldl", "-o", str(library)], check=True)
    env = {**os.environ, "LD_PRELOAD": str(library), "BRUCE_TEST_STARTS": str(starts)}
    trust, store, record = root / "trust.txt", root / "store", root / "chain.record"

    def arguments(action="run", local_store=store, target=record, first="output", second="combined",
                  first_input="3/4 hoch 2", second_input="1/2 hoch 2", first_trust=trust, second_trust=trust,
                  manifests=("first", "second")):
        args = [action, root / (first + ".brucemod"), "--first-trust", first_trust,
                "--first-input", first_input, root / (second + ".brucemod"), "--second-trust", second_trust,
                "--second-input", second_input, "--store", local_store]
        if action == "run":
            args += ["--first-output-manifest", manifests[0], "--second-output-manifest", manifests[1]]
        return list(map(str, [*args, "--record", target]))

    def invoke(args, code=0, count=None, error=None):
        starts.write_bytes(b"")
        p = subprocess.run([bruce, "logic-mod-chain-v1", *args], capture_output=True, text=True,
                           timeout=5, env=env)
        assert p.returncode == code, (p.returncode, p.stdout, p.stderr)
        assert str(root) not in p.stdout + p.stderr, "local path leaked"
        assert all(marker not in p.stdout + p.stderr for marker in
                   ("3/4 hoch 2", "1/2 hoch 2", "9/16", "1/4", "BRUCE_LOGIC_RESULT")), "raw data leaked"
        if count is not None:
            assert starts.read_bytes() == b"start\n" * count, (args, starts.read_bytes())
        if error:
            assert not p.stdout and json.loads(p.stderr) == {"error": error}, (p.stdout, p.stderr)
            return None
        assert not p.stderr and len(p.stdout.splitlines()) == 1, (p.stdout, p.stderr)
        value = json.loads(p.stdout)
        assert list(value) == ["contract", "action", "accepted", "status", "chain_id", "first", "second"], value
        assert value["contract"] == "logic-mod-artifact-chain-v1" and value["action"] == args[0]
        for artifact in (value["first"], value["second"]):
            if artifact is not None:
                assert list(artifact) == ["manifest_id", "sha256", "size_bytes", "codec"]
        return value

    # This first assertion must fail with UnknownCommand before implementation.
    good = invoke(arguments(), count=2)
    assert good["accepted"] and good["status"] == "ACCEPTED" and len(good["chain_id"]) == 64
    for output, name, digest, raw in ((good["first"], "first", A_HASH, b"9/16"),
                                     (good["second"], "second", B_HASH, b"1/4")):
        assert output == {"manifest_id": name, "sha256": digest, "size_bytes": len(raw),
                          "codec": "logic-mod-result-text-v1"}
        assert (store / "objects" / digest[:2] / (digest + ".blob")).read_bytes() == raw
    original = record.read_bytes()
    assert original.startswith(b"BRUCE_LOGIC_MOD_ARTIFACT_CHAIN_V1\n") and str(root).encode() not in original
    assert stat.S_IMODE(record.stat().st_mode) == 0o600
    before = snapshot(store)
    replay = invoke(arguments("replay"), count=0)
    assert replay == {**good, "action": "replay"} and snapshot(store) == before
    # A terminal dot denotes directory traversal, never an alias for the
    # preceding filename. Neither publication nor replay may silently drop it.
    terminal_dot_failures = []
    for action, target, error in (
            ("run", str(root / "dot-record") + "/.", "LogicModChainFailed"),
            ("run", str(root / "dot-record") + "/./", "LogicModChainFailed"),
            ("run", str(root / "dot-record") + "/", "LogicModChainFailed"),
            ("replay", str(record) + "/.", "CHAIN_REPLAY_MISMATCH"),
            ("replay", str(record) + "/./", "CHAIN_REPLAY_MISMATCH"),
            ("replay", str(record) + "/", "CHAIN_REPLAY_MISMATCH")):
        try:
            invoke(arguments(action, target=target), 2, 2 if action == "run" else 0, error)
        except AssertionError as failure:
            terminal_dot_failures.append((action, str(failure)))
    assert not terminal_dot_failures, terminal_dot_failures
    assert not (root / "dot-record").exists(), "terminal-dot publication selected another file"
    # Interior dot components retain their ordinary traversal meaning.
    assert invoke(arguments("replay", target=str(root) + "/./chain.record"), count=0) == replay
    again = invoke(arguments(target=root / "again.record", first_input=" 3/4 HOCH 2 "), count=2)
    assert again["chain_id"] == good["chain_id"]

    # Parser rejection must happen before even opening a FIFO package/trust.
    fifo = root / "fifo"
    os.mkfifo(fifo, 0o600)
    base = arguments(first_trust=fifo)
    malformed = [[], ["unknown"], base[:-1], [*base, "extra"], [*base, "--store", str(store)],
                 [*base, "--unknown", "x"], arguments("replay") + ["--first-output-manifest", "x"]]
    for i in range(1, len(base)):
        changed = base.copy(); changed[i] = ""; malformed.append(changed)
        changed = base.copy(); changed[i] = " \t"; malformed.append(changed)
    changed = base.copy(); changed[2], changed[7] = changed[7], changed[2]; malformed.append(changed)
    changed = base.copy(); changed[11:13], changed[13:15] = changed[13:15], changed[11:13]; malformed.append(changed)
    changed = base.copy(); changed[12] = "--record"; malformed.append(changed)
    for args in malformed:
        before = snapshot(store)
        invoke(args, 2, count=0, error="InvalidLogicModChainArguments")
        assert snapshot(store) == before and record.read_bytes() == original

    # Invalid second package/trust/capabilities/manifest must reject before A.
    bad = root / "bad.brucemod"; bad.write_text("private malformed " + tmp)
    bad_trust = root / "bad.trust"; bad_trust.write_text("private trust " + tmp)
    untrusted = root / "untrusted.trust"
    untrusted.write_text("BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n")
    for kwargs in ({"first": "legacy"}, {"first": "combined"}, {"second": "output"},
                   {"second": "payload"}, {"first": "bad"}, {"second": "bad"},
                   {"first_trust": bad_trust}, {"second_trust": bad_trust},
                   {"second_trust": untrusted}, {"manifests": ("../bad", "ok")},
                   {"manifests": ("ok", "../bad")}, {"manifests": ("same", "same")}):
        missing = root / "preflight-store"
        invoke(arguments(local_store=missing, target=root / "rejected.record", **kwargs), 2, 0,
               "LogicModChainFailed")
        assert not missing.exists() and not (root / "rejected.record").exists()
    for side in ("first", "second"):
        local = root / (side + "-failed-store")
        result = invoke(arguments(local_store=local, target=root / (side + "-failed.record"),
                                 **{side + "_input": "fail"}), 2, 1 if side == "first" else 2)
        assert not result["accepted"] and result["status"] == ("CHAIN_FIRST_FAILED" if side == "first" else "CHAIN_SECOND_FAILED")
        assert not (local / "manifests/second.manifest").exists() and not (root / (side + "-failed.record")).exists()
        assert (local / "manifests/first.manifest").exists() == (side == "second")

    # Both immutable Store identities must be freshly read by offline replay.
    for name, digest in (("first", A_HASH), ("second", B_HASH)):
        manifest = store / "manifests" / (name + ".manifest")
        saved = manifest.read_bytes()
        for old, new in ((b"READY", b"FAILED"), (b"logic-mod-result-text-v1", b"other-codec")):
            manifest.write_bytes(saved.replace(old, new))
            invoke(arguments("replay"), 2, 0, "CHAIN_REPLAY_MISMATCH")
            manifest.write_bytes(saved)
        manifest.unlink()
        invoke(arguments("replay"), 2, 0, "CHAIN_REPLAY_MISMATCH")
        manifest.write_bytes(saved); manifest.chmod(0o600)
        obj = store / "objects" / digest[:2] / (digest + ".blob")
        raw = obj.read_bytes()
        obj.write_bytes(b"bad")
        invoke(arguments("replay"), 2, 0, "CHAIN_REPLAY_MISMATCH")
        obj.unlink()
        invoke(arguments("replay"), 2, 0, "CHAIN_REPLAY_MISMATCH")
        obj.write_bytes(raw); obj.chmod(0o600)
    for data in (original + b"x", original.replace(A_HASH.encode(), b"0" * 64), b"x" * 524289):
        record.write_bytes(data)
        invoke(arguments("replay"), 2, 0, "CHAIN_REPLAY_MISMATCH")
    record.write_bytes(original)
    invoke(arguments("replay", second_input="3/4 hoch 2"), 2, 0, "CHAIN_REPLAY_MISMATCH")
    missing = root / "missing-store"
    invoke(arguments("replay", local_store=missing), 2, 0, "CHAIN_REPLAY_MISMATCH")
    assert not missing.exists()

    # Special files, insecure modes and path aliases never block record reads.
    link = root / "record-link"; link.symlink_to(record)
    directory = root / "nested"; directory.mkdir()
    parent_link = root / "parent-link"; parent_link.symlink_to(directory, target_is_directory=True)
    nested_record = directory / "record"; nested_record.write_bytes(original); nested_record.chmod(0o600)
    for unsafe in (fifo, link, root, pathlib.Path("/dev/null"), parent_link / "record", root / "nested/../chain.record"):
        invoke(arguments("replay", target=unsafe), 2, 0, "CHAIN_REPLAY_MISMATCH")
    hard = root / "hard-record"; os.link(record, hard)
    invoke(arguments("replay"), 2, 0, "CHAIN_REPLAY_MISMATCH")
    hard.unlink(); record.chmod(0o644)
    invoke(arguments("replay"), 2, 0, "CHAIN_REPLAY_MISMATCH")
    record.chmod(0o600)
    invoke(arguments("replay"), count=0)
    # Publication is private, bounded, atomic and never replaces any target.
    # A publication failure may leave both already accepted artifacts.
    for target in (record, fifo, link, root, parent_link / "new-record", root / "nested/../new-record"):
        before = snapshot(root)
        invoke(arguments(target=target), 2, error="LogicModChainFailed")
        after = snapshot(root)
        before.pop("starts", None); after.pop("starts", None)
        # Successful idempotent worker publications may create/remove Store
        # temporaries, updating directory mtimes; existing inodes/bytes and
        # directory identities, modes and entry sets must still survive.
        for state in (before, after):
            for key, value in state.items():
                if stat.S_ISDIR(value[0]):
                    state[key] = value[:4] + value[5:]
        assert before == after, "record publication replaced or modified a target"
    assert not list(root.rglob("*.tmp*")), "temporary record leaked"
print("logic artifact chain CLI workflows passed")
