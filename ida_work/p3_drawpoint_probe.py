# -*- coding: utf-8 -*-
# 替身抖动：Draw_Point（绘制中心点）与 loco 变换取证（只读 IDB 副本 gamemd_p1.idb）
import os
import traceback
import ida_bytes
import idc
import ida_pro
import idautils
import ida_funcs

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p3_drawpoint_probe.txt")
f = open(OUT, "w", encoding="utf-8")


def w(s):
    f.write(s + "\n")
    f.flush()


def disasm(ea, limit=70, indent="  "):
    n = 0
    while n < limit:
        dis = idc.GetDisasm(ea)
        if not dis:
            w(indent + "%08X  <cannot disasm>" % ea)
            break
        w(indent + "%08X  %s" % (ea, dis))
        m = idc.print_insn_mnem(ea)
        if m in ("retn", "ret", "retf"):
            break
        sz = idc.get_item_size(ea)
        if sz <= 0:
            break
        ea += sz
        n += 1


def nm(ea):
    if not ea:
        return "(0)"
    return idc.get_func_name(ea) or (idc.get_name(ea, idc.NAME_NO_STRING) or "(noname)")


segs = [(idc.get_segm_name(se), se, idc.get_segm_end(se)) for se in idautils.Segments()]
best = None
for n_, st, en in segs:
    if st <= 0x7E22A4 < en:
        best = (n_, st, en)
        break
start, end = best[1], best[2]
data = ida_bytes.get_bytes(start, end - start)
w("rdata %08X..%08X len=%d" % (start, end, len(data)))
w("")

# A. 名字库：Draw_Point / DrawPoint / LocomotionClass
w("############ A. 名字检索 ############")
keys = ("DrawPoint", "Draw_Point", "DrawMatrix", "ILocomotion", "LocomotionClass")
cnt = 0
for ea, name in idautils.Names():
    for k in keys:
        if k in name:
            w("   %08X  %s" % (ea, name))
            cnt += 1
            break
    if cnt > 400:
        w("   ...(截断)")
        break
w("")

# B. 找 loco 虚表：Draw_Matrix 在 ILocomotion vtable 的 +0x24
MTX = [("Drive", 0x4AFF60), ("Ship", 0x69F670), ("Default", 0x55A730),
       ("Hover", 0x54DCC0), ("Jumpjet", 0x4CF6A0)]
w("############ B. 含已知 Draw_Matrix 的虚表（+0x24）############")
found_vt = []
for label, fn in MTX:
    le = fn.to_bytes(4, "little")
    ppos = 0
    while True:
        i = data.find(le, ppos)
        if i < 0:
            break
        ppos = i + 1
        if i % 4 != 0:
            continue
        slot = start + i
        vt = slot - 0x24
        found_vt.append((label, fn, vt))
        w("---- %s Draw_Matrix=%08X  slot=%08X  vtable_base=%08X ----" % (label, fn, slot, vt))
        for off, tag in ((0x18, "Shadow_Matrix(-4?)"), (0x1C, "?"), (0x20, "?"),
                         (0x24, "Draw_Matrix"), (0x28, "Shadow_Matrix"),
                         (0x2C, "Draw_Point"), (0x30, "Shadow_Point"),
                         (0x34, "Visual_Character"), (0x38, "Z_Adjust"),
                         (0x3C, "Z_Gradient"), (0x40, "Process"),
                         (0x44, "Move_To"), (0x48, "Stop_Moving"),
                         (0x4C, "Do_Turn"), (0x50, "Unlimbo"),
                         (0x54, "Tilt_Pitch_AI"), (0x58, "Power_On"),
                         (0x6C, "In_Which_Layer"), (0x74, "Is_Moving_Now"),
                         (0x78, "Apparent_Speed"), (0x7C, "Drawing_Code")):
            raw = ida_bytes.get_bytes(vt + off, 4)
            if not raw or len(raw) < 4:
                continue
            val = int.from_bytes(raw, "little")
            w("   +%03X %-18s %08X  %s" % (off, tag, val, nm(val)))
        w("")

# C. 默认 Draw_Point
w("############ C. LocomotionClass_ILocomotion_DrawPoint 0x55ABD0 ############")
disasm(0x55ABD0, 80)
w("")

# D. 各 loco 的 Draw_Point 实现
w("############ D. 各 loco Draw_Point 实现 ############")
seen = set()
for label, fn, vt in found_vt:
    raw = ida_bytes.get_bytes(vt + 0x2C, 4)
    if not raw or len(raw) < 4:
        continue
    val = int.from_bytes(raw, "little")
    if val in seen:
        w("---- [%s] Draw_Point = %08X (已打印) ----" % (label, val))
        continue
    seen.add(val)
    w("---- [%s] Draw_Point impl @ %08X (%s) ----" % (label, val, nm(val)))
    disasm(val, 80)
    w("")

# E. call [reg+0x2C] 的分布
w("############ E. call [reg+0x2C] 分布 ############")
tot = []
for op in (0x90, 0x91, 0x92, 0x93, 0x95, 0x96, 0x97, 0x94):
    p = bytes([0xFF, op]) + (0x2C).to_bytes(4, "little")
    ppos = 0
    while True:
        j = data.find(p, ppos)
        if j < 0:
            break
        ppos = j + 1
        tot.append(start + j)
tot.sort()
w("共 %d 处" % len(tot))
for a in tot[:80]:
    w("   %08X  in %s" % (a, idc.get_func_name(a)))
w("")

# F. 搜索 0x55ABD0 / DrawPoint 的调用者
w("############ F. XrefsTo 0x55ABD0 ############")
for x in idautils.XrefsTo(0x55ABD0, 0):
    w("   from %08X type=%d in %s" % (x.frm, x.type, idc.get_func_name(x.frm)))

f.close()
ida_pro.qexit(0)
