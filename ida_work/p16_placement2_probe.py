# -*- coding: utf-8 -*-
import os
import idc
import ida_pro

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p16_placement2_probe.txt")
f = open(OUT, "w", encoding="utf-8")


def w(s):
    f.write(s + "\n")
    f.flush()


def fn_end(ea):
    e = idc.get_func_attr(ea, idc.FUNCATTR_END)
    return e if e else ea + 200


def disasm(ea, end, cap=400, indent="  "):
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


def calls_of(ea, end):
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


TAG = {
    0x4A9770: "DisplayClass::Remove",
    0x4A9720: "DisplayClass::Submit",
    0x5683C0: "MapClass::Place_Down",
    0x5687F0: "MapClass::Pick_Up",
    0x5F5850: "ObjectClass::Mark",
    0x4D3780: "FootClass_SetLayer",
    0x4DB810: "FootClass_SetCoords",
}

w("############ A) FootClass_UpdatePosition 的 a2!=2 分支 (0x4D8DFD..0x4D8F90) ############")
disasm(0x4D8DFD, 0x4D8F90)
w("   -- 调用 --")
for (x, m, t, d) in calls_of(0x4D8DFD, 0x4D8F90):
    w("      %08X %s -> %08X  %s%s" % (x, m, t, idc.get_func_name(t) or "", "  <<<< " + TAG[t] if t in TAG else ""))
w("")

w("############ B) ObjectClass_UpdatePosition 0x4264B0 (基类默认) ############")
e = fn_end(0x4264B0)
w("   end=0x%08X size=%d" % (e, e - 0x4264B0))
disasm(0x4264B0, e)
w("   -- 调用 --")
for (x, m, t, d) in calls_of(0x4264B0, e):
    w("      %08X %s -> %08X  %s%s" % (x, m, t, idc.get_func_name(t) or "", "  <<<< " + TAG[t] if t in TAG else ""))
w("")

w("############ C) 谁调用 UpdatePosition(+0x18C) —— 全 .text 扫描 call [reg+18Ch] ############")
import idautils
text = None
for seg in idautils.Segments():
    if idc.get_segm_name(seg) == ".text":
        text = (idc.get_segm_start(seg), idc.get_segm_end(seg))
        break
n = 0
for ea in idautils.Heads(text[0], text[1]):
    d = idc.GetDisasm(ea)
    if "18Ch]" in d and idc.print_insn_mnem(ea) in ("call",):
        w("   %08X  %-46s in %s" % (ea, d, idc.get_func_name(ea) or "?"))
        n += 1
w("   共 %d 处" % n)
w("")

w("############ D) 谁调用 FootClass_UpdatePosition / ObjectClass_UpdatePosition ############")
for a in (0x4D85D0, 0x4264B0):
    w("   -- 0x%08X --" % a)
    for x in idautils.XrefsTo(a, 0):
        w("      from %08X type=%d in %s" % (x.frm, x.type, idc.get_func_name(x.frm) or "?"))
w("")

f.close()
ida_pro.qexit(0)
