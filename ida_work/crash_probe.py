"""崩溃现场反汇编核实（2026-10-04 22:57 crash）。

目标：
  A. 校验 Kratos 新 hook 落点 0x4A975E 的指令边界与寄存器语义（ESI/EDI）。
  B. 解析崩溃栈里的代码地址属于哪个函数。
  C. 给出 0x4A9750..0x4A9770 的逐字节视图，确认没有和其它 hook 区间重叠。
"""

import os

import ida_auto
import ida_bytes
import ida_funcs
import ida_lines
import ida_pro
import idc


HERE = os.path.dirname(os.path.abspath(__file__))
OUT_FILE = os.path.join(HERE, "crash_probe.txt")

# 崩溃栈里出现的代码地址 / 关键地址
STACK_EAS = [
    0x1BF09EEB,   # EIP（不在 gamemd 内，应为被调坏的指针）
    0x006D9A7E,   # EAX
    0x006D40F3,   # 栈 0x1AD170
    0x004F44F9,   # 栈 0x1AD288  DrawOnTop 内
    0x0055D8F7,   # 栈 0x1AD2AC  Main_Loop 内
    0x0055D059,   # 栈 0x1AD3C8
    0x004AEB02,   # 栈 0x1AD454
    0x0054F76D,   # 栈 0x1AD464
    0x00684282,   # 栈 0x1AD46C
    0x0069BB2F,   # 栈 0x1AD470
    0x0048CE8F,   # 栈 0x1AD474
    0x0048CDA8,   # ExceptionReturnAddress
    0x007E85D4,   # 栈 0x1AD1FC
    0x004BB0C2,   # 栈 0x1AD200
    0x007B9C1B,   # 栈 0x1AD42C
]

# 关键函数起点
FUNC_EAS = [
    0x004A9720,   # DisplayClass::Submit
    0x006D3D10,   # TacticalClass::Draw
    0x006D8DB0,   # Tactical_Draw_All
    0x006D9A50,   # 读层数组的只读查询
    0x004F4480,   # GScreenClass::DrawOnTop
    0x0055D360,   # W?Main_Loop
    0x004DA870,   # FootClass::Update 附近
]


def disasm_range(lines, lo, hi, mark=None):
    addr = lo
    while addr < hi:
        line = ida_lines.generate_disasm_line(addr, 0)
        if not line:
            break
        b = ida_bytes.get_bytes(addr, 8)
        hexs = " ".join("%02X" % c for c in b) if b else "?"
        flag = ">>" if (mark is not None and addr == mark) else "  "
        lines.append("%s %08X: %-32s | %s" % (flag, addr, hexs, line))
        nxt = idc.next_head(addr, hi)
        if nxt <= addr:
            break
        addr = nxt


def describe(lines, ea):
    lines.append("=" * 108)
    f = ida_funcs.get_func(ea)
    if not f:
        lines.append("0x%08X  -> 不在任何函数内（数据或未定义）" % ea)
        # 打印附近字节
        b = ida_bytes.get_bytes(ea, 16)
        lines.append("    bytes: %s" % (" ".join("%02X" % c for c in b) if b else "?"))
        return
    name = ida_funcs.get_func_name(f.start_ea)
    lines.append("0x%08X  ->  %s  [0x%08X-0x%08X]  offset=+0x%X"
                 % (ea, name, f.start_ea, f.end_ea, ea - f.start_ea))
    lo = max(f.start_ea, ea - 0x70)
    hi = min(f.end_ea, ea + 0x70)
    lines.append("  -- disasm 0x%08X..0x%08X --" % (lo, hi))
    disasm_range(lines, lo, hi, mark=ea)
    lines.append("")


def main():
    ida_auto.auto_wait()
    lines = []

    lines.append("#" * 108)
    lines.append("A. 新 hook 落点 0x4A975E 附近逐字节（DisplayClass::Submit 内）")
    lines.append("#" * 108)
    f = ida_funcs.get_func(0x4A9720)
    if f:
        lines.append("DisplayClass::Submit: %s [0x%08X-0x%08X]"
                     % (ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea))
        disasm_range(lines, 0x4A9720, min(f.end_ea, 0x4A97A0), mark=0x4A975E)
    else:
        lines.append("!! 0x4A9720 不是函数起点")
    lines.append("")
    lines.append("-- 0x4A9740..0x4A9778 逐字节 --")
    for base in range(0x4A9740, 0x4A9778, 16):
        b = ida_bytes.get_bytes(base, 16)
        lines.append("  %08X: %s" % (base, " ".join("%02X" % c for c in b) if b else "?"))
    lines.append("")

    lines.append("#" * 108)
    lines.append("B. 崩溃栈代码地址归属")
    lines.append("#" * 108)
    for ea in STACK_EAS:
        describe(lines, ea)

    lines.append("#" * 108)
    lines.append("C. 关键函数起点")
    lines.append("#" * 108)
    for ea in FUNC_EAS:
        f = ida_funcs.get_func(ea)
        if f:
            lines.append("0x%08X -> %s [0x%08X-0x%08X]  (size=0x%X)"
                         % (ea, ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea,
                            f.end_ea - f.start_ea))
        else:
            lines.append("0x%08X -> ???" % ea)
    lines.append("")

    lines.append("#" * 108)
    lines.append("D. TacticalClass::Draw 附近调用点 0x6D40EE（栈 0x1AD170 的返回地址来源）")
    lines.append("#" * 108)
    describe(lines, 0x6D40F3)
    describe(lines, 0x6D40E0)

    text = "\n".join(lines)
    with open(OUT_FILE, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT_FILE, len(text))


main()
ida_pro.qexit(0)
