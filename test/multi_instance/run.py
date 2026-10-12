"""Multi-instance regression suite. Uses only the Python standard library.

Each case runs in its own directory under --out/<case>/; every child gets that
directory as cwd, so the bqlog_mmap/ tree of one case never sees another's.
run.py --case M4 runs one case; without --case all cases M1..M8 run.
Failure messages name the worker and the missing or duplicated seq.
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

RECORD_TIMEOUT = 120


class CaseError(Exception):
    pass


def fail(msg):
    raise CaseError(msg)


def wait_files(paths, children, timeout=RECORD_TIMEOUT):
    # paths[i] is the barrier file children[i] must produce; a child that
    # exits is only an error when its own file is still missing (a fast
    # worker may legitimately finish everything before the others start)
    deadline = time.monotonic() + timeout
    while not all(p.exists() and p.stat().st_size for p in paths):
        for child, path in zip(children, paths):
            if child.poll() is not None and not (path.exists() and path.stat().st_size):
                raise CaseError("worker exited before completing barrier")
        if time.monotonic() >= deadline:
            raise CaseError("worker barrier timed out waiting for %s" % [str(p) for p in paths if not p.exists()])
        time.sleep(0.02)


def spawn(exe, cwd, args, captures, tag):
    capture = (cwd / f"{tag}.stdout.txt").open("wb")
    captures.append(capture)
    return subprocess.Popen([str(exe)] + args, cwd=cwd,
        stdout=capture, stderr=subprocess.STDOUT)


def read_lines(path):
    try:
        return path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return []


def check_records(exe_args_done, cwd, worker_ids, records, two_logs=False, allowed_foreign=()):
    """Verify each worker's output file holds exactly its own records in seq order.

    Returns a dict {worker_id: problems} where problems is a list of strings
    naming every missing or duplicated (kind, seq).

    allowed_foreign: worker ids whose records may legitimately appear in another
    worker's file because recovering that worker's crashed run replayed its tail
    there.
    """
    problems = {}
    for wid in worker_ids:
        texts = "".join(p.read_text(encoding="utf-8", errors="replace")
            for p in sorted(cwd.glob(f"output/worker_{wid}_*.log")))
        kinds = ["normal", "fast"] + (["normal_b", "fast_b"] if two_logs else ["oversize"])
        expected = []
        for seq in range(records):
            for kind in kinds:
                if two_logs:
                    if kind == "normal" and seq % 2 == 0:
                        expected.append((kind, seq))
                    elif kind == "normal_b" and seq % 2 == 1:
                        expected.append((kind, seq))
                    elif kind == "fast" and seq % 2 == 0:
                        expected.append((kind, seq))
                    elif kind == "fast_b" and seq % 2 == 1:
                        expected.append((kind, seq))
                else:
                    if kind == "oversize" and seq % 32 != 0:
                        continue
                    expected.append((kind, seq))
        actual = []
        all_records = []
        for kind in kinds:
            # all worker tags seen in this file, needed to detect cross-contamination
            found = [(kind, int(w), int(s)) for w, s in re.findall(
                rf"\b{kind} worker=(\d+) seq=(\d+)", texts)]
            all_records += found
            # filter by worker tag: a successor that recovers a dead worker's
            # tail replays records tagged worker=<dead id> into its own file
            actual += [(kind, s) for k, w, s in found if w == wid]
        counts = {}
        for item in actual:
            counts[item] = counts.get(item, 0) + 1
        worker_problems = []
        for item in expected:
            count = counts.get(item, 0)
            if count == 0:
                worker_problems.append(f"missing {item[0]} seq={item[1]}")
            elif count > 1:
                worker_problems.append(f"duplicated {item[0]} seq={item[1]} x{count}")
        # foreign checks over ALL tags, not the worker-filtered counts:
        #  - another worker's tag in this worker's own file is contamination, unless
        #    that worker crashed and its tail was replayed here during recovery
        #  - an own-tag record whose (kind, seq) is not expected must not exist
        # The worker-filtered `counts` above can never reveal the first kind.
        cross = sorted({(k, w, s) for k, w, s in all_records
                        if w != wid and w not in allowed_foreign})
        if cross:
            worker_problems.append(f"records of another worker: {cross[:5]}")
        unknown = sorted({(k, s) for k, s in counts if (k, s) not in set(expected)})
        if unknown:
            worker_problems.append(f"unexpected records: {unknown[:5]}")
        # per-seq order within each kind
        for kind in kinds:
            seqs = [s for k, s in actual if k == kind]
            if seqs != sorted(seqs):
                worker_problems.append(f"{kind} out of order")
        problems[wid] = worker_problems
    return problems


def assert_no_diagnostics(cwd, worker_ids, tag):
    for wid in worker_ids:
        for line in read_lines(cwd / f"{tag}_{wid}.stdout.txt"):
            if re.search(r"use memory instead|failed to open mmap|memory map.*failed|"
                    r"exclusive.*failed|execlusive.*failed|sharing violation", line, re.I):
                fail(f"worker {wid} mapping diagnostic: {line.strip()}")


# ---------------------------------------------------------------- cases

def case_M1(exe, out, records):
    root = out / "M1"
    root.mkdir(parents=True)
    for count in (2, 4, 16):
        cwd = root / f"n{count}"
        cwd.mkdir()
        captures, children = [], []
        # workers hold their instance until this sentinel is deleted, so all
        # count workers are alive at once and none can recycle an instance
        hold = cwd / "hold_instances"
        hold.write_text("hold")
        m1_records = max(records, 256)
        try:
            for i in range(count):
                children.append(spawn(exe, cwd,
                    [str(i), "multi_instance", str(m1_records), "--report",
                     "--hold-until", str(hold)],
                    captures, f"worker_{i}"))
            wait_files([cwd / f"ready_{i}.json" for i in range(count)], children)
            # every worker claims its instance only while all of them are alive
            hold.unlink()
            wait_files([cwd / f"done_{i}" for i in range(count)], children)
            codes = [p.wait(timeout=RECORD_TIMEOUT) for p in children]
        finally:
            for p in children:
                if p.poll() is None:
                    p.kill()
                    p.wait()
            for capture in captures:
                capture.close()
        ready = [json.loads((cwd / f"ready_{i}.json").read_text()) for i in range(count)]
        instances = [r["instance"] for r in ready]
        if sorted(instances) != list(range(count)):
            fail(f"n={count}: instances {instances} != {{0..%d}}" % (count - 1))
        for i, r in enumerate(ready):
            if not r["lp_mapped"] or not r["hp_mapped"]:
                fail(f"n={count}: worker {i} lp_mapped={r['lp_mapped']} hp_mapped={r['hp_mapped']}")
        if any(code != 0 for code in codes):
            fail(f"n={count}: exit codes {codes}")
        problems = check_records(None, cwd, range(count), m1_records)
        for wid, plist in problems.items():
            if plist:
                fail(f"n={count} worker {wid}: {'; '.join(plist[:8])}")
        assert_no_diagnostics(cwd, range(count), "worker")


def case_M2(exe, out, records):
    cwd = out / "M2"
    cwd.mkdir(parents=True)
    captures, children = [], []
    try:
        children.append(spawn(exe, cwd, ["0", "two_logs_log", str(records), "--two-logs"], captures, "worker_0"))
        wait_files([cwd / "done_0"], children)
        code = children[0].wait(timeout=RECORD_TIMEOUT)
    finally:
        for p in children:
            if p.poll() is None:
                p.kill()
                p.wait()
        for capture in captures:
            capture.close()
    if code != 0:
        fail(f"exit code {code}")
    # Each log's output only holds its own records, count correct. Both logs write the SAME
    # format strings (T04 spec), so they are told apart by output file, not by a kind tag.
    def seqs_in(pattern, kind):
        text = "".join(p.read_text(encoding="utf-8", errors="replace")
            for p in sorted(cwd.glob(pattern)))
        return [(int(w), int(s)) for w, s in re.findall(rf"\b{kind} worker=(\d+) seq=(\d+)", text)]
    a_normal = seqs_in("output/worker_0_*.log", "normal")
    a_fast = seqs_in("output/worker_0_*.log", "fast")
    b_normal = seqs_in("output/workerb_*.log", "normal")
    b_fast = seqs_in("output/workerb_*.log", "fast")
    expect_even = [(0, n) for n in range(0, records, 2)]
    expect_odd = [(0, n) for n in range(1, records, 2)]
    if a_normal != expect_even or a_fast != expect_even:
        fail(f"log a normal={len(a_normal)} fast={len(a_fast)}, want even seqs 0..{records - 1} in order")
    if b_normal != expect_odd or b_fast != expect_odd:
        fail(f"log b normal={len(b_normal)} fast={len(b_fast)}, want odd seqs 1..{records} in order")


def case_M3(exe, out, records):
    cwd = out / "M3"
    cwd.mkdir(parents=True)
    captures, children = [], []

    def snapshot_p0():
        base = cwd / "bqlog_mmap" / "p0"
        if not base.is_dir():
            return None
        return {str(p.relative_to(base)): p.stat().st_size
            for p in sorted(base.rglob("*")) if p.is_file()}

    captures, children = [], []
    try:
        # A: recovery on; holds (instance claimed, not writing) until B is done
        hold = cwd / "hold_a"
        hold.write_text("hold")
        a = spawn(exe, cwd, ["0", "m3_log", str(records), "--hold-until", str(hold)], captures, "worker_0")
        wait_files([cwd / "ready_0.json"], [a])
        before = snapshot_p0()
        if not before:
            fail("p0 not created by recovery worker A")
        b = spawn(exe, cwd, ["1", "m3_log", str(records), "--recovery", "off"], captures, "worker_1")
        wait_files([cwd / "done_1"], [b])
        during = snapshot_p0()
        code_b = b.wait(timeout=RECORD_TIMEOUT)
        if code_b != 0:
            fail(f"recovery-off worker exit code {code_b}")
        if during != before:
            fail("p0 changed while recovery-off worker B ran:\nbefore=%s\nduring=%s" % (before, during))
        if (cwd / "bqlog_mmap" / "p1").exists():
            fail("p1 was created; recovery-off worker must not claim an instance")
        hold.unlink()
        a.wait(timeout=RECORD_TIMEOUT)
    finally:
        for p in children:
            if p.poll() is None:
                p.kill()
                p.wait()
        for capture in captures:
            capture.close()


def run_die_worker(exe, cwd, wid, log_name, die_after, captures, children):
    args = [str(wid), log_name, str(die_after), "--die-after", str(die_after)]
    child = spawn(exe, cwd, args, captures, f"worker_{wid}")
    children.append(child)
    wait_files([cwd / f"committed_{wid}.json"], [child])
    code = child.wait(timeout=RECORD_TIMEOUT)
    if code is None:
        fail(f"worker {wid} still running after committed_{wid}.json")
    return code


def find_recovered_records(cwd, worker_ids=(), kinds=("normal", "fast")):
    """Parse the output of all workers; returns [(kind, worker, seq)] per line."""
    records = []
    for wid in worker_ids:
        texts = "".join(p.read_text(encoding="utf-8", errors="replace")
            for p in sorted(cwd.glob(f"output/worker_{wid}_*.log")))
        for kind in kinds:
            for w, s in re.findall(rf"\b{kind} worker=(\d+) seq=(\d+)", texts):
                records.append((kind, int(w), int(s)))
    return records


def case_M4(exe, out, records):
    cwd = out / "M4"
    cwd.mkdir(parents=True)
    captures, children = [], []
    try:
        # A dies after 500 committed records (normal+fast pairs)
        run_die_worker(exe, cwd, 0, "m4_log", 500, captures, children)
        # C: same name, same cwd, claims the freed instance 0
        c = spawn(exe, cwd, ["2", "m4_log", str(records), "--report"], captures, "worker_2")
        children.append(c)
        wait_files([cwd / "ready_2.json"], [c])
        ready = json.loads((cwd / "ready_2.json").read_text())
        wait_files([cwd / "done_2"], [c])
        if c.wait(timeout=RECORD_TIMEOUT) != 0:
            fail(f"worker C exit code {c.returncode}")
    finally:
        for p in children:
            if p.poll() is None:
                p.kill()
                p.wait()
        for capture in captures:
            capture.close()
    if ready["instance"] != 0:
        fail(f"worker C got instance {ready['instance']}, want 0")
    # A wrote seq 0..499 (normal+fast each) into the ring buffer, then died
    # without flushing. Recovery is at-least-once (must not lose; duplicates
    # are accepted, a crash can land between the file write and the finished-
    # cursor update). Asserted across A's own output + C's output (recovery
    # replay goes into C's file):
    #   1. every committed (kind, seq) appears at least once,
    #   2. nothing beyond committed_seq,
    #   3. C's own run is complete and unaffected.
    a_texts = "".join(p.read_text(encoding="utf-8", errors="replace")
        for p in sorted(cwd.glob("output/worker_0_*.log")))
    c_texts = "".join(p.read_text(encoding="utf-8", errors="replace")
        for p in sorted(cwd.glob("output/worker_2_*.log")))
    combined = a_texts + "\n" + c_texts
    seen = {}
    for kind in ("normal", "fast"):
        for w, s in re.findall(rf"\b{kind} worker=(\d+) seq=(\d+)", combined):
            if int(w) != 0:
                continue  # C's own records use worker=2
            seen[(kind, int(s))] = seen.get((kind, int(s)), 0) + 1
    for seq in range(500):
        for kind in ("normal", "fast"):
            if seen.get((kind, seq), 0) == 0:
                fail(f"A's committed records lost: {kind} seq={seq}")
    for kind in ("normal", "fast"):
        extra = sorted(s for (k, s) in seen if k == kind and not (0 <= s < 500))
        if extra:
            fail(f"worker A wrote beyond committed_seq: {kind} seq={extra[:8]}")


def case_M5(exe, out, records):
    cwd = out / "M5"
    cwd.mkdir(parents=True)
    captures, children = [], []
    try:
        # A0 and A1 run concurrently on instances 0 and 1, then both die after
        # 300 committed records. A hold barrier keeps A0 alive until A1 has
        # claimed its own instance (instances are freed as soon as a worker
        # exits, so sequential spawning would put A1 on instance 0 too).
        hold = cwd / "hold_a0"
        hold.write_text("hold")
        a0 = spawn(exe, cwd,
            ["0", "m5_log", "300", "--die-after", "300", "--hold-until", str(hold)],
            captures, "worker_0")
        children.append(a0)
        wait_files([cwd / "ready_0.json"], [a0])
        run_die_worker(exe, cwd, 1, "m5_log", 300, captures, children)
        hold.unlink()
        # the die path self-kills: exit code 9 on Windows, -9 (SIGKILL) on POSIX
        if a0.wait(timeout=RECORD_TIMEOUT) not in (0, 9, -9):
            fail("worker A0 failed before its die point")
        committed0 = json.loads((cwd / "committed_0.json").read_text())["committed_seq"]
        p1_before = {str(p.relative_to(cwd)): p.stat().st_size
            for p in sorted((cwd / "bqlog_mmap" / "p1").rglob("*")) if p.is_file()}
        if not p1_before:
            fail("bqlog_mmap/p1/ was never created")
        # only C: claims instance 0, must not touch instance 1's storage.
        # Phase 1 (doc): C alone; while it holds (claimed but not writing),
        # p1 must be exactly as the dead A1 left it.
        hold = cwd / "hold_c"
        hold.write_text("hold")
        c = spawn(exe, cwd,
            ["2", "m5_log", str(records), "--report", "--hold-until", str(hold)],
            captures, "worker_2")
        children.append(c)
        wait_files([cwd / "ready_2.json"], [c])
        ready2 = json.loads((cwd / "ready_2.json").read_text())
        p1_after_c = {str(p.relative_to(cwd)): p.stat().st_size
            for p in sorted((cwd / "bqlog_mmap" / "p1").rglob("*")) if p.is_file()}
        if p1_after_c != p1_before:
            fail("p1 changed when C (instance 0) claimed and held:\nbefore=%s\nafter=%s"
                 % (p1_before, p1_after_c))
        # Phase 2 (doc): C first, D after, both running at once; D takes instance 1
        d = spawn(exe, cwd, ["3", "m5_log", str(records), "--report"], captures, "worker_3")
        children.append(d)
        wait_files([cwd / "ready_3.json"], [d])
        ready3 = json.loads((cwd / "ready_3.json").read_text())
        hold.unlink()
        wait_files([cwd / "done_2"], [c])
        if c.wait(timeout=RECORD_TIMEOUT) != 0:
            fail("worker C failed")
        wait_files([cwd / "done_3"], [d])
        if d.wait(timeout=RECORD_TIMEOUT) != 0:
            fail("worker D failed")
    finally:
        for p in children:
            if p.poll() is None:
                p.kill()
                p.wait()
        for capture in captures:
            capture.close()
    if ready2["instance"] != 0:
        fail(f"worker C got instance {ready2['instance']}, want 0")
    if ready3["instance"] != 1:
        fail(f"worker D got instance {ready3['instance']}, want 1")
    # The die path kills without flushing. Recovery is at-least-once: no loss,
    # duplicates accepted (a crash can land between the file write and the
    # finished-cursor update). Asserted per dead worker across its own output
    # plus the successor's output (C for A0, D for A1):
    #   1. every committed (kind, seq) appears at least once,
    #   2. nothing beyond committed_seq,
    #   3. its instance's mmap storage was untouched while the other instance's
    #      worker ran (checked above),
    #   4. the successor's own run is complete and unaffected.
    for wid, n_committed in ((0, committed0), (1, 300)):
        combined = ""
        for w in (wid, 2 if wid == 0 else 3):
            combined += "".join(p.read_text(encoding="utf-8", errors="replace")
                for p in sorted(cwd.glob(f"output/worker_{w}_*.log")))
        seen = {}
        for kind in ("normal", "fast"):
            for w, s in re.findall(rf"\b{kind} worker=(\d+) seq=(\d+)", combined):
                if int(w) != wid:
                    continue  # the successor's own records (worker=2/3)
                seen[(kind, int(s))] = seen.get((kind, int(s)), 0) + 1
        for seq in range(n_committed):
            for kind in ("normal", "fast"):
                if seen.get((kind, seq), 0) == 0:
                    fail(f"A{wid}'s committed records lost: {kind} seq={seq}")
        for kind in ("normal", "fast"):
            extra = sorted(s for (k, s) in seen if k == kind and not (0 <= s < n_committed))
            if extra:
                fail(f"worker A{wid} wrote beyond committed_seq: {kind} seq={extra[:8]}")
    # C and D each produce their own full records. C recovered A0's crash and D
    # recovered A1's, so their files may also carry those dead workers' replayed
    # tails (at-least-once), which check_records must not treat as contamination.
    for wid in (2, 3):
        problems = check_records(None, cwd, [wid], records, allowed_foreign=(0, 1))
        if problems[wid]:
            fail(f"worker {wid}: {'; '.join(problems[wid][:8])}")


def case_M6(exe, out, records):
    legacy = exe.parent / "bq_multi_instance_worker_legacy"
    if not legacy.exists():
        legacy = exe.parent / "bq_multi_instance_worker_legacy.exe"
    if not legacy.exists():
        print("SKIP M6: legacy worker not built (use -DBQ_LEGACY_SRC=...)")
        return
    cwd = out / "M6"
    cwd.mkdir(parents=True)
    captures, children = [], []
    mmap_dir = cwd / "bqlog_mmap" / "mmap_m6_log"
    # legacy writes many times the new worker's count so it is still running
    # when the new worker claims its instance and runs cleanup_legacy_dirs
    # (keep the count modest: each record embeds a 128 KB oversize string)
    legacy_records = max(records * 32, 8192)
    try:
        children.append(spawn(legacy, cwd, ["0", "m6_log", str(legacy_records)], captures, "worker_0"))
        # wait until the legacy worker is producing output, then race the new
        # worker against it
        deadline = time.monotonic() + RECORD_TIMEOUT
        while True:
            out_files = list(cwd.glob("output/worker_0_*.log"))
            if out_files and any(p.stat().st_size for p in out_files):
                break
            if children[0].poll() is not None:
                fail("legacy worker exited before the new worker could join")
            if time.monotonic() >= deadline:
                fail("legacy worker never produced output")
            time.sleep(0.05)
        # new worker joins while legacy is still writing
        new = spawn(exe, cwd, ["1", "m6_log", str(records), "--report"], captures, "worker_1")
        children.append(new)
        wait_files([cwd / f"ready_1.json"], [new])
        ready = json.loads((cwd / "ready_1.json").read_text())
        if not ready["lp_mapped"]:
            fail(f"new worker lp_mapped={ready['lp_mapped']}, want true")
        wait_files([cwd / "done_1"], [new])
        if new.wait(timeout=RECORD_TIMEOUT) != 0:
            fail("new worker failed")
        # legacy still running: its mmap_<name>/ must survive cleanup_legacy_dirs
        if not mmap_dir.exists():
            fail("mmap_m6_log was removed while the legacy worker still holds it")
        # after legacy exits and a fresh new worker runs, cleanup removes it
        code0 = children[0].wait(timeout=RECORD_TIMEOUT * 10)
        if code0 != 0:
            fail(f"legacy worker exit code {code0}")
        legacy_problems = check_records(None, cwd, [0], legacy_records)
        if legacy_problems[0]:
            fail(f"legacy worker: {'; '.join(legacy_problems[0][:8])}")
        last = spawn(exe, cwd, ["2", "m6_log", str(records), "--report"], captures, "worker_2")
        children.append(last)
        wait_files([cwd / "done_2"], [last])
        if last.wait(timeout=RECORD_TIMEOUT) != 0:
            fail("final worker failed")
        if mmap_dir.exists():
            fail("mmap_m6_log still exists after all legacy workers exited")
    finally:
        for p in children:
            if p.poll() is None:
                p.kill()
                p.wait()
        for capture in captures:
            capture.close()


def case_M7(exe, out, records):
    host = exe.parent / "bq_dual_runtime_host"
    if not host.exists():
        host = exe.parent / "bq_dual_runtime_host.exe"
    if not host.exists():
        print("SKIP M7: dual runtime host not built")
        return
    cwd = out / "M7"
    cwd.mkdir(parents=True)
    proc = subprocess.run([str(host), "m7_log"], cwd=cwd,
        capture_output=True, text=True, timeout=RECORD_TIMEOUT)
    if proc.returncode != 0:
        fail(f"host exit code {proc.returncode}: {proc.stdout} {proc.stderr}")
    instances = sorted(int(m) for m in re.findall(r"plugin \w+ instance (\d+)", proc.stdout))
    if instances != [0, 1]:
        fail(f"plugin instances {instances}, want [0, 1] (output: {proc.stdout.strip()})")


def case_M8(exe, out, records):
    cwd = out / "M8"
    cwd.mkdir(parents=True)
    captures, children = [], []
    try:
        # parent creates the recovery log (holds p0), spawns --sleep child, exits
        p = spawn(exe, cwd, ["0", "m8_log", str(records), "--child-lock-test", str(cwd)], captures, "worker_0")
        children.append(p)
        # the parent must exit 0 immediately, leaving its sleeping child behind
        if p.wait(timeout=RECORD_TIMEOUT) != 0:
            fail("worker D (parent) did not exit 0")
        # immediately a new worker: must get instance 0 (child inherited nothing)
        e = spawn(exe, cwd, ["1", "m8_log", str(records), "--report"], captures, "worker_1")
        children.append(e)
        wait_files([cwd / "ready_1.json"], [e])
        ready = json.loads((cwd / "ready_1.json").read_text())
        wait_files([cwd / "done_1"], [e])
        if e.wait(timeout=RECORD_TIMEOUT) != 0:
            fail("worker E failed")
    finally:
        for p in children:
            if p.poll() is None:
                p.kill()
                p.wait()
        for capture in captures:
            capture.close()
    if ready["instance"] != 0:
        fail(f"worker E got instance {ready['instance']}, want 0 "
             f"(the sleeping child must not inherit the lock handle)")
    if not ready["lp_mapped"]:
        fail(f"worker E lp_mapped={ready['lp_mapped']}, want true")


def case_M9(exe, out, records):
    """Recovery after a flush-then-die: everything committed was on disk, so the
    successor must deliver each committed record exactly once (no replay)."""
    cwd = out / "M9"
    cwd.mkdir(parents=True)
    captures, children = [], []
    committed = min(records, 300)
    try:
        d = spawn(exe, cwd,
            ["0", "m9_log", str(records), "--die-after", str(committed), "--die-flush"],
            captures, "worker_0")
        children.append(d)
        wait_files([cwd / "committed_0.json"], [d])
        # the die path self-kills regardless of the flush
        if d.wait(timeout=RECORD_TIMEOUT) not in (0, 9, -9):
            fail("worker D (flush-die) did not self-kill as expected")
        committed0 = json.loads((cwd / "committed_0.json").read_text())["committed_seq"]
        # the successor must recover the deferred records
        e = spawn(exe, cwd, ["1", "m9_log", str(records), "--report"], captures, "worker_1")
        children.append(e)
        wait_files([cwd / "done_1"], [e])
        if e.wait(timeout=RECORD_TIMEOUT) != 0:
            fail("worker E failed")
    finally:
        for p in children:
            if p.poll() is None:
                p.kill()
                p.wait()
        for capture in captures:
            capture.close()
    # flush-die means the records were persisted before the kill, so the successor must
    # not produce a duplicate of any record the dead worker already wrote
    combined = ""
    for w in (0, 1):
        combined += "".join(p.read_text(encoding="utf-8", errors="replace")
            for p in sorted(cwd.glob(f"output/worker_{w}_*.log")))
    seen = {}
    for kind in ("normal", "fast"):
        for w, s in re.findall(rf"\b{kind} worker=(\d+) seq=(\d+)", combined):
            if int(w) != 0:
                continue
            seen[(kind, int(s))] = seen.get((kind, int(s)), 0) + 1
    for seq in range(committed0):
        for kind in ("normal", "fast"):
            if seen.get((kind, seq), 0) == 0:
                fail(f"flush-die: committed record lost: {kind} seq={seq}")
            if seen.get((kind, seq), 0) > 1:
                fail(f"flush-die: committed record replayed: {kind} seq={seq} x{seen[(kind, seq)]}")


CASES = {
    "M1": case_M1,
    "M2": case_M2,
    "M3": case_M3,
    "M4": case_M4,
    "M5": case_M5,
    "M6": case_M6,
    "M7": case_M7,
    "M8": case_M8,
    "M9": case_M9,
}


def run_case(name, exe, out, records):
    result = {"case": name, "passed": False}
    try:
        CASES[name](exe, out, records)
        result["passed"] = True
    except CaseError as exc:
        result["error"] = str(exc)
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as exc:
        result["error"] = f"{type(exc).__name__}: {exc}"
    (out / name / "result.json").parent.mkdir(parents=True, exist_ok=True)
    (out / name / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    return result


def clear_dir(path):
    # Windows can hold directory handles briefly after worker processes exit
    for _ in range(10):
        if not path.exists():
            return
        try:
            shutil.rmtree(path)
            return
        except PermissionError:
            time.sleep(2)
    raise CaseError(f"cannot clear {path}: still locked after 20s")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path,
        help="output directory; cleaned before the run")
    parser.add_argument("--case", choices=sorted(CASES),
        help="run a single case; default all")
    parser.add_argument("--records", type=int, default=256)
    args = parser.parse_args()
    exe, out = args.worker.resolve(), args.out.resolve()
    if not exe.is_file() or args.records < 1:
        parser.error("worker must exist and records >= 1")
    # every case needs the instance directory, which needs the platform instance lock
    capability = subprocess.run([str(exe), "--recovery-supported"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10)
    if capability.returncode == 77:
        print("SKIP multi-instance cases: runtime reports no platform recovery support")
        return 77
    if capability.returncode != 0:
        print("FAIL runtime capability query:", capability.stdout.decode(errors="replace"))
        return 1
    clear_dir(out)
    out.mkdir(parents=True)
    names = [args.case] if args.case else sorted(CASES)
    summary = {}
    failed = False
    for name in names:
        result = run_case(name, exe, out, args.records)
        summary[name] = result
        status = "PASS" if result["passed"] else "FAIL"
        if not result["passed"]:
            failed = True
        print(f"{name}: {status}")
        if not result["passed"]:
            print(f"  {result.get('error', 'unknown error')}")
        sys.stdout.flush()
    (out / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
