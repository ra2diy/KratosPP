import traceback
out = []
try:
    import idaapi, idautils, idc, ida_funcs, ida_bytes

    for addr, nm in ((0x738890, "UnitClass_ClickedMission"),
                     (0x6FFBE0, "Player_Assign_Mission"),
                     (0x6FFE00, "TechnoClass_374 = ClickedEvent(NetworkEvents)"),
                     (0x738910, "UnitClass_140"),
                     (0x5F6A10, "TechnoClass_GetCell1")):
        out.append("=== code xrefs to 0x%X (%s) ===" % (addr, nm))
        for x in idautils.XrefsTo(addr):
            if x.iscode:
                f = ida_funcs.get_func(x.frm)
                out.append("   from 0x%X in %s" % (x.frm, idc.get_func_name(f.start_ea) if f else "?"))
        out.append("")

    # retn of Player_Assign_Mission : scan its body for 'retn'
    f = ida_funcs.get_func(0x6FFBE0)
    if f:
        out.append("Player_Assign_Mission 0x%X..0x%X" % (f.start_ea, f.end_ea))
        ea = f.start_ea
        while ea < f.end_ea:
            line = idc.generate_disasm_line(ea, 0)
            if line.startswith("retn") or "retn" in line.split()[0:1]:
                out.append("   0x%08X  %s" % (ea, line))
            nxt = idc.next_head(ea)
            if nxt <= ea:
                break
            ea = nxt
    out.append("")

    # ida name of 0x4C6860 and 0x4C65E0
    for a in (0x4C6860, 0x4C65E0, 0x4C66C0, 0x639FD0, 0x4C74CB, 0x4C71CA):
        out.append("0x%X = %s" % (a, idc.get_name(a)))
except Exception:
    out.append(traceback.format_exc())

open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\event_execute_dump8.txt", "w", encoding="utf-8").write("\n".join(out))
print("SCRIPT DONE")
import idc
idc.qexit(0)
