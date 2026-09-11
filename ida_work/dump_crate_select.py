import os

import ida_auto
import ida_funcs
import ida_hexrays
import ida_lines
import ida_pro
import idc


OUT_FILE = os.path.join(os.path.dirname(__file__), "crate_select_dump.txt")
LO = 0x481A00
HI = 0x481D90


def plain(text):
    try:
        return ida_lines.tag_remove(text)
    except AttributeError:
        return text


def main():
    ida_auto.auto_wait()
    ida_hexrays.init_hexrays_plugin()
    lines = []
    f = ida_funcs.get_func(LO)
    lines.append("FUNC %s [0x%X-0x%X]" % (ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea))
    addr = LO
    while addr < HI:
        t = plain(ida_lines.generate_disasm_line(addr, 0))
        if t:
            lines.append("%08X: %s" % (addr, t.strip()))
        addr = idc.next_head(addr, HI)
    cf = ida_hexrays.decompile(f.start_ea)
    txt = str(cf)
    keep = False
    out = []
    for ln in txt.splitlines():
        out.append(ln)
    text = "\n".join(lines) + "\n==== pseudo (full) ====\n" + "\n".join(out)
    with open(OUT_FILE, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT_FILE, len(text))


main()
ida_pro.qexit(0)
