import os
import sys

import ida_auto
import ida_bytes
import ida_funcs
import ida_hexrays
import ida_name
import ida_pro
import ida_ua
import idautils
import idc

OUT_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sync_sites_disasm.txt")

# (start, end) 反汇编区间
RANGES = [
    (0x738870, 0x738920),   # Kratos hook 0x7388FD 所在函数
    (0x7388FD, 0x738907),
    (0x6FF8F1, 0x6FF901),   # TechnoClass_DeployFire (UnitExtHook)
    (0x702299, 0x7022B0),   # TechnoClass_Destroy_VxlDebris_Remap
    (0x7023D0, 0x7023F0),   # 上述 hook 的跳转目标 0x7023E5 附近
    (0x67CEF0, 0x67CF20),   # SaveGame_Start
]

FUNCS = [0x738890, 0x702299, 0x7023E5]


def ea_name(ea):
    return ida_name.get_name(ea) or ("0x%X" % ea)


def main():
    ida_auto.auto_wait()
    lines = []

    for start, end in RANGES:
        lines.append("=" * 90)
        lines.append("DISASM 0x%X - 0x%X" % (start, end))
        ea = start
        while ea < end:
            f = ida_funcs.get_func(ea)
            dis = idc.generate_disasm_line(ea, 0)
            func = ea_name(f.start_ea) if f else "?"
            lines.append("  0x%08X  [%s]  %s" % (ea, func, dis))
            nxt = idc.next_head(ea, end)
            if nxt <= ea:
                ea += 1
            else:
                ea = nxt
        lines.append("")

    for ea in FUNCS:
        f = ida_funcs.get_func(ea)
        lines.append("=" * 90)
        if not f:
            lines.append("NO FUNC at 0x%X" % ea)
            continue
        lines.append("FUNC 0x%X %s [0x%X-0x%X]" % (ea, ea_name(f.start_ea), f.start_ea, f.end_ea))
        lines.append("Callers:")
        seen = set()
        for xref in idautils.XrefsTo(f.start_ea, 0):
            cf = ida_funcs.get_func(xref.frm)
            nm = ea_name(cf.start_ea) if cf else "?"
            if nm in seen:
                continue
            seen.add(nm)
            lines.append("    %s (from 0x%X, type=%d)" % (nm, xref.frm, xref.type))
        lines.append("")

    text = "\n".join(lines)
    with open(OUT_FILE, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT_FILE, len(text))


main()
ida_pro.qexit(0)
