#!/usr/bin/env python3
"""OpenBSD dependency-closure walker.

Walks @depend entries from the official package mirror and downloads the
full dependency closure for a set of root packages, without needing a VM.

Usage: obsd_walker.py <release> <arch> <outdir> <root1.tgz> [root2.tgz ...]
Roots are exact filenames from the mirror listing.
"""
import re, os, subprocess, sys, urllib.request

release, arch, outdir = sys.argv[1], sys.argv[2], sys.argv[3]
roots = sys.argv[4:]
BASE = f"https://cdn.openbsd.org/pub/OpenBSD/{release}/packages/{arch}/"
os.makedirs(outdir, exist_ok=True)

def fetch(url, dest):
    urllib.request.urlretrieve(url, dest)

# package listing
listing_path = os.path.join(outdir, "..", f"listing-{release}-{arch}.txt")
if os.path.exists(listing_path):
    listing = [l.strip() for l in open(listing_path) if l.strip()]
else:
    import html.parser
    class P(html.parser.HTMLParser):
        names = []
        def handle_starttag(self, tag, attrs):
            if tag == "a":
                for k, v in attrs:
                    if k == "href" and v.endswith(".tgz"):
                        self.names.append(v)
    p = P()
    p.feed(urllib.request.urlopen(BASE).read().decode())
    listing = p.names
    open(listing_path, "w").write("\n".join(listing) + "\n")
listing = set(listing)
print(f"listing: {len(listing)} packages at {BASE}")

def stem(fn):
    m = re.match(r"(.+?)-\d", fn)
    return m.group(1) if m else fn
by_stem = {}
for fn in listing:
    by_stem.setdefault(stem(fn), []).append(fn)

def resolve(spec):
    # @depend pkgpath,flavors:pkgspec:default-name -> concrete filename
    last = spec.rsplit(":", 1)[-1].strip()
    if last and "*" not in last:
        fn = last + ".tgz"
        if fn in listing:
            return fn
    cand = last.replace("-*", "")
    s = stem(cand) if cand else ""
    hits = sorted(by_stem.get(s, []))
    if hits:
        return hits[-1]
    pp = spec.split(":", 1)[0].split(",")[0]
    for comp in reversed([c for c in pp.split("/") if c]):
        hits = sorted(by_stem.get(comp, []))
        if hits:
            return hits[-1]
    return None

seen, queue = set(), list(roots)
fetched, missing = [], []
while queue:
    fn = queue.pop(0)
    if fn in seen:
        continue
    seen.add(fn)
    path = os.path.join(outdir, fn)
    if not os.path.exists(path):
        try:
            fetch(BASE + fn, path)
        except Exception as e:
            print("DOWNLOAD FAIL:", fn, e)
            if os.path.exists(path):
                os.remove(path)  # drop partial download so re-runs refetch it
            missing.append(fn); continue
    fetched.append(fn)
    r = subprocess.run(["tar", "-xOzf", path, "+CONTENTS"], capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if line.startswith("@depend "):
            dep = resolve(line[len("@depend "):].strip())
            if dep is None:
                print("UNRESOLVED:", fn, "->", line); missing.append(line)
            elif dep not in seen:
                queue.append(dep)

print(f"fetched {len(fetched)} packages, {len(missing)} missing")
for m in missing:
    print("  MISSING:", m)
sys.exit(1 if missing else 0)
