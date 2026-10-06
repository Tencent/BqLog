#!/usr/bin/env python3
# Turns one platform's result directory into the markdown tables used in docs/BENCHMARK.md.
# usage: make_tables.py <run_dir> [en|chs]    (results.csv, latency.csv; several rounds each, medians are taken)
import collections
import math
import csv
import os
import statistics
import sys

RUN = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "run")
LANG = sys.argv[2] if len(sys.argv) > 2 else "en"

# (key, English label, Chinese label). BqLog appears in fast mode only, compressed and text; the encrypted and normal
# mode rows are printed by fast_vs_normal() and encryption() for the text that summarises them.
THROUGHPUT_ROWS = [
    ("bqlog_fast_compress", "BqLog, compressed", "BqLog，压缩"),
    ("bqlog_fast_text", "BqLog, text", "BqLog，文本"),
    ("quill", "quill", "quill"),
    ("fmtlog", "fmtlog", "fmtlog"),
    ("spdlog_async", "spdlog (async)", "spdlog（异步）"),
    ("glog", "glog (synchronous)", "glog（同步）"),
    ("log4j2", "Log4j2 (Java)", "Log4j2（Java）"),
    ("bqlog_fast_compress_expand", "BqLog, compressed, expand", "BqLog，压缩，扩容"),
    ("bqlog_fast_text_expand", "BqLog, text, expand", "BqLog，文本，扩容"),
    ("quill_expand", "quill, default queue (grows)", "quill，默认队列（扩容）"),
]
LATENCY_ROWS = [
    ("bqlog_fast", "BqLog", "BqLog"),
    ("quill", "quill", "quill"),
    ("fmtlog", "fmtlog", "fmtlog"),
    ("spdlog_async", "spdlog (async)", "spdlog（异步）"),
]
THREADS_SHOWN = (1, 2, 4, 6, 8, 10)


def label(row):
    return row[2] if LANG == "chs" else row[1]


def threads_header(threads):
    if LANG == "chs":
        return "| | " + " | ".join(f"{n} 线程" for n in threads) + " |"
    return "| | " + " | ".join(f"{n} Thread{'s' if n > 1 else ''}" for n in threads) + " |"


def load_throughput():
    data = collections.defaultdict(lambda: collections.defaultdict(list))
    with open(os.path.join(RUN, "results.csv")) as f:
        for row in csv.DictReader(f):
            data[(row["lib"], row["test"])][int(row["threads"])].append(
                (int(row["ms"]), int(row["cpu_ms"]), float(row["peak_mb"] or 0)))
    return data


def med(values, i):
    return statistics.median(v[i] for v in values)


def throughput():
    data = load_throughput()
    titles = {
        "en": {"multi_param": "4 parameters", "no_param": "no parameter"},
        "chs": {"multi_param": "4 个参数", "no_param": "无参数"},
    }[LANG]
    tables = (
        ("Total time (ms, lower is better)", "总耗时（毫秒，越小越好）", lambda v: "{:.0f}".format(med(v, 0))),
        ("CPU time, all threads (ms, lower is better)", "CPU 时间，所有线程合计（毫秒，越小越好）", lambda v: "{:.0f}".format(med(v, 1))),
        ("Peak memory (MB)", "峰值内存（MB）", lambda v: "{:.1f}".format(med(v, 2))),
    )
    for test in ("multi_param", "no_param"):
        for i, (en, chs, cell) in enumerate(tables):
            if i == 2 and test == "no_param":
                continue
            print(f"\n#### {chs if LANG == 'chs' else en}, {titles[test]}\n")
            print(threads_header(THREADS_SHOWN))
            print("|---|" + "---:|" * len(THREADS_SHOWN))
            for row in THROUGHPUT_ROWS:
                values = data.get((row[0], test))
                if not values or (i == 2 and row[0] == "log4j2"):
                    continue
                print(f"| {label(row)} | " + " | ".join(cell(values[n]) if n in values else "-" for n in THREADS_SHOWN) + " |")


