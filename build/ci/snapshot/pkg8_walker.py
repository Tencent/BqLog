#!/usr/bin/env python3
"""pkg(8)-family dependency-closure walker (FreeBSD / DragonFlyBSD dports).

Downloads the repo catalog (packagesite.pkg, zstd tar) and walks the deps
graph, fetching every package needed for a set of root packages.

Usage: pkg8_walker.py <repo_base_url> <outdir> <root1> [root2 ...]
Roots are package names as in the catalog "name" field.
"""
import json, os, subprocess, sys, urllib.request

repo_base, outdir = sys.argv[1], sys.argv[2]
roots = sys.argv[3:]
os.makedirs(outdir, exist_ok=True)

def fetch(url, dest):
    urllib.request.urlretrieve(url, dest)

# --- catalog ---
cat_path = os.path.join(outdir, "..", "catalog.yaml")
if not os.path.exists(cat_path):
    pkgf = os.path.join(outdir, "..", "packagesite.pkg")
    fetch(repo_base.rstrip("/") + "/packagesite.pkg", pkgf)
    subprocess.run(["tar", "--zstd", "-xf", pkgf, "-C", os.path.dirname(pkgf)], check=True)
    os.replace(os.path.join(os.path.dirname(pkgf), "packagesite.yaml"), cat_path)

catalog = {}
with open(cat_path) as f:
    for line in f:
        d = json.loads(line)
        catalog[d["name"]] = d
print(f"catalog: {len(catalog)} packages at {repo_base}")

missing_roots = [r for r in roots if r not in catalog]
if missing_roots:
    print("ROOTS NOT IN CATALOG:", missing_roots); sys.exit(1)

seen, queue, missing = set(), list(roots), []
while queue:
    name = queue.pop(0)
    if name in seen:
        continue
    seen.add(name)
    d = catalog.get(name)
    if d is None:
        print("NOT IN CATALOG:", name); missing.append(name); continue
    path = os.path.join(outdir, os.path.basename(d["path"]))
    if not os.path.exists(path):
        try:
            fetch(repo_base.rstrip("/") + "/" + d["path"], path)
        except Exception as e:
            print("DOWNLOAD FAIL:", name, e); missing.append(name); continue
    for dep in (d.get("deps") or {}):
        if dep not in seen:
            queue.append(dep)

print(f"fetched {len(seen) - len(missing)} packages, {len(missing)} missing")
for m in missing:
    print("  MISSING:", m)
sys.exit(1 if missing else 0)
