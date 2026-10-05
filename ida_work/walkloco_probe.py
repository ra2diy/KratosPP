# -*- coding: utf-8 -*-
import os
import ida_auto, ida_bytes, ida_funcs, ida_lines, ida_pro, idc
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "walkloco_probe.txt")
lines = []
def dis(start, end, title, marks=()):
    lines.append("== %s  0x%08X..0x%08X ==" % (title, start, end))
    ea = start; n = 0
    while ea < end and n < 300:
        line = ida_lines.generate_disasm_line(ea, 0)
        if not line: break
        b = ida_bytes.get_bytes(ea, 8)
        hexs = " ".join("%02X" % c for c in b) if b else "?"
        m = ">>" if ea in marks else "  "
        lines.append("%s %08X: %-30s | %s" % (m, ea, hexs, line))
        nxt = idc.next_head(ea)
        if nxt <= ea: break
        ea = nxt; n += 1
    lines.append("")
def main():
    ida_auto.auto_wait()
    for a, nm in ((0x0075C7E0,"WalkLoco_InWhichLayer"), (0x006A3E50,"ShipLoco_InWhichLayer"),
                  (0x004B4820,"DriveLoco_InWhichLayer"), (0x00517F00,"InfantryClass_Update?"),
                  (0x004DA87A,"FootClass_AI_hook")):
        f = ida_funcs.get_func(a)
        if f:
            lines.append("%s 0x%08X -> func %s [0x%08X..0x%08X]" % (nm, a, ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea))
    lines.append("")
    dis(0x0075C7D0, 0x0075C820, "WalkLocomotionClass::InWhichLayer 原始字节", marks=(0x0075C7E0,))
    dis(0x006A3E40, 0x006A3E70, "ShipLocomotionClass::InWhichLayer 原始字节", marks=(0x006A3E50,))
    dis(0x004B4810, 0x004B4840, "DriveLocomotionClass::InWhichLayer 原始字节", marks=(0x004B4820,))
    # 搜所有 InWhichLayer 虚槽(vt+0x78) 的实现：找 "mov eax, 2" 且函数很小 的候选
    text = "\n".join(lines)
    open(OUT, "w", encoding="utf-8", errors="replace").write(text)
    print("written", OUT, len(text))
main()
ida_pro.qexit(0)