# fast mode / normal mode and encrypted / plain, as ratios of the medians, for the summary text
def ratios():
    data = load_throughput()
    pairs = (
        ("fast/normal compressed", "bqlog_fast_compress", "bqlog_compress"),
        ("fast/normal text", "bqlog_fast_text", "bqlog_text"),
        ("fast/normal compressed expand", "bqlog_fast_compress_expand", "bqlog_compress_expand"),
        ("fast/normal text expand", "bqlog_fast_text_expand", "bqlog_text_expand"),
        ("encrypted/plain fast", "bqlog_fast_compress_enc", "bqlog_fast_compress"),
        ("encrypted/plain normal", "bqlog_compress_enc", "bqlog_compress"),
    )
    print("\n<!-- ratios, 4 parameters, time | CPU, threads " + " ".join(map(str, THREADS_SHOWN)))
    for name, a, b in pairs:
        va, vb = data[(a, "multi_param")], data[(b, "multi_param")]
        t = " ".join("{:.2f}".format(med(va[n], 0) / med(vb[n], 0)) for n in THREADS_SHOWN)
        c = " ".join("{:.2f}".format(med(va[n], 1) / med(vb[n], 1)) for n in THREADS_SHOWN)
        print(f"  {name}: {t} | {c}")
    print("-->")


def latency():
    path = os.path.join(RUN, "latency.csv")
    if not os.path.exists(path):
        return
    data = collections.defaultdict(lambda: collections.defaultdict(list))
    fields = ("mean_ns", "p50_ns", "p99_ns", "p999_ns", "consumer_cpu_pct", "peak_mb")
    with open(path) as f:
        for row in csv.DictReader(f):
            key = (int(row["threads"]), row["lib"])
            for field in fields:
                data[key][field].append(float(row[field]))
    for threads in sorted({k[0] for k in data}):
        if LANG == "chs":
            print(f"\n#### {threads} 个日志线程（各轮中位数）\n")
            print("| | p50（ns） | p99（ns） | p99.9（ns） | 平均（ns） | 消费端 CPU | 峰值内存（MB） |")
        else:
            print(f"\n#### {threads} logging thread{'s' if threads > 1 else ''} (median of rounds)\n")
            print("| | p50 (ns) | p99 (ns) | p99.9 (ns) | mean (ns) | consumer CPU | peak memory (MB) |")
        print("|---|---:|---:|---:|---:|---:|---:|")
        for row in LATENCY_ROWS:
            values = data.get((threads, row[0]))
            if not values:
                continue
            m = lambda field: statistics.median(values[field])
            print(f"| {label(row)} | {m('p50_ns'):.0f} | {m('p99_ns'):.0f} | {m('p999_ns'):.0f} | {m('mean_ns'):.1f} | "
                  f"{m('consumer_cpu_pct'):.1f}% | {m('peak_mb'):.1f} |")


CHART_COLOR = "#2a78d6"  # one series per chart; validated against GitHub's light (#ffffff) and dark (#0d1117) pages


def nice_max(value):
    # the smallest 1/2/2.5/5 x 10^n at least 10% above the largest bar, so bars use most of the axis
    magnitude = 10 ** math.floor(math.log10(value * 1.1))
    for step in (1, 2, 2.5, 5, 10):
        if step * magnitude >= value * 1.1:
            top = step * magnitude
            return int(top) if top >= 1 else top
    return value


# a column chart for GitHub's Mermaid renderer, one bar per library. The values are in the tables right below, so the
# bars carry no labels (Mermaid sizes them to the bar and they come out uneven). The axis title is the short Latin unit:
# without one Mermaid leaves no room for the tick labels, and a rotated CJK title collides with them.
def mermaid_bar(title, unit, rows, fmt="{:.0f}"):
    top = nice_max(max(v for _, v in rows))
    labels = ", ".join(f'"{name}"' for name, _ in rows)
    values = ", ".join(fmt.format(v) for _, v in rows)
    return f"""```mermaid
---
config:
  xyChart:
    width: 760
    height: 360
  themeVariables:
    xyChart:
      plotColorPalette: "{CHART_COLOR}"
---
xychart-beta
    title "{title}"
    x-axis [{labels}]
    y-axis "{unit}" 0 --> {top}
    bar [{values}]
```"""


