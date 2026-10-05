# -*- coding: utf-8 -*-
import os
import idc
import ida_pro
import idautils
import ida_bytes

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p11_loco_probe.txt")
f = open(OUT, "w", encoding="utf-8")


def w(s):
    f.write(s + "\n")
    f.flush()


def fn_end(ea):
    e = idc.get_func_attr(ea, idc.FUNCATTR_END)
    return e if e else ea + 200


def disasm(ea, end=None, cap=500, indent="  "):
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


# ---------------------------------------------------------- 1) Drive_Matrix 全量
w("############ 1) 0x4AFF60 DriveLocomotionClass_Draw_Matrix ############")
w("   end = 0x%08X size=%d" % (fn_end(0x4AFF60), fn_end(0x4AFF60) - 0x4AFF60))
disasm(0x4AFF60, fn_end(0x4AFF60), cap=700)
w("   -- 调用 --")
for (x, m, t, d) in calls_of(0x4AFF60, fn_end(0x4AFF60)):
    w("      %08X %s -> %08X  %s" % (x, m, t, idc.get_func_name(t) or ""))
w("")

# ---------------------------------------------------------- 2) Frame 全局
w("############ 2) 指令 0x4AFF90 的操作数（Frame 全局）############")
for op in range(0, 3):
    w("   op%d type=%d value=0x%08X  | %s" % (op, idc.get_operand_type(0x4AFF90, op),
                                              idc.get_operand_value(0x4AFF90, op),
                                              idc.GetDisasm(0x4AFF90)))
fa = idc.get_operand_value(0x4AFF90, 1)
w("   Frame 疑似地址 = 0x%08X  名称=%s" % (fa, idc.get_name(fa) or "<none>"))
w("   -- XrefsTo 0x%08X --" % fa)
for x in idautils.XrefsTo(fa, 0):
    w("      from %08X type=%d in %s | %s" % (x.frm, x.type, idc.get_func_name(x.frm) or "?", idc.GetDisasm(x.frm)))
w("")

# ---------------------------------------------------------- 3) Drive loco vtable
w("############ 3) DriveLocomotionClass vtable 0x7E7EB0 ############")
base = 0x7E7EB0
for i in range(0, 40):
    v = ida_bytes.get_dword(base + i * 4)
    nm = idc.get_func_name(v) or ""
    w("   +0x%03X  [%2d] = 0x%08X  %s" % (i * 4, i, v, nm))
w("")

# ---------------------------------------------------------- 4) ILocomotion vtable 相关名字
w("############ 4) 名字库：Locomotion / Facing / Track ############")
for ea, nm in sorted(idautils.Names()):
    if any(k in nm for k in ("LocomotionClass", "LocomotionFacing", "DriveLocomotion",
                             "WalkLocomotion", "ShipLocomotion", "JumpjetLocomotion",
                             "Set_Primary_Facing", "FacingClass", "WalkedFrames")):
        w("   %08X  %s" % (ea, nm))
w("")

# ---------------------------------------------------------- 5) 谁写 Frame
w("############ 5) 关键：FacingClass::Update / FootClass_Set_Coords 引用 ############")
for nm in ("W?Update$:FacingClass$n(x)v", "W?Set_Current$:FacingClass$n(x)v"):
    a = idc.get_name_ea_simple(nm)
    w("   %s -> 0x%08X" % (nm, a))
w("")

f.close()
ida_pro.qexit(0)
