#!/usr/bin/env python3
"""Counts instructions of the fast path probe functions.

Usage:
  python count.py --cxx g++ --std c++20 --extra "-mcmodel=small"

Compiles probe.cpp to assembly and counts, for every probe_* function, the
instructions from the label line to the first ret (inclusive), skipping empty
lines, labels and lines beginning with a dot (assembler directives).
MSVC: cl /O2 /FA /c probe.cpp, rules are adjusted for .asm output on the fly.
"""
import argparse
import re
import subprocess
import sys
import tempfile
import os

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def parse_gcc_style(asm_text):
    results = {}
    current = None
    for raw in asm_text.splitlines():
        line = raw.strip()
        if not line:
            continue
        m = re.match(r"^(_Z\d+probe_\w+|probe_\w+):$", line)
        if m:
            current = m.group(1)
            results.setdefault(current, 0)
            continue
        if current is None:
            continue
        if line.startswith("."):
            continue
        if line.endswith(":"):
            continue
        # count the instruction until the first ret
        results[current] += 1
        if re.match(r"^(ret|rep ret)\b", line):
            current = None
    return results


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--std", default="c++20")
    parser.add_argument("--extra", default="")
    args = parser.parse_args()

    probe = os.path.join(os.path.dirname(os.path.abspath(__file__)), "probe.cpp")
    with tempfile.TemporaryDirectory() as tmp:
        asm_path = os.path.join(tmp, "probe.s")
        cmd = [args.cxx, "-std=" + args.std, "-O2"] + args.extra.split() + [
            "-S", "-I" + os.path.join(REPO, "include"), probe, "-o", asm_path]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            sys.stderr.write(r.stdout + r.stderr)
            return 1
        with open(asm_path, "r", encoding="utf-8", errors="replace") as f:
            asm_text = f.read()

    results = parse_gcc_style(asm_text)
    names = sorted(n for n in results if "probe_" in n)
    if not names:
        sys.stderr.write("no probe_* symbols found\n")
        return 1
    for n in names:
        print("%s %d" % (n, results[n]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
