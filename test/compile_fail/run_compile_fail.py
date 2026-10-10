#!/usr/bin/env python3
"""Compiles each fast_info compile-fail case and asserts it is rejected with the right message.

Usage:
  python run_compile_fail.py [--cxx g++] [--std c++20] [--include <dir> ...]

Each .cpp in this directory is compiled on its own; it must fail, and the compiler output
must contain the case's expected substring. Exits non-zero if any case unexpectedly
compiles or fails with the wrong message. The include directories default to the
repository's include/ and src/ trees, which is what the unit-test build uses.
"""
import argparse
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# case file -> (extra compiler flags, expected substring in the error output)
CASES = {
    "width16_format.cpp": ([], "only narrow or u8"),
    "foreign_category.cpp": ([], "does not belong to this log class"),
    "plain_log_category.cpp": ([], "does not belong to this log class"),
    # only meaningful on GCC/Clang, which support -fexec-charset
    "non_utf8_charset.cpp": (["-fexec-charset=GBK"], "bq_fast_format_requires_utf8_execution_charset"),
}


def default_includes():
    return [
        os.path.join(REPO, "include"),
        os.path.join(REPO, "src"),
        os.path.dirname(os.path.abspath(__file__)),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--std", default="c++20")
    parser.add_argument("--include", action="append", default=[])
    args = parser.parse_args()

    includes = args.include or default_includes()
    inc_flags = []
    for inc in includes:
        inc_flags += ["-I", inc]

    failures = []
    for case, (extra, expected) in CASES.items():
        path = os.path.join(os.path.dirname(os.path.abspath(__file__)), case)
        cmd = [args.cxx, "-std=" + args.std, "-c", path, "-o", os.devnull] + inc_flags + extra
        proc = subprocess.run(cmd, capture_output=True, text=True)
        combined = proc.stdout + proc.stderr
        if proc.returncode == 0:
            failures.append(f"{case}: compiled but must not have")
            print(f"FAIL {case}: compiled but must not have")
        elif expected not in combined:
            failures.append(f"{case}: failed but without '{expected}'")
            print(f"FAIL {case}: no '{expected}' in the error output:\n{combined}")
        else:
            print(f"PASS {case}")

    print(f"{len(CASES) - len(failures)}/{len(CASES)} compile-fail cases behaved as expected")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
