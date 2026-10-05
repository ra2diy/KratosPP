# -*- coding: utf-8 -*-
# P1 处置前的引擎事实确认探针（只读 IDB 副本 gamemd_p1.idb）
#
# A) 21 条 WARN hook 的覆盖区间逐字节解码：区间内是否含相对跳转、目标是否落在区间内
# B) UnitClass::WhatAction(0x74041B) 的调用者（验证 CanLift 是否可达每帧模拟路径）
# C) TechnoClass_InAir(0x5F6B90) 反汇编（判断 IsInAir 的确定性）
# D) 0x4DA87A 落点字节 + 5 处 In_Which_Layer 落点字节
import os
import ida_bytes
import idc
import ida_pro
import idautils

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p1_probe.txt")

WARN = [
    (0x004184FC, 0x6), (0x00706342, 0x7), (0x00426598, 0x7), (0x00424298, 0x6),
    (0x00469EB4, 0x6), (0x004683E7, 0x9), (0x00466E18, 0x6), (0x006FD494, 0x7),
    (0x006B6D4D, 0x6), (0x006B78E4, 0x6), (0x00701C08, 0xA), (0x006F683C, 0x7),
    (0x00730DEB, 0x6), (0x00730E56, 0x6), (0x00730EEB, 0x6), (0x006FC016, 0x8),
    (0x00702299, 0xA), (0x0070256C, 0x6), (0x007024B0, 0x6), (0x0071C94C, 0xA),
    (0x00736B7E, 0xA),
]

f = open(OUT, "w", encoding="utf-8")


def w(s):
    f.write(s + "\n")


def decode_range(ea, size):
    lines = []
    branches = []
    cur = ea
    end = ea + size
    while cur < end:
        n = idc.get_item_size(cur)
        if n <= 0:
            lines.append("  %08X  <decode fail>" % cur)
            break
        mnem = idc.print_insn_mnem(cur)
        dis = idc.GetDisasm(cur)
        operands = dis.split(None, 1)[1] if " " in dis else ""
        text = "  %08X  %-8s %s" % (cur, mnem, operands)
        if mnem == "call" or mnem.startswith("j"):
            tgt = idc.get_operand_value(cur, 0)
            if tgt and tgt != 0xFFFFFFFF:
                inside = (ea <= tgt < end)
                branches.append((cur, mnem, tgt, inside))
                text += "   --> TGT %08X %s" % (tgt, "[区间内]" if inside else "[区间外]")
        lines.append(text)
        cur += n
    return lines, branches


try:
    w("################ A. 21 条 WARN hook 覆盖区间解码 ################")
    for ea, size in WARN:
        w("---- %08X size=0x%X  (%s) ----" % (ea, size, idc.get_func_name(ea)))
        lines, branches = decode_range(ea, size)
        for l in lines:
            w(l)
        if branches:
            for (b, m, t, ins) in branches:
                w("   !! 相对跳转 @%08X %s -> %08X  %s" % (b, m, t, "区间内(自跳)" if ins else "区间外"))
        else:
            w("   (区间内无相对跳转)")
        w("")

    w("################ B. UnitClass::WhatAction(0x74041B) 调用者 ################")
    fs = idc.get_func_attr(0x74041B, idc.FUNCATTR_START)
    fe = idc.get_func_attr(0x74041B, idc.FUNCATTR_END)
    w("containing func: %s  [%08X..%08X]" % (idc.get_func_name(fs), fs, fe))
    w("-- xrefs to containing func start --")
    for x in idautils.XrefsTo(fs, 0):
        w("   from %08X  type=%d  in %s" % (x.frm, x.type, idc.get_func_name(x.frm)))
    w("-- xrefs to 0x74041B --")
    for x in idautils.XrefsTo(0x74041B, 0):
        w("   from %08X  type=%d  in %s" % (x.frm, x.type, idc.get_func_name(x.frm)))

    w("################ C. TechnoClass_InAir 0x5F6B90 ################")
    ea = 0x5F6B90
    for _ in range(40):
        dis = idc.GetDisasm(ea)
        w("  %08X  %s" % (ea, dis))
        m = idc.print_insn_mnem(ea)
        if m in ("retn", "ret"):
            break
        ea += idc.get_item_size(ea)

    w("################ D. 落点字节 ################")
    def dump(ea, n, title):
        b = ida_bytes.get_bytes(ea, n)
        w("%s @ %08X : %s" % (title, ea, " ".join("%02X" % c for c in b)))

    dump(0x4DA87A, 8, "0x4DA87A")
    for a in (0x75C7E0, 0x6A3E50, 0x5B19D0, 0x517100, 0x4B4820):
        dump(a, 8, "InWhichLayer")

finally:
    f.close()
    ida_pro.qexit(0)
