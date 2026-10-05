# -*- coding: utf-8 -*-
"""第六轮：最后确认（只读 + 写报告）。

  V1 ObjectClass_SetPosition(0x5F6940) 在哪些 vtable 的哪个槽位（确认它就是 mod 的 SetLocation）
  V2 GameInFocus 写入点 0x7778CE 的上下文（哪个窗口消息把它置 0/1）
  V3 sub_647260 的跳表 jpt_6474D5：哪些 Session.idxGameMode 走 sub_6475F0（每帧算校验值）
  V4 层数组元素指针 = 对象本体？（Tactical_Draw_All 里元素的使用）
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

def section(title, fn):
    log("")
    log("=" * 100)
    log(title)
    log("=" * 100)
    try:
        fn()
    except Exception:
        log("  <SECTION FAILED>")
        log(traceback.format_exc())

try:
    ida_auto.auto_wait()
    log("idb = %s   hexrays=%s" % (idc.get_idb_path(), HAVE_HR))

    def v1():
        for fx in (0x5F6940, 0x5F6060):
            log("   == xrefs of %08X (%s) ==" % (fx, nm(fx)))
            for x in idautils.XrefsTo(fx, 0):
                seg = idc.get_segm_name(x.frm)
                log("      %s %08X type=%d seg=%s" % ("CODE" if x.iscode else "DATA", x.frm, x.type, seg))
                if not x.iscode:
                    # 找出该 vtable 的起点与槽位偏移：向上找连续的函数指针
                    base = x.frm
                    while True:
                        prev = base - 4
                        v = ida_bytes.get_dword(prev)
                        f2 = ida_funcs.get_func(v)
                        if f2 and f2.start_ea == v and idc.get_segm_name(prev) in (".rdata", ".data"):
                            base = prev
                        else:
                            break
                        if x.frm - base > 0x400:
                            break
                    log("         所在 vtable 起点约 %08X，槽位 = +0x%X" % (base, x.frm - base))
                    cm = idc.get_cmt(x.frm, 0) or idc.get_cmt(base, 0)
                    if cm:
                        log("         vtable 注释: %s" % cm[:200])
                    # 打印 vtable 前 4 个 dword 以判断是哪一类
                    log("         vtable[0..3] = %08X %08X %08X %08X" % (
                        ida_bytes.get_dword(base), ida_bytes.get_dword(base + 4),
                        ida_bytes.get_dword(base + 8), ida_bytes.get_dword(base + 12)))
    section("V1) ObjectClass_SetPosition / SetZ 的 vtable 槽位", v1)

    def v2():
        dump(0x777870, 0x777900, "Windows_Procedure 里 GameInFocus 的写入点上下文", 60)
        log("")
        log("   WM_ 常量参考: WM_ACTIVATE=0x6 WM_SETFOCUS=0x7 WM_KILLFOCUS=0x8 WM_ACTIVATEAPP=0x1C WM_SIZE=0x5")
        dump(0x7776A0, 0x777720, "Windows_Procedure 的 switch 头", 40)
    section("V2) GameInFocus 写入点（窗口焦点消息）", v2)

    def v3():
        t = idc.get_name_ea(idaapi.BADADDR, "jpt_6474D5")
        log("   jpt_6474D5 = %s" % (hex(t) if t != idaapi.BADADDR else "<无>"))
        if t != idaapi.BADADDR:
            for i in range(6):
                v = ida_bytes.get_dword(t + i * 4)
                log("      [%d] -> %08X  %s   (%s)" % (i, v, nm(v), dis(v)))
        log("")
        log("   -- sub_6475F0 从哪被调用 / 谁引用 Session.idxGameMode 的 switch --")
        for x in idautils.XrefsTo(0x6475F0, 0):
            log("      <- %08X in %s : %s" % (x.frm, fname(x.frm), dis(x.frm)))
    section("V3) sub_647260 跳表：哪些 game mode 每帧算校验值", v3)

    def v4():
        dump(0x6D8F39, 0x6D8F60, "Tactical_Draw_All 层元素使用（元素=对象指针）", 20)
        dump(0x6D95C5, 0x6D95E0, "Tactical_Draw_All 第二段层循环元素", 15)
    section("V4) 层数组元素的使用方式", v4)

except Exception:
    log("")
    log("!!!! EXCEPTION !!!!")
    log(traceback.format_exc())
finally:
    open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\probe_render_vs_logic6.txt", "w",
         encoding="utf-8", errors="replace").write("\n".join(report))
    ida_pro.qexit(0)
