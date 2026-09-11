import os

import ida_auto
import ida_bytes
import ida_ida
import ida_idaapi
import ida_hexrays
import ida_funcs
import ida_lines
import ida_pro
import idc


OUT_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "hook_disasm.txt")

TARGETS = [
    0x739971,
    0x44A04C,
]


def main():
    ida_auto.auto_wait()
    if not ida_hexrays.init_hexrays_plugin():
        print("hexrays init failed")
        return
    lines = []
    for ea in TARGETS:
        f = ida_funcs.get_func(ea)
        lines.append("=" * 100)
        lines.append("TARGET 0x%X func %s [0x%X-0x%X]" % (ea, ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea))

        # disasm window around ea
        lo = max(f.start_ea, ea - 0x90)
        hi = min(f.end_ea, ea + 0x90)
        addr = lo
        lines.append("-- disasm %08X..%08X --" % (lo, hi))
        while addr < hi:
            line = ida_lines.generate_disasm_line(addr, 0)
            if not line:
                break
            marker = ">>" if addr == ea else "  "
            lines.append("%s %08X: %s" % (marker, addr, line))
            addr = idc.next_head(addr, hi)

        # try to find the pseudocode line containing ea
        try:
            cf = ida_hexrays.decompile(f.start_ea)
            lines.append("-- pseudo lines containing EA --")
            for lnum, codeline in enumerate(cf.pseudocode):
                txt = str(codeline).strip()
                if ("0x%X" % ea) in txt or ("0x%x" % ea) in txt:
                    lines.append("  line %d: %s" % (lnum, txt))
        except Exception as e:
            lines.append("decompile failed: %r" % e)
        lines.append("")

    text = "\n".join(lines)
    with open(OUT_FILE, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT_FILE, len(text))


main()
ida_pro.qexit(0)
