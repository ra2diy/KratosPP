import os

import ida_auto
import ida_funcs
import ida_hexrays
import ida_lines
import ida_pro
import idc


OUT_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "range_funcs_dump.txt")

REGIONS = [
    (0x7012C2, 0x701290, 0x7013A0),
    (0x6F72E3, 0x6F7230, 0x6F7300),
]


def dump_region(lines, anchor, lo, hi):
    f = ida_funcs.get_func(anchor)
    if not f:
        lines.append("NO FUNCTION at 0x%X" % anchor)
        return
    lines.append("=" * 100)
    lines.append("FUNC containing 0x%X: %s [0x%X - 0x%X]" % (anchor, ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea))
    lines.append("-- disasm %08X..%08X --" % (lo, hi))
    addr = max(f.start_ea, lo)
    end = min(f.end_ea, hi)
    while addr < end:
        line = ida_lines.generate_disasm_line(addr, 0)
        if not line:
            break
        lines.append("  %08X: %s" % (addr, line))
        addr = idc.next_head(addr, end)
    lines.append("")
    lines.append("-- hexrays --")
    try:
        cf = ida_hexrays.decompile(f.start_ea)
        lines.append(str(cf))
    except Exception as e:
        lines.append("decompile failed: %r" % e)
    lines.append("")


def main():
    ida_auto.auto_wait()
    if not ida_hexrays.init_hexrays_plugin():
        print("hexrays init failed")
        return
    lines = []
    for anchor, lo, hi in REGIONS:
        dump_region(lines, anchor, lo, hi)
    text = "\n".join(lines)
    with open(OUT_FILE, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT_FILE, len(text))


main()
ida_pro.qexit(0)
