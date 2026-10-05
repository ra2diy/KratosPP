# -*- coding: utf-8 -*-
# p22: 重新取证 FootClass::Mark(0x4D3780) / TechnoClass::Mark(0x6F4A70) /
#      ObjectClass::Mark(0x5F5850) / FootClass_SetCoords(0x4DB810) 的精确语义
import idaapi, idc, idautils, ida_bytes, ida_hexrays

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p22_mark.txt"
f = open(OUT, "w", encoding="utf-8")


def w(s=""):
    f.write(str(s) + "\n")
    f.flush()


def dis(ea, n=120):
    cur = ea
    for _ in range(n):
        w("   %08X  %s" % (cur, idc.generate_disasm_line(cur, 0)))
        nxt = idc.next_head(cur)
        if nxt == idaapi.BADADDR or nxt == cur:
            break
        cur = nxt


def decomp(ea):
    try:
        w(ida_hexrays.decompile(ea))
    except Exception as ex:
        w("decompile fail %08X: %r" % (ea, ex))


for ea, name in ((0x4D3780, "FootClass::Mark?"),
                 (0x6F4A70, "TechnoClass::Mark"),
                 (0x5F5850, "ObjectClass::Mark"),
                 (0x4DB810, "FootClass_SetCoords"),
                 (0x5F4D10, "TechnoClass_134")):
    try:
        w()
        w("=" * 70)
        w("== %s @ %08X  size=%d" % (name, ea, idc.get_func_attr(ea, idc.FUNCATTR_END) - ea))
        w("=" * 70)
        dis(ea, 140)
        w()
        w("--- decompile ---")
        decomp(ea)
    except Exception as ex:
        w("EXC %08X: %r" % (ea, ex))

# 谁调用 0x4D3780
try:
    w()
    w("== XrefsTo 0x4D3780 ==")
    for x in idautils.XrefsTo(0x4D3780):
        w("  %08X  from %08X %s" % (x.frm, x.frm, idc.get_func_name(x.frm)))
except Exception as ex:
    w("EXC xrefs: %r" % (ex,))

# MarkType 相关常量：搜 char* 或 enum 名
try:
    w()
    w("== IDB 中 MarkType / Mark 名称 ==")
    for nm in ("MarkType", "MarkType::Up", "MarkType::Down", "MarkType::Change", "_MarkType"):
        ea = idc.get_name_ea_simple(nm)
        w("  %-18s -> %s" % (nm, hex(ea) if ea != idaapi.BADADDR else "N/A"))
except Exception as ex:
    w("EXC nm: %r" % (ex,))

f.close()
print("DONE p22")
