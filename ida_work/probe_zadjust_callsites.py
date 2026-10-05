# -*- coding: utf-8 -*-
"""ZAdjust 判定第二步：10 处 `call [reg+2ECh]` 的宿主函数与现场；外加被 hook 函数全文。

只读查询；try/finally 保证 qexit(0)。
"""
import traceback

import ida_auto
import ida_funcs
import ida_pro
import idautils
import idc

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\zadjust_callsites_report.txt"

HOOKED_FN = 0x4DAFC0
SITES = [0x5191D8, 0x706020, 0x707208, 0x7073F3, 0x707708,
         0x73B33F, 0x73B3B4, 0x73B414, 0x73C774, 0x73D238]


def disasm_range(start, end, add):
    ea = start
    while ea < end:
        line = idc.generate_disasm_line(ea, 0) or ""
        add("    0x%08X  %s" % (ea, line))
        nxt = idc.next_head(ea, end)
        if nxt <= ea:
            break
        ea = nxt


def main():
    ida_auto.auto_wait()
    lines = []

    def add(s):
        lines.append(s)

    try:
        add("=== 被 hook 的函数 0x%08X（%s）全文 ===" % (HOOKED_FN, ida_funcs.get_func_name(HOOKED_FN)))
        f = ida_funcs.get_func(HOOKED_FN)
        disasm_range(f.start_ea, f.end_ea, add)
        add("")

        add("=== 10 处 call [reg+2ECh] 的宿主函数 ===")
        for va in SITES:
            f = ida_funcs.get_func(va)
            add("")
            add("--- 调用点 0x%08X   宿主 = %s (0x%08X..0x%08X)"
                % (va, ida_funcs.get_func_name(f.start_ea) if f else "(无)", f.start_ea if f else 0, f.end_ea if f else 0))
            disasm_range(max(f.start_ea, va - 0x40) if f else va - 0x40, va + 0x18, add)

        add("")
        add("=== 统计：同一函数内出现多次的调用点 ===")
        from collections import Counter
        c = Counter()
        for va in SITES:
            f = ida_funcs.get_func(va)
            c[ida_funcs.get_func_name(f.start_ea) if f else "?"] += 1
        for name, n in c.most_common():
            add("  %-46s x%d" % (name, n))
    except Exception as exc:                                     # noqa: BLE001
        add("FAILED: %r" % (exc,))
        add(traceback.format_exc())

    text = "\n".join(lines) + "\n"
    with open(OUT, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written %d lines" % len(lines))


try:
    main()
except Exception as exc:                                         # noqa: BLE001
    with open(OUT, "w", encoding="utf-8", errors="replace") as fp:
        fp.write("FAILED: %r\n%s\n" % (exc, traceback.format_exc()))
finally:
    ida_pro.qexit(0)
