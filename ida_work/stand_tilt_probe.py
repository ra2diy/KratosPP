# -*- coding: utf-8 -*-
# 替身抖动/倾斜取证：渲染时序、GetRenderCoords vs GetCoords、引擎 rocking 实现
import os
import struct

import ida_auto
import ida_bytes
import ida_funcs
import ida_lines
import ida_name
import ida_pro
import idc

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "stand_tilt_probe.txt")
lines = []


def w(s=""):
    lines.append(s)


def dis(start, end, title, marks=(), ctx_note=None):
    w("== %s  0x%08X..0x%08X ==" % (title, start, end))
    if ctx_note:
        w("   # %s" % ctx_note)
    ea = start
    n = 0
    while ea < end and n < 400:
        line = ida_lines.generate_disasm_line(ea, 0)
        if not line:
            break
        b = ida_bytes.get_bytes(ea, 8)
        hexs = " ".join("%02X" % c for c in b) if b else "?"
        m = ">>" if ea in marks else "  "
        w("%s %08X: %-28s | %s" % (m, ea, hexs, ida_lines.tag_remove(line)))
        nxt = idc.next_head(ea)
        if nxt <= ea:
            break
        ea = nxt
        n += 1
    w("")


def func_of(ea):
    f = ida_funcs.get_func(ea)
    if f:
        return "func %s [0x%08X..0x%08X]" % (ida_funcs.get_func_name(f.start_ea), f.start_ea, f.end_ea)
    return "(no func)"


def find_bytes(pattern, limit=40):
    """pattern 形如 'DB 0F 49 3F'，线性搜索 .text"""
    res = []
    ea = 0x401000
    end = 0x8A0000
    pat = bytes(int(x, 16) for x in pattern.split())
    try:
        import ida_search
        cur = ea
        while len(res) < limit:
            hit = ida_search.find_binary(cur, end, " ".join("%02X" % c for c in pat), 16,
                                         ida_search.SEARCH_DOWN)
            if hit == idc.BADADDR or hit is None:
                break
            res.append(hit)
            cur = hit + 1
    except Exception as e:
        w("find_binary err: %s" % e)
    return res


def main():
    ida_auto.auto_wait()

    # ---------- A. 名字库检索 ----------
    w("############ A. 名字库检索 ############")
    keys = ["RenderCoords", "GetCoords", "YSort", "Draw_Matrix", "DrawMatrix", "Sinking",
            "Rock", "Tilt", "LocomotionClass", "Submit", "ZAdjust", "GetHeight"]
    n = ida_name.get_nlist_size()
    hits = []
    for i in range(n):
        nm = ida_name.get_nlist_name(i)
        if not nm:
            continue
        for k in keys:
            if k.lower() in nm.lower():
                hits.append((ida_name.get_nlist_ea(i), nm, k))
                break
    hits.sort()
    w("  共 %d 条（名字库总数 %d）" % (len(hits), n))
    for ea, nm, k in hits[:300]:
        w("  %08X  [%s] %s" % (ea, k, nm))
    w("")

    # ---------- B. 关键函数归属 ----------
    w("############ B. 关键地址归属 ############")
    for a, nm in ((0x4F4480, "GScreenClass_DrawOnTop"), (0x4F4497, "GScreen_Render_EventHook"),
                  (0x4F4583, "GScreen_Render_Late"), (0x6D3D10, "TacticalClass_Draw"),
                  (0x5F6BD0, "ObjectClass_GetYSort"), (0x5F6BF7, "GetYSort_Patch"),
                  (0x4DB091, "FootClass_GetZAdjustment"), (0x551A30, "LayerClass_Sort"),
                  (0x55D360, "Main_Loop"), (0x55DBBE, "call_DrawOnTop"),
                  (0x55DC9E, "call_LogicClass_Update")):
        w("  0x%08X  %-28s -> %s" % (a, nm, func_of(a)))
    w("")

    # ---------- C. DrawOnTop 渲染时序 ----------
    w("############ C. GScreenClass::DrawOnTop 渲染时序（确认 AE 渲染事件在 TacticalClass::Draw 之前）############")
    dis(0x4F4480, 0x4F45A0, "GScreenClass_DrawOnTop", marks=(0x4F4497, 0x4F44F9, 0x4F4583))

    # ---------- D. DoList / Main_Loop 帧内顺序 ----------
    w("############ D. Main_Loop 帧内顺序 0x55DBA0..0x55DCC0 ############")
    dis(0x55DBA0, 0x55DCC0, "Main_Loop_frame_order", marks=(0x55DBBE, 0x55DBC3, 0x55DBC8, 0x55DC9E))

    # ---------- E. GetYSort 与 GetRenderCoords ----------
    w("############ E. ObjectClass::GetYSort 0x5F6BD0..0x5F6C20 ############")
    dis(0x5F6BD0, 0x5F6C20, "GetYSort", marks=(0x5F6BF7,))

    # ---------- F. 浮点常量检索：rocking 实现 ----------
    w("############ F. 浮点常量检索 ############")
    for name, pat in (("PI/2 = 1.5707964 (0x3FC90FDB)", "DB 0F C9 3F"),
                      ("PI/4 = 0.7853982 (0x3F490FDB)", "DB 0F 49 3F"),
                      ("1.0f = 0x3F800000", "00 00 80 3F")):
        hits = find_bytes(pat, 40)
        w("  --- %s：命中 %d 处 ---" % (name, len(hits)))
        for h in hits[:24]:
            w("      0x%08X  %s" % (h, func_of(h)))
    w("")

    w("############ G. PI/4 与 PI/2 常量所在函数的前后 60 条指令 ############")
    cand = find_bytes("DB 0F 49 3F", 8) + find_bytes("DB 0F C9 3F", 8)
    seen = set()
    for h in cand[:10]:
        f = ida_funcs.get_func(h)
        if not f:
            continue
        if f.start_ea in seen:
            continue
        seen.add(f.start_ea)
        w("  === 引用点 0x%08X  %s ===" % (h, func_of(h)))
        s = max(f.start_ea, h - 0x120)
        e = min(f.end_ea, h + 0x180)
        dis(s, e, "around_%08X" % h, marks=(h,))
    w("")

    text = "\n".join(lines)
    with open(OUT, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT, len(text))


main()
ida_pro.qexit(0)
