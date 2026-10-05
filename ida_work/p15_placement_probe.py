# -*- coding: utf-8 -*-
import os
import idc
import ida_pro
import idautils
import ida_struct
import ida_bytes
import idaapi

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p15_placement_probe.txt")
f = open(OUT, "w", encoding="utf-8")


def w(s):
    f.write(s + "\n")
    f.flush()


def fn_end(ea):
    e = idc.get_func_attr(ea, idc.FUNCATTR_END)
    return e if e else ea + 200


def disasm(ea, end=None, cap=300, indent="  "):
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


# ---------------------------------------------- 1) vt_ObjectClass / vt_FootClass 成员
w("############ 1) vt_ObjectClass / vt_FootClass 结构体成员（含 UpdatePlacement） ############")
for sname in ("vt_ObjectClass", "vt_FootClass"):
    sid = ida_struct.get_struc_id(sname)
    if sid == idaapi.BADADDR:
        w("   <%s 不存在>" % sname)
        continue
    s = ida_struct.get_struc(sid)
    w("   [%s] size=0x%X" % (sname, ida_struct.get_struc_size(s)))
    for i in range(0, ida_struct.get_struc_size(s)):
        m = ida_struct.get_member(s, i)
        if m is None:
            continue
        nm = ida_struct.get_member_name(m.id) or ""
        if any(k in nm for k in ("lacement", "SetLocation", "SetPosition", "Limbo", "Unlimbo",
                                 "InWhichLayer", "Mark", "GetCell", "Update")):
            w("      +0x%03X  %s" % (m.get_soff(), nm))
    w("")

# ---------------------------------------------- 2) 定位 FootClass 虚表的 UpdatePlacement 槽
w("############ 2) FootClass 虚表槽：找 UpdatePlacement ############")
foot_vts = [0x7E22A4, 0x7E8C94, 0x7EB058, 0x7F5C70]
for base in foot_vts[:1]:
    w("   -- vtable 0x%08X --" % base)
    for i in range(0, 0x60):
        off = i * 4
        if 0x180 <= off <= 0x1C0:
            v = ida_bytes.get_dword(base + off)
            w("      +0x%03X [%3d] = 0x%08X  %s" % (off, i, v, idc.get_func_name(v) or ""))
w("")

# ---------------------------------------------- 3) 反汇编候选槽
w("############ 3) 反汇编 +0x184 / +0x188 / +0x18C / +0x190 指向的函数 ############")
base = foot_vts[0]
for off in (0x184, 0x188, 0x18C, 0x190):
    v = ida_bytes.get_dword(base + off)
    w("   ==== slot +0x%03X = 0x%08X  %s ====" % (off, v, idc.get_func_name(v) or ""))
    disasm(v, min(fn_end(v), v + 300))
    w("      -- 调用/跳转 --")
    for (x, m, t, d) in calls_of(v):
        nm = idc.get_func_name(t) or ""
        tag = ""
        if t == 0x4A9770: tag = "  <<<< DisplayClass::Remove"
        if t == 0x4A9720: tag = "  <<<< DisplayClass::Submit"
        if t == 0x5683C0: tag = "  <<<< MapClass::Place_Down"
        if t == 0x5687F0: tag = "  <<<< MapClass::Pick_Up"
        w("         %08X %s -> %08X  %s%s" % (x, m, t, nm, tag))
    w("")

# ---------------------------------------------- 4) 对照：ObjectClass 默认实现
w("############ 4) ObjectClass 默认槽 +0x18C ############")
obj_vt = 0x7E3354
for off in (0x188, 0x18C, 0x190):
    v = ida_bytes.get_dword(obj_vt + off)
    w("   slot +0x%03X = 0x%08X %s" % (off, v, idc.get_func_name(v) or ""))
    if v > 0x401000 and v < 0x900000:
        for (x, m, t, d) in calls_of(v):
            tag = ""
            if t == 0x4A9770: tag = "  <<<< DisplayClass::Remove"
            if t == 0x4A9720: tag = "  <<<< DisplayClass::Submit"
            w("         %08X %s -> %08X  %s%s" % (x, m, t, idc.get_func_name(t) or "", tag))
w("")

f.close()
ida_pro.qexit(0)
