# -*- coding: utf-8 -*-
import os
import idc
import ida_pro
import idautils
import ida_bytes

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p8_mark_probe.txt")
f = open(OUT, "w", encoding="utf-8")


def w(s):
    f.write(s + "\n")
    f.flush()


def disasm(ea, nbytes=400, indent="  "):
    end = ea + nbytes
    n = 0
    while ea < end and n < 300:
        dis = idc.GetDisasm(ea)
        if not dis:
            break
        w(indent + "%08X  %s" % (ea, dis))
        sz = idc.get_item_size(ea)
        if sz <= 0:
            break
        ea += sz
        n += 1


def calls_of(ea, nbytes=600):
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


def find_vtable_for_slot(slot_off, target):
    """在 .rdata 中找 (ptr == target) 且其所在的表基址满足 +slot_off == target"""
    res = []
    for seg in idautils.Segments():
        name = idc.get_segm_name(seg)
        if name not in (".rdata", ".data"):
            continue
        start = idc.get_segm_start(seg)
        end = idc.get_segm_end(seg)
        data = ida_bytes.get_bytes(start, end - start)
        if not data:
            continue
        pat = target.to_bytes(4, "little")
        pos = 0
        while True:
            i = data.find(pat, pos)
            if i < 0:
                break
            pos = i + 1
            off = start + i
            base = off - slot_off
            if base < start or base + slot_off + 4 > end:
                continue
            # base 必须是 4 对齐且 base 本身也是一个指针表（第一个 dword 是代码指针）
            first = ida_bytes.get_dword(base)
            if first < 0x401000 or first > 0x900000:
                continue
            res.append((name, base, off))
    return res


# ---------------------------------------------------------------- 1) 虚表定位
w("############ 1) 虚表槽定位 ############")
w("-- FootClass: slot +0x124 == FootClass_SetLayer(0x4D3780) --")
foot_vts = find_vtable_for_slot(0x124, 0x4D3780)
for (n, base, off) in foot_vts:
    w("   seg=%s vtable=0x%08X (slot@0x%08X)" % (n, base, off))
    w("      slot+0x1B4 (SetLocation) = 0x%08X %s" % (ida_bytes.get_dword(base + 0x1B4), idc.get_func_name(ida_bytes.get_dword(base + 0x1B4)) or ""))
    w("      slot+0x48  (GetCoords)   = 0x%08X %s" % (ida_bytes.get_dword(base + 0x48), idc.get_func_name(ida_bytes.get_dword(base + 0x48)) or ""))
    w("      slot+0xB8  (GetYSort)    = 0x%08X %s" % (ida_bytes.get_dword(base + 0xB8), idc.get_func_name(ida_bytes.get_dword(base + 0xB8)) or ""))
    w("      slot+0x78  (GetLayer)    = 0x%08X %s" % (ida_bytes.get_dword(base + 0x78), idc.get_func_name(ida_bytes.get_dword(base + 0x78)) or ""))
    w("      slot+0x1B8 (GetCellStruct)=0x%08X %s" % (ida_bytes.get_dword(base + 0x1B8), idc.get_func_name(ida_bytes.get_dword(base + 0x1B8)) or ""))
w("")

w("-- TechnoClass/ObjectClass: slot +0x1B4 == 0x5F6940 (SetPosition) --")
obj_vts = find_vtable_for_slot(0x1B4, 0x5F6940)
for (n, base, off) in obj_vts:
    w("   seg=%s vtable=0x%08X" % (n, base))
w("")

# ---------------------------------------------------------------- 2) 两个候选 SetCoords
w("############ 2) 0x4D3810 (疑似 FootClass_SetCoords) ############")
disasm(0x4D3810, 300)
w("   -- 调用目标 --")
for (x, m, t, d) in calls_of(0x4D3810, 300):
    w("      %08X %s -> %08X  %s" % (x, m, t, idc.get_func_name(t) or ""))
w("")

w("############ 3) 0x4DB810 (summary 所称 FootClass_SetCoords) ############")
disasm(0x4DB810, 280)
w("   -- 调用目标 --")
for (x, m, t, d) in calls_of(0x4DB810, 280):
    w("      %08X %s -> %08X  %s" % (x, m, t, idc.get_func_name(t) or ""))
w("")

# ---------------------------------------------------------------- 4) TechnoClass::Mark
w("############ 4) TechnoClass::Mark (0x4D3799 call 目标) ############")
mark = None
for (x, m, t, d) in calls_of(0x4D3780, 120):
    if x == 0x4D3799:
        mark = t
if mark is None:
    # 直接取
    mark = idc.get_operand_value(0x4D3799, 0)
w("   TechnoClass::Mark = 0x%08X  name=%s" % (mark, idc.get_func_name(mark) or ""))
w("")
disasm(mark, 700)
w("   -- 调用目标 --")
mk_calls = calls_of(mark, 700)
for (x, m, t, d) in mk_calls:
    w("      %08X %s -> %08X  %s" % (x, m, t, idc.get_func_name(t) or ""))
w("")

# ---------------------------------------------------------------- 5) Mark 是否接触层数组
w("############ 5) Mark 的一级调用树是否触碰 vec_ObjectsInLayers 0x8A0360 ############")
w("-- 所有 XrefsTo 0x8A0360 --")
writers = set()
for x in idautils.XrefsTo(0x8A0360, 0):
    nm = idc.get_func_name(x.frm) or "?"
    w("   from %08X type=%d in %s" % (x.frm, x.type, nm))
    writers.add(x.frm)

w("")
w("-- Mark 及其一级被调函数体内是否出现 0x8A0360 --")
def body_has(ea, target, nbytes=900):
    end = ea + nbytes
    cur = ea
    while cur < end:
        m = idc.print_insn_mnem(cur)
        if m in ("mov", "lea", "push", "cmp", "add", "sub", "and", "or", "xor", "test"):
            for op in range(0, 3):
                if idc.get_operand_type(cur, op) in (idc.o_imm, idc.o_mem):
                    v = idc.get_operand_value(cur, op)
                    if v == target:
                        return cur
        sz = idc.get_item_size(cur)
        if sz <= 0:
            break
        cur += sz
    return None

hit = body_has(mark, 0x8A0360, 700)
w("   Mark 自身: %s" % ("命中 @0x%08X" % hit if hit else "未命中"))
for (x, m, t, d) in mk_calls:
    h = body_has(t, 0x8A0360, 1200)
    nm = idc.get_func_name(t) or "?"
    w("   -> %08X %-40s : %s" % (t, nm, ("命中 @0x%08X" % h if h else "未命中")))
w("")

# ---------------------------------------------------------------- 6) ObjectClass::SetPosition
w("############ 6) 0x5F6940 ObjectClass_SetPosition ############")
disasm(0x5F6940, 60)
w("")

# ---------------------------------------------------------------- 7) GetZAdjustment 里 Mark 使用点
w("############ 7) 0x4DAFC0 FootClass_Get_ZAdjustment 中的调用 ############")
for (x, m, t, d) in calls_of(0x4DAFC0, 0xE0):
    w("      %08X %s -> %08X  %s" % (x, m, t, idc.get_func_name(t) or ""))
w("")

f.close()
ida_pro.qexit(0)
