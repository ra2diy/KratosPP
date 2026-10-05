# -*- coding: utf-8 -*-
import os
import ida_bytes
import idc
import ida_pro
import idautils

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p4_jitter_probe2.txt")
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
    return idc.get_func_name(ea) or (idc.get_name(ea, idc.NAME_NO_STRING) or "(noname)")


# FootClass 虚表：base 0x7E8C94（FootClass_InWhichLayer 在 +0x78）
VT = 0x7E8C94
w("############ vt+0x120 / +0x124 / +0x128 of FootClass vtable 0x%08X ############" % VT)
for off in (0x118, 0x11C, 0x120, 0x124, 0x128, 0x12C, 0x130, 0x1B4, 0x1C8, 0x1CC):
    raw = ida_bytes.get_bytes(VT + off, 4)
    val = int.from_bytes(raw, "little")
    w("   +%03X  %08X  %s" % (off, val, nm(val)))
w("")
w("---- impl of +0x124 ----")
raw = ida_bytes.get_bytes(VT + 0x124, 4)
val = int.from_bytes(raw, "little")
disasm(val, 60)
w("")

w("############ MapClass_GetCellFloorHeight 0x578080 ############")
disasm(0x578080, 70)
w("")

w("############ sub_7104F0 (FootClass_SetCoords 命中分支) ############")
disasm(0x7104F0, 70)
w("")

w("############ TechnoClass_GetCoords? 检查 FootClass(+0x48) 同上 ############")
for vt, label in ((0x7E22A4, "AircraftClass"), (0x7E8C94, "FootClass"), (0x7F5C70, "FootClassB")):
    raw = ida_bytes.get_bytes(vt + 0x48, 4)
    val = int.from_bytes(raw, "little")
    w("   %s +0x48 = %08X  %s" % (label, val, nm(val)))

f.close()
ida_pro.qexit(0)
