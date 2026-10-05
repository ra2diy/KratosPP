# -*- coding: utf-8 -*-
# Enumerate vtables by their WhatAmI (slot +0x2C) implementation name -> class name,
# then report slots +0x1C4 and +0x378 => definitive subclass-override check.
import idaapi, idc, idautils, ida_bytes, ida_segment

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\vt_final.txt"
f = open(OUT, "w", encoding="utf-8")


def w(s=""):
    f.write(str(s) + "\n")
    f.flush()


QI = ida_bytes.get_dword(0x7F5C70)          # AbstractClass_QueryInterface
A = 0x5F6A10
B = 0x6FFBE0
rdata = None
for seg in idautils.Segments():
    if idc.get_segm_name(seg) == ".rdata":
        rdata = (idc.get_segm_start(seg), idc.get_segm_end(seg))
lo, hi = rdata
w("[.rdata] %08X-%08X  QI=0x%08X %s" % (lo, hi, QI, idc.get_name(QI)))


def fname(ea):
    return idc.get_name(ea) or ""


hits = []
for b in range(lo, hi - 0x400, 4):
    if ida_bytes.get_dword(b) != QI:
        continue
    nm = fname(ida_bytes.get_dword(b + 0x2C))
    if not nm or nm in ("purecall",):
        continue
    if ("WhatAmI" in nm or "What_Am_I" in nm or "GetAbstractDerivationID" in nm
            or nm.endswith("_2C") or nm.endswith("_2c")):
        hits.append((b, nm, ida_bytes.get_dword(b + 0x1C4), ida_bytes.get_dword(b + 0x378)))

w("[vtables with recognised WhatAmI implementation] %d" % len(hits))
w()
g1, g2 = {}, {}
for b, nm, d1, d2 in hits:
    w("  vt=0x%08X  class=%-46s  +1C4=0x%08X %-26s  +378=0x%08X %s"
      % (b, nm[:46], d1, fname(d1)[:26], d2, fname(d2)[:40]))
    g1.setdefault(d1, []).append(nm)
    g2.setdefault(d2, []).append(nm)
w()
for lab, g, tgt in (("+0x1C4 (slot 113)", g1, A), ("+0x378 (slot 222)", g2, B)):
    w("=" * 100)
    w("### %s : expected engine target 0x%08X" % (lab, tgt))
    for val, names in sorted(g.items(), key=lambda kv: -len(kv[1])):
        w("  0x%08X  x%-3d  %s" % (val, len(names), "SAME for all below"))
        for n in sorted(names):
            w("        %s" % n)
    w()
w("[done]")
f.close()
import ida_pro
ida_pro.qexit(0)
