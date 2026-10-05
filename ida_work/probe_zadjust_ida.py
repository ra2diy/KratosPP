# -*- coding: utf-8 -*-
"""查 TechnoClass::GetZAdjustment(0x4DAFC0) 的身份：所属虚表 + 槽偏移 + 引用者。

只读查询；try/finally 保证 qexit(0)，让 IDA 正常打包收尾。

用法：
  ida.exe -A -L<log> -S<this.py> "<正本 idb>"
"""
import traceback

import ida_auto
import ida_funcs
import ida_name
import ida_pro
import idautils
import idc

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\zadjust_ida_report.txt"

FN = 0x4DAFC0          # 疑似 TechnoClass::GetZAdjustment 的函数入口
SLOTS = [0x7E2590, 0x7E8F80, 0x7EB344, 0x7F5F5C]   # .rdata 中指向 FN 的 4 个槽


def vtable_base_of(slot):
    """从槽地址向前找 IDA 命名为 `??_7...` 的虚表起点。"""
    for k in range(0, 0x400):
        base = slot - k * 4
        nm = ida_name.get_name(base) or ""
        if nm.startswith("??_7"):
            return base, nm, k * 4
    return None, None, None


def main():
    ida_auto.auto_wait()
    lines = []

    def add(s):
        lines.append(s)

    try:
        f = ida_funcs.get_func(FN)
        add("函数 0x%08X 名 = %s（范围 0x%08X..0x%08X）"
            % (FN, ida_funcs.get_func_name(FN) if f else "(无)",
               f.start_ea if f else 0, f.end_ea if f else 0))
        add("hook 落点 0x4DB091 是否在本函数内: %s"
            % bool(f and f.start_ea <= 0x4DB091 < f.end_ea))
        add("")

        offsets = set()
        for slot in SLOTS:
            base, nm, off = vtable_base_of(slot)
            add("槽 0x%08X -> 虚表 %s @0x%08X  槽偏移 = +0x%X"
                % (slot, nm, base or 0, off or 0))
            if off:
                offsets.add(off)
        add("")
        add("★ 槽偏移集合 = %s" % ", ".join("+0x%X" % o for o in sorted(offsets)))
        add("")

        add("--- 谁引用该函数入口（XrefsTo 0x%08X）---" % FN)
        n = 0
        for x in idautils.XrefsTo(FN, 0):
            n += 1
            add("  0x%08X  type=%d  %s" % (x.frm, x.type, idc.generate_disasm_line(x.frm, 0) or ""))
            if n > 40:
                add("  ...（截断）")
                break
        add("共 %d 处" % n)
    except Exception as exc:                                     # noqa: BLE001
        add("FAILED: %r" % (exc,))
        add(traceback.format_exc())

    text = "\n".join(lines) + "\n"
    with open(OUT, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print(text)


try:
    main()
except Exception as exc:                                         # noqa: BLE001
    with open(OUT, "w", encoding="utf-8", errors="replace") as fp:
        fp.write("FAILED: %r\n%s\n" % (exc, traceback.format_exc()))
finally:
    ida_pro.qexit(0)
