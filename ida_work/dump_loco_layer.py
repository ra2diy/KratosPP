# -*- coding: utf-8 -*-
# 判定 StandExtHook.cpp 里 5 个 *LocomotionClass_In_Which_Layer Hook 的 this 类型
import idaapi, idc, idautils, ida_funcs, ida_bytes, ida_segment

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\loco_layer.txt"
f = open(OUT, "w", encoding="utf-8")
def w(s=""):
    f.write(str(s) + "\n")

def is_data_seg(ea):
    seg = ida_segment.getseg(ea)
    if not seg:
        return False
    return idc.get_segm_name(ea) in (".rdata", ".data", ".text") or seg.type == ida_segment.SEG_DATA

TARGETS = [0x75C7E0, 0x6A3E50, 0x5B19D0, 0x517100, 0x4B4820]

for addr in TARGETS:
    w("=" * 96)
    w("TARGET %08X" % addr)
    fn = ida_funcs.get_func(addr)
    if fn:
        w("  func: %s   %08X - %08X" % (idc.get_func_name(fn.start_ea), fn.start_ea, fn.end_ea))
        ea = fn.start_ea
        while ea < fn.end_ea and ea != idaapi.BADADDR:
            w("    %08X  %s" % (ea, idc.generate_disasm_line(ea, 0)))
            ea = idc.next_head(ea, fn.end_ea)
    else:
        w("  (no func)")

    # 列出对该函数的引用（含 vtable 数据引用）
    slots = []
    for xr in idautils.XrefsTo(addr):
        w("  XREF  from %08X type=%d  -> %s" % (xr.frm, xr.type, idc.generate_disasm_line(xr.frm, 0)))
        slots.append(xr.frm)
    if fn and fn.start_ea != addr:
        for xr in idautils.XrefsTo(fn.start_ea):
            w("  XREFfn from %08X type=%d  -> %s" % (xr.frm, xr.type, idc.generate_disasm_line(xr.frm, 0)))
            slots.append(xr.frm)

    # 对 vtable 槽地址再查一层：谁调用这个虚函数
    for slot in set(slots):
        w("    -- callers of slot %08X (name=%s) --" % (slot, idc.get_name(slot)))
        cnt = 0
        for xr2 in idautils.XrefsTo(slot):
            cnt += 1
            if cnt > 30:
                break
            w("       call from %08X type=%d : %s" % (xr2.frm, xr2.type, idc.generate_disasm_line(xr2.frm, 0)))
            # 往上 6 条指令看上下文（找 ESI 来源）
            p = xr2.frm
            ctx = []
            for _ in range(8):
                p = idc.prev_head(p)
                if p == idaapi.BADADDR:
                    break
                ctx.append("          %08X  %s" % (p, idc.generate_disasm_line(p, 0)))
            for line in reversed(ctx):
                w(line)

# 额外：FootClass_Update_UpdateLayer 附近 (0x4DA87A 所在 FootClass::AI)
w("=" * 96)
w("CONTEXT @0x4DA87A (FootClass::AI UpdateLayer 调用点)")
p = 0x4DA87A
ctx = []
for _ in range(40):
    ctx.append("  %08X  %s" % (p, idc.generate_disasm_line(p, 0)))
    p = idc.next_head(p)
for line in ctx:
    w(line)

f.close()
print("DONE ->", OUT)
