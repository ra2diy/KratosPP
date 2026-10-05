import traceback
out = []
try:
    import idaapi, idautils, idc, ida_funcs, ida_bytes, ida_typeinf

    name = "vt_TechnoClass"
    t = ida_typeinf.tinfo_t()
    ok = t.get_named_type(ida_typeinf.get_idati(), name)
    out.append("get_named_type(%s) -> %s size=0x%X" % (name, ok, t.get_size()))
    tid = t.get_tid()
    out.append("tid=%s" % tid)
    off = 0
    while off < 0x4D4:
        nm = idc.get_member_name(tid, off)
        if nm:
            out.append("  +0x%03X  %s" % (off, nm))
        off += 4
except Exception:
    out.append(traceback.format_exc())

open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\event_execute_dump4.txt", "w", encoding="utf-8").write("\n".join(out))
print("SCRIPT DONE")
import idc
idc.qexit(0)
