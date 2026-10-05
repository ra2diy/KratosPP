import idaapi, idautils, idc, ida_funcs

out = []

def dump(lo, hi, title):
    out.append("=== %s (0x%X-0x%X) ===" % (title, lo, hi))
    ea = lo
    while ea < hi:
        line = idc.generate_disasm_line(ea, 0)
        cmt = idc.get_cmt(ea, 0)
        out.append("0x%08X  %s%s" % (ea, line, ("  ; " + cmt) if cmt else ""))
        nxt = idc.next_head(ea)
        if nxt <= ea:
            break
        ea = nxt
    out.append("")

dump(0x738890, 0x738915, "UnitClass_ClickedMission body")
dump(0x4DCA60, 0x4DCAB0, "FootClass_ExecutePlanningWaypoint @0x4DCA93")
dump(0x730D60, 0x730E95, "GuardCommandClass_Execute")
dump(0x730EA0, 0x730F21, "StopCommandClass_Execute")

# callers of GuardCommandClass_Execute / StopCommandClass_Execute
for addr, nm in ((0x730D60, "GuardCommandClass_Execute"), (0x730EA0, "StopCommandClass_Execute")):
    out.append("=== Callers of %s (0x%X) ===" % (nm, addr))
    for x in idautils.XrefsTo(addr):
        f = ida_funcs.get_func(x.frm)
        out.append("  from 0x%X in %s" % (x.frm, idc.get_func_name(f.start_ea) if f else "?"))
    out.append("")

open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\carryall_disasm.txt", "w", encoding="utf-8").write("\n".join(out))
print("SCRIPT DONE")
idc.qexit(0)
