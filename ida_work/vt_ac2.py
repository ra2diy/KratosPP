# -*- coding: utf-8 -*-
import idaapi, idc, ida_bytes, ida_funcs, idautils

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\vt_ac2.txt"
f = open(OUT, "w", encoding="utf-8")


def w(s=""):
    f.write(str(s) + "\n")
    f.flush()


def dis(ea, n, title):
    w("### %s 0x%08X" % (title, ea))
    cur = ea
    end = idc.get_func_attr(ea, idc.FUNCATTR_END) or (ea + 0x400)
    for _ in range(n):
        if cur >= end:
            break
        w("   0x%08X  %s" % (cur, idc.generate_disasm_line(cur, 0)))
        nx = idc.next_head(cur, end)
        if nx == idaapi.BADADDR or nx <= cur:
            break
        cur = nx
    w()


dis(0x417CA0, 40, "AircraftClass slot-222 override (sub_417CA0)")
w("[xrefs from 0x417CA0 block]")
for x in idautils.XrefsFrom(0x417CA0, 3):
    w("   -> 0x%08X  %s" % (x.to, idc.get_name(x.to)))
w("[done]")
f.close()
import ida_pro
ida_pro.qexit(0)
