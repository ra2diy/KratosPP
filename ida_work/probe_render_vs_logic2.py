# -*- coding: utf-8 -*-
"""第二轮：把第一轮剩下的疑点钉死（只读 + 写报告）。

  R1  dword_A8EDA0 / arg_ATTRACT_Flags / GameInFocus 的写入者（确证“跳过渲染”的触发源）
  R2  sub_55E160 / sub_48C8B0 / sub_647260 —— 帧末尾这些函数会不会再渲染一次
  R3  Call_Back / Networking_RespondToEvent —— 帧内的网络路径会不会中途渲染
  R4  ObjectClass_Update 里的 Submit/Remove（层数组每帧维护点）
  R5  0x9C 是不是 Location：ObjectClass::GetCoords / SetLocation 反汇编取证 + 结构体成员
  R6  sub_64DAB0 剩余部分：Z 分量到底进不进哈希
"""
import idaapi, idc, idautils, ida_funcs, ida_bytes, ida_auto, ida_pro, ida_segment, ida_name, ida_typeinf
import traceback

try:
    import ida_hexrays
    HAVE_HR = ida_hexrays.init_hexrays_plugin()
except Exception:
    HAVE_HR = False

report = []
def log(s=""):
    report.append(str(s))

def nm(ea):
    n = idc.get_name(ea)
    return n if n else ("sub_%X" % ea if ea else "?")

def fname(ea):
    f = ida_funcs.get_func(ea)
    return nm(f.start_ea) if f else "?"

def dis(ea):
    try:
        return idc.generate_disasm_line(ea, 0)
    except Exception:
        return "<disasm failed>"

def dump(ea_start, ea_end, label="", limit=500):
    log("---- %s  [%08X - %08X] ----" % (label, ea_start, ea_end))
    cur = ea_start
    n = 0
    while cur is not None and cur != idaapi.BADADDR and cur < ea_end and n < limit:
        log("   %08X  %s" % (cur, dis(cur)))
        nxt = idc.next_head(cur, ea_end)
        if nxt is None or nxt == idaapi.BADADDR or nxt <= cur:
            break
        cur = nxt
        n += 1

def dump_func(ea, label="", limit=500):
    f = ida_funcs.get_func(ea)
    if not f:
        log("---- %s: no function at %08X" % (label, ea))
        return
    dump(f.start_ea, f.end_ea, "%s = %s" % (label, nm(f.start_ea)), limit)

def decomp(ea, label="", trunc=0):
    log("")
    log("#### PSEUDOCODE %s @ %08X (%s) ####" % (label, ea, fname(ea)))
    if not HAVE_HR:
        log("   <hexrays unavailable>")
        return
    f = ida_funcs.get_func(ea)
    if not f:
        log("   <no function>")
        return
    try:
        cf = ida_hexrays.decompile(f.start_ea)
        if cf is None:
            log("   <decompile returned None>")
            return
        txt = str(cf)
    except Exception as e:
        log("   <decompile failed: %r>" % e)
        return
    lines = txt.splitlines()
    for l in (lines[:trunc] if trunc else lines):
        log("   " + l)
    if trunc and len(lines) > trunc:
        log("   ... (%d more lines)" % (len(lines) - trunc))

def xrefs_of(ea, label="", ctx=8):
    log("")
    log("---- xrefs of %s (%08X) = %s ----" % (label, ea, nm(ea)))
    for x in idautils.XrefsTo(ea, 0):
        kind = "CODE" if x.iscode else "DATA"
        log("   %s %08X type=%d in %s : %s" % (kind, x.frm, x.type, fname(x.frm), dis(x.frm)))
        if x.iscode:
            # 前后若干条，看条件
            f = ida_funcs.get_func(x.frm)
            if f:
                a = x.frm
                for _ in range(6):
                    p = idc.prev_head(a, f.start_ea)
                    if p is None or p == idaapi.BADADDR or p <= f.start_ea:
                        break
                    a = p
                dump(a, x.frm + 1, "  ctx around %08X" % x.frm, 20)

