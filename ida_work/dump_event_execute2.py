import idaapi, idautils, idc, ida_funcs, ida_bytes

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

def funcname(a):
    f = ida_funcs.get_func(a)
    if not f:
        return "?"
    return "%s (0x%X)" % (idc.get_func_name(f.start_ea), f.start_ea)

# 1. IDLE branch tail (after Phobos hook site)
dump(0x4C75C0, 0x4C7700, "IDLE handler tail (continues at 0x4C75C0)")

# 2. where does 0x4C8114 come from?
dump(0x4C8090, 0x4C8150, "unknown xref to IDLE branch")

# 3. DoList execution loop that calls RespondToEvent
dump(0x64C8B0, 0x64C940, "caller of RespondToEvent #1")
dump(0x64CBC0, 0x64CC40, "caller of RespondToEvent #2")

# 4. Locate every `lea ecx,[esi+7] / call TargetClass::As_Techno` switch case entry
#    dump the whole switch body of Networking_RespondToEvent
dump(0x4C6D60, 0x4C7400, "Networking_RespondToEvent switch body (mid)")

# 5. vtable member names: vt_TechnoClass + 0x1C4 / 0x374 / 0x378
for vtsym in ("vt_TechnoClass", "??_7TechnoClass@@6B@", "vt_UnitClass", "vt_FootClass"):
    ea = idc.get_name_ea_simple(vtsym)
    out.append("symbol %s -> 0x%X" % (vtsym, ea))
    if ea != idaapi.BADADDR:
        for off in (0x1C4, 0x2B0, 0x3C8, 0x374, 0x378, 0x480, 0x47C, 0x84):
            ptr = ida_bytes.get_dword(ea + off)
            out.append("   +0x%03X -> 0x%08X  %s" % (off, ptr, idc.get_name(ptr) if ptr else "?"))
    out.append("")

# 6. sub_6386E0 / sub_6E7A80 / sub_6E6E20 identity
for a in (0x6386E0, 0x6E7A80, 0x6E6E20, 0x710550):
    out.append("0x%X = %s" % (a, funcname(a)))
    f = ida_funcs.get_func(a)
    if f:
        dump(f.start_ea, min(f.start_ea + 0x40, f.end_ea), "prologue of 0x%X" % a)

open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\event_execute_dump2.txt", "w", encoding="utf-8").write("\n".join(out))
print("SCRIPT DONE")
idc.qexit(0)
