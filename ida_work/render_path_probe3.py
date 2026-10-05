# -*- coding: utf-8 -*-
# 第三轮（收尾）：确认层数组的"写入语义"，方案 A 的正确性依赖它。
#  W1  DisplayClass::Remove (0x4A9770) 全文 —— 删除是"整体前移"(保序) 还是"末位顶替"(乱序)？
#  W2  sub_4AA2B0 / sub_4AA380 —— 这两个读层数组的函数是否会改写数组
#  W3  谁调用 DisplayClass::Submit(0x4A9720) —— Submit 的触发源
#  W4  LayerClass::AddObject 的 sorted=false 分支再确认（附加路径）
import idc
import idautils
import idaapi

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\render_path_probe3.txt"
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


def dump_func(ea, label="", limit=400):
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


w("=" * 78)
w("W1  DisplayClass::Remove (0x4A9770) —— 删除语义（关键：是否保序）")
w("=" * 78)
dump_func(0x4A9770, "DisplayClass::Remove", 200)

w("")
w("=" * 78)
w("W2  sub_4AA2B0 / sub_4AA380（是否改写数组）")
w("=" * 78)
dump_func(0x4AA2B0, "sub_4AA2B0", 90)
dump_func(0x4AA380, "sub_4AA380", 90)
w("")
w("---- 它们的调用者 ----")
for ea in (0x4AA2B0, 0x4AA380):
    w("  ** %s" % fname(ea))
    for x in sorted(set(idautils.CodeRefsTo(ea, 0))):
        w("    %08X  in %s : %s" % (x, fname(x), disasm_line(x)))
w("")
w("---- sub_536610 / sub_536A80 的调用者（再上一层）----")
for ea in (0x536610, 0x536A80):
    w("  ** %s (0x%X)" % (fname(ea), ea))
    for x in sorted(set(idautils.CodeRefsTo(ea, 0))):
        w("    %08X  in %s : %s" % (x, fname(x), disasm_line(x)))

w("")
w("=" * 78)
w("W3  谁调用 DisplayClass::Submit (0x4A9720)")
w("=" * 78)
for x in sorted(set(idautils.CodeRefsTo(0x4A9720, 0))):
    w("  %08X  in %s : %s" % (x, fname(x), disasm_line(x)))

w("")
w("=" * 78)
w("W4  LayerClass::AddObject 的 sorted=false 分支（0x5519CF - 0x551A2C）")
w("=" * 78)
f = idaapi.get_func(0x5519B0)
cur = 0x5519B0
while f and cur < f.end_ea:
    w("  %08X  %s" % (cur, disasm_line(cur)))
    cur = idc.next_head(cur, f.end_ea)

w("")
w("=" * 78)
w("W5  LayerClass::Sort (0x551A30) 再确认：是否只一趟")
w("=" * 78)
f = idaapi.get_func(0x551A30)
cur = 0x551A30
while f and cur < f.end_ea:
    w("  %08X  %s" % (cur, disasm_line(cur)))
    cur = idc.next_head(cur, f.end_ea)

fp.close()
print("DONE -> " + OUT)
