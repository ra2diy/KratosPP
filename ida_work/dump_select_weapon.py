import os

import ida_auto
import ida_funcs
import ida_hexrays
import ida_lines
import ida_pro
import idc


OUT_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "select_weapon_dump.txt")

ANCHOR = 0x6F36DB
WINDOW_START = 0x6F35C0
WINDOW_END = 0x6F3860


def main():
    ida_auto.auto_wait()
    if not ida_hexrays.init_hexrays_plugin():
        print("hexrays init failed")
        return

    f = ida_funcs.get_func(ANCHOR)
    lines = []
    if not f:
        lines.append("NO FUNCTION at 0x%X" % ANCHOR)
    else:
        lines.append("=" * 100)
        lines.append("FUNC containing 0x%X: %s [0x%X - 0x%X]" % (ANCHOR, ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea))

        # labels of interest
        for ea in [0x6F36DB, 0x6F36E3, 0x6F3745, 0x6F37AD, 0x6F37AF, 0x6F37EB, 0x6F3807]:
            lines.append("LABEL 0x%X: %s" % (ea, ida_funcs.get_func_name(ea) or "(inside func)"))

        lo = max(f.start_ea, WINDOW_START)
        hi = min(f.end_ea, WINDOW_END)
        lines.append("-- disasm %08X..%08X --" % (lo, hi))
        addr = lo
        while addr < hi:
            line = ida_lines.generate_disasm_line(addr, 0)
            if not line:
                break
            marker = ">>" if addr in [0x6F36DB, 0x6F36E3, 0x6F3745, 0x6F37AD, 0x6F37AF, 0x6F37EB, 0x6F3807] else "  "
            lines.append("%s %08X: %s" % (marker, addr, line))
            addr = idc.next_head(addr, hi)

        lines.append("")
        lines.append("-- hexrays --")
        try:
            cf = ida_hexrays.decompile(f.start_ea)
            lines.append(str(cf))
        except Exception as e:
            lines.append("decompile failed: %r" % e)

    text = "\n".join(lines)
    with open(OUT_FILE, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT_FILE, len(text))


main()
ida_pro.qexit(0)
