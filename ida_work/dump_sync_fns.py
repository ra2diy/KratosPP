import os

import ida_auto
import ida_hexrays
import ida_funcs
import ida_name
import ida_pro
import idautils
import idc


OUT_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sync_fns_dump.txt")

# 关注点：YR 联机同步/失步检测 + 随机数 + 命令队列
TARGETS = [
    0x64736D,   # Kratos Queue_AI_WriteDesyncLog (失步写入点)
    0x64CD11,   # Kratos ExecuteDoList_WriteDesyncLog
    0x65C7D0,   # Random2Class::Random
    0x65C88A,   # Random2Class::RandomRanged
    0x55AFB3,   # LogicClass::Update
    0x55B719,   # LogicClass::Update_Late
    0x6F9B7E,   # TechnoClass::SelectAutoTarget
    0x6FF08B,   # TechnoClass::Fire 内部
    0x7013A0,   # TechnoClass::OverrideMission
    0x4D8F40,   # FootClass::OverrideMission
    0x41BB30,   # AircraftClass::OverrideMission
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
    for ea in TARGETS:
        f = ida_funcs.get_func(ea)
        if not f:
            lines.append("// NO FUNCTION at 0x%X" % ea)
            continue
        lines.append("=" * 100)
        lines.append("TARGET 0x%X in %s [0x%X - 0x%X]" % (ea, ea_name(f.start_ea), f.start_ea, f.end_ea))
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
