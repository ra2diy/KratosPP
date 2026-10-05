# -*- coding: utf-8 -*-
import os
import idc
import ida_pro
import idautils

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p6_placedown_probe.txt")
f = open(OUT, "w", encoding="utf-8")


def w(s):
    f.write(s + "\n")
    f.flush()


def disasm(ea, nbytes=400, indent="  "):
    end = ea + nbytes
    n = 0
    while ea < end and n < 200:
        dis = idc.GetDisasm(ea)
        if not dis:
            break
        w(indent + "%08X  %s" % (ea, dis))
        sz = idc.get_item_size(ea)
        if sz <= 0:
            break
        ea += sz
        n += 1


def calls_of(ea, nbytes=500):
    out = []
    end = ea + nbytes
    while ea < end:
        m = idc.print_insn_mnem(ea)
        if m in ("call", "jmp"):
            t = idc.get_operand_value(ea, 0)
            if t and t != 0xFFFFFFFF:
                out.append((ea, m, t, idc.GetDisasm(ea)))
        sz = idc.get_item_size(ea)
        if sz <= 0:
            break
        ea += sz
    return out


fns = [
    ("MapClass::Place_Down", 0x5683C0),
    ("MapClass::Pick_Up", 0x5687F0),
    ("MapClass_AddObjectToALayer", 0x551A90),
    ("MapClass_AddObjectToALayerX", 0x5519B0),
]

for nm, a in fns:
    w("############ %s @ %08X ############" % (nm, a))
    disasm(a, 420)
    w("   -- 调用目标 --")
    for (x, m, t, d) in calls_of(a, 420):
        w("      %08X %s -> %08X  %s" % (x, m, t, idc.get_func_name(t) or ""))
    w("")

w("############ 谁引用 vec_ObjectsInLayers 0x8A0360 ############")
for x in idautils.XrefsTo(0x8A0360, 0):
    w("   from %08X type=%d in %s" % (x.frm, x.type, idc.get_func_name(x.frm)))

w("")
w("############ AddObjectToALayer 的引用者 ############")
for a in (0x551A90, 0x5519B0):
    w("-- %08X --" % a)
    for x in idautils.XrefsTo(a, 0):
        w("   from %08X type=%d in %s" % (x.frm, x.type, idc.get_func_name(x.frm)))

f.close()
ida_pro.qexit(0)
