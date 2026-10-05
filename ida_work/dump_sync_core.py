import os

import ida_auto
import ida_hexrays
import ida_funcs
import ida_name
import ida_pro
import ida_bytes
import ida_ua
import idautils
import idc


OUT_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sync_core_dump.txt")

# 目标：YR 同步校验核心 + ActionClick 调用路径
FUNC_TARGETS = [
    0x64DAB0,   # sub_64DAB0 - 疑似同步 CRC / 每帧写入
    0x64CDA0,   # DoList 内调用的 sub_64CDA0
    0x64D990,
    0x64D9E0,
    0x7388B0,   # UnitClass::ActionClick 附近（Kratos hook 0x7388FD）
    0x6BEC60,   # Kratos hook 里 CALL 的函数
]

NAME_TARGETS = [
    "Multiplay_LogToSYNC_NOMPDEBUG",
    "Multiplay_LogToSync_MPDEBUG",
    "dword_B04474",
]


def ea_name(ea):
    return ida_name.get_name(ea) or ida_funcs.get_func_name(ea) or ("0x%X" % ea)


def func_text(f):
    try:
        cf = ida_hexrays.decompile(f.start_ea)
        return str(cf)
    except Exception as e:
        return "// decompile failed: %r" % e


def callers(start, max_depth=1):
    seen = set()
    out = []

    def rec(ea, depth):
        f = ida_funcs.get_func(ea)
        if not f:
            return
        if f.start_ea in seen:
            return
        seen.add(f.start_ea)
        out.append((f.start_ea, depth))
        if depth >= max_depth:
            return
        for xref in idautils.XrefsTo(f.start_ea, 0):
            if xref.iscode:
                rec(xref.frm, depth + 1)

    rec(start, 0)
    return out


def main():
    ida_auto.auto_wait()
    if not ida_hexrays.init_hexrays_plugin():
        print("hexrays init failed")
        return
    lines = []

    for name in NAME_TARGETS:
        ea = idc.get_name_ea_simple(name)
        lines.append("=" * 100)
        lines.append("NAMEREF %s => 0x%X" % (name, ea))
        if ea == idc.BADADDR:
            lines.append("  (not found by name)")
            continue
        cnt = 0
        for xref in idautils.XrefsTo(ea, 0):
            f = ida_funcs.get_func(xref.frm)
            lines.append("    xref from 0x%X (%s) type=%d" % (xref.frm, ea_name(f.start_ea) if f else "?", xref.type))
            cnt += 1
            if cnt > 60:
                lines.append("    ... (truncated)")
                break
    lines.append("")

    for ea in FUNC_TARGETS:
        f = ida_funcs.get_func(ea)
        if not f:
            lines.append("// NO FUNCTION at 0x%X" % ea)
            continue
        lines.append("=" * 100)
        lines.append("FUNC 0x%X in %s [0x%X - 0x%X]" % (ea, ea_name(f.start_ea), f.start_ea, f.end_ea))
        lines.append("Callers:")
        for cstart, depth in callers(f.start_ea, 1):
            lines.append("    %s depth=%d" % (ea_name(cstart), depth))
        lines.append("")
        lines.append(func_text(f))
        lines.append("")

    text = "\n".join(lines)
    with open(OUT_FILE, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT_FILE, len(text))


main()
ida_pro.qexit(0)
