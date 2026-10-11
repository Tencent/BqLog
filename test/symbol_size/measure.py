#!/usr/bin/env python3
"""Measures what <count> fast-log call sites cost in the object file and the shared library.

Usage: python measure.py [--cxx g++] [--nm nm] [--extra "<flags>"] [--counts 200,2000] [--out <json>]

For each count and mode (macro / fast_info) it reports the object file size, the total bytes of the
mangled symbol names (the number T10 shrinks, because every format string is part of a template
entity), and how many of the site-holder symbols are still global. With an ELF compiler it also
builds a shared library and counts the site holders that reached the dynamic symbol table -- the
visibility attribute does nothing on PE, so use the NDK or a Linux toolchain for that check.

The translation unit is not linked against the BqLog library: an object file and a shared library
are enough to see the symbols, and that keeps this test free of a library build.
"""
import argparse
import collections
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SITE_HOLDER = "fast_info_site_holder"
GLOBAL_BINDINGS = "ABCDGRSTUVW"


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, errors="replace")


def tool(cxx, name, fallback):
    candidate = os.path.join(os.path.dirname(cxx), name)
    return candidate if os.path.exists(candidate) else fallback


def nm_lines(nm, path, dynamic):
    r = run([nm, "-D", path] if dynamic else [nm, path])
    return r.stdout.splitlines() if r.returncode == 0 else []


def parse(lines):
    """(binding, name) for every symbol line, dropping section symbols like '.data$_ZN...'."""
    out = []
    for line in lines:
        parts = line.split(None, 2)
        if len(parts) == 3 and not parts[2].startswith("."):
            out.append((parts[1], parts[2]))
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--nm", default="")
    parser.add_argument("--extra", default="")
    parser.add_argument("--counts", default="200,2000")
    parser.add_argument("--label", default="current")
    parser.add_argument("--out", default="")
    args = parser.parse_args()

    cxx = args.cxx
    nm = args.nm or tool(cxx, "llvm-nm", tool(cxx, "nm", "nm"))
    inc = ["-I" + os.path.join(REPO, "include"), "-I" + os.path.join(REPO, "src")]
    extra = args.extra.split()
    tmp = os.path.join(HERE, "mi_out")
    os.makedirs(tmp, exist_ok=True)

    rows = []
    for mode in ("macro", "fast"):
        for count in [int(c) for c in args.counts.split(",") if c]:
            src = os.path.join(tmp, "gen_%s_%d.cpp" % (mode, count))
            if run([sys.executable, os.path.join(HERE, "gen.py"), str(count), mode, src]).returncode != 0:
                raise SystemExit("gen.py failed")
            obj = os.path.join(tmp, "gen_%s_%d.o" % (mode, count))
            r = run([cxx, "-std=c++20", "-O2", "-c", src, "-o", obj] + extra + inc)
            if r.returncode != 0:
                raise SystemExit("compile failed:\n" + r.stderr)

            symbols = parse(nm_lines(nm, obj, False))
            holders = [b for b, n in symbols if SITE_HOLDER in n]
            row = {
                "mode": mode,
                "count": count,
                "sym_bytes": sum(len(n) + 1 for _, n in symbols),
                "sym_count": len(symbols),
                "obj_bytes": os.path.getsize(obj),
                "site_holder_symbols": len(holders),
                "site_holder_global": sum(1 for b in holders if b in GLOBAL_BINDINGS),
            }

            so = os.path.join(tmp, "gen_%s_%d.so" % (mode, count))
            r = run([cxx, "-std=c++20", "-O2", "-fPIC", "-shared", src, "-o", so] + extra + inc)
            if r.returncode == 0:
                row["so_bytes"] = os.path.getsize(so)
                dynamic = parse(nm_lines(nm, so, True))
                row["so_dynamic_symbols"] = len(dynamic)
                row["so_site_holder_dynamic"] = sum(1 for _, n in dynamic if SITE_HOLDER in n)
                if run([tool(cxx, "strip", "strip"), so]).returncode == 0:
                    row["so_bytes_stripped"] = os.path.getsize(so)
            rows.append(row)

    print("%-6s %-6s %10s %9s %10s %9s %9s %9s" % (
        "mode", "count", "sym_bytes", "sym_cnt", "obj_bytes", "holders", "global", "so_dyn"))
    for row in rows:
        print("%-6s %-6d %10d %9d %10d %9d %9d %9s" % (
            row["mode"], row["count"], row["sym_bytes"], row["sym_count"], row["obj_bytes"],
            row["site_holder_symbols"], row["site_holder_global"],
            row.get("so_site_holder_dynamic", "-")))

    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            json.dump({"label": args.label, "cxx": cxx, "extra": args.extra, "rows": rows}, f, indent=2)
        print("wrote " + args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
