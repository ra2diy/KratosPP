# -*- coding: utf-8 -*-
# 探针 3：vtable 槽位（GetCoords / GetRenderCoords）、Draw_Matrix 实现、FootClass::Crash
import os

import ida_auto
import ida_bytes
import ida_funcs
import ida_lines
import ida_name
import ida_pro
import idc

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "stand_tilt_probe3.txt")
lines = []


def w(s=""):
    lines.append(s)


def dis(start, end, title, marks=(), maxn=700):
    w("== %s  0x%08X..0x%08X ==" % (title, start, end))
    ea = start
    n = 0
    while ea < end and n < maxn:
        line = ida_lines.generate_disasm_line(ea, 0)
        if not line:
            break
        b = ida_bytes.get_bytes(ea, 8)
        hexs = " ".join("%02X" % c for c in b) if b else "?"
        m = ">>" if ea in marks else "  "
        w("%s %08X: %-24s | %s" % (m, ea, hexs, ida_lines.tag_remove(line)))
        nxt = idc.next_head(ea)
        if nxt <= ea:
            break
        ea = nxt
        n += 1
    w("")


def main():
    ida_auto.auto_wait()

    # ---------- A. 找 vtable ----------
    w("############ A. 目标 vtable ############")
    wanted = ["TechnoClass", "FootClass", "ObjectClass", "AnimClass", "UnitClass",
              "InfantryClass", "AircraftClass", "BuildingClass"]
    found = {}
    n = ida_name.get_nlist_size()
    names = []
    for i in range(n):
        nm = ida_name.get_nlist_name(i)
        if nm and nm.startswith("??_7") and "$" not in nm and "@@6B@" in nm:
            names.append((ida_name.get_nlist_ea(i), nm))
    names.sort()
    for ea, nm in names:
        for k in wanted:
            if nm == "??_7%s@@6B@" % k:
                found[k] = ea
    for k in wanted:
        w("  %-14s vtable = %s" % (k, ("0x%08X" % found[k]) if k in found else "(未找到)"))
    w("")
    for k in ("ObjectClass", "TechnoClass", "FootClass", "UnitClass", "InfantryClass",
              "AircraftClass", "AnimClass"):
        vt = found.get(k)
        if not vt:
            continue
        w("  === %s vtable @0x%08X ===" % (k, vt))
        for i in range(0x3C, 0x120, 4):
            tgt = ida_bytes.get_dword(vt + i)
            if tgt == 0:
                continue
            f = ida_funcs.get_func(tgt)
            fname = ida_funcs.get_func_name(f.start_ea) if f else ""
            mark = "   <<<<" if i in (0x48, 0xA0, 0xAC, 0xB0, 0xB4, 0xB8, 0xBC, 0x104, 0x110) else ""
            w("      vt+0x%03X -> 0x%08X  %s%s" % (i, tgt, fname, mark))
        w("")

    # ---------- B. Draw_Matrix 公共实现 ----------
    w("############ B. LocomotionClass::ILocomotion::Draw_Matrix (0x55A730) ############")
    f = ida_funcs.get_func(0x55A730)
    if f:
        dis(f.start_ea, f.end_ea, "LocomotionClass_ILocomotion_DrawMatrix")
    w("############ B2. LocomotionClass_ILocomotion_ZAdjust / TiltPitchAI ############")
    for a in (0x55AB90, 0x55ABA0):
        f = ida_funcs.get_func(a)
        if f:
            dis(f.start_ea, f.end_ea, "loco_%08X" % a)

    # ---------- C. FootClass::Crash（坠落/下沉倾斜） ----------
    w("############ C. FootClass::Crash 0x4DEC80..0x4DED60 ############")
    dis(0x4DEC60, 0x4DED70, "FootClass_Crash", marks=(0x4DECF0, 0x4DED0B, 0x4DED13))

    # ---------- D. IonBlastClass_Update 中的倾斜（对照） ----------
    w("############ D. IonBlast 的倾斜写法（对照，0x53D290..0x53D2E0） ############")
    dis(0x53D290, 0x53D2E0, "IonBlast_sinking_tilt", marks=(0x53D2B7, 0x53D2CB))

    text = "\n".join(lines)
    with open(OUT, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT, len(text))


main()
ida_pro.qexit(0)
