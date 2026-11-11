#!/usr/bin/env python3
# Copyright (C) 2026 Tencent.
# BQLOG is licensed under the Apache License, Version 2.0.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
"""Check the Apple binaries and their dSYMs in a prebuilt package.

Usage: check_apple_symbols.py ios-libs|mac-libs|engine-pkg <dir>

No xcframework may declare DebugSymbolsPath; every shipped dSYM must hold DWARF with the
UUIDs of its binary; the Unity/Tuanjie iOS plugin must be iOS only and embedded.
"""

import plistlib
import re
import struct
import sys
from pathlib import Path

FAT_MAGIC, FAT_MAGIC_64 = 0xCAFEBABE, 0xCAFEBABF
MH_MAGIC, MH_MAGIC_64 = 0xFEEDFACE, 0xFEEDFACF
LC_UUID = 0x1B
CPU_NAMES = {(0x0100000C, 0): "arm64", (0x0100000C, 2): "arm64e", (0x01000007, 3): "x86_64",
             (0x0200000C, 1): "arm64_32", (12, 12): "armv7k", (12, 9): "armv7", (7, 3): "i386"}

errors = []


def fail(msg):
    errors.append(msg)
    print(f"  FAIL {msg}")


def macho_uuids(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < 8:
        return {}
    magic = struct.unpack(">I", data[:4])[0]
    offsets = []
    if magic in (FAT_MAGIC, FAT_MAGIC_64):
        off = 8
        for _ in range(struct.unpack(">I", data[4:8])[0]):
            if magic == FAT_MAGIC:
                offsets.append(struct.unpack(">iiIII", data[off:off + 20])[2])
                off += 20
            else:
                offsets.append(struct.unpack(">iiQQII", data[off:off + 32])[2])
                off += 32
    else:
        offsets.append(0)
    out = {}
    for o in offsets:
        m = struct.unpack("<I", data[o:o + 4])[0]
        if m not in (MH_MAGIC, MH_MAGIC_64):
            continue
        cputype, cpusub, _, ncmds = struct.unpack("<iiII", data[o + 4:o + 20])
        p = o + (32 if m == MH_MAGIC_64 else 28)
        for _ in range(ncmds):
            cmd, cmdsize = struct.unpack("<II", data[p:p + 8])
            if cmd == LC_UUID:
                u = data[p + 8:p + 24].hex().upper()
                arch = CPU_NAMES.get((cputype & 0xFFFFFFFF, cpusub & 0x00FFFFFF), f"cpu{cputype}/{cpusub}")
                out[arch] = f"{u[:8]}-{u[8:12]}-{u[12:16]}-{u[16:20]}-{u[20:]}"
                break
            p += cmdsize
    return out


def check_dsym(dsym: Path, binary: Path, label: str):
    dwarf_dir = dsym / "Contents/Resources/DWARF"
    files = sorted(p for p in dwarf_dir.glob("*") if p.is_file()) if dwarf_dir.is_dir() else []
    if not files:
        fail(f"{label}: empty dSYM: {dsym}")
        return
    if b"__debug_info" not in files[0].read_bytes():
        fail(f"{label}: dSYM has no __debug_info: {files[0]}")
    bu, du = macho_uuids(binary), macho_uuids(files[0])
    if not bu:
        fail(f"{label}: cannot read the UUID of {binary}")
    elif bu != du:
        fail(f"{label}: UUID mismatch, binary {bu}, dSYM {du}")
    else:
        print(f"  ok   {label}: {', '.join(f'{a}={u}' for a, u in sorted(bu.items()))}")


def framework_binary(fw: Path) -> Path:
    name = fw.name[:-len(".framework")]
    for cand in (fw / name, fw / "Versions/Current" / name, fw / "Versions/A" / name):
        if cand.is_file():
            return cand
    return fw / name


def xcframework_libs(xcfw: Path):
    with open(xcfw / "Info.plist", "rb") as f:
        return plistlib.load(f)["AvailableLibraries"]


def check_xcframework(xcfw: Path, dsyms: Path, label: str):
    libs = xcframework_libs(xcfw)
    ids = {lib["LibraryIdentifier"] for lib in libs}
    for lib in libs:
        if "DebugSymbolsPath" in lib:
            fail(f"{label}{lib['LibraryIdentifier']}: Info.plist declares DebugSymbolsPath")
    on_disk = {p.name for p in xcfw.iterdir() if p.is_dir()}
    if on_disk != ids:
        fail(f"{label}slice directories {sorted(on_disk)} != Info.plist {sorted(ids)}")
    if dsyms is None:
        return
    ds_ids = {p.name for p in dsyms.iterdir() if p.is_dir()} if dsyms.is_dir() else set()
    if ds_ids != ids:
        fail(f"{label}dSYMs {sorted(ds_ids)} != slices {sorted(ids)}")
    for lib in libs:
        lid, lp = lib["LibraryIdentifier"], lib["LibraryPath"]
        if (dsyms / lid / f"{lp}.dSYM").is_dir():
            check_dsym(dsyms / lid / f"{lp}.dSYM", framework_binary(xcfw / lid / lp), f"{label}{lid}")


def mode_ios_libs(root: Path):
    dynamic = sorted(root.glob("**/dynamic_lib/lib/*/BqLog.xcframework"))
    if not dynamic:
        fail(f"no dynamic_lib/lib/<config>/BqLog.xcframework under {root}")
    for xcfw in dynamic:
        cfg = xcfw.parent.name
        has_dsyms = cfg in ("Debug", "RelWithDebInfo")
        check_xcframework(xcfw, xcfw.parent / "dSYMs" if has_dsyms else None, f"{cfg}/")
    for xcfw in sorted(root.glob("**/static_lib/lib/*/BqLog.xcframework")):
        check_xcframework(xcfw, None, f"static/{xcfw.parent.name}/")


def mode_mac_libs(root: Path):
    found = False
    for cfg_dir in sorted(root.glob("**/dynamic_lib/lib/*")):
        if cfg_dir.name not in ("Debug", "RelWithDebInfo"):
            continue
        for dylib in sorted(cfg_dir.glob("libBqLog.*.*.*.dylib")):
            found = True
            check_dsym(cfg_dir / f"{dylib.name}.dSYM", dylib, f"{cfg_dir.name}/{dylib.name}")
        fw = cfg_dir / "BqLog.framework"
        if fw.is_dir():
            check_dsym(cfg_dir / "BqLog.framework.dSYM", framework_binary(fw), f"{cfg_dir.name}/BqLog.framework")
    if not found:
        fail(f"no dynamic_lib/lib/<config>/libBqLog.<version>.dylib under {root}")


def meta_sections(text: str) -> dict:
    out = {}
    for m in re.finditer(r"- first:\n\s+(.*?)\n\s+second:\n(.*?)(?=\n  - first:|\n  userData:)", text, re.S):
        out[m.group(1).strip()] = m.group(2)
    return out


def mode_engine_pkg(root: Path):
    plugins = root / "Runtime/Plugins"
    ios = plugins / "iOS"
    xcfw = ios / "BqLog.xcframework"
    check_xcframework(xcfw, None, "iOS/")

    meta = (ios / "BqLog.xcframework.meta").read_text()
    if not re.search(r"^guid: [0-9a-f]{32}$", meta, re.M):
        fail("iOS meta: the guid must be 32 hex digits, Unity ignores any other form")
    sections = meta_sections(meta)
    iphone = sections.get("iPhone: iOS", "")
    if "enabled: 1" not in iphone or "AddToEmbeddedBinaries: true" not in iphone:
        fail("iOS meta: iPhone must be enabled with AddToEmbeddedBinaries: true")
    for name, body in sections.items():
        if name.startswith(("Standalone:", "Editor:")) and "enabled: 1" in body:
            fail(f"iOS meta: '{name}' must be disabled")

    device = {lib["LibraryIdentifier"]: lib for lib in xcframework_libs(xcfw)}.get("ios-arm64")
    if device is None:
        fail("iOS xcframework has no ios-arm64 slice")
    else:
        check_dsym(ios / "Symbols~/ios-arm64/BqLog.framework.dSYM",
                   framework_binary(xcfw / "ios-arm64" / device["LibraryPath"]), "iOS Symbols~/ios-arm64")
    mac = plugins / "macOS/libBqLog.dylib"
    if mac.is_file():
        check_dsym(plugins / "macOS/Symbols~/libBqLog.dylib.dSYM", mac, "macOS Symbols~")


MODES = {"ios-libs": mode_ios_libs, "mac-libs": mode_mac_libs, "engine-pkg": mode_engine_pkg}

if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in MODES:
        print(__doc__)
        sys.exit(2)
    MODES[sys.argv[1]](Path(sys.argv[2]))
    if errors:
        print(f"check_apple_symbols: {len(errors)} problem(s)")
        sys.exit(1)
    print("check_apple_symbols: ok")
