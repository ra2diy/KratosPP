# -*- coding: utf-8 -*-
import os, idc, ida_pro, idautils, ida_struct, idaapi, ida_bytes
HERE = os.path.dirname(os.path.abspath(__file__))
f = open(os.path.join(HERE, "p20_slots.txt"), "w", encoding="utf-8")
def w(s):
    f.write(s + "\n"); f.flush()
sid = ida_struct.get_struc_id("vt_FootClass")
if sid != idaapi.BADADDR:
    s = ida_struct.get_struc(sid)
    w("== vt_FootClass 槽位名 (0x180..0x320) ==")
    for i in range(ida_struct.get_struc_size(s)):
        m = ida_struct.get_member(s, i)
        if m is None: continue
        off = m.get_soff()
        if 0x180 <= off <= 0x320:
            w("   +0x%03X  %s" % (off, ida_struct.get_member_name(m.id) or "?"))
w("")
w("== 全 .text: call dword ptr [reg+1B4h] ==")
text = None
for seg in idautils.Segments():
    if idc.get_segm_name(seg) == ".text":
        text = (idc.get_segm_start(seg), idc.get_segm_end(seg)); break
for pat, label in (("+1B4h]", "SetCoords +0x1B4"), ("+2CCh]", "+0x2CC")):
    w("-- %s --" % label)
    n = 0
    for ea in idautils.Heads(text[0], text[1]):
        d = idc.GetDisasm(ea)
        if pat in d and idc.print_insn_mnem(ea) == "call":
            w("   %08X  %-44s in %s" % (ea, d, idc.get_func_name(ea) or "?"))
            n += 1
    w("   (%d 处)" % n)
    w("")
f.close(); ida_pro.qexit(0)
