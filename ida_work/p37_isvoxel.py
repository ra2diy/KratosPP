# -*- coding: utf-8 -*-
"""
p37: 通过已知的 TechnoClass 虚表槽 (+0x41C = 0x70B570) 反查 TechnoClass 虚表基址，
     再取 +0x298 (IsVoxel) 的实现地址，用于判定 CARRIER 是否走倾斜积分器。
"""
import sys, struct
sys.path.insert(0, r"D:\Workspace\ra2mod\platform\KratosPP\ida_work")
from xdis import DATA, IMGBASE, SECS, read, NAMES, va2off

TARGET = 0x70B570
SLOT = 0x41C
ISVOXEL_SLOT = 0x298


def find_u32(val):
    out = []
    pat = struct.pack("<I", val)
    for name, vaddr, vsize, raddr, rsize in SECS:
        seg = DATA[raddr:raddr + rsize]
        i = seg.find(pat)
        while i != -1:
            if i % 4 == 0:
                out.append(IMGBASE + vaddr + i)
            i = seg.find(pat, i + 1)
    return out


def main():
    hits = find_u32(TARGET)
    print("### occurrences of 0x%X (4-byte aligned): %d" % (TARGET, len(hits)))
    for h in hits:
        base = h - SLOT
        v = read(base + ISVOXEL_SLOT, 4)
        isvoxel = struct.unpack("<I", v)[0] if v else 0
        # sanity: first entries should be code pointers
        v0 = read(base, 4)
        v0 = struct.unpack("<I", v0)[0] if v0 else 0
        ok = 0x401000 <= v0 < 0x7E0000
        print("  slot@0x%06X  vtableBase=0x%06X  v0=0x%06X %s  +0x298=0x%06X %s"
              % (h, base, v0, "OK" if ok else "??", isvoxel, NAMES.get(isvoxel, "")))


if __name__ == "__main__":
    main()
