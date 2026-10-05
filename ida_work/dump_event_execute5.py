import traceback
out = []
try:
    import idaapi, idautils, idc, ida_funcs, ida_bytes

    pats = ("ClickedEvent", "ClickedMission", "vftable", "vt_TechnoClass", "vtable")
    hits = []
    for ea, nm in idautils.Names():
        if not nm:
            continue
        for p in pats:
            if p in nm:
                hits.append((ea, nm))
                break
    out.append("hits=%d" % len(hits))
    for ea, nm in hits[:120]:
        out.append("  0x%08X  %s" % (ea, nm))
    out.append("")
except Exception:
    out.append(traceback.format_exc())

open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\event_execute_dump5.txt", "w", encoding="utf-8").write("\n".join(out))
print("SCRIPT DONE")
import idc
idc.qexit(0)
