import os

import ida_auto
import ida_bytes
import ida_funcs
import ida_name
import ida_pro
import ida_ua
import idautils
import idc

OUT_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sync_check_core.txt")

TARGET = 0x64DAB0


def ea_name(ea):
    n = ida_name.get_name(ea)
    if n:
        return n
    f = ida_funcs.get_func(ea)
    return ida_funcs.get_func_name(f.start_ea) if f else ("0x%X" % ea)


def main():
    ida_auto.auto_wait()
    lines = []
    f = ida_funcs.get_func(TARGET)
    lines.append("FUNC 0x%X %s [0x%X-0x%X]" % (TARGET, ea_name(f.start_ea), f.start_ea, f.end_ea))
    lines.append("")

    calls = {}
    globals_ref = {}
    lines.append("---- DISASM ----")
    ea = f.start_ea
    while ea < f.end_ea:
        dis = idc.generate_disasm_line(ea, 0)
        lines.append("0x%08X  %s" % (ea, dis))
        # collect call targets
        for xref in idautils.XrefsFrom(ea, 0):
            if xref.type in (16, 17, 18, 19, 20, 21):  # call variants
                calls.setdefault(xref.to, 0)
                calls[xref.to] += 1
            elif xref.type in (1, 2, 3):  # data read/write
                globals_ref.setdefault(xref.to, 0)
                globals_ref[xref.to] += 1
        nxt = idc.next_head(ea, f.end_ea)
        ea = nxt if nxt > ea else ea + 1

    lines.append("")
    lines.append("---- CALLED FUNCTIONS ----")
    for to, cnt in sorted(calls.items(), key=lambda x: -x[1]):
        lines.append("  x%-3d %s" % (cnt, ea_name(to)))

    lines.append("")
    lines.append("---- GLOBAL / DATA REFS ----")
    for to, cnt in sorted(globals_ref.items(), key=lambda x: -x[1]):
        nm = ida_name.get_name(to) or ("0x%X" % to)
        lines.append("  x%-3d 0x%X %s" % (cnt, to, nm))

    text = "\n".join(lines)
    with open(OUT_FILE, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT_FILE, len(text))


main()
ida_pro.qexit(0)
