#!/usr/bin/env python3
"""Writes a translation unit with <count> distinct fast-log call sites.

Usage: python gen.py <count> <macro|fast> [output_file]

Every call site uses its own format string (the room number makes it distinct), so the
compiler has to instantiate one site per call -- which is what makes the symbol table grow.
"""
import os
import sys

FORMAT = "player {} entered room %d at position ({}) with hp {} and shield {} of {}"


def generate(count, mode):
    lines = [
        '#include "bq_log/bq_log.h"',
        "",
        "extern bq::log g_log;",
        "",
        "void work(int32_t x, float y, const char* name)",
        "{",
    ]
    for i in range(count):
        fmt = FORMAT % i
        if mode == "macro":
            lines.append('    BQ_LOG_FAST_INFO(g_log, "%s", x, name, y, x, y, x);' % fmt)
        else:
            lines.append('    g_log.fast_info<"%s">(x, name, y, x, y, x);' % fmt)
    lines.append("}")
    return "\n".join(lines) + "\n"


def main():
    if len(sys.argv) < 3:
        sys.stderr.write(__doc__)
        return 1
    count = int(sys.argv[1])
    mode = sys.argv[2]
    if mode not in ("macro", "fast"):
        sys.stderr.write("mode must be macro or fast\n")
        return 1
    out_path = sys.argv[3] if len(sys.argv) > 3 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "gen.cpp")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(generate(count, mode))
    return 0


if __name__ == "__main__":
    sys.exit(main())
