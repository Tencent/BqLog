#!/usr/bin/env bash
# make_snapshots.sh — regenerate all BSD dependency snapshots.
#
# Pure-HTTP bootstrap: no VM / root needed. Each walker resolves the
# install-time dependency closure of the CI toolchain from the official
# package mirror and downloads it. Outputs dist/<target>-deps.tar.gz
# ready for `oras push ghcr.io/pippocao/bqlog-ci-deps/...`.
#
# Why: upstream BSD repos delete packages for EOL releases (all OpenBSD
# < 7.7 packages are already gone from every mirror), so CI pins rot.
# Snapshots freeze the closure forever.
#
# Usage: bash build/ci/snapshot/make_snapshots.sh [target ...]
#   No args = all targets. Targets:
#     openbsd-amd64 openbsd-aarch64 freebsd-amd64 freebsd-aarch64
#     netbsd-amd64 netbsd-aarch64 dragonflybsd-amd64
set -euo pipefail
cd "$(dirname "$0")"
OUT=dist
mkdir -p "$OUT"

pack() {  # pack <target-name> <deps-dir>
    (cd "$2" && tar czf "$(cd ../../"$OUT" && pwd)/$1-deps.tar.gz" .)
    (cd "$OUT" && sha256sum "$1-deps.tar.gz" | tee "$1-deps.tar.gz.sha256")
    echo "=== $1 done ==="
}

openbsd_amd64() {
    d=work-openbsd-7.7-amd64/deps; mkdir -p "$d"
    python3 obsd_walker.py 7.7 amd64 "$d" \
        bash-5.2.37.tgz cmake-3.31.6v1.tgz jdk-11.0.26.4.1v0.tgz node-22.14.0v0.tgz quirks-7.103.tgz
    pack openbsd-7.7-amd64 "$d"
}
openbsd_aarch64() {
    d=work-openbsd-7.7-aarch64/deps; mkdir -p "$d"
    python3 obsd_walker.py 7.7 aarch64 "$d" \
        bash-5.2.37.tgz cmake-3.31.6v1.tgz jdk-11.0.26.4.1v0.tgz node-22.14.0v0.tgz quirks-7.103.tgz
    pack openbsd-7.7-aarch64 "$d"
}

# FreeBSD 13-branch repo now targets 13.5; verified installable on 13.2 for
# the C++ chain (nodejs jobs run on 13.5 where pkg node works).
freebsd_amd64() {
    d=work-freebsd-13-amd64/deps; mkdir -p "$d"
    python3 pkg8_walker.py "https://pkg.freebsd.org/FreeBSD:13:amd64/latest" "$d" \
        llvm cmake bash openjdk11 gdb gcc node npm-node22 \
        python3 py311-pip py311-setuptools py311-wheel
    pack freebsd-13-amd64 "$d"
}
freebsd_aarch64() {
    d=work-freebsd-13-aarch64/deps; mkdir -p "$d"
    python3 pkg8_walker.py "https://pkg.freebsd.org/FreeBSD:13:aarch64/latest" "$d" \
        llvm cmake bash openjdk11 gdb gcc node npm-node22
    pack freebsd-13-aarch64 "$d"
}

# nodejs is absent from the NetBSD 10.1 repo; append the closure from the
# 10.0_2026Q1 repo into the same snapshot dir (mirrors the CI fallback).
# NOTE: aarch64/10.1 publishes older package versions than amd64/10.1, and
# has no gdb package at all (CI tolerates it via `|| true`).
netbsd_amd64() {
    d=work-netbsd-10.1-amd64/deps; mkdir -p "$d"
    python3 nbsd_walker.py "https://cdn.NetBSD.org/pub/pkgsrc/packages/NetBSD/amd64/10.1/All" "$d" \
        cmake-4.3.3.tgz gmake-4.4.1.tgz bash-5.3.15.tgz gdb-10.1nb6.tgz \
        clang-21.1.8.tgz gcc12-12.5.0nb2.tgz openjdk17-1.17.0.18.8nb2.tgz pkgin-26.4.0.tgz
    python3 nbsd_walker.py "https://cdn.NetBSD.org/pub/pkgsrc/packages/NetBSD/amd64/10.0_2026Q1/All" "$d" \
        nodejs-25.8.2.tgz
    pack netbsd-10.1-amd64 "$d"
}
netbsd_aarch64() {
    d=work-netbsd-10.1-aarch64/deps; mkdir -p "$d"
    python3 nbsd_walker.py "https://cdn.NetBSD.org/pub/pkgsrc/packages/NetBSD/aarch64/10.1/All" "$d" \
        cmake-4.2.3nb1.tgz gmake-4.4.1.tgz bash-5.3.9.tgz \
        clang-19.1.7nb2.tgz gcc12-12.5.0nb1.tgz openjdk17-1.17.0.18.8nb1.tgz pkgin-26.2.0.tgz
    python3 nbsd_walker.py "https://cdn.NetBSD.org/pub/pkgsrc/packages/NetBSD/aarch64/10.0_2026Q1/All" "$d" \
        nodejs-25.8.2.tgz
    pack netbsd-10.1-aarch64 "$d"
}

# NOTE: DragonFly's base pkg(8) cannot create a repository catalogue from
# zstd-compressed packages ("zstd is supported, but not builtin"), so the
# snapshot is laid out as a ready-made partial mirror (meta.conf +
# packagesite.txz + All/*.pkg) and consumers skip `pkg repo` entirely.
dragonflybsd_amd64() {
    d=work-dragonflybsd-6.4.2-amd64/deps; mkdir -p "$d"
    python3 pkg8_walker.py "https://mirror-master.dragonflybsd.org/dports/dragonfly:6.4:x86:64/LATEST" "$d" \
        cmake bash gmake gdb openjdk11 gcc llvm node20 npm-node20
    base="https://mirror-master.dragonflybsd.org/dports/dragonfly:6.4:x86:64/LATEST"
    mkdir -p "$d/mirror/All"
    mv "$d"/*.pkg "$d/mirror/All/"
    # Ship ALL repo metadata: this pkg(8) version probes the data archive
    # unconditionally during `update` and fails when it is absent, so a
    # slim meta.conf is not enough. filesite is not published by dports.
    for f in meta.conf meta data.txz data.pkg packagesite.pkg packagesite.txz; do
        curl -fsSL -o "$d/mirror/$f" "$base/$f"
    done
    pack dragonflybsd-6.4.2-amd64 "$d/mirror"
}

if [ $# -gt 0 ]; then
    for t in "$@"; do "${t//-/_}"; done
else
    openbsd_amd64
    openbsd_aarch64
    freebsd_amd64
    freebsd_aarch64
    netbsd_amd64
    netbsd_aarch64
    dragonflybsd_amd64
fi
echo "all snapshots in $OUT/"
