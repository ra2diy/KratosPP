# -*- coding: utf-8 -*-
# 目标：确定「层数组 vec_ObjectsInLayers 到底是不是绘制列表」，以及渲染路径长什么样。
#
# 要回答的问题：
#  Q1  层数组基址 0x8A0360 及各层，究竟有哪些代码在读？（如果只有 Submit/Remove/Sort/哈希，
#      那它就不是绘制列表，"下标序 = 绘制序" 的假设就需要推翻）
#  Q2  Main_Loop (0x55D360) 一帧的调用序列是什么？尾部的 Ground Sort 在绘制**之前**还是**之后**？
#  Q3  谁在遍历层数组并调用对象绘制虚函数？（找渲染侧的遍历点）
#  Q4  渲染侧有没有「按 Y 排序」的逻辑？（谁调用 Get_YSort 0x5F6BD0 / CompareYSortValues 0x5F6220）
#  Q5  ObjectClass 的绘制虚函数槽位（DrawIfVisible / DrawIt）与 Get_YSort 同表，可反推渲染遍历点。
import idc
import idautils
import idaapi

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\render_path_probe.txt"
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


def raw_bytes(ea, n=8):
    try:
        b = idc.get_bytes(ea, n)
        if b:
            return " ".join("%02X" % c for c in bytearray(b))
    except Exception:
        pass
    return ""


def func_of(ea):
    f = idaapi.get_func(ea)
    return f


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
    w("---- func %08X - %08X ----" % (f.start_ea, f.end_ea))
    cur = f.start_ea
    n = 0
    while cur < f.end_ea and n < limit:
        w("  %08X  %-46s  %s" % (cur, disasm_line(cur), raw_bytes(cur)))
        cur = idc.next_head(cur, f.end_ea)
        n += 1
    w("  ...(shown %d instrs, func size %d)" % (n, f.end_ea - f.start_ea))
    return f


def window(ea, before=16, after=6, mark=True):
    f = func_of(ea)
    if not f:
        w("    <no func>")
        return
    cur = f.start_ea
    lines = []
    while cur < f.end_ea:
        lines.append(cur)
        cur = idc.next_head(cur, f.end_ea)
    try:
        idx = lines.index(ea)
    except ValueError:
        idx = 0
    for p in lines[max(0, idx - before): idx + after]:
        tag = " <<<< " if (mark and p == ea) else ""
        w("    %08X  %s%s" % (p, disasm_line(p), tag))


LAYER_BASE = 0x8A0360
LAYER_SZ = 0x18
LAYER_N = 5
LAYER_END = LAYER_BASE + LAYER_SZ * LAYER_N          # 0x8A03D8
LAYER_NAMES = ["Underground", "Surface", "Ground", "Air", "Top"]

# ============================================================================
# Q1  谁引用了层数组（含每层基址与 Items/Count 字段的偏移写法）
# ============================================================================
w("=" * 78)
w("Q1  对层数组区域的引用（数据引用 + 代码引用）")
w("=" * 78)

code_refs = {}
for a in range(LAYER_BASE, LAYER_END + 0x10, 4):
    for x in idautils.CodeRefsTo(a, 0):
        code_refs.setdefault(a, set()).add(x)
    for x in idautils.DataRefsTo(a):
        code_refs.setdefault(a, set()).add(x)

for a in sorted(code_refs):
    if a < LAYER_BASE:
        continue
    d = (a - LAYER_BASE) // LAYER_SZ
    within = (a - LAYER_BASE) % LAYER_SZ
    lay = LAYER_NAMES[d] if d < LAYER_N else "?%d" % d
    w("")
    w("---- 地址 %08X  (= 层[%d]%s + 0x%X) ----" % (a, d, lay, within))
    for x in sorted(code_refs[a]):
        w("   ref @%08X   in  %s   | %s" % (x, fname(x), disasm_line(x)))

# 按函数归并，看看涉及哪些函数
w("")
w("---- 归并：涉及层数组的函数清单 ----")
byfn = {}
for a, s in code_refs.items():
    for x in s:
        byfn.setdefault(fname(x), []).append((x, a))
