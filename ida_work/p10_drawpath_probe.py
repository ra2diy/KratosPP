# -*- coding: utf-8 -*-
import os
import idc
import ida_pro
import idautils
import ida_bytes

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p10_drawpath_probe.txt")
f = open(OUT, "w", encoding="utf-8")


def w(s):
    f.write(s + "\n")
    f.flush()


def fn_end(ea):
    e = idc.get_func_attr(ea, idc.FUNCATTR_END)
    return e if e else ea + 200


def disasm(ea, end=None, cap=400, indent="  "):
    if end is None:
        end = fn_end(ea)
    n = 0
    while ea < end and n < cap:
        dis = idc.GetDisasm(ea)
        if not dis:
            break
        w(indent + "%08X  %s" % (ea, dis))
        sz = idc.get_item_size(ea)
        if sz <= 0:
            break
        ea += sz
        n += 1


def calls_of(ea, end=None):
    if end is None:
        end = fn_end(ea)
    out = []
    cur = ea
    while cur < end:
        m = idc.print_insn_mnem(cur)
        if m in ("call", "jmp"):
            t = idc.get_operand_value(cur, 0)
            if t and t != 0xFFFFFFFF:
                out.append((cur, m, t, idc.GetDisasm(cur)))
        sz = idc.get_item_size(cur)
        if sz <= 0:
            break
        cur += sz
    return out


# ---------------------------------------------------------- 1) 名字库：Draw/Render/Sort
w("############ 1) 名字库检索 (Draw / Render / Tactical / Sort / Frame) ############")
pats = ("Draw", "Render", "Tactical", "Sort", "LayerClass")
names = []
for ea, nm in idautils.Names():
    for p in pats:
        if p in nm:
            names.append((ea, nm))
            break
for ea, nm in sorted(names):
    w("   %08X  %s" % (ea, nm))
w("")

# ---------------------------------------------------------- 2) Tactical_Draw_All
w("############ 2) 0x6D8DB0 Tactical_Draw_All ############")
w("   end = 0x%08X size=%d" % (fn_end(0x6D8DB0), fn_end(0x6D8DB0) - 0x6D8DB0))
disasm(0x6D8DB0, min(fn_end(0x6D8DB0), 0x6D8DB0 + 700), cap=200)
w("   -- 调用 --")
for (x, m, t, d) in calls_of(0x6D8DB0):
    w("      %08X %s -> %08X  %s" % (x, m, t, idc.get_func_name(t) or ""))
w("")

# ---------------------------------------------------------- 3) GScreenClass_DrawOnTop
w("############ 3) 0x4F4480 GScreenClass_DrawOnTop ############")
w("   end = 0x%08X size=%d" % (fn_end(0x4F4480), fn_end(0x4F4480) - 0x4F4480))
disasm(0x4F4480, min(fn_end(0x4F4480), 0x4F4480 + 600), cap=200)
w("   -- 调用 --")
for (x, m, t, d) in calls_of(0x4F4480):
    w("      %08X %s -> %08X  %s" % (x, m, t, idc.get_func_name(t) or ""))
w("")

# ---------------------------------------------------------- 4) Main_Loop 渲染段
w("############ 4) W?Main_Loop 0x55D360 —— 0x55DB00..0x55DC40 ############")
ea = 0x55DB00
while ea < 0x55DC40:
    dis = idc.GetDisasm(ea)
    if not dis:
        break
    w("  %08X  %s" % (ea, dis))
    sz = idc.get_item_size(ea)
    if sz <= 0:
        break
    ea += sz
w("")

# ---------------------------------------------------------- 5) Frame 全局
w("############ 5) Frame 全局计数器 ############")
for nm in ("Frame", "frame", "CurFrame", "CurrentFrame", "g_Frame"):
    a = idc.get_name_ea_simple(nm)
    if a != idc.BADADDR:
        w("   %s = 0x%08X" % (nm, a))
        for x in idautils.XrefsTo(a, 0):
            w("      xref from %08X type=%d in %s | %s" % (x.frm, x.type, idc.get_func_name(x.frm) or "?", idc.GetDisasm(x.frm)))
w("")
w("   -- 名字中含 Frame 的符号 --")
for ea, nm in idautils.Names():
    if "Frame" in nm or "frame" in nm:
        w("      %08X  %s" % (ea, nm))
w("")

# ---------------------------------------------------------- 6) LayerClass::Sort
w("############ 6) 0x551A30 LayerClass::Sort ############")
w("   -- 调用 --")
for (x, m, t, d) in calls_of(0x551A30):
    w("      %08X %s -> %08X  %s" % (x, m, t, idc.get_func_name(t) or ""))
w("")

f.close()
ida_pro.qexit(0)
