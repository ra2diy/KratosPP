# -*- coding: utf-8 -*-
# 批量审计 Kratos 全部 Hook：
#  A. 每个 Hook 的地址是否落在指令边界上；hook_size 是否覆盖整数条指令；
#     addr+size 是否正好是下一条指令（否则 trampoline 跳回会落在指令中间）。
#  B. 热点 Hook 附近的反汇编（确认被替换的原指令 + 跳回点是否正确）。
#  C. 热点 Hook 所在函数是否（直接）调用了随机数函数。
import idaapi, idc, idautils, ida_hexrays

TSV = r"D:/Workspace/ra2mod/platform/KratosPP/ida_work/hooks.tsv"
OUT = r"D:/Workspace/ra2mod/platform/KratosPP/ida_work/audit_hooks.txt"

HOT = [
    0x6F36DB,  # TechnoClass_SelectWeapon
    0x6FC339,  # TechnoClass_CanFire
    0x6FDD50,  # TechnoClass_Fire
    0x6F9039,  # Greatest_Threat_HealWeaponRange
    0x7012DF,  # In_WeaponRange (GetWeaponRange)
    0x6F72E3,  # In_Range
    0x6FF29E,  # Fire_ROFMultiplier
    0x6F9E50,  # TechnoClass_Update
    0x6FAF7A,  # TechnoClass_UpdateEnd
    0x6FAFFD,  # TechnoClass_UpdateEnd
    0x5F45A0,  # TechnoClass_Select
    0x6F6CA0,  # TechnoClass_Put
    0x6F42ED,  # TechnoClass_Init
    0x6F3260,  # TechnoClass_CTOR
    0x6F9B7E,  # SyncLog SelectAutoTarget 监控点 (非Hook, 参考)
]

# 这些函数是否吃随机数
RNG_FUNCS = [
    (0x6F8DF0, "TechnoClass::Greatest_Threat"),
    (0x6F36A0, "TechnoClass::SelectWeapon 区"),
]


def is_rng_call(callee_ea):
    n = idc.get_func_name(callee_ea) or ""
    nn = n.lower()
    return ("random" in nn) or ("rand" in nn)


def callers_rng(func_ea):
    """返回该函数体内直接调用的随机数函数列表"""
    hits = []
    if func_ea == idaapi.BADADDR:
        return hits
    f = idaapi.get_func(func_ea)
    if not f:
        return hits
    ea = f.start_ea
    while ea < f.end_ea:
        for x in idautils.CodeRefsFrom(ea, 0):
            if is_rng_call(x):
                hits.append((ea, idc.get_func_name(x)))
        ea = idc.next_head(ea, f.end_ea)
    return hits


def main():
    lines = []
    lines.append("=" * 110)
    lines.append("### A. 全部 Hook 指令边界 / 长度体检")
    lines.append("=" * 110)
    lines.append("%-9s %-8s %-6s %-9s %-9s %s" % ("ADDR", "NAME", "SIZE", "addr+size", "是否指令头", "备注 / 文件"))
    bad = []
    n = 0
    for ln in open(TSV, encoding="utf-8"):
        ln = ln.strip()
        if not ln:
            continue
        p = ln.split("\t")
        if len(p) < 4:
            continue
        addr = int(p[0], 16)
        name = p[1]
        size = int(p[2], 16)
        src = p[3]
        n += 1
        next_ea = addr + size
        is_head = idc.is_code(idc.get_full_flags(next_ea)) and (idc.get_item_head(next_ea) == next_ea)
        # hook 起点是否指令头
        start_head = (idc.get_item_head(addr) == addr)
        note = []
        if not start_head:
            note.append("起点非指令头!")
        if not is_head:
            note.append("addr+size 非指令头!!")
        if note:
            bad.append((addr, name, size, src, " ".join(note)))
        lines.append("%08X %-8s 0x%-4X %08X %-9s %s %s" % (
            addr, name[:8], size, next_ea, "OK" if is_head else "NO", src,
            " ".join(note)))
    lines.append("")
    lines.append("总计 %d 个 Hook；疑似问题 %d 个" % (n, len(bad)))
    for b in bad:
        lines.append("   [BAD] 0x%08X %s size=0x%X  %s  %s" % b)
    lines.append("")

    lines.append("=" * 110)
    lines.append("### B. 热点 Hook 反汇编")
    lines.append("=" * 110)
    for a in HOT:
        f = idaapi.get_func(a)
        lines.append("-" * 110)
        lines.append("### Hook 0x%X   所在函数: %s  [0x%X - 0x%X]" % (
            a, (idc.get_func_name(f.start_ea) if f else "(未识别)"),
            (f.start_ea if f else 0), (f.end_ea if f else 0)))
        # 打印 a-24 .. a+24 的指令，并标出 a
        ea = a
        for _ in range(8):
            prev = idc.prev_head(ea)
            if prev == idaapi.BADADDR:
                break
            ea = prev
        end = a + 32
        while ea < end:
            dis = idc.generate_disasm_line(ea, 0)
            mark = "  <<<< HOOK 点" if ea == a else ""
            lines.append("  0x%08X  %-44s%s" % (ea, dis, mark))
            ea = idc.next_head(ea, end)
        # 该函数直接调用的随机数
        rng = callers_rng(a)
        if rng:
            lines.append("  [该函数体直接调用随机数] ")
            for (site, nm) in rng:
                lines.append("     0x%08X -> %s" % (site, nm))
        else:
            lines.append("  [该函数体未直接调用随机数]")
        lines.append("")

    lines.append("=" * 110)
    lines.append("### C. 关键判定函数是否吃随机数")
    lines.append("=" * 110)
    for (fa, nm) in RNG_FUNCS:
        f = idaapi.get_func(fa)
        real = idc.get_func_name(f.start_ea) if f else "(none)"
        rng = callers_rng(fa)
        lines.append("%s  实际函数名=%s  是否直接调用随机数=%s" % (nm, real, "是" if rng else "否"))
        for (site, rn) in rng:
            lines.append("     0x%08X -> %s" % (site, rn))
    lines.append("")

    lines.append("=" * 110)
    lines.append("### D. XrefsTo 热点 Hook")
    lines.append("=" * 110)
    for a in HOT:
        lines.append("--- 0x%X ---" % a)
        cnt = 0
        for x in idautils.XrefsTo(a, 0):
            lines.append("   来自 0x%08X (%s)" % (x.frm, idc.get_func_name(x.frm)))
            cnt += 1
        if cnt == 0:
            lines.append("   (无，可能被直接 jmp/调用或为数据)")
    lines.append("")

    with open(OUT, "w", encoding="utf-8") as fp:
        fp.write("\n".join(lines))


main()
idc.qexit(0)