for k in sorted(byfn):
    w("  %-46s  %d 处：%s" % (k, len(byfn[k]),
                             " ".join("%08X" % p for p, _ in sorted(byfn[k])[:12])))

# ============================================================================
# Q2  Main_Loop 全文（看一帧的绘制序列）
# ============================================================================
w("")
w("=" * 78)
w("Q2  Main_Loop (0x55D360) 全文 —— 一帧的调用序列")
w("=" * 78)
dump_func(0x55D360, "W?Main_Loop$n()i", 900)

# Main_Loop 内所有 call，便于定位绘制调用
w("")
w("---- Main_Loop 内的全部 call（按地址序）----")
f = func_of(0x55D360)
if f:
    cur = f.start_ea
    while cur < f.end_ea:
        if idc.print_insn_mnem(cur) == "call":
            for y in idautils.CodeRefsFrom(cur, 0):
                w("  %08X -> %08X  %s" % (cur, y, idc.get_name(y) or "sub_%X" % y))
        cur = idc.next_head(cur, f.end_ea)

# ============================================================================
# Q4  谁调用 Get_YSort / CompareYSortValues（渲染侧是否有 Y 排序）
# ============================================================================
w("")
w("=" * 78)
w("Q4  调用 Get_YSort(0x5F6BD0) 的位置 —— 谁在用排序键")
w("=" * 78)
for x in sorted(set(list(idautils.CodeRefsTo(0x5F6BD0, 0)) +
                    list(idautils.CodeRefsTo(0x5F6BF7, 0)))):
    w("")
    w("---- caller @%08X  in %s : %s" % (x, fname(x), disasm_line(x)))
    window(x, 10, 4)

w("")
w("=" * 78)
w("Q4b  调用 CompareYSortValues(0x5F6220) 的位置")
w("=" * 78)
for x in sorted(set(list(idautils.CodeRefsTo(0x5F6220, 0)) +
                    list(idautils.CodeRefsTo(0x5F6220, 1)))):
    w("")
    w("---- caller @%08X  in %s : %s" % (x, fname(x), disasm_line(x)))
    window(x, 10, 4)

w("")
w("=" * 78)
w("Q4c  调用 LayerClass::Sort(0x551A30) 的位置")
w("=" * 78)
for x in sorted(set(list(idautils.CodeRefsTo(0x551A30, 0)))):
    w("")
    w("---- caller @%08X  in %s : %s" % (x, fname(x), disasm_line(x)))
    window(x, 12, 4)

# ============================================================================
# Q5  ObjectClass vtable 槽位反推：找到 DrawIfVisible / DrawIt
# ============================================================================
w("")
w("=" * 78)
w("Q5  ObjectClass 派生 vtable 槽位（以 Get_YSort 0x5F6BD0 为锚点 +0xB8）")
w("=" * 78)
for vref in sorted(set(idautils.DataRefsTo(0x5F6BD0)) |
                   set(idautils.DataRefsTo(0x5F6BF7))):
    base = vref - 0xB8
    w("")
    w("---- vtable base ~%08X (slot+0xB8 指向 Get_YSort) ----" % base)
    for i in range(0, 0x120, 4):
        val = idc.get_wide_dword(base + i)
        nm = idc.get_name(val)
        if val and nm:
            w("    +%03X : %08X  %s" % (i, val, nm))
        elif val:
            w("    +%03X : %08X  sub_%X" % (i, val, val))

# ============================================================================
# Q6  疑似渲染函数：名字里含 Render/Draw/Tactical/Display 的函数
# ============================================================================
w("")
w("=" * 78)
w("Q6  名字含 Render/Draw 的函数（渲染路径候选）")
w("=" * 78)
pat = ("render", "draw", "tactical")
for ea in idautils.Functions():
    n = (idc.get_name(ea) or "").lower()
    if any(k in n for k in pat):
        w("  %08X  %s" % (ea, idc.get_name(ea)))

fp.close()
print("DONE -> " + OUT)
