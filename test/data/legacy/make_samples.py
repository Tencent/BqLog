#!/usr/bin/env python3
"""Builds test/data/legacy: real files written by old tags, plus their golden decodes.

Each sample is produced by a small generator that links the tag's own runtime (see the manifest's
build_command) and is decoded with that tag's own BqLog_LogDecoder, so the golden output is what
the release that wrote the file produced, not what this tree believes it should produce.

Crash samples are made by killing the generator mid-stream and restarting it on the same
directory; the retry loop exists because which of the two recovery segments appears depends on
whether the process died with the appender cache dirty or the log buffer dirty.

Usage:
    make_samples.py --tag 2.3.1 --worktree /path/to/worktree --gen /path/to/BqLog_LegacySampleGen \
                    --decoder /path/to/BqLog_LogDecoder --out /path/to/test/data/legacy
"""
import argparse
import hashlib
import json
import os
import shutil
import struct
import random
import subprocess
import sys
import tempfile

KEY = os.path.join(os.path.dirname(os.path.abspath(__file__)), "legacy_key.pem")

SEG_TYPE = {1: "normal", 2: "recovery_by_appender", 3: "recovery_by_log_buffer"}


def scan(path):
    with open(path, "rb") as f:
        data = f.read()
    version = struct.unpack_from("<I", data, 0)[0]
    segments = []
    pos = 8
    while pos + 12 <= len(data):
        next_pos, seg_type, enc_type = struct.unpack_from("<QBB", data, pos)
        has_key = data[pos + 10] if version == 9 else 0
        payload_base = pos + 12
        if version == 9 and enc_type == 2 and has_key:
            payload_base += 256 + 16 + 32768
        elif version == 10 and enc_type == 2:
            payload_base += 8 + 256 + 16 + 32768
        elif version >= 11 and enc_type == 3:
            payload_base += 268
        segments.append(
            {
                "start": pos,
                "seg_type": SEG_TYPE.get(seg_type, "unknown(%d)" % seg_type),
                "enc_type": enc_type,
                "payload_base": payload_base,
                "payload_len": (len(data) if next_pos == 0xFFFFFFFFFFFFFFFF else next_pos) - payload_base,
            }
        )
        if next_pos == 0xFFFFFFFFFFFFFFFF:
            break
        pos = next_pos
    return {"version": version, "size": len(data), "segments": segments}


def run(gen, args, cwd, expect_crash=False):
    env = dict(os.environ)
    env["TZ"] = "Asia/Shanghai"
    proc = subprocess.run([gen] + args, cwd=cwd, env=env, capture_output=True)
    if expect_crash:
        if proc.returncode not in (-9, 137):
            raise SystemExit("expected the generator to be killed, got %d" % proc.returncode)
    elif proc.returncode != 0:
        raise SystemExit("generator failed: %d %s" % (proc.returncode, proc.stderr.decode("utf-8", "replace")))


def collect(workdir):
    """Returns {file name: scan} for every log file the run produced."""
    found = {}
    for name in sorted(os.listdir(workdir)):
        if name.endswith(".logcompr"):
            found[name] = scan(os.path.join(workdir, name))
    return found


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def want(found, predicate, what):
    for name, info in found.items():
        if predicate(info):
            return name, info
    raise LookupError(what)


def gen_clean(gen, workdir, pub, mode, entries):
    run(gen, [workdir, "0", pub, mode, str(entries), "0", "0"], workdir)


def gen_crash(gen, workdir, pub, mode, entries, recovery, delay_us, throttle_us):
    run(gen, [workdir, "0", pub, mode, str(entries), "1", str(recovery), str(delay_us), str(throttle_us)],
        workdir, expect_crash=True)
    run(gen, [workdir, "resume", pub, mode, str(entries), "0", str(recovery)], workdir)


