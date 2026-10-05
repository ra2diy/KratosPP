import idaapi, idautils, idc, ida_funcs

out = []

seg = None
for s in idautils.Segments():
    if idc.get_segm_name(s) == ".text":
        seg = s
        break
start = idc.get_segm_start(seg)
end = idc.get_segm_end(seg)
out.append(".text = 0x%X-0x%X" % (start, end))

targets = {0x144, 0x140, 0x148}
hits = []
ea = start
while ea < end:
    mnem = idc.print_insn_mnem(ea)
    if mnem in ("call", "jmp"):
        ot = idc.get_operand_type(ea, 0)
        if ot in (3, 4):  # o_displ=3, o_phrase... print for inspection
            disp = idc.get_operand_value(ea, 0)
            if disp in targets:
                f = ida_funcs.get_func(ea)
                hits.append((ea, mnem, disp, idc.print_operand(ea, 0), idc.get_func_name(f.start_ea) if f else "?"))
    ea = idc.next_head(ea)
    if ea == idaapi.BADADDR:
        break

out.append("=== indirect calls with displacement 0x140/0x144/0x148 ===")
for ea, mnem, v, opnd, fn in hits:
    out.append("  0x%08X  %s %s   in %s" % (ea, mnem, opnd, fn))

open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\vt_calls2.txt", "w", encoding="utf-8").write("\n".join(out))
print("SCRIPT DONE")
idc.qexit(0)
