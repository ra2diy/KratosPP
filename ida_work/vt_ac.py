# -*- coding: utf-8 -*-
# Resolve what 0x7E22A4 actually is, and dump +0x1C4 / +0x378 for the FootClass-family vtables.
import idaapi, idc, idautils, ida_bytes

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\vt_ac.txt"
f = open(OUT, "w", encoding="utf-8")


def w(s=""):
    f.write(str(s) + "\n")
    f.flush()


def fn(ea):
    return idc.get_name(ea) or ""


def dump(base, title, offs):
    w("### %s  base=0x%08X" % (title, base))
    for off in offs:
        v = ida_bytes.get_dword(base + off)
        w("    +0x%03X = 0x%08X  %s" % (off, v, fn(v)))
    w()


CAND = [0x7E22A4, 0x7E2468, 0x7E22A4 - 0x1C4, 0x7E22A4 - 0x184, 0x7E2468 - 0x1C4,
        0x7E8C94, 0x7EB058, 0x7F4960, 0x7F5C70, 0x7E3EBC]
for b in CAND:
    dump(b, "candidate", [0x0, 0x4, 0x8, 0x2C, 0x144, 0x184, 0x1C4, 0x1C8, 0x374, 0x378])

w("[search .rdata for vtables whose +0x2C name contains Aircraft]")
for seg in idautils.Segments():
    if idc.get_segm_name(seg) != ".rdata":
        continue
    lo, hi = idc.get_segm_start(seg), idc.get_segm_end(seg)
    for b in range(lo, hi - 0x400, 4):
        if ida_bytes.get_dword(b) != ida_bytes.get_dword(0x7F5C70):
            continue
        nm = fn(ida_bytes.get_dword(b + 0x2C))
        if "Aircraft" in nm:
            w("   base=0x%08X whatami=%s  +1C4=0x%08X %s  +378=0x%08X %s"
              % (b, nm, ida_bytes.get_dword(b + 0x1C4), fn(ida_bytes.get_dword(b + 0x1C4)),
                 ida_bytes.get_dword(b + 0x378), fn(ida_bytes.get_dword(b + 0x378))))
w("[also: any .rdata dword == 0x6FFBE0]")
for seg in idautils.Segments():
    if idc.get_segm_name(seg) != ".rdata":
        continue
    lo, hi = idc.get_segm_start(seg), idc.get_segm_end(seg)
    for a in range(lo, hi - 4, 4):
        if ida_bytes.get_dword(a) == 0x6FFBE0:
            w("   0x%08X" % a)
w("[done]")
f.close()
import ida_pro
ida_pro.qexit(0)
