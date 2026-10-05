import idaapi, idautils, idc, ida_funcs

out = []

def fname(ea):
    f = ida_funcs.get_func(ea)
    return (idc.get_func_name(f.start_ea), f.start_ea, f.end_ea) if f else ("<none>", 0, 0)

out.append("=== Resolve key hook functions ===")
for a in [0x7388FD, 0x74041B, 0x730DEB, 0x730E56, 0x730EEB, 0x707000]:
    n, s, e = fname(a)
    out.append("  0x%08X -> %s [0x%X-0x%X]" % (a, n, s, e))

out.append("")
out.append("=== Functions in 0x7F5000-0x7F7000 ===")
for ea in idautils.Functions(0x7F1000, 0x7F8000):
    out.append("  0x%X  %s" % (ea, idc.get_func_name(ea)))

out.append("")
out.append("=== Disasm 0x7F5D70-0x7F5DD0 (ClickedMission caller) ===")
ea = 0x7F5D70
while ea < 0x7F5DD0:
    out.append("0x%08X  %s" % (ea, idc.generate_disasm_line(ea, 0)))
    nxt = idc.next_head(ea)
    if nxt <= ea:
        break
    ea = nxt

out.append("")
out.append("=== Disasm 0x7F5CA0-0x7F5D00 (What_Action caller) ===")
ea = 0x7F5CA0
while ea < 0x7F5D00:
    out.append("0x%08X  %s" % (ea, idc.generate_disasm_line(ea, 0)))
    nxt = idc.next_head(ea)
    if nxt <= ea:
        break
    ea = nxt

# search backwards from 0x7F5DB4 for the nearest function start
out.append("")
for probe in (0x7F5DB4, 0x7F5CE4):
    cur = probe
    f = None
    while cur > 0x7E0000:
        f = ida_funcs.get_func(cur)
        if f:
            break
        cur -= 1
    out.append("Probe 0x%X nearest enclosing func start = 0x%X (%s)" % (probe, f.start_ea if f else 0, idc.get_func_name(f.start_ea) if f else "NONE"))

# who calls the enclosing function
if ida_funcs.get_func(0x7F5DB4):
    f = ida_funcs.get_func(0x7F5DB4)
    out.append("")
    out.append("=== Callers of 0x%X ===" % f.start_ea)
    for x in idautils.XrefsTo(f.start_ea):
        out.append("  from 0x%X in %s" % (x.frm, fname(x.frm)[0]))

open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\caller_area.txt", "w", encoding="utf-8").write("\n".join(out))
print("SCRIPT DONE")
idc.qexit(0)
