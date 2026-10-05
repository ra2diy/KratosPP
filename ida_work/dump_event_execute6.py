import traceback
out = []
try:
    import idaapi, idautils, idc, ida_funcs, ida_bytes

    target = 0x738890  # UnitClass_ClickedMission
    cands = []
    for seg in idautils.Segments():
        s = idc.get_segm_start(seg); e = idc.get_segm_end(seg)
        nm = idc.get_segm_name(seg)
        if nm not in (".data", ".rdata", ".text", "_DATA", ".idata"):
            continue
        ea = s
        while ea < e - 4:
            if ida_bytes.get_dword(ea) == target:
                cands.append((ea, nm))
            ea += 4
    out.append("occurrences of %.8X: %s" % (target, ["0x%X(%s)" % c for c in cands]))
    out.append("")

    for a, nm in cands:
        out.append("=== candidate slot 0x%X in %s ===" % (a, nm))
        for off in range(-0x24, 0x28, 4):
            p = ida_bytes.get_dword(a + off)
            out.append("  %+04X  0x%08X  %s" % (off, p, idc.get_name(p) if p else "-"))
        out.append("")
        # find vtable base: walk back while dwords are function starts
        base = a
        steps = 0
        while steps < 0x200:
            p = ida_bytes.get_dword(base - 4)
            f = ida_funcs.get_func(p) if p else None
            if not f or f.start_ea != p:
                break
            base -= 4
            steps += 1
        out.append("  walk-back base=0x%X  (ClickedMission index = %d = 0x%X)" % (base, (a - base) // 4, a - base))
        out.append("")

    # EventClass ctors callers: which event types are produced
    for ctor in (0x4C66C0, 0x4C65E0, 0x4C6AE0, 0x4C6860):
        out.append("=== callers of 0x%X ===" % ctor)
        for x in idautils.XrefsTo(ctor):
            f = ida_funcs.get_func(x.frm)
            out.append("   from 0x%X in %s" % (x.frm, idc.get_func_name(f.start_ea) if f else "?"))
        out.append("")
except Exception:
    out.append(traceback.format_exc())

open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\event_execute_dump6.txt", "w", encoding="utf-8").write("\n".join(out))
print("SCRIPT DONE")
import idc
idc.qexit(0)
