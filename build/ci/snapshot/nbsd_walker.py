#!/usr/bin/env python3
"""NetBSD pkgsrc dependency-closure walker.

Walks @pkgdep entries from a pkgsrc binary package mirror and downloads the
full dependency closure for a set of root packages.

Usage: nbsd_walker.py <repo_base_url> <outdir> <root1.tgz> [root2.tgz ...]
Roots are exact filenames from the mirror listing.
"""
import re, os, subprocess, sys, urllib.request

base, outdir = sys.argv[1].rstrip("/") + "/", sys.argv[2]
roots = sys.argv[3:]
os.makedirs(outdir, exist_ok=True)

def ver_key(v):
    # "8.20.0nb1" -> ([8,20,0], 1); "1.17.0.18.8nb2" -> ([1,17,0,18,8], 2)
    nb = 0
    m = re.search(r"nb(\d+)$", v)
    if m:
        nb = int(m.group(1)); v = v[:m.start()]
    parts = []
    for seg in v.split("."):
        parts.append(int(seg) if seg.isdigit() else seg)
    return (parts, nb)

def cmp_ver(a, b):
    ka, kb = ver_key(a), ver_key(b)
    pa, pb = ka[0], kb[0]
    for i in range(max(len(pa), len(pb))):
        sa = pa[i] if i < len(pa) else 0
        sb = pb[i] if i < len(pb) else 0
        if sa == sb: continue
        if isinstance(sa, int) and isinstance(sb, int):
            return -1 if sa < sb else 1
        return -1 if str(sa) < str(sb) else 1
    return (ka[1] > kb[1]) - (ka[1] < kb[1])

def stem(fn):
    m = re.match(r"(.+?)-\d", fn)
    return m.group(1) if m else fn

def version_of(fn):
    return fn[len(stem(fn)) + 1:-4]

# --- listing ---
lfile = os.path.join(outdir, "..", "listing-" + re.sub(r"[^A-Za-z0-9]", "_", base) + ".txt")
if os.path.exists(lfile):
    listing = [l.strip() for l in open(lfile) if l.strip()]
else:
    import html.parser
    class P(html.parser.HTMLParser):
        names = []
        def handle_starttag(self, tag, attrs):
            if tag == "a":
                for k, v in attrs:
                    if k == "href" and v.endswith(".tgz"):
                        self.names.append(v.split("/")[-1])
    p = P()
    p.feed(urllib.request.urlopen(base).read().decode())
    listing = p.names
    open(lfile, "w").write("\n".join(listing) + "\n")
listing = sorted(set(listing))
print(f"listing: {len(listing)} at {base}")

by_stem = {}
for fn in listing:
    by_stem.setdefault(stem(fn), []).append(fn)

def resolve(spec):
    # @pkgdep forms: curl>=8.20.0nb1 | foo>=1.0<2.0 | foo-1.2.3 | {a>=1,b>=2}
    #              | bash-[0-9]* | llvm-21.1.8{,nb*}
    spec = spec.strip()
    if spec.startswith("{"):
        for alt in spec.strip("{}").split(","):
            r = resolve(alt)
            if r: return r
        return None
    # exact version with optional nb: "llvm-21.1.8{,nb*}"
    m = re.match(r"^([A-Za-z0-9_\-\.\+]+?)-([0-9][0-9A-Za-z\.]*)\{,nb\*\}$", spec)
    if m:
        name, ver = m.groups()
        cands = [fn for fn in by_stem.get(name, [])
                 if version_of(fn) == ver or version_of(fn).startswith(ver + "nb")]
        return sorted(cands, key=lambda f: ver_key(version_of(f)))[-1] if cands else None
    # glob: "bash-[0-9]*" or "foo-*"
    m = re.match(r"^([A-Za-z0-9_\-\.\+]+?)-\[0-9\]\*$", spec) or re.match(r"^([A-Za-z0-9_\-\.\+]+?)-\*$", spec)
    if m:
        cands = sorted(by_stem.get(m.group(1), []), key=lambda f: ver_key(version_of(f)))
        return cands[-1] if cands else None
    m = re.match(r"^([A-Za-z0-9_\-\.\+]+?)(>=|<=|>|<|=)([0-9][0-9A-Za-z\._]*)(.*)$", spec)
    if m:
        name, op, ver, rest = m.groups()
        # name may contain trailing dashes from glob forms like foo-*
        name = name.rstrip("-*")
        cands = [fn for fn in by_stem.get(name, [])]
        def ok(fn):
            c = cmp_ver(version_of(fn), ver)
            return {">=": c >= 0, "<=": c <= 0, ">": c > 0, "<": c < 0, "=": c == 0}[op]
        good = [fn for fn in cands if ok(fn)]
        # handle second constraint e.g. >=1.0<2.0
        m2 = re.match(r"^(>=|<=|>|<)([0-9][0-9A-Za-z\._]*)$", rest)
        if m2:
            op2, ver2 = m2.groups()
            good = [fn for fn in good if {">=": cmp_ver(version_of(fn), ver2) >= 0, "<=": cmp_ver(version_of(fn), ver2) <= 0, ">": cmp_ver(version_of(fn), ver2) > 0, "<": cmp_ver(version_of(fn), ver2) < 0}[op2]]
        if good:
            return sorted(good, key=lambda f: ver_key(version_of(f)))[-1]
        return None
    # exact filename form
    if spec + ".tgz" in listing: return spec + ".tgz"
    # stem only
    cands = sorted(by_stem.get(spec.rstrip("-*"), []), key=lambda f: ver_key(version_of(f)))
    return cands[-1] if cands else None

seen, queue, missing = set(), list(roots), []
while queue:
    fn = queue.pop(0)
    if fn in seen: continue
    seen.add(fn)
    path = os.path.join(outdir, fn)
    if not os.path.exists(path):
        for attempt in (1, 2, 3):
            try:
                urllib.request.urlretrieve(base + fn, path); break
            except Exception as e:
                if attempt == 3:
                    print("DOWNLOAD FAIL:", fn, e); missing.append(fn)
    if fn in missing: continue
    r = subprocess.run(["tar", "-xOf", path, "+CONTENTS"], capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if line.startswith("@pkgdep "):
            dep = resolve(line[len("@pkgdep "):])
            if dep is None:
                print("UNRESOLVED:", fn, "->", line); missing.append(line)
            elif dep not in seen:
                queue.append(dep)

fetched = len([f for f in seen if f not in missing])
print(f"fetched {fetched} packages, {len(missing)} missing")
for m in missing: print("  MISSING:", m)
sys.exit(1 if missing else 0)
