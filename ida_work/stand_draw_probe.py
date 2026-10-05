# -*- coding: utf-8 -*-
# 探查「Stand 绘制顺序」机制的三个关键点：
#  A) DisplayClass::Submit (0x4A9720) 全文 —— 看 layer / sortable 如何决定
#  B) 0x4A9759 的调用目标（层插入原语）—— 看 sortable 的有序插入到底用哪个比较函数、等值怎么处理
#  C) ObjectClass::GetYSort 所在函数 (0x5F6BF7) —— 看 *x/*y 指向的是不是真实渲染坐标缓冲
import os
import idc
import idautils
import idaapi

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\stand_draw_probe.txt"

fp = open(OUT, "w", encoding="utf-8", errors="replace")


def w(s=""):
    fp.write(str(s) + "\n")


def disasm_line(ea):
    try:
        return idc.generate_disasm_line(ea, 0)
    except Exception:
        try:
            return idc.GetDisasm(ea)
        except Exception:
            return "<disasm failed>"


def dump_func(ea, label="", limit=260):
    w("")
    w("################ %s  @%08X ################" % (label, ea))
    f = idaapi.get_func(ea)
    if not f:
        w("  <no function at %08X>" % ea)
        return None
    w("---- func %08X - %08X ----" % (f.start_ea, f.end_ea))
    cur = f.start_ea
    n = 0
    while cur < f.end_ea and n < limit:
        raw = ""
        try:
            b = idc.get_bytes(cur, 8)
            if b:
                raw = " ".join("%02X" % c for c in bytearray(b))
        except Exception:
            pass
        w("  %08X  %-46s  %s" % (cur, disasm_line(cur), raw))
        cur = idc.next_head(cur, f.end_ea)
        n += 1
    w("  ...(total %d instrs shown, func size %d)" % (n, f.end_ea - f.start_ea))
    return f


# ---------- A) Submit ----------
dump_func(0x4A9720, "DisplayClass::Submit", 80)

# ---------- B) 层插入原语：0x4A9759 的 call 目标 ----------
target = None
ea = 0x4A9759
w("")
w("=== 0x4A9759 处指令: %s" % disasm_line(ea))
for x in idautils.CodeRefsFrom(ea, 0):
    target = x
w("=== call 目标 = %s (0x%X)" % (idc.get_name(target) if target else "?", target or 0))
if target:
    dump_func(target, "Layer insert primitive", 220)
    # 该原语内部调用的子函数
    sub = set()
    cur = idaapi.get_func(target).start_ea
    end = idaapi.get_func(target).end_ea
    while cur < end:
        if idc.print_insn_mnem(cur) in ("call", "jmp"):
            for y in idautils.CodeRefsFrom(cur, 0):
                sub.add((cur, y))
        cur = idc.next_head(cur, end)
    w("")
    w("---- 原语内部调用的子函数 ----")
    for src, dst in sorted(sub):
        w("  %08X -> %08X  %s" % (src, dst, idc.get_name(dst)))
    shown = 0
    for src, dst in sorted(sub):
        f2 = idaapi.get_func(dst)
        if f2 and f2.start_ea == dst and shown < 3:
            dump_func(dst, "  sub of primitive", 90)
            shown += 1

# ---------- C) GetYSort 所在函数 ----------
dump_func(0x5F6BF7, "ObjectClass::GetYSort (0x5F6BF7)", 60)
w("")
w("=== 0x5F6BF7 之前的指令（确认 *x/*y 来源）===")
cur = idaapi.get_func(0x5F6BF7).start_ea
w("---- function start = %08X ----" % cur)

# 调用者上下文
w("")
w("################ 调用 0x5F6BF7 的函数（含调用前后 12 条指令）################")
callers = set()
for x in idautils.CodeRefsTo(0x5F6BF7, 0):
    callers.add(x)
for x in idautils.CodeRefsTo(idaapi.get_func(0x5F6BF7).start_ea, 0):
    callers.add(x)
for c in sorted(callers):
    w("")
    w("---- caller @%08X : %s ----" % (c, disasm_line(c)))
    f3 = idaapi.get_func(c)
    if not f3:
        continue
    cur = f3.start_ea
    lines = []
    while cur < f3.end_ea:
        lines.append(cur)
        cur = idc.next_head(cur, f3.end_ea)
    try:
        idx = lines.index(c)
    except ValueError:
        idx = 0
    for p in lines[max(0, idx - 12): idx + 4]:
        w("    %08X  %s" % (p, disasm_line(p)))

# ---------- D) 谁通过 vt[0xB8] 调用 ----------
w("")
w("################ 数据引用 0x5F6BF7（vtable 槽定位）################")
for x in idautils.DataRefsTo(0x5F6BF7):
    w("  data ref at %08X  (slot off from %08X = 0x%X)" % (x, x - 0xB8, 0xB8))
    w("     附近 dword: %s" % " ".join(
        "%08X" % (idc.get_wide_dword(x - 0xB8 + i * 4) if True else 0) for i in range(0, 6)))

# ---------- E) LayerClass::Sort 的比较目标确认 ----------
w("")
w("################ LayerClass::Sort sub_551A30 => vt[0xB8] ################")
w("  (0x551A4F / 0x551A5D 都是 call dword ptr [reg+0B8h])")

fp.close()
print("DONE -> " + OUT)
