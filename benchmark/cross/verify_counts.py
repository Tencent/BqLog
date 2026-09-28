import glob
import os
import shutil
import subprocess
import sys

RUN = r"E:\BqLog\benchmark\cross\run"
EXE = r"E:\BqLog\benchmark\cross\build\Release"
THREADS = 4
EXPECT_PER_TEST = 2_000_000 * THREADS  # 8M per test file

def count_lines(pattern):
    total = 0
    for p in glob.glob(os.path.join(RUN, "output", pattern)):
        n = 0
        with open(p, "rb") as f:
            while chunk := f.read(1 << 24):
                n += chunk.count(b"\n")
        total += n
    return total

def clean():
    out = os.path.join(RUN, "output")
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)

def run(lib, args):
    r = subprocess.run([os.path.join(EXE, f"bench_{lib}.exe")] + args,
                       cwd=RUN, capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if line.startswith("RESULT|"):
            print(" ", line)

for lib in ["bqlog", "spdlog", "glog", "quill"]:
    clean()
    run(lib, [str(THREADS)])
    if lib == "bqlog":
        n = count_lines("bqlog_text*")
        print(f"bqlog  t={THREADS} text_lines={n} (expect {2*EXPECT_PER_TEST})")
    elif lib == "glog":
        n = count_lines("*INFO*")
        print(f"glog   t={THREADS} total_lines={n} (expect {2*EXPECT_PER_TEST})")
    else:
        mp, np_ = count_lines("*_mp*"), count_lines("*_np*")
        print(f"{lib:7s}t={THREADS} mp={mp} np={np_} (each expect {EXPECT_PER_TEST})")

clean()
run("fmtlog", [str(THREADS), "mp"])
mp = count_lines("fmtlog_mp*")
run("fmtlog", [str(THREADS), "np"])
np_ = count_lines("fmtlog_np*")
print(f"fmtlog t={THREADS} mp={mp} np={np_} (each expect {EXPECT_PER_TEST})")
