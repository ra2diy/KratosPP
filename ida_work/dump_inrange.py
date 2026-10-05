# -*- coding: utf-8 -*-
# 反汇编 TechnoClass::In_WeaponRange (0x7012DF 附近) 与 In_Range(0x6F72E3)，
# 确认 Kratos Hook 读写寄存器是否与引擎用法一致。
import idaapi, idc, idautils, ida_hexrays

OUT = r"D:/Workspace/ra2mod/platform/KratosPP/ida_work/inrange_disasm.txt"

TARGETS = [
    (0x701260, 0x7013A0, "TechnoClass::In_WeaponRange 区域"),
    (0x6F72C0, 0x6F7340, "TechnoClass::In_Range 区域(另一Hook 0x6F72E3)"),
    (0x6F9000, 0x6F9050, "TechnoClass::Greatest_Threat 相关(0x6F9039)"),
]

def main():
    lines = []
    for start, end, title in TARGETS:
        lines.append("=" * 100)
        lines.append("### %s  [0x%X - 0x%X]" % (title, start, end))
        lines.append("=" * 100)
        # 找函数起点
        f = idaapi.get_func(start)
        if f:
            lines.append("函数: %s  起点=0x%X 终点=0x%X" % (idc.get_func_name(f.start_ea), f.start_ea, f.end_ea))
        else:
            lines.append("(未识别为函数)")
        lines.append("")
        ea = start
        while ea < end:
            dis = idc.generate_disasm_line(ea, 0)
            lines.append("0x%08X  %-40s ; %s" % (ea, dis, idc.get_cmt(ea, 0) or ""))
            ea = idc.next_head(ea, end)
        lines.append("")
        # 反编译
        if f:
            lines.append("----- Hex-Rays -----")
            try:
                cf = ida_hexrays.decompile(f.start_ea)
                if cf:
                    txt = str(cf)
                    lines.append(txt)
                else:
                    lines.append("(decompile 返回 None)")
            except Exception as e:
                lines.append("(decompile 失败: %s)" % e)
        lines.append("")

    # 交叉引用: 谁调用 In_WeaponRange
    lines.append("=" * 100)
    lines.append("### XrefsTo 0x7012DF / 0x6F72E3 / 0x6F9039")
    lines.append("=" * 100)
    for a in (0x7012DF, 0x6F72E3, 0x6F9039):
        lines.append("--- Xrefs to 0x%X ---" % a)
        for x in idautils.XrefsTo(a, 0):
            lines.append("   来自 0x%08X  (%s)" % (x.frm, idc.get_func_name(x.frm)))
        lines.append("")

    with open(OUT, "w", encoding="utf-8") as fp:
        fp.write("\n".join(lines))

main()
idc.qexit(0)
