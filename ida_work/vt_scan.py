# -*- coding: utf-8 -*-
# Enumerate every AbstractClass-derived vtable (by the shared QueryInterface/AddRef/Release
# header triple) and report the values at +0x1C4 and +0x378 -> subclass override check.
import idaapi, idc, idautils, ida_bytes, ida_name, ida_segment, ida_funcs

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\vt_scan.txt"
f = open(OUT, "w", encoding="utf-8")


def w(s=""):
    f.write(str(s) + "\n")
    f.flush()


UNIT_VT = 0x7F5C70            # UnitClass vtable (verified: +0x1C4 -> 0x5F6A10)
A = 0x5F6A10                  # GetCell1 / YRpp ObjectClass::GetCellAgain  (slot 0x1C4)
B = 0x6FFBE0                  # Player_Assign_Mission / YRpp TechnoClass::ClickedMission (slot 0x378)

hdr = [ida_bytes.get_dword(UNIT_VT + 4 * i) for i in range(3)]
w("[UNIT vtable 0x%08X header]" % UNIT_VT)
for i, v in enumerate(hdr):
    w("   +0x%02X = 0x%08X %s" % (4 * i, v, idc.get_name(v)))
w()

rdata = None
for seg in idautils.Segments():
    nm = idc.get_segm_name(seg)
    if nm == ".rdata":
        rdata = (idc.get_segm_start(seg), idc.get_segm_end(seg))
w("[.rdata] %s" % (("%08X-%08X" % rdata) if rdata else "NOT FOUND"))
if not rdata:
    f.close()
    import ida_pro
    ida_pro.qexit(0)


def fname(ea):
    return idc.get_name(ea) or ""


bases = []
lo, hi = rdata
for x in range(lo, hi - 12, 4):
    if (ida_bytes.get_dword(x) == hdr[0] and ida_bytes.get_dword(x + 4) == hdr[1]
            and ida_bytes.get_dword(x + 8) == hdr[2]):
        bases.append(x)
w("[AbstractClass-derived vtable bases found] %d" % len(bases))
w()

g1 = {}
g2 = {}
for b in bases:
    what = ida_bytes.get_dword(b + 0x2C)      # slot 11: AbstractClass::WhatAmI
    d1 = ida_bytes.get_dword(b + 0x1C4)
    d2 = ida_bytes.get_dword(b + 0x378)
    line = ("  vt=0x%08X whatami=%-46s +1C4=0x%08X %-28s +378=0x%08X %s"
            % (b, fname(what)[:46], d1, fname(d1)[:28], d2, fname(d2)[:34]))
    w(line)
    g1.setdefault(d1, []).append((b, what))
    g2.setdefault(d2, []).append((b, what))
w()

for key, groups, tgt, lab in (("+0x1C4", g1, A, "ObjectClass::GetCellAgain"), ("+0x378", g2, B, "TechnoClass::ClickedMission")):
    w("=" * 100)
    w("### slot %s -> YRpp %s ; expected engine target 0x%08X" % (key, lab, tgt))
    for val, members in sorted(groups.items(), key=lambda kv: -len(kv[1])):
        mark = "EXPECTED" if val == tgt else "DIFFERENT (subclass override!)"
        w("  0x%08X  x%-3d  %s" % (val, len(members), mark))
        w("        %s" % fname(val))
        if val != tgt:
            for b, what in members:
                w("          from vt=0x%08X whatami=%s" % (b, fname(what)))
    w()

w("[detail around the two slots for a few representative vtables]")
for b in bases:
    w("  vt 0x%08X:" % b)
    for off in (0x1B8, 0x1BC, 0x1C0, 0x1C4, 0x1C8, 0x36C, 0x370, 0x374, 0x378, 0x37C, 0x380):
        v = ida_bytes.get_dword(b + off)
        w("      +0x%03X = 0x%08X %s" % (off, v, fname(v)))
w("[done]")
f.close()
import ida_pro
ida_pro.qexit(0)
