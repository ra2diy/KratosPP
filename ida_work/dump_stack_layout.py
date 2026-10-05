# -*- coding: utf-8 -*-
# 精确反汇编 Can_Fire(0x6FC0B0) 与 SelectWeapon(0x6F3330) 的函数序言与栈布局，
# 以便核对 Kratos Hook 里的 GET_STACK 偏移是否正确。
import idaapi, idc, idautils, ida_hexrays

OUT = r"D:/Workspace/ra2mod/platform/KratosPP/ida_work/stack_layout.txt"

REGIONS = [
    (0x6FC0B0, 0x6FC360, "TechnoClass::Can_Fire 序言+到Hook点"),
    (0x6F3330, 0x6F3400, "TechnoClass::SelectWeapon 序言"),
    (0x6F3400, 0x6F3480, "TechnoClass::SelectWeapon 中段"),
    (0x5F4520, 0x5F45B0, "ObjectClass::Select 序言+到Hook点"),
    (0x55B710, 0x55B760, "LogicClass_Update_Late Hook 0x55B719 附近"),
]

# 需要检查栈引用的地址（Hook 点）
HOOKPTS = [0x6FC339, 0x6F36DB, 0x5F45A0, 0x55B719]


def main():
    lines = []
    for start, end, title in REGIONS:
        lines.append("=" * 110)
        lines.append("### %s  [0x%X - 0x%X]" % (title, start, end))
        lines.append("=" * 110)
        f = idaapi.get_func(start)
        if f:
            lines.append("函数: %s  0x%X - 0x%X" % (idc.get_func_name(f.start_ea), f.start_ea, f.end_ea))
        ea = start
        while ea < end:
            dis = idc.generate_disasm_line(ea, 0)
            mark = ""
            for hp in HOOKPTS:
                if ea == hp:
                    mark = "   <<<<<< HOOK 点"
            lines.append("  0x%08X  %-46s%s" % (ea, dis, mark))
            nxt = idc.next_head(ea, end)
            if nxt == idaapi.BADADDR or nxt <= ea:
                break
            ea = nxt
        lines.append("")

    # Can_Fire 里所有 [esp+..] 引用的汇总，帮助定位 pTarget
    lines.append("=" * 110)
    lines.append("### Can_Fire(0x6FC0B0-0x6FCD38) 全部栈引用")
    lines.append("=" * 110)
    f = idaapi.get_func(0x6FC0B0)
    if f:
        ea = f.start_ea
        while ea < f.end_ea:
            dis = idc.generate_disasm_line(ea, 0)
            if "esp+" in dis or "esp-" in dis or "ebp+" in dis or "ebp-" in dis:
                lines.append("  0x%08X  %s" % (ea, dis))
            nxt = idc.next_head(ea, f.end_ea)
            if nxt == idaapi.BADADDR or nxt <= ea:
                break
            ea = nxt
    lines.append("")

    # SelectWeapon 全部栈引用
    lines.append("=" * 110)
    lines.append("### SelectWeapon(0x6F3330-0x6F3816) 栈引用（前 60 条）")
    lines.append("=" * 110)
    f = idaapi.get_func(0x6F3330)
    if f:
        ea = f.start_ea
        c = 0
        while ea < f.end_ea and c < 60:
            dis = idc.generate_disasm_line(ea, 0)
            if "esp+" in dis or "esp-" in dis or "ebp+" in dis or "ebp-" in dis:
                lines.append("  0x%08X  %s" % (ea, dis))
                c += 1
            nxt = idc.next_head(ea, f.end_ea)
            if nxt == idaapi.BADADDR or nxt <= ea:
                break
            ea = nxt
    lines.append("")

    with open(OUT, "w", encoding="utf-8") as fp:
        fp.write("\n".join(lines))


main()
idc.qexit(0)
