# -*- coding: utf-8 -*-
import os, idc, ida_pro, idautils, ida_struct, idaapi
HERE = os.path.dirname(os.path.abspath(__file__))
f = open(os.path.join(HERE, "p19_callers.txt"), "w", encoding="utf-8")
def w(s):
    f.write(s + "\n"); f.flush()
for a, nm in ((0x4DB810, "FootClass::SetCoords (虚表+0x1B4)"),
              (0x4D3810, "sub_4D3810 (带 CurrentLocation 比较)"),
              (0x4D85D0, "FootClass_UpdatePosition (虚表+0x18C)")):
    w("==== %s @ 0x%08X ====" % (nm, a))
    for x in idautils.XrefsTo(a, 0):
        w("   from %08X type=%d in %s | %s" % (x.frm, x.type, idc.get_func_name(x.frm) or "?", idc.GetDisasm(x.frm)))
    w("")
# FootClass 结构体里 0x550..0x570 成员
for sname in ("FootClass", "_FootClass"):
    sid = ida_struct.get_struc_id(sname)
    if sid == idaapi.BADADDR: continue
    s = ida_struct.get_struc(sid)
    w("== %s 0x54C..0x5C0 ==" % sname)
    for i in range(ida_struct.get_struc_size(s)):
        m = ida_struct.get_member(s, i)
        if m is None: continue
        off = m.get_soff()
        if 0x54C <= off <= 0x5C0:
            w("   +0x%03X  %s" % (off, ida_struct.get_member_name(m.id) or "?"))
    w("")
f.close(); ida_pro.qexit(0)