def find_names(sub):
    out = []
    for ea, n in idautils.Names():
        if sub.lower() in n.lower():
            out.append((ea, n))
    return out

try:
    ida_auto.auto_wait()
    log("idb = %s   hexrays=%s" % (idc.get_idb_path(), HAVE_HR))

    # ---------- R1 跳过渲染的触发源 ----------
    log("=" * 100)
    log("R1) dword_A8EDA0 / arg_ATTRACT_Flags / GameInFocus / Scenario->field_62C 的读写点")
    log("=" * 100)
    for sym, addr in (("dword_A8EDA0", 0xA8EDA0), ("arg_ATTRACT_Flags", 0xA8D5F8)):
        xrefs_of(addr, sym, 6)
    gf = None
    for ea, n in idautils.Names():
        if n.lower() in ("gameinfocus", "_gameinfocus"):
            gf = ea
            break
    log("")
    log("  GameInFocus 地址 = %s" % (("%08X" % gf) if gf else "<未找到>"))
    if gf:
        xrefs_of(gf, "GameInFocus", 8)
    # Scenario 全局 + field_62C
    sc = None
    for ea, n in idautils.Names():
        if n == "Scenario":
            sc = ea
            break
    log("  Scenario 全局 = %s" % (("%08X" % sc) if sc else "?"))
    if sc:
        xrefs_of(sc + 0x62C, "Scenario->field_62C", 6)

    # ---------- R2 帧尾/帧首的几个函数 ----------
    log("")
    log("=" * 100)
    log("R2) sub_55E160 / sub_48C8B0 / sub_647260（帧内是否还有第二次渲染 / 校验值的调用条件）")
    log("=" * 100)
    for a, lab in ((0x55E160, "sub_55E160 (帧尾)"), (0x48C8B0, "sub_48C8B0 (帧循环每轮都调)"),
                   (0x647260, "sub_647260 (校验值驱动)")):
        dump_func(a, lab, 200)
        decomp(a, lab, 120)
        xrefs_of(a, lab, 0)

    # ---------- R3 帧内网络路径 ----------
    log("")
    log("=" * 100)
    log("R3) 网络路径是否会中途渲染：Call_Back / Networking_RespondToEvent")
    log("=" * 100)
    for tgt in find_names("Networking_RespondToEvent") + find_names("Call_Back"):
        log("  符号 %08X %s" % (tgt[0], tgt[1]))
        for x in idautils.XrefsTo(tgt[0], 0):
            log("     <- %08X in %s : %s" % (x.frm, fname(x.frm), dis(x.frm)))
    cb = None
    for ea, n in idautils.Names():
        if n in ("Call_Back", "W?Call_Back$n()v", "_Call_Back"):
            cb = ea
            break
    log("  Call_Back = %s" % (("%08X" % cb) if cb else "?"))
    if cb:
        decomp(cb, "Call_Back", 80)
        # Call_Back 一级被调函数里有没有 DrawOnTop
        f = ida_funcs.get_func(cb)
        if f:
            cur = f.start_ea
            while cur < f.end_ea:
                d = dis(cur)
                if "call" in d:
                    try:
                        t = idc.get_operand_value(cur, 0)
                    except Exception:
                        t = 0
                    log("     %08X %s -> %s" % (cur, d, nm(t) if t else ""))
                nxt = idc.next_head(cur, f.end_ea)
                if nxt is None or nxt == idaapi.BADADDR or nxt <= cur:
                    break
                cur = nxt

    # ---------- R4 层数组每帧维护点 ----------
    log("")
    log("=" * 100)
    log("R4) ObjectClass_Update 0x5F3??? 里的 Submit/Remove（层数组每帧维护点）")
    log("=" * 100)
    f = ida_funcs.get_func(0x5F400E)
    if f:
        log("  Submit 调用点 0x5F400E / Remove 调用点 0x5F414C 所在函数 = %s [%08X-%08X]" % (
            nm(f.start_ea), f.start_ea, f.end_ea))
        dump(0x5F3FA0, 0x5F41C0, "ObjectClass_Update Submit/Remove 区段", 200)
        decomp(f.start_ea, "ObjectClass_Update", 140)
    for a in (0x5F4184, 0x5F4196):
        log("  附：0x%08X 在 %s" % (a, fname(a)))

    # ---------- R5 0x9C 是不是 Location ----------
    log("")
    log("=" * 100)
    log("R5) 0x9C 的字段身份：GetCoords / SetLocation / 结构体成员")
    log("=" * 100)
    for sub in ("GetCoords", "SetLocation", "Set_Location", "Get_Coords"):
        for ea, n in find_names(sub):
            log("  NAME %08X %s" % (ea, n))
    # 结构体成员
    til = idaapi.get_idati()
    for sname in ("ObjectClass", "TechnoClass", "FootClass", "AbstractClass"):
        tif = ida_typeinf.tinfo_t()
        try:
            res = ida_typeinf.get_named_type(til, sname, ida_typeinf.NTF_TYPE)
            if isinstance(res, tuple):
                tif = res[0]
        except Exception as e:
            log("   [%s] 查询失败 %r" % (sname, e))
            continue
        if tif is None or tif.empty():
            log("   [%s] idb 没有该结构体" % sname)
            continue
        log("   [%s] %s  (size=0x%X)" % (sname, tif._print(), tif.get_size()))
        try:
            udt = ida_typeinf.udt_type_data_t()
            if tif.get_udt_details(udt):
                for i in range(udt.size()):
                    m = udt[i]
                    off = m.offset // 8
                    if 0x60 <= off <= 0xC0:
                        log("        +0x%03X  %-30s %s" % (off, m.name, m.type._print()))
        except Exception as e:
            log("        <udt 枚举失败 %r>" % e)
    # 直接反汇编 GetCoords / SetLocation
    for sub in ("GetCoords", "SetLocation"):
        for ea, n in find_names(sub)[:4]:
            dump_func(ea, n, 40)

    # ---------- R6 Z 分量 ----------
    log("")
    log("=" * 100)
    log("R6) sub_64DAB0 剩余部分（Z 分量 var_4 到底用没用）")
    log("=" * 100)
    dump(0x64DD5F, 0x64DE80, "sub_64DAB0 after field read", 200)
    f = ida_funcs.get_func(0x64DAB0)
    if f:
        hits = 0
        cur = f.start_ea
        while cur < f.end_ea:
            d = dis(cur)
            if "var_4]" in d or "var_4 " in d:
                log("   [var_4 引用] %08X  %s" % (cur, d))
                hits += 1
            nxt = idc.next_head(cur, f.end_ea)
            if nxt is None or nxt == idaapi.BADADDR or nxt <= cur:
                break
            cur = nxt
        log("   var_4 相关指令数 = %d" % hits)
    log("")
    log("  -- dword_AC51FC（校验值累加器）的引用点 --")
    xrefs_of(0xAC51FC, "dword_AC51FC", 4)

    # ---------- R7 TacticalClass_Draw -> Tactical_Draw_All ----------
    log("")
    log("=" * 100)
    log("R7) TacticalClass_Draw (0x6D3D10) 是否调用 Tactical_Draw_All (0x6D8DB0)")
    log("=" * 100)
    for x in idautils.XrefsTo(0x6D8DB0, 0):
        log("   <- %08X in %s : %s" % (x.frm, fname(x.frm), dis(x.frm)))
    log("")
    log("  -- 谁调用 sub_551A30 (LayerClass::Sort) --")
    for x in idautils.XrefsTo(0x551A30, 0):
        log("   <- %08X in %s : %s" % (x.frm, fname(x.frm), dis(x.frm)))

except Exception:
    log("")
    log("!!!! EXCEPTION !!!!")
    log(traceback.format_exc())
finally:
    open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\probe_render_vs_logic2.txt", "w",
         encoding="utf-8", errors="replace").write("\n".join(report))
    ida_pro.qexit(0)
