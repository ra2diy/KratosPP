# -*- coding: utf-8 -*-
"""1) DisplayClass::Submit 全文（确认内部是否先 Remove）
   2) ObjectClass::Update 中 Submit/Remove 配对上下文
   3) 全程序搜索 LastLayer(+0x90) 的写入点
输出：ida_work/lastlayer_probe.txt
"""

import os
import re

import ida_auto
import ida_bytes
import ida_funcs
import ida_lines
import ida_pro
import idc

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_FILE = os.path.join(HERE, "lastlayer_probe.txt")

lines = []

# 匹配 [reg+90h] 形式的目的操作数
RE_DISP90 = re.compile(r"\[e[a-z]{2}\+90h\]", re.I)


def dis(start, end, title, mark=None):
    lines.append("== %s  0x%08X..0x%08X ==" % (title, start, end))
    ea = start
    n = 0
    while ea < end and n < 400:
        line = ida_lines.generate_disasm_line(ea, 0)
        if not line:
            break
        b = ida_bytes.get_bytes(ea, 8)
        hexs = " ".join("%02X" % c for c in b) if b else "?"
        m = ">>" if (mark is not None and ea == mark) else "  "
        lines.append("%s %08X: %-30s | %s" % (m, ea, hexs, line))
        nxt = idc.next_head(ea)
        if nxt <= ea:
            break
        ea = nxt
        n += 1
    lines.append("")


def find_writes_to_90():
    lines.append("== 全程序：写入 [reg+90h] (ObjectClass::LastLayer) 的指令 ==")
    hits = []
    ea = 0x00401000
    limit = 0x00800000
    while ea < limit:
        mn = idc.print_insn_mnem(ea)
        if mn and mn.lower() in ("mov", "and", "or", "xor", "inc", "dec"):
            op0 = idc.print_operand(ea, 0)
            if op0 and RE_DISP90.search(op0):
                f = ida_funcs.get_func(ea)
                fn = ida_funcs.get_func_name(f.start_ea) if f else "?"
                hits.append((ea, fn, ida_lines.generate_disasm_line(ea, 0)))
        ea = idc.next_head(ea)
        if ea <= 0:
            break
    for a, fn, txt in hits:
        lines.append("    %08X  %-52s in %s" % (a, txt.strip(), fn))
    lines.append("    (共 %d 处)" % len(hits))
    lines.append("")
    return hits


def main():
    ida_auto.auto_wait()

    dis(0x004A9720, 0x004A97A0, "DisplayClass::Submit 全文 + Remove 开头", mark=0x004A975E)

    f = ida_funcs.get_func(0x005F400E)
    if f:
        lines.append("ObjectClass::Update = %s [0x%08X..0x%08X]"
                     % (ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea))
        lines.append("")

    dis(0x005F3FC0, 0x005F4210, "ObjectClass::Update 内 Submit/Remove 配对上下文")

    find_writes_to_90()

    text = "\n".join(lines)
    with open(OUT_FILE, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT_FILE, len(text))


main()
ida_pro.qexit(0)
