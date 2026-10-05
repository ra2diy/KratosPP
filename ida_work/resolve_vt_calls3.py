import idaapi, idautils, idc, ida_funcs, ida_segment

out = []
seg = None
for s in idautils.Segments():
    if idc.get_segm_name(s) == ".text":
        seg = s
        break
start = idc.get_segm_start(seg)
end = idc.get_segm_end(seg)
out.append(".text = 0x%X-0x%X" % (start, end))

hits = []
ea = start
while ea < end:
    mnem = idc.print_insn_mnem(ea)
    if mnem in ("call", "jmp"):
        txt = idc.print_operand(ea, 0)
        if ("144h" in txt) or ("140h" in txt) or ("148h" in txt):
            f = ida_funcs.get_func(ea)
            hits.append((ea, mnem, txt, idc.get_func_name(f.start_ea) if f else "?"))
    ea = idc.next_head(ea)
    if ea == idaapi.BADADDR:
        break

out.append("=== indirect calls referencing +144h/+140h/+148h ===")
for ea, mnem, txt, fn in hits:
    out.append("  0x%08X  %s %-40s in %s" % (ea, mnem, txt, fn))

# Also: find all vtable entries pointing to UnitClass_ClickedMission and ObjectClass_ClickedMission
out.append("")
out.append("=== vtable entries -> UnitClass_ClickedMission (0x738890) ===")
for x in idautils.XrefsTo(0x738890):
    f = ida_funcs.get_func(x.frm)
    out.append("  at 0x%X (seg %s) in %s" % (x.frm, idc.get_segm_name(x.frm), idc.get_func_name(f.start_ea) if f else "-"))

out.append("")
out.append("=== vtable entries -> FootClass_ClickedMission (0x4D74E0) ===")
for x in idautils.XrefsTo(0x4D74E0):
    f = ida_funcs.get_func(x.frm)
    out.append("  at 0x%X (seg %s) in %s" % (x.frm, idc.get_segm_name(x.frm), idc.get_func_name(f.start_ea) if f else "-"))

out.append("")
out.append("=== callers of FootClass_ExecutePlanningWaypoint ===")
for x in idautils.XrefsTo(0x4DCA00):
    f = ida_funcs.get_func(x.frm)
    out.append("  0x%X in %s" % (x.frm, idc.get_func_name(f.start_ea) if f else "?"))

# callers of the function containing 0x4DCA60
f = ida_funcs.get_func(0x4DCA93)
out.append("")
out.append("containing func of 0x4DCA93 = %s [0x%X-0x%X]" % (idc.get_func_name(f.start_ea), f.start_ea, f.end_ea))
for x in idautils.XrefsTo(f.start_ea):
    ff = ida_funcs.get_func(x.frm)
    out.append("  0x%X in %s" % (x.frm, idc.get_func_name(ff.start_ea) if ff else "?"))

open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\vt_calls3.txt", "w", encoding="utf-8").write("\n".join(out))
print("SCRIPT DONE")
idc.qexit(0)
