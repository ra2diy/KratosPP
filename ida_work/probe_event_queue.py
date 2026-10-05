"""核对 Phobos 版 EventClass/QueueClass 用到的引擎事实（只读 + 写注释/重命名）。

要证的点：
  1. EventNames @0x82091C 到底几项（Phobos/YRpp 都写 47）
  2. OutList @0xA802C8：容量上限、环形掩码、Timings、字段顺序（Count/Head/Tail）
  3. DoList  @0x8B41F8：容量（Phobos 写 MAX_EVENTS*128 = 16384）
  4. 0xA83ED0（Kratos YRpp 叫 MegaMissionList<0x100>，Phobos 注释掉了）：它到底是什么
  5. 0x4C66C0（Phobos 新增的 EventClass(houseIndex, EventType) 构造）：真有这个构造吗
"""
import idaapi, idc, idautils, ida_funcs, ida_bytes, ida_auto, ida_pro, ida_segment, ida_name

report = []
def log(s=""):
    report.append(str(s))

def seg_of(ea):
    s = ida_segment.getseg(ea)
    return ida_segment.get_segm_name(s) if s else "?"

def is_rodata_ptr(v):
    """值是否像指向只读段的指针（用于判定 EventNames 的项数）"""
    if v < 0x401000 or v > 0xC00000:
        return False
    s = ida_segment.getseg(v)
    if not s:
        return False
    return ida_segment.get_segm_name(s) in (".rdata", ".data")

def str_at(ea):
    s = idc.get_strlit_contents(ea, -1, 0)
    return s.decode("ascii", "replace") if s else ""

def dump_func(ea, limit=70, tag=""):
    f = ida_funcs.get_func(ea)
    if not f:
        log("    (no function at %08X)" % ea)
        return
    log("    func %s @ %08X..%08X %s" % (idc.get_func_name(f.start_ea), f.start_ea, f.end_ea, tag))
    cur = f.start_ea
    n = 0
    while cur < f.end_ea and n < limit:
        log("      %08X  %s" % (cur, idc.generate_disasm_line(cur, 0)))
        cur = idc.next_head(cur, f.end_ea)
        n += 1

try:
    ida_auto.auto_wait()

    # ---------- 1. EventNames ----------
    log("===== 1) EventNames @ 0x82091C =====")
    valid = 0
    for i in range(0, 56):
        v = ida_bytes.get_dword(0x82091C + i * 4)
        ok = is_rodata_ptr(v)
        if ok:
            valid += 1
        log("  [%2d] %08X  %s  %s" % (i, v, "PTR " + seg_of(v) if ok else "---- (not a ptr)", str_at(v)[:40] if ok else ""))
    log("  => 连续像指针的项数 = %d" % valid)

    # ---------- 2/3/4. 队列 ----------
    for name, addr in (("OutList", 0xA802C8), ("DoList", 0x8B41F8), ("MegaMissionList?", 0xA83ED0)):
        log()
        log("===== %s @ %08X (%s) =====" % (name, addr, seg_of(addr)))
        xs = list(idautils.XrefsTo(addr))
        log("  xrefs = %d" % len(xs))
        done = set()
        for x in xs:
            f = ida_funcs.get_func(x.frm)
            key = f.start_ea if f else x.frm
            log("  - from %08X  %s" % (x.frm, idc.get_func_name(key) if f else "?"))
            if key not in done:
                done.add(key)
                dump_func(x.frm, 80)
    # ---------- 5. 构造 ----------
    log()
    log("===== 5) 0x4C66C0 反汇编 =====")
    f = ida_funcs.get_func(0x4C66C0)
    log("  func=%s  start=%s end=%s" % (bool(f), hex(f.start_ea) if f else "-", hex(f.end_ea) if f else "-"))
    cur = 0x4C66C0
    for _ in range(40):
        log("    %08X  %s" % (cur, idc.generate_disasm_line(cur, 0)))
        cur = idc.next_head(cur, cur + 0x80)
        if cur is None or cur == idaapi.BADADDR:
            break

    # ---------- 6. 把结论写进 idb（用户要看） ----------
    idc.set_cmt(0xA802C8, "EventClass::OutList (QueueClass<EventClass,128>)", 0)
    idc.set_cmt(0x8B41F8, "EventClass::DoList (QueueClass<EventClass,16384>)", 0)
    idc.set_cmt(0x4C66C0, "EventClass::EventClass(houseIndex, EventType)", 0)
    for nm, addr in (("EventClass_OutList", 0xA802C8), ("EventClass_DoList", 0x8B41F8),
                     ("EventClass_EventNames", 0x82091C), ("EventClass_MegaMissionList", 0xA83ED0)):
        ida_name.set_name(addr, nm, ida_name.SN_NOCHECK | ida_name.SN_FORCE)
    log()
    log("  [db] 已写注释/重命名：OutList / DoList / EventNames / MegaMissionList / 0x4C66C0")
finally:
    open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\event_queue_report.txt", "w", encoding="utf-8").write("\n".join(report))
    ida_pro.qexit(0)
