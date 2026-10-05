# -*- coding: utf-8 -*-
import struct, sys
sys.path.insert(0, r"D:\Workspace\ra2mod\platform\KratosPP\ida_work")
from xdis import read, flt, dbl, NAMES


def dump_floats(addrs):
    for a in addrs:
        f4 = flt(a)
        d8 = dbl(a)
        print("  0x%06X  float=%-16s double=%s" % (a, f4, d8))


def dump_vtable(base, n, label=""):
    print("### vtable 0x%06X %s" % (base, label))
    raw = read(base, 4 * n)
    for i in range(n):
        v, = struct.unpack_from("<I", raw, 4 * i)
        note = ""
        if v in NAMES:
            note = "  ; " + NAMES[v]
        print("   +0x%03X [%2d] = 0x%08X%s" % (4 * i, i, v, note))
    print()


if __name__ == "__main__":
    mode = sys.argv[1]
    if mode == "f":
        dump_floats([int(x, 16) for x in sys.argv[2:]])
    elif mode == "v":
        dump_vtable(int(sys.argv[2], 16), int(sys.argv[3]), sys.argv[4] if len(sys.argv) > 4 else "")
