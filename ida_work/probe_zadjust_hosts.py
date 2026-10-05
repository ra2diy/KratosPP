# -*- coding: utf-8 -*-
"""补全：0x706ED0 / 0x707280 / 0x707480 / UnitClass_55C(0x73B140) 的性质与调用者。

只读查询；try/finally 保证 qexit(0)。
"""
import traceback

import ida_auto
import ida_funcs
import ida_pro
import idautils
import idc

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\zadjust_hosts_report.txt"

TARGETS = [0x706ED0, 0x707280, 0x707480, 0x73B140]


def main():
    ida_auto.auto_wait()
    lines = []

    def add(s):
        lines.append(s)

    try:
        for t in TARGETS:
            f = ida_funcs.get_func(t)
            add("=== 0x%08X  %s  (0x%08X..0x%08X)"
                % (t, ida_funcs.get_func_name(t), f.start_ea, f.end_ea))
            # 前 14 条指令
            ea = f.start_ea
            for _ in range(14):
                add("    0x%08X  %s" % (ea, idc.generate_disasm_line(ea, 0) or ""))
                nxt = idc.next_head(ea, f.end_ea)
                if nxt <= ea:
                    break
                ea = nxt
            # 调用者
            add("    -- 被谁引用 --")
            n = 0
            for x in idautils.XrefsTo(t, 0):
                n += 1
                cf = ida_funcs.get_func(x.frm)
                add("      0x%08X (%s)  %s"
                    % (x.frm, ida_funcs.get_func_name(cf.start_ea) if cf else "?",
                       idc.generate_disasm_line(x.frm, 0) or ""))
                if n >= 12:
                    add("      ...")
                    break
            add("")
    except Exception as exc:                                     # noqa: BLE001
        add("FAILED: %r" % (exc,))
        add(traceback.format_exc())

    with open(OUT, "w", encoding="utf-8", errors="replace") as fp:
        fp.write("\n".join(lines) + "\n")
    print("written %d lines" % len(lines))


try:
    main()
except Exception as exc:                                         # noqa: BLE001
    with open(OUT, "w", encoding="utf-8", errors="replace") as fp:
        fp.write("FAILED: %r\n%s\n" % (exc, traceback.format_exc()))
finally:
    ida_pro.qexit(0)
