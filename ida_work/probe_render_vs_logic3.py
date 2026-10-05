# -*- coding: utf-8 -*-
"""第三轮：补齐取证（只读 + 写报告）。

  S1 GameInFocus 全局的确切地址（从 Main_Loop / sub_55E160 的反汇编里认出来）+ 谁写它
  S2 ObjectClass 结构体成员（0x9C = pos）+ GetCoords 族函数反汇编（证明 0x9C = 对象位置 CoordStruct）
  S3 LayerClass::Sort / AddObject：层数组的排序/插入语义
  S4 sub_647260 正常（非回放）分支：sub_64DAB0 与 DoList 的先后与频率
  S5 sub_64DAB0 全文里 Z 分量（var_4）到底进不进哈希
  S6 TacticalClass_Draw(0x6D3D10) 是否调用 Tactical_Draw_All(0x6D8DB0)
  S7 Main_Loop LABEL_72 的门控条件反汇编原件（0x55D860-0x55D900）
  S8 0x887640 的 vtable[0x40]（渲染期的 Do_Blit / Sleep 帧率限制）
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

    # ---------------- S7 先拿到门控原件 ----------------
    def s7():
        dump(0x55D860, 0x55D905, "Main_Loop LABEL_72 门控（渲染调用前置条件）", 80)
        dump(0x55D8F2, 0x55D92C, "Main_Loop 0x55D8F2 渲染调用 + 后续", 30)
        dump(0x55DE30, 0x55DEA8, "Main_Loop 帧尾（hash / 帧限速 / Frame++）", 60)
    section("S7) Main_Loop LABEL_72 门控 / 渲染点 / 帧尾原件", s7)

    # ---------------- S1 GameInFocus ----------------
    def s1():
        # 从 sub_55E160 里 `cmp GameInFocus, 1` / Main_Loop 里的 GameInFocus 读取认地址
        f = ida_funcs.get_func(0x55E160)
        cur = 0x55E1C0
        cands = set()
        while cur < 0x55E2C0:
            d = dis(cur)
            if "Focus" in d or "A8E3" in d or "A8ED" in d:
                log("   sub_55E160: %08X %s" % (cur, d))
            # 收集所有 [imm32] 形式的全局引用
            for i in range(2):
                try:
                    if idc.get_operand_type(cur, i) in (idc.o_mem, idc.o_imm):
                        v = idc.get_operand_value(cur, i)
                        if 0xA80000 <= v <= 0xAA0000 and v != 0:
                            cands.add(v)
                except Exception:
                    pass
            nxt = idc.next_head(cur, 0x55E2C0)
            if nxt is None or nxt == idaapi.BADADDR or nxt <= cur:
                break
            cur = nxt
        log("   sub_55E160 [0x55E1C0-0x55E2C0] 引用到的全局: %s" % ", ".join("%08X(%s)" % (c, nm(c)) for c in sorted(cands)))
        log("")
        log("   Main_Loop [0x55D860-0x55D905] 引用/条件原件见 S7")
        # 逐个候选看名字与 xrefs
        for c in sorted(cands):
            log("   --- 候选全局 %08X = %s" % (c, nm(c)))
            cnt = 0
            for x in idautils.XrefsTo(c, 0):
                log("        %s %08X in %s : %s" % ("CODE" if x.iscode else "DATA", x.frm, fname(x.frm), dis(x.frm)))
                cnt += 1
                if cnt > 25:
                    log("        ...")
                    break
        # 名字里含 Focus 的符号
        log("")
        log("   名字含 'Focus' 的符号：")
        for ea, n in idautils.Names():
            if "focus" in n.lower():
                log("      %08X %s (seg=%s)" % (ea, n, idc.get_segm_name(ea)))
    section("S1) GameInFocus 的确切地址与写入者", s1)

    # ---------------- S2 0x9C = pos ----------------
    def s2():
        til = idaapi.get_idati()
        for sname in ("ObjectClass", "TechnoClass", "AbstractClass"):
            tif = ida_typeinf.tinfo_t()
            ok = False
            try:
                ok = tif.get_named_type(til, sname, ida_typeinf.NTF_TYPE)
            except Exception as e:
                log("   [%s] tinfo_t.get_named_type 失败: %r" % (sname, e))
            if not ok:
                try:
                    r = ida_typeinf.get_named_type(til, sname, ida_typeinf.NTF_TYPE)
                    log("   [%s] module-level get_named_type -> %r (%s)" % (sname, r, type(r)))
                    if isinstance(r, tuple):
                        tif = r[0]
                        ok = True
                except Exception as e:
                    log("   [%s] module-level 也失败: %r" % (sname, e))
            if not ok:
                log("   [%s] 取不到类型" % sname)
                continue
            log("   [%s] %s  size=0x%X" % (sname, tif._print(), tif.get_size()))
            udt = ida_typeinf.udt_type_data_t()
            if tif.get_udt_details(udt):
                for i in range(udt.size()):
                    m = udt[i]
                    off = m.offset // 8
                    if 0x80 <= off <= 0xB0:
                        log("        +0x%03X  %-26s %s" % (off, m.name, m.type._print()))
        log("")
        log("   -- GetCoords 族反汇编（看返回值是不是 this+0x9C）--")
        for a in (0x4104C0, 0x4104F0, 0x410540, 0x41BE00, 0x5F6C80, 0x4DBDF0):
            dump_func(a, nm(a), 25)
            if HAVE_HR:
                try:
                    cf = ida_hexrays.decompile(a)
                    if cf:
                        for l in str(cf).splitlines()[:16]:
                            log("      | " + l)
                except Exception as e:
                    log("      <decompile failed: %r>" % e)
    section("S2) 0x9C 是不是对象位置 CoordStruct（pos / GetCoords）", s2)

    # ---------------- S3 层数组语义 ----------------
    def s3():
        dump_func(0x551A30, "LayerClass::Sort", 60)
        f = ida_funcs.get_func(0x5519B0)
        if f:
            dump(f.start_ea, f.end_ea, "LayerClass::AddObject(0x5519B0)", 90)
        if HAVE_HR:
            for a in (0x551A30, 0x5519B0):
                try:
                    cf = ida_hexrays.decompile(a)
                    if cf:
                        log("   #### PSEUDOCODE %s" % nm(a))
                        for l in str(cf).splitlines()[:45]:
                            log("      " + l)
                except Exception as e:
                    log("   <decompile %08X failed: %r>" % (a, e))
    section("S3) LayerClass::Sort / AddObject 语义", s3)

    # ---------------- S4 sub_647260 正常分支 ----------------
    def s4():
        dump_func(0x647260, "sub_647260 全文", 400)
        if HAVE_HR:
            try:
                cf = ida_hexrays.decompile(0x647260)
                if cf:
                    log("   #### PSEUDOCODE sub_647260 全文")
                    for l in str(cf).splitlines():
                        log("      " + l)
            except Exception as e:
                log("   <decompile failed: %r>" % e)
    section("S4) sub_647260：校验值调用频率与 DoList 的先后", s4)

    # ---------------- S5 Z 分量 ----------------
    def s5():
        dump(0x64DD5F, 0x64DE00, "sub_64DAB0 层循环尾部", 120)
        dump(0x64DE00, 0x64DE80, "sub_64DAB0 函数尾部", 80)
        f = ida_funcs.get_func(0x64DAB0)
        n = 0
        cur = f.start_ea
        while cur < f.end_ea:
            d = dis(cur)
            if "var_4" in d:
                log("   [var_4] %08X %s" % (cur, d))
                n += 1
            nxt = idc.next_head(cur, f.end_ea)
            if nxt is None or nxt == idaapi.BADADDR or nxt <= cur:
                break
            cur = nxt
        log("   var_4 相关指令 = %d" % n)
    section("S5) sub_64DAB0 里 Z 分量（var_4）的最终去向", s5)

    # ---------------- S6 / S8 ----------------
    def s6():
        for x in idautils.XrefsTo(0x6D8DB0, 0):
            log("   Tactical_Draw_All <- %08X in %s : %s" % (x.frm, fname(x.frm), dis(x.frm)))
        log("")
        dump_func(0x6D3D10, "TacticalClass_Draw", 90)
        log("")
        log("   -- 谁调用 TacticalClass_Draw(0x6D3D10) --")
        for x in idautils.XrefsTo(0x6D3D10, 0):
            if x.iscode:
                log("      <- %08X in %s : %s" % (x.frm, fname(x.frm), dis(x.frm)))
    section("S6) TacticalClass_Draw -> Tactical_Draw_All -> 层数组消费", s6)

    def s8():
        glob = 0x887640
        vt = ida_bytes.get_dword(glob)
        log("   [%08X] = %08X (%s)  -> 对象 vtable" % (glob, vt, nm(vt)))
        if vt and vt != 0xFFFFFFFF:
            for i in range(0, 24):
                a = vt + i * 4
                v = ida_bytes.get_dword(a)
                log("      vt[0x%02X] slot %2d = %08X  %s" % (i * 4, i, v, nm(v)))
        log("")
        log("   -- GScreenClass::Do_Blit (0x4F4B??) 里的 Sleep --")
        for a, n in idautils.Names():
            if "Do_Blit" in n:
                log("      %08X %s" % (a, n))
                dump_func(a, n, 60)
    section("S8) 0x887640 的 vtable[0x40] 与 Do_Blit 帧率限制", s8)

except Exception:
    log("")
    log("!!!! EXCEPTION !!!!")
    log(traceback.format_exc())
finally:
    open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\probe_render_vs_logic3.txt", "w",
         encoding="utf-8", errors="replace").write("\n".join(report))
    ida_pro.qexit(0)
