# -*- coding: utf-8 -*-
"""第四轮：最后几块拼图（只读 + 写报告）。

  T1 sub_647260 的 switch 跳表：哪些 Session.idxGameMode 会走 sub_6475F0（含 sub_64DAB0）
  T2 sub_6475F0 全文：正常模式下校验值的调用频率
  T3 GameInFocus(0xA8ED80) 的所有写入者（Alt-Tab/失焦是谁置 0）
  T4 GetCoords 族函数体：返回 this+0x9C 的取证；0x5F6000-0x5F7000 的符号（找 SetLocation 类）
  T5 ObjectClass::pos 的成员偏移（idc.get_member_offset 回退路径）
  T6 sub_650A90（哈希收尾）与 Do_Blit 的调用者
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

    def t1():
        dump(0x6474C0, 0x6474E0, "sub_647260 switch 头", 12)
        # 取跳表地址
        tbl = None
        for ea in (0x6474CF, 0x6474D5):
            for i in range(2):
                try:
                    if idc.get_operand_type(ea, i) in (idc.o_mem, idc.o_imm, idc.o_near, idc.o_far):
                        v = idc.get_operand_value(ea, i)
                        if 0x7C0000 <= v <= 0x820000:
                            log("   0x%08X 操作数%d -> 表 %08X" % (ea, i, v))
                            tbl = v
                except Exception:
                    pass
        if tbl:
            for i in range(8):
                t = ida_bytes.get_dword(tbl + i * 4)
                log("   跳表[%d] = %08X  %s   (%s)" % (i, t, nm(t), dis(t)))
        log("")
        log("   所有 case 目标附近的代码：")
        for i in range(8):
            if tbl:
                t = ida_bytes.get_dword(tbl + i * 4)
                if t and t != 0xFFFFFFFF and 0x640000 <= t <= 0x660000:
                    dump(t, t + 0x20, "case target %08X" % t, 10)
    section("T1) sub_647260 的 switch 跳表（Session.idxGameMode -> 处理分支）", t1)

    def t2():
        dump_func(0x6475F0, "sub_6475F0", 200)
        if HAVE_HR:
            try:
                cf = ida_hexrays.decompile(0x6475F0)
                if cf:
                    log("   #### PSEUDOCODE sub_6475F0")
                    for l in str(cf).splitlines():
                        log("      " + l)
            except Exception as e:
                log("   <decompile failed: %r>" % e)
        log("")
        log("   -- 谁调用 sub_6475F0 --")
        for x in idautils.XrefsTo(0x6475F0, 0):
            log("      <- %08X in %s : %s" % (x.frm, fname(x.frm), dis(x.frm)))
    section("T2) sub_6475F0：正常模式下的校验值路径", t2)

    def t3():
        g = 0xA8ED80
        log("   GameInFocus = %08X (%s)" % (g, nm(g)))
        wr = []
        for x in idautils.XrefsTo(g, 0):
            d = dis(x.frm)
            tag = ""
            if d.replace(" ", "").startswith("movGameInFocus,") or "mov     GameInFocus" in d:
                tag = "  <<< 写入"
                wr.append(x.frm)
            log("   %s %08X in %s : %s%s" % ("CODE" if x.iscode else "DATA", x.frm, fname(x.frm), d, tag))
        log("   写入点：%s" % ", ".join("%08X(%s)" % (a, fname(a)) for a in wr))
        for a in wr:
            f = ida_funcs.get_func(a)
            if f:
                log("")
                dump(f.start_ea, min(f.start_ea + 0x120, f.end_ea), "写入者函数 %s" % nm(f.start_ea), 90)
    section("T3) GameInFocus 的写入者（失焦/Alt-Tab）", t3)

    def t4():
        for a in (0x4104C0, 0x4104F0, 0x410540, 0x41BE00, 0x5F6C80):
            dump_func(a, nm(a), 30)
            if HAVE_HR:
                try:
                    cf = ida_hexrays.decompile(a)
                    if cf:
                        for l in str(cf).splitlines()[:14]:
                            log("      | " + l)
                except Exception as e:
                    log("      <decompile %08X failed: %r>" % (a, e))
        log("")
        log("   -- 0x5F6000-0x5F7000 区间的符号（找 SetLocation / SetCoords 类）--")
        for ea, n in idautils.Names():
            if 0x5F6000 <= ea <= 0x5F7000:
                log("      %08X %s" % (ea, n))
        log("")
        log("   -- 名字里含 'Coords' / 'Location' / 'Position' 的函数 --")
        for ea, n in idautils.Names():
            low = n.lower()
            if ("coord" in low or "location" in low or "position" in low) and not n.startswith("nullsub"):
                f = ida_funcs.get_func(ea)
                if f and f.start_ea == ea:
                    log("      %08X %s" % (ea, n))
    section("T4) GetCoords / 位置设置类函数的取证", t4)

    def t5():
        for sname in ("ObjectClass", "TechnoClass"):
            sid = idc.get_struc_id(sname)
            log("   get_struc_id(%s) = %08X (%s)" % (sname, sid, "ok" if sid != 0xFFFFFFFF else "fail"))
            if sid != 0xFFFFFFFF and sid != 0:
                for m in ("pos", "Location", "field_98", "field_9C"):
                    off = idc.get_member_offset(sid, m)
                    log("      %-12s -> %s" % (m, ("0x%X" % off) if off not in (0xFFFFFFFF, -1) else "<无>" ))
    section("T5) ObjectClass::pos 成员偏移（idc 路径）", t5)

    def t6():
        dump_func(0x650A90, "sub_650A90（哈希收尾）", 120)
        log("")
        log("   -- sub_650A90 调用者 --")
        for x in idautils.XrefsTo(0x650A90, 0):
            log("      <- %08X in %s : %s" % (x.frm, fname(x.frm), dis(x.frm)))
        log("")
        log("   -- Do_Blit(0x4F4780) 调用者 --")
        for x in idautils.XrefsTo(0x4F4780, 0):
            log("      <- %08X in %s : %s" % (x.frm, fname(x.frm), dis(x.frm)))
    section("T6) 哈希收尾与 Do_Blit 调用者", t6)

except Exception:
    log("")
    log("!!!! EXCEPTION !!!!")
    log(traceback.format_exc())
finally:
    open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\probe_render_vs_logic4.txt", "w",
         encoding="utf-8", errors="replace").write("\n".join(report))
    ida_pro.qexit(0)
