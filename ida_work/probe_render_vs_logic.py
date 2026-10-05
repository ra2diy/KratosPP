# -*- coding: utf-8 -*-
"""实机核对：GScreenClass::Render(0x4F4497) / Render_Late(0x4F4583) 与逻辑帧的关系。

要证的点（只读 + 写报告，不改 mod 源码）：
  Q1 0x4F4497 在主循环里被调用的频率与条件（是否每逻辑帧一次；有没有分支跳过它）
  Q2 sub_64DAB0（校验值哈希）读的是不是 Location；vec_ObjectsInLayers 每帧何时重建
  Q3 0x4F4497 在 Draw -> Sort -> Logic -> hash 序列里的相对位置

写法参照 ida_work/probe_event_queue.py。IDA 8.3 API 差异已按任务说明处理。
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

def fof(ea):
    return ida_funcs.get_func(ea)

def fname(ea):
    f = ida_funcs.get_func(ea)
    return nm(f.start_ea) if f else "?"

def dis(ea):
    try:
        return idc.generate_disasm_line(ea, 0)
    except Exception:
        return "<disasm failed>"

def dump(ea_start, ea_end, label="", limit=600):
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
    if n >= limit:
        log("   ... (truncated at %d insns)" % limit)

def dump_func(ea, label="", limit=600):
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
        txt = str(cf) if cf else "<null>"
    except Exception as e:
        log("   <decompile failed: %r>" % e)
        return
    lines = txt.splitlines()
    for l in lines[:trunc] if trunc else lines:
        log("   " + l)
    if trunc and len(lines) > trunc:
        log("   ... (%d more lines)" % (len(lines) - trunc))

def callers(ea, label="", depth=1):
    log("")
    log("---- callers of %s (%08X), depth<=%d ----" % (label, ea, depth))
    seen = set()
    def rec(target, d):
        for x in idautils.XrefsTo(target, 0):
            f = ida_funcs.get_func(x.frm)
            fs = f.start_ea if f else x.frm
            kind = "CODE" if x.iscode else "DATA"
            log("   [d%d] %08X  type=%d %s  in %s : %s" % (d, x.frm, x.type, kind, fname(x.frm), dis(x.frm)))
            if f and d < depth and fs not in seen:
                seen.add(fs)
                rec(fs, d + 1)
    rec(ea, 0)

# ---------------------------------------------------------------- 开始
try:
    ida_auto.auto_wait()
    log("idb = %s" % idc.get_idb_path())
    log("hexrays = %s" % HAVE_HR)

    RENDER      = 0x4F4480   # 含 0x4F4497 的函数
    HOOK_RENDER = 0x4F4497   # mod: GScreenClass_Render
    HOOK_LATE   = 0x4F4583   # mod: GScreenClass_Render_Late
    MAINLOOP    = 0x55D360   # W?Main_Loop$n()i
    HASH        = 0x64DAB0
    LAYERBASE   = 0x8A0360
    SUBMIT      = 0x4A9720   # DisplayClass::Submit
    REMOVE      = 0x4A9770   # DisplayClass::Remove
    TACDRAWALL  = 0x6D8DB0   # Tactical_Draw_All

    # =============== 0. 三个关键地址的身份 ===============
    log("=" * 100)
    log("0) 关键地址身份")
    log("=" * 100)
    for ea, tag in ((RENDER, "0x4F4480 (含两个 mod hook 的函数)"), (HOOK_RENDER, "0x4F4497"),
                    (HOOK_LATE, "0x4F4583"), (MAINLOOP, "0x55D360"), (HASH, "0x64DAB0"),
                    (SUBMIT, "0x4A9720"), (REMOVE, "0x4A9770"), (TACDRAWALL, "0x6D8DB0")):
        f = fof(ea)
        log("  %08X  %-34s func=%s  [%s - %s]" % (
            ea, tag, nm(f.start_ea) if f else "<none>",
            ("%08X" % f.start_ea) if f else "-", ("%08X" % f.end_ea) if f else "-"))
        c = idc.get_cmt(ea, 0)
        if c:
            log("        existing cmt: %s" % c)
    log("  Map global @ %08X = %s" % (0x87F7E8, nm(0x87F7E8)))
    log("  TacticalMap global @ %08X = %s" % (0x887324, nm(0x887324)))

    # =============== Q1-A. 0x4F4480 的所有引用（含 vtable 数据引用） ===============
    log("")
    log("=" * 100)
    log("Q1-A) 谁引用 0x4F4480（Render 函数体）/ 谁引用两个 hook 地址")
    log("=" * 100)
    callers(RENDER, "GScreen render func 0x4F4480", depth=0)
    callers(HOOK_RENDER, "hook addr 0x4F4497", depth=0)
    callers(HOOK_LATE, "hook addr 0x4F4583", depth=0)
    log("")
    log("  -- 0x4F4480 出现在哪些 vtable/数据里（数据引用，type=dr_O 等）--")
    for x in idautils.XrefsTo(RENDER, 0):
        if not x.iscode:
            log("   DATA %08X type=%d in_func=%s : %s" % (x.frm, x.type, fname(x.frm), dis(x.frm)))
    log("")
    log("  -- vtable 归属：向上找最近的段/名字 --")
    for x in idautils.XrefsTo(RENDER, 0):
        if not x.iscode:
            log("   %08X -> 附近符号: %08X=%s ; %08X=%s" % (
                x.frm, x.frm, nm(x.frm), x.frm - 4, nm(x.frm - 4)))

    # =============== Q1-B. 0x4F4480 函数体 ===============
    log("")
    log("=" * 100)
    log("Q1-B) 0x4F4480 函数体（反汇编）")
    log("=" * 100)
    dump_func(RENDER, "0x4F4480", 400)
    decomp(RENDER, "0x4F4480")

    # =============== Q1-C. Main_Loop 上游：谁调它、外层帧循环 ===============
    log("")
    log("=" * 100)
    log("Q1-C) W?Main_Loop (0x55D360) 的调用者链（向上找外层循环）")
    log("=" * 100)
    callers(MAINLOOP, "W?Main_Loop 0x55D360", depth=3)
    for x in idautils.XrefsTo(MAINLOOP, 0):
        if x.iscode:
            f = fof(x.frm)
            if f:
                log("")
                log("  >>> 调用者函数体（前 120 条）: %s @ %08X" % (nm(f.start_ea), f.start_ea))
                dump(f.start_ea, f.end_ea, nm(f.start_ea), 120)
                decomp(f.start_ea, nm(f.start_ea), 200)

    # =============== Q1-D. Main_Loop 全文 ===============
    log("")
    log("=" * 100)
    log("Q1-D) W?Main_Loop (0x55D360) 反汇编全文")
    log("=" * 100)
    dump_func(MAINLOOP, "W?Main_Loop", 900)
    decomp(MAINLOOP, "W?Main_Loop")

    # =============== Q1-E. 窗口/焦点/帧率相关导入的调用点 ===============
    log("")
    log("=" * 100)
    log("Q1-E) 窗口最小化/焦点/暂停/帧率相关 API 的调用点（找跳过渲染的分支）")
    log("=" * 100)
    KEYS = ("IsIconic", "IsWindowVisible", "GetForegroundWindow", "GetActiveWindow",
            "ShowWindow", "Sleep", "PeekMessage", "GetMessage", "timeGetTime",
            "GetTickCount", "QueryPerformanceCounter", "GetSystemMetrics", "WS_VISIBLE")
    names = list(idautils.Names())
    for ea, n in names:
        for k in KEYS:
            if k.lower() in n.lower():
                log("  NAME %08X  %s   (seg=%s)" % (ea, n, idc.get_segm_name(ea)))
                cnt = 0
                for x in idautils.XrefsTo(ea, 0):
                    log("        <- %08X in %s : %s" % (x.frm, fname(x.frm), dis(x.frm)))
                    cnt += 1
                    if cnt > 40:
                        log("        ... more")
                        break
                break

    # =============== Q2-A. 校验值哈希 sub_64DAB0 读了什么 ===============
    log("")
    log("=" * 100)
    log("Q2-A) sub_64DAB0 (联机校验值) 读取对象字段的片段（层数组循环）")
    log("=" * 100)
    dump(0x64DCE4, 0x64DE00, "sub_64DAB0 layer loop + field reads", 400)
    decomp(HASH, "sub_64DAB0 (sync hash)")

    # ---------- Q2-A2. 对象结构体 offset 0x9C / 0x90 的成员名 ----------
    log("")
    log("Q2-A2) 结构体成员名（确证 0x9C = 什么字段）")
    til = None
    try:
        til = idaapi.get_idati()
    except Exception:
        pass
    for sname in ("ObjectClass", "TechnoClass", "FootClass", "AbstractClass", "DisplayClass",
                  "GScreenClass", "TacticalClass", "LayerClass", "VectorClass"):
        tif = ida_typeinf.tinfo_t()
        ok = False
        try:
            res = ida_typeinf.get_named_type(til, sname, ida_typeinf.NTF_TYPE, 0)
            if isinstance(res, tuple):
                tif = res[0]
                ok = tif is not None and not tif.empty()
            else:
                ok = bool(res)
        except Exception as e:
            log("   [%s] get_named_type failed: %r" % (sname, e))
            continue
        if not ok:
            log("   [%s] not found in idb local types" % sname)
            continue
        log("   [%s] %s" % (sname, tif._print()))
        try:
            udt = ida_typeinf.udt_type_data_t()
            if tif.get_udt_details(udt):
                n = udt.size()
                for i in range(n):
                    m = udt[i]
                    off = m.offset // 8
                    if off in (0x8C, 0x90, 0x94, 0x98, 0x9C, 0xA0, 0xA4, 0xA8, 0xAC, 0xB0):
                        log("        +0x%03X  %-28s  %s" % (off, m.name, m.type._print()))
        except Exception as e:
            log("        <udt details failed: %r>" % e)

    # =============== Q2-B. 层数组每帧重建点 ===============
    log("")
    log("=" * 100)
    log("Q2-B) vec_ObjectsInLayers 每帧重建：Submit/Remove/InitClear 的调用者")
    log("=" * 100)
    callers(SUBMIT, "DisplayClass::Submit 0x4A9720", depth=0)
    log("")
    callers(REMOVE, "DisplayClass::Remove 0x4A9770", depth=0)
    log("")
    for a in (0x4A8900, 0x4A88F0, 0x4A8860):
        f = fof(a)
        if f:
            log("  InitClear 候选 %08X -> %s [%08X-%08X]" % (a, nm(f.start_ea), f.start_ea, f.end_ea))
    # 找出包含 0x4A8909 的函数
    f = fof(0x4A8909)
    if f:
        log("  含 0x4A8909 的函数 = %s [%08X-%08X]" % (nm(f.start_ea), f.start_ea, f.end_ea))
        callers(f.start_ea, "DisplayClass_InitClear", depth=2)
        decomp(f.start_ea, "DisplayClass_InitClear", 80)

    log("")
    log("  -- Tactical_Draw_All 0x6D8DB0：层数组的消费点（反汇编 0x6D8F00-0x6D8F80, 0x6D93F0-0x6D9430, 0x6D9590-0x6D95D0, 0x6D9780-0x6D97D0）--")
    for a, b in ((0x6D8F00, 0x6D8F60), (0x6D93F0, 0x6D9440), (0x6D9590, 0x6D95D0), (0x6D9780, 0x6D97D0)):
        dump(a, b, "tacdraw")
    decomp(TACDRAWALL, "Tactical_Draw_All", 200)

    log("")
    log("  -- TacticalClass_Draw 0x6D3D10 反汇编头 60 条 --")
    dump_func(0x6D3D10, "TacticalClass_Draw", 60)

    log("")
    log("  -- 层数组被整体清零的代码：搜索 0x8A0360..0x8A03D8 引用所在函数的“初始化”语义 --")
    for a in (0x4AA2B0, 0x4AA380, 0x650A90, 0x6D97D0, 0x6D9920, 0x6D9A50, 0x4ADFF0, 0x4AE0B0, 0x4AE118):
        f = fof(a)
        if f:
            log("     候选 %08X %s [%08X-%08X]  callers:" % (a, nm(f.start_ea), f.start_ea, f.end_ea))
            for x in idautils.XrefsTo(f.start_ea, 0):
                log("        <- %08X in %s : %s" % (x.frm, fname(x.frm), dis(x.frm)))

    # =============== Q3. 一帧顺序：Main_Loop 里的关键调用点 ===============
    log("")
    log("=" * 100)
    log("Q3) 一帧内关键调用点的地址顺序（Main_Loop 0x55D360-0x55DEDC 内所有 call）")
    log("=" * 100)
    f = fof(MAINLOOP)
    cur = f.start_ea
    idx = 0
    while cur < f.end_ea:
        d = dis(cur)
        if "call" in d:
            tgt = ""
            try:
                t = idc.get_operand_value(cur, 0)
                tgt = nm(t) if t else ""
            except Exception:
                pass
            log("   [%3d] %08X  %-46s -> %s" % (idx, cur, d, tgt))
            idx += 1
        nxt = idc.next_head(cur, f.end_ea)
        if nxt is None or nxt == idaapi.BADADDR or nxt <= cur:
            break
        cur = nxt

    # =============== 附：层数组元素 stride 复核 ===============
    log("")
    log("=" * 100)
    log("附) 层数组 stride / 各层基址复核（0x8A0360 + 0x18*n）")
    log("=" * 100)
    for i in range(6):
        a = LAYERBASE + 0x18 * i
        log("   layer[%d] @ %08X  vt=%08X name_of_vt=%s  length@+0x10=%08X" % (
            i, a, ida_bytes.get_dword(a), nm(ida_bytes.get_dword(a)), ida_bytes.get_dword(a + 0x10)))

except Exception:
    log("")
    log("!!!! EXCEPTION !!!!")
    log(traceback.format_exc())
finally:
    open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\probe_render_vs_logic.txt", "w",
         encoding="utf-8", errors="replace").write("\n".join(report))
    ida_pro.qexit(0)
