import os

import ida_auto
import ida_funcs
import ida_hexrays
import ida_pro


OUT_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "goodie_full.txt")
FUNC_START = 0x481A00


def main():
    ida_auto.auto_wait()
    if not ida_hexrays.init_hexrays_plugin():
        print("hexrays init failed")
        return
    f = ida_funcs.get_func(FUNC_START)
    lines = []
    if not f:
        lines.append("NO FUNCTION")
    else:
        lines.append("FUNC %s [0x%X-0x%X]" % (ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea))
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
