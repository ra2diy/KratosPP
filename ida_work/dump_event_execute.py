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

# EventClass::Execute -- Idle branch + Phobos hook site 0x4C7512 (6 bytes)
dump(0x4C7400, 0x4C75C0, "EventClass_Execute around Idle branch")

# Function boundaries / names
for addr in (0x4C74CB, 0x4C7512, 0x4C7462, 0x4C6CC8):
    f = ida_funcs.get_func(addr)
    out.append("0x%08X -> func 0x%X %s" % (addr, f.start_ea if f else 0,
                                           idc.get_func_name(f.start_ea) if f else "?"))
out.append("")

for addr in (0x4C74CB, 0x4C7512):
    out.append("=== Xrefs to 0x%X ===" % addr)
    for x in idautils.XrefsTo(addr):
        f = ida_funcs.get_func(x.frm)
        out.append("  from 0x%X in %s" % (x.frm, idc.get_func_name(f.start_ea) if f else "?"))
    out.append("")

# Who calls EventClass::Execute
f = ida_funcs.get_func(0x4C7400)
if f:
    out.append("=== Callers of %s (0x%X) ===" % (idc.get_func_name(f.start_ea), f.start_ea))
    for x in idautils.XrefsTo(f.start_ea):
        cf = ida_funcs.get_func(x.frm)
        out.append("  from 0x%X in %s" % (x.frm, idc.get_func_name(cf.start_ea) if cf else "?"))
    out.append("")

# Sub_4C6CC8 (RespondToEvent) region: how the event is dispatched
dump(0x4C6C60, 0x4C6D60, "Networking_RespondToEvent area")

open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\event_execute_dump.txt", "w", encoding="utf-8").write("\n".join(out))
print("SCRIPT DONE")
idc.qexit(0)
