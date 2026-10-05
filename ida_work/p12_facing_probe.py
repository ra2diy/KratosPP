# -*- coding: utf-8 -*-
import os
import idc
import ida_pro

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p12_facing_probe.txt")
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


items = [
    ("LocomotionClass_ILocomotion_DrawMatrix (基类: facing->旋转矩阵)", 0x55A730),
    ("DriveLocomotionClass_ILocomotion_IsMoving", 0x4AFB80),
    ("DriveLocomotionClass_ILocomotion_IsMovingNow", 0x4AFC20),
    ("sub_4AFC90", 0x4AFC90),
    ("sub_4AFCC0", 0x4AFCC0),
    ("sub_4AFD40", 0x4AFD40),
    ("sub_4AFE00", 0x4AFE00),
    ("sub_4AFB40", 0x4AFB40),
]
for nm, a in items:
    w("############ %s @ 0x%08X  (size=%d) ############" % (nm, a, fn_end(a) - a))
    disasm(a, min(fn_end(a), a + 420))
    w("   -- 调用/跳转 --")
    for (x, m, t, d) in calls_of(a):
        w("      %08X %s -> %08X  %s" % (x, m, t, idc.get_func_name(t) or ""))
    w("")

w("############ Draw_Matrix 尾部 0x4B023B..0x4B0402 (Frame 过渡混合段) ############")
disasm(0x4B023B, 0x4B0402)
w("")

f.close()
ida_pro.qexit(0)
