import os

import ida_auto
import ida_hexrays
import ida_funcs
import ida_name
import ida_pro
import idautils
import idc


OUT_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "vanilla_decompile.txt")

TARGETS = [
    0x739971,   # Kratos UnitClass_TryToDeploy_TransferAE (部署: 载具->建筑)
    0x44A04C,   # Kratos BuildingClass_Selling_TransferAE (反部署: 建筑->载具)
]


def ea_name(ea):
    return ida_name.get_name(ea) or ida_funcs.get_func_name(ea) or ("0x%X" % ea)


def func_text(f):
    try:
        cf = ida_hexrays.decompile(f.start_ea)
        return str(cf)
    except Exception as e:
        return "// decompile failed: %r" % e


def callers(start, max_depth=1):
    """Collect callers of function start, optionally recursively."""
    seen = set()
    out = []

    def rec(ea, depth):
        f = ida_funcs.get_func(ea)
        if not f:
            return
        if f.start_ea in seen:
            return
        seen.add(f.start_ea)
        out.append((f.start_ea, depth))
        if depth >= max_depth:
            return
        for xref in idautils.XrefsTo(f.start_ea, 0):
            if xref.iscode:
                rec(xref.frm, depth + 1)

    rec(start, 0)
    return out


def main():
    ida_auto.auto_wait()
    if not ida_hexrays.init_hexrays_plugin():
        print("hexrays init failed")
        return
    lines = []
    for ea in TARGETS:
        f = ida_funcs.get_func(ea)
        if not f:
            lines.append("// NO FUNCTION at 0x%X" % ea)
            continue
        lines.append("=" * 100)
        lines.append("TARGET 0x%X in %s [0x%X - 0x%X]" % (ea, ea_name(f.start_ea), f.start_ea, f.end_ea))
        lines.append("Callers:")
        for cstart, depth in callers(f.start_ea, 1):
            lines.append("    %s depth=%d (caller of: %s)" % (ea_name(cstart), depth, ea_name(f.start_ea)))
        lines.append("")
        lines.append(func_text(f))
        lines.append("")

    text = "\n".join(lines)
    with open(OUT_FILE, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT_FILE, len(text))


main()
ida_pro.qexit(0)
