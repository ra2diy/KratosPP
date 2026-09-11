import os

import ida_auto
import ida_funcs
import ida_hexrays
import ida_lines
import ida_pro
import idc


OUT_FILE = os.path.join(os.path.dirname(__file__), "multiplier_fns.txt")

# func anchor addresses to inspect
ANCHORS = [
    0x6F9E50,  # TechnoClass::Update (Kratos hook site)
    0x6F3260,  # TechnoClass ctor hook site
    0x6FDD50,  # TechnoClass::Fire
    0x6FE352,  # FireAt multiplier use (Phobos)
    0x6FDBE2,  # AdjustDamage multiplier use (Phobos)
    0x6FDC87,  # AdjustDamage armor use (Phobos)
]

KEYS = ["FirepowerMultiplier", "ArmorMultiplier", "SpeedMult", "0x158", "0x15C",
        "0x160", "0x164", "0x580", "0x584"]


def plain(text):
    try:
        return ida_lines.tag_remove(text)
    except AttributeError:
        return text


def main():
    ida_auto.auto_wait()
    ida_hexrays.init_hexrays_plugin()
    lines = []
    for anchor in ANCHORS:
        f = ida_funcs.get_func(anchor)
        if not f:
            lines.append("== no func for 0x%X ==" % anchor)
            continue
        lines.append("=" * 100)
        lines.append("ANCHOR 0x%X in %s [0x%X-0x%X]" % (anchor, ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea))
        # disasm around anchor
        lo = max(f.start_ea, anchor - 0x40)
        hi = min(f.end_ea, anchor + 0x40)
        addr = lo
        while addr < hi:
            t = plain(ida_lines.generate_disasm_line(addr, 0))
            if t:
                lines.append("   %08X: %s" % (addr, t.strip()))
            addr = idc.next_head(addr, hi)
        # pseudo lines with multiplier keywords
        try:
            cf = ida_hexrays.decompile(f.start_ea)
            lines.append("-- pseudo lines with keywords --")
            for ln in str(cf).splitlines():
                low = ln.lower()
                if any(k.lower() in low for k in KEYS):
                    lines.append("   " + ln.strip())
        except Exception as e:
            lines.append("decompile failed: %r" % e)
        lines.append("")
    text = "\n".join(lines)
    with open(OUT_FILE, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT_FILE, len(text))


main()
ida_pro.qexit(0)