# short names for the charts, so the axis labels are never cut
CHART_NAMES = {
    "bqlog_fast_compress": ("BqLog compressed", "BqLog 压缩"),
    "bqlog_fast_text": ("BqLog text", "BqLog 文本"),
    "quill": ("quill", "quill"),
    "fmtlog": ("fmtlog", "fmtlog"),
    "spdlog_async": ("spdlog", "spdlog"),
    "glog": ("glog", "glog"),
    "log4j2": ("Log4j2", "Log4j2"),
    "bqlog_fast_compress_expand": ("BqLog compressed", "BqLog 压缩"),
    "bqlog_fast_text_expand": ("BqLog text", "BqLog 文本"),
    "quill_expand": ("quill", "quill"),
    "bqlog_fast": ("BqLog", "BqLog"),
}


def chart_name(key):
    names = CHART_NAMES[key]
    return names[1] if LANG == "chs" else names[0]


# charts for the key comparisons: totals at the largest thread count every library ran (spdlog and glog stop at 6),
# so each chart compares like with like
def charts():
    data = load_throughput()
    chs = LANG == "chs"
    fixed = ("bqlog_fast_compress", "bqlog_fast_text", "quill", "fmtlog", "log4j2")
    expand = ("bqlog_fast_compress_expand", "bqlog_fast_text_expand", "quill_expand")

    def rows(i, wanted, threads):
        out = []
        for key in wanted:
            values = data.get((key, "multi_param"), {}).get(threads)
            if values and med(values, i) > 0:
                out.append((chart_name(key), med(values, i)))
        return out

    def emit(name, title_chs, title_en, unit_chs, unit_en, chart_rows, fmt="{:.0f}"):
        print(f"\n<!-- chart: {name} -->")
        print(mermaid_bar(title_chs if chs else title_en, unit_en, chart_rows, fmt))

    for i, (what_chs, what_en, unit_chs, unit_en, fmt) in enumerate((
            ("总耗时", "Total time", "毫秒", "ms", "{:.0f}"),
            ("CPU 时间", "CPU time", "毫秒", "ms", "{:.0f}"),
            ("峰值内存", "Peak memory", "MB", "MB", "{:.1f}"))):
        emit(f"{what_en} fixed", f"{what_chs}，固定缓冲区，10 线程，4 个参数", f"{what_en}, fixed size buffers, 10 threads, 4 parameters",
            unit_chs, unit_en, rows(i, fixed if i < 2 else fixed[:4], 10), fmt)
        emit(f"{what_en} expand", f"{what_chs}，可扩容，10 线程，4 个参数", f"{what_en}, growing buffers, 10 threads, 4 parameters",
            unit_chs, unit_en, rows(i, expand, 10), "{:.0f}")

    lat = collections.defaultdict(list)
    path = os.path.join(RUN, "latency.csv")
    if os.path.exists(path):
        with open(path) as f:
            for row in csv.DictReader(f):
                if int(row["threads"]) == 1:
                    lat[row["lib"]].append(float(row["mean_ns"]))
        # spdlog is 30x the others and would flatten every other bar to zero: it stays in the table only
        lat_rows = [(chart_name(r[0]), statistics.median(lat[r[0]])) for r in LATENCY_ROWS
                    if lat.get(r[0]) and r[0] != "spdlog_async"]
        spdlog_ns = "{:.0f}".format(statistics.median(lat["spdlog_async"])) if lat.get("spdlog_async") else "-"
        print("\n<!-- chart: latency -->")
        print(mermaid_bar(f"日志线程平均延迟，1 个日志线程（spdlog {spdlog_ns} ns，见表）" if chs
            else f"Mean latency on the logging thread, 1 logging thread (spdlog: {spdlog_ns} ns, see the table)", "ns", lat_rows, "{:.1f}"))


if len(sys.argv) > 3 and sys.argv[3] == "charts":
    charts()
else:
    throughput()
    ratios()
    latency()
