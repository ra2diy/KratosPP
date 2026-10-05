import idaapi, idc, idautils

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\verify_esi.txt"
f = open(OUT, "w", encoding="utf-8")


def w(s=""):
    f.write(s + "\n")


def dump(start, n, title):
    w("=" * 118)
    w("### " + title)
    fn = idaapi.get_func(start)
    if fn:
        w("func: %s [%08X-%08X]" % (idc.get_func_name(fn.start_ea), fn.start_ea, fn.end_ea))
    w("-" * 118)
    cur = start
    for i in range(n):
        w("   %08X  %s" % (cur, idc.generate_disasm_line(cur, 0)))
        nxt = idc.next_head(cur, cur + 16)
        if nxt <= cur:
            break
        cur = nxt
    w("")


# 1) TechnoClass::Greatest_Threat 序言：确认 ESI 是否 = this(ECX)
dump(0x6F8DF0, 26, "TechnoClass::Greatest_Threat (0x6F8DF0) 序言 —— ESI 是否 = this")

# 2) 0x6F9039 之前的上下文：到达该点的分支条件
dump(0x6F8FB0, 22, "0x6F9039 之前的上下文（进入条件）")

# 3) FootClass::Greatest_Threat 序言
dump(0x4D9920, 16, "FootClass::Greatest_Threat (0x4D9920) 序言")

# 4) LogicClass_Update 起点（Hook 在 0x55AFB3 = 函数内偏移 3）
dump(0x55AFB0, 12, "LogicClass_Update (0x55AFB0) 起点")

# 5) WalkLocomotionClass / LocomotionClass::In_Which_Layer 所在 vtable 槽位与调用者（虚调用扫描）
w("=" * 118)
w("### LocomotionClass::In_Which_Layer (0x75C7E0) 的直接引用 / vtable 归属")
w("=" * 118)
target = 0x75C7E0
found = 0
for seg in [".rdata", ".data"]:
    s = idaapi.get_segm_by_name(seg)
    if not s:
        continue
    ea = s.start_ea
    while ea < s.end_ea - 4:
        v = idc.get_wide_dword(ea)
        if v == target:
            w("  vtable 引用 @ %08X (%s)" % (ea, seg))
            found += 1
        ea += 4
w("  共 %d 处 vtable 引用（0 表示不在静态 vtable，或 vtable 为运行时构造）" % found)

# 6) 谁写了 [esi+688h]（FootClass::Greatest_Threat 里那个字段），辅助判断
w("")
w("=" * 118)
w("### 关键字段确认")
w("=" * 118)
for ea, label in [(0x75C7E0, "WalkLoco::In_Which_Layer"), (0x4DA87A, "FootClass::AI cmp [esi+90h],bl")]:
    w("--- %s" % label)
    for i in range(6):
        v = idc.get_wide_dword(ea + i)
        w("    0x%08X +%d: 0x%08X" % (ea, i, v))
    w("")

f.close()
print("DONE")
