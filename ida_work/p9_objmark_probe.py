# -*- coding: utf-8 -*-
import os
import idc
import ida_pro
import idautils

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p9_objmark_probe.txt")
f = open(OUT, "w", encoding="utf-8")


def w(s):
    f.write(s + "\n")
    f.flush()


def fn_end(ea):
    e = idc.get_func_attr(ea, idc.FUNCATTR_END)
    return e if e else ea + 200


def disasm(ea, end=None, indent="  "):
    if end is None:
        end = fn_end(ea)
    n = 0
    while ea < end and n < 600:
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


def body_hits(ea, targets, end=None):
    if end is None:
        end = fn_end(ea)
    hits = []
    cur = ea
    while cur < end:
        for op in range(0, 3):
            if idc.get_operand_type(cur, op) in (idc.o_imm, idc.o_mem, idc.o_displ, idc.o_near, idc.o_far):
                v = idc.get_operand_value(cur, op)
                if v in targets:
                    hits.append((cur, v, idc.GetDisasm(cur)))
        if idc.print_insn_mnem(cur) in ("call", "jmp"):
            t = idc.get_operand_value(cur, 0)
            if t in targets:
                hits.append((cur, t, idc.GetDisasm(cur)))
        sz = idc.get_item_size(cur)
        if sz <= 0:
            break
        cur += sz
    return hits


OBJ_MARK = 0x5F5850
SUBMIT = 0x4A9720
REMOVE = 0x4A9770
LAYERS = 0x8A0360

w("############ A) ObjectClass::Mark @ 0x%08X ############" % OBJ_MARK)
w("   func end = 0x%08X (size=%d)" % (fn_end(OBJ_MARK), fn_end(OBJ_MARK) - OBJ_MARK))
disasm(OBJ_MARK)
w("")
w("   -- 调用/跳转目标 --")
mk_calls = calls_of(OBJ_MARK)
for (x, m, t, d) in mk_calls:
    nm = idc.get_func_name(t) or ""
    w("      %08X %s -> %08X  %s" % (x, m, t, nm))
w("")

w("############ B) Mark 是否触碰 层数组 / Submit / Remove ############")
tg = {LAYERS: "vec_ObjectsInLayers", SUBMIT: "DisplayClass::Submit", REMOVE: "DisplayClass::Remove"}
h = body_hits(OBJ_MARK, set(tg.keys()))
if h:
    for (x, v, d) in h:
        w("   命中 %08X -> %s   (%s)" % (x, tg[v], d))
else:
    w("   Mark 函数体内未出现 层数组/Submit/Remove")
w("")

# 一级调用树
w("   -- Mark 一级被调函数内是否出现 Submit/Remove/层数组 --")
for (x, m, t, d) in mk_calls:
    if m != "call":
        continue
    if t < 0x401000 or t > 0x900000:
        w("      %08X (virtual/thunk, 跳过)" % t)
        continue
    hh = body_hits(t, set(tg.keys()))
    nm = idc.get_func_name(t) or "?"
    if hh:
        for (xx, vv, dd) in hh:
            w("      -> %08X %-42s : 命中 %08X %s" % (t, nm, xx, tg[vv]))
    else:
        w("      -> %08X %-42s : 未命中" % (t, nm))
w("")

w("############ C) 谁调用 DisplayClass::Submit / Remove ############")
for a, nm in ((SUBMIT, "Submit"), (REMOVE, "Remove")):
    w("-- %s @ 0x%08X --" % (nm, a))
    for x in idautils.XrefsTo(a, 0):
        w("   from %08X type=%d in %s" % (x.frm, x.type, idc.get_func_name(x.frm) or "?"))
    w("")

w("############ D) 谁调用 ObjectClass::Mark 0x5F5850 ############")
for x in idautils.XrefsTo(OBJ_MARK, 0):
    w("   from %08X type=%d in %s" % (x.frm, x.type, idc.get_func_name(x.frm) or "?"))
w("")

w("############ E) FootClass_SetLayer 0x4D3780 的引用者(虚表 + 直接) ############")
w("-- XrefsTo 0x4D3780 --")
for x in idautils.XrefsTo(0x4D3780, 0):
    w("   from %08X type=%d in %s" % (x.frm, x.type, idc.get_func_name(x.frm) or "?"))
w("")

w("############ F) sub_7104F0 (SetCoords 末尾调用) ############")
w("   func end = 0x%08X" % fn_end(0x7104F0))
disasm(0x7104F0, min(fn_end(0x7104F0), 0x7104F0 + 260))
w("")
for (x, m, t, d) in calls_of(0x7104F0):
    w("      %08X %s -> %08X  %s" % (x, m, t, idc.get_func_name(t) or ""))
w("")

f.close()
ida_pro.qexit(0)
