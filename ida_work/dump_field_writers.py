import os
import re

import ida_auto
import ida_bytes
import ida_funcs
import ida_lines
import ida_pro
import ida_segment
import ida_ua
import idc


OUT_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "field_writers.txt")


def plain_text(text):
    try:
        return ida_lines.tag_remove(text)
    except AttributeError:
        pass
    try:
        return ida_lines.remove_color(text)
    except AttributeError:
        pass
    return text

# offset -> label
TARGETS = {
    0x158: "TechnoClass.ArmorMultiplier",
    0x15C: "TechnoClass.ArmorMultiplier+4",
    0x160: "TechnoClass.FirepowerMultiplier",
    0x164: "TechnoClass.FirepowerMultiplier+4",
    0x580: "FootClass.SpeedMultiplier",
    0x584: "FootClass.SpeedMultiplier+4",
}

def find_candidates():
    """Return list of (ea, offset, text) where an instruction touches a
    disp32 equal to one of the target offsets."""
    found = []
    for s in range(ida_segment.get_segm_qty()):
        seg = ida_segment.getnseg(s)
        if not seg or (seg.perm & ida_segment.SEGPERM_EXEC) == 0:
            continue
        ea = seg.start_ea
        while ea < seg.end_ea:
            insn = ida_ua.insn_t()
            ln = ida_ua.decode_insn(insn, ea)
            if ln > 0:
                for op in insn.ops:
                    if op.type == ida_ua.o_displ and op.addr in TARGETS:
                        text = plain_text(ida_lines.generate_disasm_line(ea, 0))
                        found.append((ea, op.addr, TARGETS[op.addr], text))
                        break
            ea = idc.next_head(ea, seg.end_ea)
    return found


def main():
    ida_auto.auto_wait()
    lines = []
    try:
        found = find_candidates()
        lines.append("total writer hits: %d" % len(found))
        for ea, off, label, text in found:
            f = ida_funcs.get_func(ea)
            fname = ida_funcs.get_func_name(ea) if f else "?"
            lines.append("0x%08X off=0x%X %s :: %s  [%s 0x%X-0x%X]" % (ea, off, label, text.strip(), fname, f.start_ea if f else 0, f.end_ea if f else 0))
            lo = max((f.start_ea if f else ea - 16), ea - 8)
            hi = min((f.end_ea if f else ea + 16), ea + 24)
            addr = lo
            while addr < hi:
                l = plain_text(ida_lines.generate_disasm_line(addr, 0))
                if l:
                    mark = ">>" if addr == ea else "  "
                    lines.append("   %s %08X: %s" % (mark, addr, l.strip()))
                addr = idc.next_head(addr, hi)
            lines.append("")
    except Exception as e:
        lines.append("scan failed: %r" % e)
    text = "\n".join(lines)
    with open(OUT_FILE, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT_FILE, len(text))


main()
ida_pro.qexit(0)
