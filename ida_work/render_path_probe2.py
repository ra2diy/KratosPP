# -*- coding: utf-8 -*-
# 第二轮：确认「层数组 -> 屏幕」的实际绘制遍历。
#  A) Tactical_Draw_All_6D8DB0 (0x6D8DB0) 全文 —— 层数组的绘制消费者
#  B) 它的调用者链（TacticalClass_Draw -> ... -> Draw_All）
#  C) 其余读层数组的函数分类（是否只碰 Ground）
#  D) sub_64DAB0（校验值）里的层循环
import idc
import idautils
import idaapi

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\render_path_probe2.txt"
fp = open(OUT, "w", encoding="utf-8", errors="replace")


def w(s=""):
    fp.write(str(s) + "\n")


def disasm_line(ea):
    try:
        return idc.generate_disasm_line(ea, 0)
    except Exception:
        return "<disasm failed>"


def fname(ea):
    f = idaapi.get_func(ea)
    if not f:
        return "<no func>"
    return idc.get_name(f.start_ea) or ("sub_%X" % f.start_ea)


def dump_func(ea, label="", limit=800):
    w("")
    w("################ %s  @%08X ################" % (label, ea))
    f = idaapi.get_func(ea)
    if not f:
        w("  <no function at %08X>" % ea)
        return None
    w("---- func %08X - %08X  (size %d) ----" % (f.start_ea, f.end_ea, f.end_ea - f.start_ea))
    cur = f.start_ea
    n = 0
    while cur < f.end_ea and n < limit:
        w("  %08X  %s" % (cur, disasm_line(cur)))
        cur = idc.next_head(cur, f.end_ea)
        n += 1
    w("  ...(shown %d)" % n)
    return f


def callees(ea, limit=200):
    f = idaapi.get_func(ea)
    if not f:
        return
    cur = f.start_ea
    while cur < f.end_ea and limit > 0:
        mn = idc.print_insn_mnem(cur)
        if mn in ("call", "jmp"):
            for y in idautils.CodeRefsFrom(cur, 0):
                w("  %08X -> %08X  %s" % (cur, y, idc.get_name(y) or "sub_%X" % y))
        cur = idc.next_head(cur, f.end_ea)


# ============================================================ A
w("=" * 78)
w("A  Tactical_Draw_All_6D8DB0 (0x6D8DB0) —— 层数组的绘制消费者")
w("=" * 78)
dump_func(0x6D8DB0, "Tactical_Draw_All_6D8DB0", 900)

w("")
w("---- 0x6D8DB0 内的全部 call ----")
callees(0x6D8DB0)

# ============================================================ B
w("")
w("=" * 78)
w("B  调用者链")
w("=" * 78)
w("---- 谁调用 0x6D8DB0 ----")
for x in sorted(set(idautils.CodeRefsTo(0x6D8DB0, 0))):
    w("  %08X  in %s : %s" % (x, fname(x), disasm_line(x)))

w("")
w("---- TacticalClass_Draw (0x6D3D10) 内的 call ----")
callees(0x6D3D10)

w("")
w("---- GScreenClass_DrawOnTop (0x4F4480) 内的 call 与 jump ----")
callees(0x4F4480)
w("")
w("---- 谁调用 GameDraw / 0x4F4480 ----")
for x in sorted(set(idautils.CodeRefsTo(0x4F4480, 0))):
    w("  %08X  in %s : %s" % (x, fname(x), disasm_line(x)))

# ============================================================ C
w("")
w("=" * 78)
w("C  其余「读层数组」的函数分类（看它们碰的是哪一层）")
w("=" * 78)
for ea, label in [(0x4AA2B0, "sub_4AA2B0"), (0x4AA380, "sub_4AA380"),
                  (0x4ADFF0, "sub_4ADFF0"), (0x4AE0B0, "sub_4AE0B0"),
                  (0x4AE118, "sub_4AE118"), (0x6D97D0, "sub_6D97D0"),
                  (0x6D9920, "sub_6D9920"), (0x6D9A50, "sub_6D9A50"),
                  (0x650A90, "sub_650A90")]:
    dump_func(ea, label, 60)

w("")
w("=" * 78)
w("C2  这些函数的调用者（判断它们是「查询」还是「绘制」）")
w("=" * 78)
for ea in (0x4AA2B0, 0x4AA380, 0x4ADFF0, 0x4AE0B0, 0x4AE118, 0x6D97D0, 0x6D9920, 0x6D9A50, 0x650A90):
    w("")
    w("  ---- %s (0x%X) 的调用者 ----" % (fname(ea), ea))
    for x in sorted(set(idautils.CodeRefsTo(ea, 0))):
        w("    %08X  in %s : %s" % (x, fname(x), disasm_line(x)))

# ============================================================ D
w("")
w("=" * 78)
w("D  sub_64DAB0（校验值）里的层数组循环")
w("=" * 78)
f = idaapi.get_func(0x64DAB0)
if f:
    w("---- func %08X - %08X ----" % (f.start_ea, f.end_ea))
    cur = f.start_ea
    while cur < f.end_ea:
        d = disasm_line(cur)
        if "8A0360" in d or "8A0390" in d or "8A03" in d or "vec_ObjectsInLayers" in d:
            w("  >>> %08X  %s" % (cur, d))
        cur = idc.next_head(cur, f.end_ea)

w("")
w("---- 谁调用 sub_64DAB0 ----")
for x in sorted(set(idautils.CodeRefsTo(0x64DAB0, 0))):
    w("  %08X  in %s : %s" % (x, fname(x), disasm_line(x)))

fp.close()
print("DONE -> " + OUT)
