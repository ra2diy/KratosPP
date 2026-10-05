# -*- coding: utf-8 -*-
"""第五轮：位置字段偏移的最终取证（只读 + 写报告）。

  U1 ObjectClass 结构体逐偏移成员表（0x80-0xB0）—— 确认 pos 到底在 0x98 还是 0x9C
  U2 ObjectClass_SetPosition(0x5F6940) / ObjectClass_SetZ(0x5F6060) 反汇编：写了哪些偏移
  U3 vt_ObjectClass / vt_TechnoClass 里 SetLocation / GetCoords 的槽位与实际实现函数
  U4 实现函数的反汇编（看它将写入/返回 [this+XX]）
  U5 层数组元素指针指向的对象类型（ObjectClass*）+ 哈希读的 [esi+9Ch] 与 pos 的关系
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

    def u1():
        for sname in ("ObjectClass", "TechnoClass", "AbstractClass", "FootClass"):
            sid = idc.get_struc_id(sname)
            if sid in (0xFFFFFFFF, 0):
                log("   [%s] 无此结构体" % sname)
                continue
            log("   [%s] sid=%08X" % (sname, sid))
            for off in range(0x00, 0xC0, 4):
                n = idc.get_member_name(sid, off)
                if n:
                    sz = idc.get_member_size(sid, off)
                    log("      +0x%03X  %-28s size=%d" % (off, n, sz))
    section("U1) ObjectClass/TechnoClass 成员逐偏移表（0x00-0xBC）", u1)

    def u2():
        for a, lab in ((0x5F6940, "ObjectClass_SetPosition"), (0x5F6060, "ObjectClass_SetZ"),
                       (0x5F60A0, "ObjectClass_F0"), (0x5F6120, "ObjectClass_F4")):
            dump_func(a, lab, 60)
            if HAVE_HR:
                try:
                    cf = ida_hexrays.decompile(a)
                    if cf:
                        for l in str(cf).splitlines()[:25]:
                            log("      | " + l)
                except Exception as e:
                    log("      <decompile %08X failed: %r>" % (a, e))
    section("U2) 位置设置类函数反汇编（写了哪些偏移）", u2)

    def u3():
        # 找 vt_* 类型，输出成员名与偏移，定位 SetLocation / GetCoords
        til = idaapi.get_idati()
        order = 0
        for sname in ("vt_ObjectClass", "vt_TechnoClass", "vt_FootClass", "vt_AbstractClass"):
            sid = idc.get_struc_id(sname)
            log("   [%s] sid=%s" % (sname, hex(sid) if sid not in (0xFFFFFFFF, 0) else "<无>"))
            if sid in (0xFFFFFFFF, 0):
                continue
            for off in range(0x00, 0x260, 4):
                n = idc.get_member_name(sid, off)
                if n and ("location" in n.lower() or "coord" in n.lower() or "position" in n.lower()):
                    log("      +0x%03X  %s" % (off, n))
        log("")
        log("   -- .rdata 里的 vt 实例 --")
        for vn in ("vt_ObjectClass", "vt_TechnoClass", "vt_FootClass", "vt_UnitClass"):
            ea = idc.get_name_ea(idaapi.BADADDR, vn)
            log("      %s @ %s" % (vn, hex(ea) if ea != idaapi.BADADDR else "<无>"))
    section("U3) vtable 类型里的 SetLocation / GetCoords 槽位", u3)

    def u4():
        # 已知：ObjectClass_GetCoords_1 里 call [eax+48h] —— 打印 TechnoClass vtable 的 +0x48 槽
        vt = idc.get_name_ea(idaapi.BADADDR, "vt_TechnoClass")
        log("   vt_TechnoClass = %s" % (hex(vt) if vt != idaapi.BADADDR else "<无>"))
        if vt != idaapi.BADADDR:
            for off in (0x44, 0x48, 0x4C, 0x50, 0x54, 0x58):
                v = ida_bytes.get_dword(vt + off)
                log("      vt[+0x%02X] = %08X  %s" % (off, v, nm(v)))
                if v and v != 0xFFFFFFFF:
                    dump_func(v, "vt[+0x%02X] impl" % off, 30)
        # 直接看 ObjectClass 虚表中 SetLocation 的槽：从 YRpp 头文件的顺序估算不可靠，
        # 改为在整个 .rdata 的 vt_* 结构里找名字含 SetLocation 的成员
        for ea, n in idautils.Names():
            if "SetLocation" in n or "Set_Position" in n or "SetPosition" in n:
                log("   NAME %08X %s" % (ea, n))
    section("U4) GetCoords 实际实现（返回 this+?）+ SetLocation 符号", u4)

    def u5():
        # 直接反汇编 ObjectClass 的 vt 实例（从 Map/对象类型推断不可靠）：
        # 用 ObjectClass::GetCoords_1 的调用链反推：vt+0x48 = 真 GetCoords
        vt = idc.get_name_ea(idaapi.BADADDR, "vt_TechnoClass")
        if vt != idaapi.BADADDR:
            g = ida_bytes.get_dword(vt + 0x48)
            dump_func(g, "TechnoClass 的 GetCoords 实现（vt+0x48）", 40)
        log("")
        log("   -- 层数组元素类型：Tactical_Draw_All 里对元素的调用（vt+0x104 / vt+0x110）--")
        vt2 = idc.get_name_ea(idaapi.BADADDR, "vt_TechnoClass")
        for off in (0x104, 0x110):
            if vt2 != idaapi.BADADDR:
                v = ida_bytes.get_dword(vt2 + off)
                log("      vt[+0x%03X] = %08X %s" % (off, v, nm(v)))
    section("U5) 真 GetCoords / 层元素绘制槽", u5)

except Exception:
    log("")
    log("!!!! EXCEPTION !!!!")
    log(traceback.format_exc())
finally:
    open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\probe_render_vs_logic5.txt", "w",
         encoding="utf-8", errors="replace").write("\n".join(report))
    ida_pro.qexit(0)
