# -*- coding: utf-8 -*-
import os
import idc
import ida_pro
import idautils
import ida_struct
import idaapi

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p14_facing388b_probe.txt")
f = open(OUT, "w", encoding="utf-8")


def w(s):
    f.write(s + "\n")
    f.flush()


# ------------------------------------------------ 1) IDB 里所有结构体的名字
w("############ 1) 结构体名字（含 Techno/Foot/Object/Facing） ############")
for sidx in range(0, ida_struct.get_struc_qty()):
    sid = ida_struct.get_struc_by_idx(sidx)
    nm = ida_struct.get_struc_name(sid) or "?"
    if any(k in nm for k in ("Techno", "Foot", "Object", "Facing", "Abstract")):
        sz = ida_struct.get_struc_size(ida_struct.get_struc(sid))
        w("   %-40s size=0x%X" % (nm, sz))
w("")


def dump_named(name):
    sid = ida_struct.get_struc_id(name)
    if sid == idaapi.BADADDR:
        w("   <%s 不存在>" % name)
        return
    s = ida_struct.get_struc(sid)
    w("   [%s] size=0x%X" % (name, ida_struct.get_struc_size(s)))
    for i in range(0, ida_struct.get_struc_size(s)):
        m = ida_struct.get_member(s, i)
        if m is None:
            continue
        off = m.get_soff()
        if 0x320 <= off <= 0x3B0:
            w("      +0x%03X  %s" % (off, ida_struct.get_member_name(m.id) or "?"))


w("############ 2) TechnoClass / FootClass 结构体 0x320..0x3B0 ############")
for nm in ("TechnoClass", "_TechnoClass", "FootClass", "_FootClass",
           "TechnoClassClass", "ObjectClass", "_ObjectClass"):
    dump_named(nm)
w("")

# ------------------------------------------------ 3) 全 .text 扫描 388h（所有操作数）
w("############ 3) 全 .text：任何操作数里含 388h 的指令（TechnoClass 相关函数） ############")
text = None
for seg in idautils.Segments():
    if idc.get_segm_name(seg) == ".text":
        text = (idc.get_segm_start(seg), idc.get_segm_end(seg))
        break
hits = {}
for ea in idautils.Heads(text[0], text[1]):
    d = idc.GetDisasm(ea)
    if "388h" not in d:
        continue
    fn = idc.get_func_name(ea) or "?"
    hits.setdefault(fn, []).append((ea, d))

for fn in sorted(hits):
    w("   == %s ==" % fn)
    for ea, d in hits[fn][:12]:
        w("      %08X  %s" % (ea, d))
w("   共 %d 个函数涉及 388h" % len(hits))
w("")

f.close()
ida_pro.qexit(0)
