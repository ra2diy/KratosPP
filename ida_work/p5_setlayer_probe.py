# -*- coding: utf-8 -*-
import os
import ida_bytes
import idc
import ida_pro
import idautils

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p5_setlayer_probe.txt")
f = open(OUT, "w", encoding="utf-8")


def w(s):
    f.write(s + "\n")
    f.flush()


def disasm(ea, nbytes=260, indent="  "):
    """线性反汇编 nbytes 字节，不在 retn 处停"""
    end = ea + nbytes
    while ea < end:
        dis = idc.GetDisasm(ea)
        if not dis:
            break
        w(indent + "%08X  %s" % (ea, dis))
        sz = idc.get_item_size(ea)
        if sz <= 0:
            break
        ea += sz


w("############ FootClass_SetLayer 0x4D3780 (完整) ############")
disasm(0x4D3780, 300)
w("")

w("############ 名字库：SetLayer / Submit / UpdatePlacement ############")
for ea, name in idautils.Names():
    if "SetLayer" in name or "UpdatePlacement" in name or "Submit" in name:
        w("   %08X  %s" % (ea, name))
w("")

w("############ XrefsTo 0x4D3780 (FootClass_SetLayer) ############")
for x in idautils.XrefsTo(0x4D3780, 0):
    w("   from %08X type=%d in %s" % (x.frm, x.type, idc.get_func_name(x.frm)))

w("")
w("############ DisplayClass::Submit 0x4A9720 (前 60 条) ############")
disasm(0x4A9720, 220)

f.close()
ida_pro.qexit(0)