def make_samples(gen, decoder, tag, out_dir, version, attempts=12, delay_lo=3000, delay_mid=20000,
                 delay_hi=50000, throttle_us=300):
    samples = []

    def emit(sample_id, src_path, description):
        target = os.path.join(out_dir, sample_id + ".logcompr")
        shutil.copyfile(src_path, target)
        info = scan(target)
        if info["size"] > 200 * 1024:
            raise SystemExit("%s is %d bytes, over the 200 KB budget" % (sample_id, info["size"]))
        expected = os.path.join(out_dir, sample_id + ".expected.txt")
        subprocess.run([decoder, "-k", KEY, "-o", expected, target], check=True, capture_output=True)
        samples.append(
            {
                "id": sample_id,
                "file": os.path.basename(target),
                "expected": os.path.basename(expected),
                "description": description,
                "size": info["size"],
                "segments": [s["seg_type"] for s in info["segments"]],
                "enc_types": sorted({s["enc_type"] for s in info["segments"]}),
                "sha256_file": sha256(target),
                "sha256_expected": sha256(expected),
            }
        )
        print("  %-10s %6d B  %s" % (sample_id, info["size"], " + ".join(s["seg_type"] for s in info["segments"])))

    workdir = tempfile.mkdtemp(prefix="bqsample_")
    pub_key = open(os.path.join(os.path.dirname(KEY), "legacy_pub.txt")).read().strip()

    # Plaintext and encrypted, one clean run each: no recovery, so the file is nothing but normal
    # segments and the record layer alone is under test.
    for sample_id, use_key, mode in (
        ("plain", "-", "sync"),
        ("enc", pub_key, "sync"),
    ):
        work = os.path.join(workdir, sample_id)
        os.makedirs(work)
        gen_clean(gen, work, use_key, mode, 40)
        name, _ = want(collect(work), lambda i: True, "no file from the clean run")
        emit(sample_id, os.path.join(work, name), "clean run, %s" % ("encrypted" if use_key != "-" else "plaintext"))

    # Two crash runs, because the two recovery kinds come from two different states at the moment
    # of death: entries still queued in the log buffer, or entries the worker has moved into the
    # appender's write cache but not yet flushed. One run does not reliably produce both.
    for sample_id, kind, lo, hi in (
        ("enc_recover_buffer", "recovery_by_log_buffer", delay_lo, delay_mid),
        ("enc_recover_appender", "recovery_by_appender", delay_mid, delay_hi),
    ):
        for attempt in range(attempts):
            work = os.path.join(workdir, "%s_%d" % (sample_id, attempt))
            os.makedirs(work)
            delay = random.randrange(lo, hi, 500)
            gen_crash(gen, work, pub_key, "async", 40, 1, delay, throttle_us)
            found = collect(work)
            kinds = {n: [x["seg_type"] for x in i["segments"]] for n, i in found.items()}
            hit = [n for n, k in kinds.items() if kind in k]
            print("  %s attempt %d (delay %d): %s" % (sample_id, attempt, delay, kinds))
            if hit and found[hit[0]]["size"] <= 200 * 1024:
                emit(sample_id, os.path.join(work, hit[0]), "crashed with a dirty %s" % kind.replace("recovery_by_", ""))
                break
            shutil.rmtree(work, ignore_errors=True)
        else:
            raise SystemExit("no %s sample in %d attempts" % (kind, attempts))

    shutil.rmtree(workdir, ignore_errors=True)
    return samples


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tag", required=True)
    ap.add_argument("--commit", required=True)
    ap.add_argument("--gen", required=True)
    ap.add_argument("--decoder", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--version", required=True)
    ap.add_argument("--build-command", required=True)
    ap.add_argument("--delay-lo", type=int, default=3000)
    ap.add_argument("--delay-mid", type=int, default=20000)
    ap.add_argument("--delay-hi", type=int, default=50000)
    ap.add_argument("--throttle-us", type=int, default=300)
    ap.add_argument("--attempts", type=int, default=12)
    args = ap.parse_args()

    out_dir = os.path.join(args.out, "v" + args.version)
    os.makedirs(out_dir, exist_ok=True)
    samples = make_samples(args.gen, args.decoder, args.tag, out_dir, args.version,
                           attempts=args.attempts, delay_lo=args.delay_lo, delay_mid=args.delay_mid,
                           delay_hi=args.delay_hi, throttle_us=args.throttle_us)

    manifest_path = os.path.join(out_dir, "manifest.json")
    manifest = {
        "format_version": int(args.version),
        "source_tag": args.tag,
        "source_commit": args.commit,
        "build_command": args.build_command,
        "key": "legacy_key.pem / legacy_pub.txt (the pair the unit tests already carry)",
        "samples": samples,
    }
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print("wrote %s" % manifest_path)


if __name__ == "__main__":
    main()
