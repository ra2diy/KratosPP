# -*- coding: utf-8 -*-
import os
import re
import idc
import ida_pro
import idautils
import ida_struct
import idaapi

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p13_facing388_probe.txt")
f = open(OUT, "w", encoding="utf-8")


def w(s):
    f.write(s + "\n")
    f.flush()


# ------------------------------------------------ 1) 所有结构体里偏移 0x388 附近的成员
w("############ 1) 结构体成员：查找 offset 0x388 (及 0x370..0x3A0) ############")
try:
    qty = ida_struct.get_struc_qty()
except Exception as e:
    qty = 0
    w("   get_struc_qty 异常: %r" % e)

def dump_struct(sid, sidx):
    nm = ida_struct.get_struc_name(sid) or "?"
    if not any(k in nm for k in ("Techno", "Foot", "Object", "Abstract", "Facing", "Unit", "Building", "Infantry", "Aircraft")):
        return False
    s = ida_struct.get_struc(sid)
    if s is None:
        return False
    out = []
    off = 0
    for i in range(0, ida_struct.get_struc_size(s)):
        m = ida_struct.get_member(s, i)
        if m is None:
            continue
        moff = m.get_soff()
        if 0x360 <= moff <= 0x3A8:
            out.append("      +0x%03X  %s" % (moff, ida_struct.get_member_name(m.id) or "?"))
    if out:
        w("   [struct] %s" % nm)
        for l in out:
            w(l)
        return True
    return False

try:
    for sidx in range(0, ida_struct.get_struc_qty()):
        sid = ida_struct.get_struc_by_idx(sidx)
        try:
            dump_struct(sid, sidx)
        except Exception:
            pass
except Exception as e:
    w("   遍历异常: %r" % e)
w("")

# ------------------------------------------------ 2) 谁写 [reg+388h]
w("############ 2) 全 .text 扫描：写入 [reg+388h] 的指令 ############")
text = None
for seg in idautils.Segments():
    if idc.get_segm_name(seg) == ".text":
        text = (idc.get_segm_start(seg), idc.get_segm_end(seg))
        break
w("   .text = 0x%08X..0x%08X" % text)
count = 0
reads = 0
for ea in idautils.Heads(text[0], text[1]):
    if idc.print_insn_mnem(ea) not in ("mov", "add", "sub", "inc", "dec", "and", "or", "xor", "lea"):
        continue
    t = idc.get_operand_type(ea, 0)
    if t not in (idc.o_displ, idc.o_mem):
        continue
    d = idc.GetDisasm(ea)
    if "+388h]" not in d and "388h]" not in d:
        continue
    count += 1
    w("   %08X  %-46s  in %s" % (ea, d, idc.get_func_name(ea) or "?"))
w("   写入/操作 [..+388h] 的指令共 %d 条" % count)
w("")

# ------------------------------------------------ 3) 邻近字段：谁操作 +0x384 / +0x38C
w("############ 3) 邻近字段 +0x384 / +0x38C #############")
for tgt in ("+384h]", "384h]", "+38Ch]", "38Ch]", "+380h]", "380h]", "+394h]", "394h]"):
    n = 0
    w("   -- %s --" % tgt)
    for ea in idautils.Heads(text[0], text[1]):
        d = idc.GetDisasm(ea)
        if tgt in d:
            w("      %08X  %-46s  in %s" % (ea, d, idc.get_func_name(ea) or "?"))
            n += 1
            if n > 14:
                break
w("")

f.close()
ida_pro.qexit(0)
