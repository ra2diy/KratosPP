import os

import ida_auto
import ida_funcs
import ida_hexrays
import ida_pro
import idc


OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "verify_report.txt")

EAS = [
    0x7393C0,
    0x739971,
    0x449C30,
    0x449FE7,
    0x44A002,
    0x44A04C,
]


def main():
    ida_auto.auto_wait()
    lines = []
    for ea in EAS:
        f = ida_funcs.get_func(ea)
        if not f:
            lines.append("0x%08X: NO FUNCTION" % ea)
            continue
        lines.append("=" * 70)
        lines.append("0x%08X in %s [0x%X-0x%X]" % (ea, ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea))
        if ea == f.start_ea:
            fc0 = idc.get_func_cmt(f.start_ea, 0)
            fc1 = idc.get_func_cmt(f.start_ea, 1)
            if fc0:
                lines.append("FUNC CMT(0): %s" % fc0)
            if fc1:
                lines.append("FUNC CMT(1): %s" % fc1)
        c0 = idc.get_cmt(ea, 0)
        c1 = idc.get_cmt(ea, 1)
        if c0:
            lines.append("CMT(0): %s" % c0)
        if c1:
            lines.append("CMT(1): %s" % c1)

    text = "\n".join(lines)
    with open(OUT, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text + "\n")
    print("written", OUT)


main()
ida_pro.qexit(0)
