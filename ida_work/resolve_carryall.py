import idaapi, idautils, idc, ida_funcs, ida_bytes

targets = [0x7388FD, 0x7F5DB4, 0x74041B, 0x6FF8F1, 0x739971]

out = []
for t in targets:
    f = ida_funcs.get_func(t)
    if f:
        name = idc.get_func_name(f.start_ea)
        out.append("ADDR 0x%X  -> FUNC %s [0x%X-0x%X]" % (t, name, f.start_ea, f.end_ea))
    else:
        out.append("ADDR 0x%X  -> no function" % t)

# callers of the function containing 0x7388FD
f1 = ida_funcs.get_func(0x7388FD)
if f1:
    out.append("")
    out.append("=== Callers of %s (0x%X) ===" % (idc.get_func_name(f1.start_ea), f1.start_ea))
    for x in idautils.XrefsTo(f1.start_ea):
        cf = ida_funcs.get_func(x.frm)
        cname = idc.get_func_name(cf.start_ea) if cf else "?"
        out.append("  from 0x%X in %s" % (x.frm, cname))

f2 = ida_funcs.get_func(0x7F5DB4)
if f2:
    out.append("")
    out.append("=== Callers of %s (0x%X) ===" % (idc.get_func_name(f2.start_ea), f2.start_ea))
    for x in idautils.XrefsTo(f2.start_ea):
        cf = ida_funcs.get_func(x.frm)
        cname = idc.get_func_name(cf.start_ea) if cf else "?"
        out.append("  from 0x%X in %s" % (x.frm, cname))

# callers of WhatAction (0x74041B container)
f3 = ida_funcs.get_func(0x74041B)
if f3:
    out.append("")
    out.append("=== Callers of %s (0x%X) ===" % (idc.get_func_name(f3.start_ea), f3.start_ea))
    for x in idautils.XrefsTo(f3.start_ea):
        cf = ida_funcs.get_func(x.frm)
        cname = idc.get_func_name(cf.start_ea) if cf else "?"
        out.append("  from 0x%X in %s" % (x.frm, cname))

open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\carryall_resolve.txt", "w", encoding="utf-8").write("\n".join(out))
print("done")
