# -*- coding: utf-8 -*-
"""只读查询持久 IDB 的命名/注释状态（不修改任何内容；末尾 qexit(0) 让 IDA 正常收尾）。

用法：
  ida.exe -A -Lida_work\ida_dbstate.log -Sida_work\check_db_state.py "<idb>"

注意：IDA 8.3 的 ida_idaapi 下没有 get_path / get_root_filename —— 要用 idc / ida_nalt。
"""
import os
import traceback

import ida_auto
import ida_funcs
import ida_hexrays
import ida_name
import ida_pro
import idc

try:
    import ida_nalt
except ImportError:                                              # pragma: no cover
    ida_nalt = None

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\db_state_report.txt"

PROBES = [
    (0x4A9720, "DisplayClass::Submit"),
    (0x4A9768, "Submit 公共出口（Kratos hook 落点）"),
    (0x424CB0, "AnimClass::In_Which_Layer"),
    (0x41ADC0, "TechnoClass::InWhichLayer / FootClass 版"),
    (0x70B570, "TechnoClass vt+41C 倾斜积分器"),
    (0x54B8D0, "JumpjetLocomotionClass::In_Which_Layer"),
    (0x4CFCF0, "FlyLocomotionClass::In_Which_Layer"),
    (0x739971, "annotate_ida.py 注释过的 hook 点"),
    (0x449C30, "BuildingClass::Mission_Deconstruction"),
]

CMT_PROBES = [
    (0x739971, "HOOK_DEPLOY（annotate 落点）"),
    (0x44A04C, "HOOK_UNDEPLOY（annotate 落点）"),
    (0x449FE7, "CALL_REMOVE（annotate 落点）"),
    (0x44A002, "CALL_PUT（annotate 落点）"),
]


def main():
    ida_auto.auto_wait()
    lines = []

    def add(msg):
        lines.append(msg)

    def safe(label, fn):
        try:
            add("%-10s = %s" % (label, fn()))
        except Exception as exc:                                 # noqa: BLE001
            add("%-10s = <失败: %r>" % (label, exc))

    safe("idb", idc.get_idb_path)
    if ida_nalt:
        safe("input", ida_nalt.get_root_filename)
    safe("hexrays", lambda: bool(ida_hexrays.init_hexrays_plugin()))

    total = named = subnamed = 0
    try:
        import idautils
        for ea in idautils.Functions():
            total += 1
            n = ida_funcs.get_func_name(ea) or ""
            if n.startswith("sub_") or n.startswith("nullsub_"):
                subnamed += 1
            elif n:
                named += 1
        add("functions  = %d（有实义名 %d / 自动名 sub_* %d）" % (total, named, subnamed))
    except Exception as exc:                                     # noqa: BLE001
        add("functions  = <失败: %r>" % (exc,))

    add("")
    add("--- 关键地址命名 ---")
    for ea, why in PROBES:
        try:
            f = ida_funcs.get_func(ea)
            fname = ida_funcs.get_func_name(f.start_ea) if f else "(无函数)"
            add("0x%08X  %-46s  func=%-42s  name@ea=%s"
                % (ea, why, fname, ida_name.get_name(ea) or "(无)"))
        except Exception as exc:                                 # noqa: BLE001
            add("0x%08X  %-46s  <失败: %r>" % (ea, why, exc))

    add("")
    add("--- 注释存在性（判断 annotate_ida.py 是否在本 DB 上跑过）---")
    for ea, why in CMT_PROBES:
        try:
            cmt = idc.get_cmt(ea, 1) or idc.get_cmt(ea, 0) or ""
            add("0x%08X  %-26s cmt_len=%d  %s" % (ea, why, len(cmt), cmt[:60].replace("\n", " | ")))
        except Exception as exc:                                 # noqa: BLE001
            add("0x%08X  %-26s <失败: %r>" % (ea, why, exc))
    try:
        f = ida_funcs.get_func(0x7393C0)
        fcmt = idc.get_func_cmt(f.start_ea, 1) if f else None
        add("func_cmt @0x7393C0 len=%d  %s"
            % (len(fcmt) if fcmt else 0, (fcmt or "")[:60].replace("\n", " | ")))
    except Exception as exc:                                     # noqa: BLE001
        add("func_cmt @0x7393C0 <失败: %r>" % (exc,))

    text = "\n".join(lines) + "\n"
    with open(OUT, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print(text)


try:
    main()
except Exception as exc:                                         # noqa: BLE001
    with open(OUT, "w", encoding="utf-8", errors="replace") as fp:
        fp.write("FAILED: %r\n%s\n" % (exc, traceback.format_exc()))
    print("FAILED: %r" % (exc,))
finally:
    ida_pro.qexit(0)
