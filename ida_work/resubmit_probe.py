# -*- coding: utf-8 -*-
import os
import ida_auto
import ida_bytes
import ida_funcs
import ida_lines
import ida_pro
import idc

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "resubmit_probe.txt")
lines = []


def dis(start, end, title, marks=()):
    lines.append("== %s  0x%08X..0x%08X ==" % (title, start, end))
    ea = start
    n = 0
    while ea < end and n < 160:
        line = ida_lines.generate_disasm_line(ea, 0)
        if not line:
            break
        b = ida_bytes.get_bytes(ea, 8)
        hexs = " ".join("%02X" % c for c in b) if b else "?"
        m = ">>" if ea in marks else "  "
        lines.append("%s %08X: %-30s | %s" % (m, ea, hexs, line))
        nxt = idc.next_head(ea)
        if nxt <= ea:
            break
        ea = nxt
        n += 1
    lines.append("")


def main():
    ida_auto.auto_wait()

    f = ida_funcs.get_func(0x54B18E)
    if f:
        lines.append("JumpjetLocomotionClass_Process: %s [0x%08X..0x%08X]" %
                     (ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea))
    f = ida_funcs.get_func(0x4CD4E7)
    if f:
        lines.append("sub_4CD2A0 holder: %s [0x%08X..0x%08X]" %
                     (ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea))
    f = ida_funcs.get_func(0x424C00)
    if f:
        lines.append("AnimClass::Attach_To: %s [0x%08X..0x%08X]" %
                     (ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea))
    lines.append("")

    dis(0x54B130, 0x54B1C0, "JumpjetLocomotionClass_Process 中 Submit 前后",
        marks=(0x54B18E,))
    dis(0x4CD490, 0x4CD510, "sub_4CD2A0 中 Submit 前后", marks=(0x4CD4E7,))
    dis(0x424BB0, 0x424C90, "AnimClass::Attach_To（两处 Submit）",
        marks=(0x424C00, 0x424C7C))

    text = "\n".join(lines)
    open(OUT, "w", encoding="utf-8", errors="replace").write(text)
    print("written", OUT, len(text))


main()
ida_pro.qexit(0)
