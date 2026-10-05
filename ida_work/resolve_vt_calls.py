import idaapi, idautils, idc, ida_funcs, ida_segment

out = []

# locate .text
seg = None
for s in idautils.Segments():
    if idc.get_segm_name(s) == ".text":
        seg = s
        break
if seg is None:
    seg = idautils.Segments()[0]
start = idc.get_segm_start(seg)
end = idc.get_segm_end(seg)
out.append(".text = 0x%X-0x%X" % (start, end))

targets = {0x144, 0x140}
hits = []
ea = start
while ea < end:
    mnem = idc.print_insn_mnem(ea)
    if mnem in ("call", "jmp") and idc.get_operand_type(ea, 0) == 2:  # o_mem
        v = idc.get_operand_value(ea, 0)
        if v in targets:
            f = ida_funcs.get_func(ea)
            hits.append((ea, mnem, v, idc.get_func_name(f.start_ea) if f else "?"))
    ea = idc.next_head(ea)
    if ea == idaapi.BADADDR:
        break

out.append("=== virtual slot calls (offset 0x140 / 0x144) ===")
for ea, mnem, v, fn in hits:
    out.append("  0x%08X  %s [..+0x%X]   in %s" % (ea, mnem, v, fn))

# base ClickedMission: find vtable ref - locate function whose name contains ClickedMission
out.append("")
out.append("=== Functions named *ClickedMission* ===")
for f_ea in idautils.Functions():
    n = idc.get_func_name(f_ea)
    if "ClickedMission" in n or "What_Action" in n or "WhatAction" in n:
        out.append("  0x%X  %s" % (f_ea, n))
        for x in idautils.XrefsTo(f_ea):
            ff = ida_funcs.get_func(x.frm)
            out.append("      <- 0x%X  in %s" % (x.frm, idc.get_func_name(ff.start_ea) if ff else "?"))

open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\vt_calls.txt", "w", encoding="utf-8").write("\n".join(out))
print("SCRIPT DONE")
idc.qexit(0)
