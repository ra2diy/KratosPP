# -*- coding: utf-8 -*-
"""查 FootClass::Update 中 0x4DA87A 处的原始逻辑，
   以及全局所有 DisplayClass::Submit / Remove 的调用点。

目的：确认 Kratos 的 FootClass_Update_UpdateLayer hook 是否
      替换/篡改了引擎的「层变更时 Remove+Submit」语义。
输出：ida_work/layer_submit_probe.txt
"""

import os

import ida_auto
import ida_bytes
import ida_funcs
import ida_lines
import ida_pro
import idc

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_FILE = os.path.join(HERE, "layer_submit_probe.txt")

SUBMIT = 0x004A9720     # DisplayClass::Submit
REMOVE = 0x004A9770     # DisplayClass::Remove

lines = []


def dis(start, end, title):
    lines.append("== %s  0x%08X..0x%08X ==" % (title, start, end))
    ea = start
    while ea < end:
        line = ida_lines.generate_disasm_line(ea, 0)
        b = ida_bytes.get_bytes(ea, 8)
        hexs = " ".join("%02X" % c for c in b) if b else "?"
        mark = ">>" if ea == 0x004DA87A else "  "
        lines.append("%s %08X: %-30s | %s" % (mark, ea, hexs, line))
        nxt = idc.next_head(ea)
        if nxt <= ea:
            break
        ea = nxt
    lines.append("")


def callers_of(target, name):
    lines.append("== 所有 call/jmp 到 %s (0x%08X) 的站点 ==" % (name, target))
    hits = []
    ea = 0x00401000
    limit = 0x00800000
    while ea < limit:
        if idc.print_insn_mnem(ea) in ("call", "jmp"):
            op = idc.get_operand_value(ea, 0)
            if op == target:
                f = ida_funcs.get_func(ea)
                fname = ida_funcs.get_func_name(f.start_ea) if f else "?"
                hits.append((ea, fname))
        ea = idc.next_head(ea)
        if ea <= 0:
            break
    for a, fn in hits:
        lines.append("    %08X  in  %s" % (a, fn))
    lines.append("    (共 %d 处)" % len(hits))
    lines.append("")
    return hits


def main():
    ida_auto.auto_wait()

    # 1. 0x4DA87A 所在函数
    f = ida_funcs.get_func(0x004DA87A)
    if f:
        lines.append("0x4DA87A 属于函数 %s [0x%08X..0x%08X]"
                     % (ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea))
    lines.append("")

    # 2. 前后上下文
    dis(0x004DA840, 0x004DA900, "0x4DA87A 前后上下文")

    # 3. 整函数反汇编
    if f:
        dis(f.start_ea, min(f.end_ea, f.start_ea + 0x200), "所在函数完整（前 0x200 字节）")

    # 4. Submit / Remove 的全部调用者
    callers_of(SUBMIT, "DisplayClass::Submit")
    callers_of(REMOVE, "DisplayClass::Remove")

    text = "\n".join(lines)
    with open(OUT_FILE, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT_FILE, len(text))


main()
ida_pro.qexit(0)
