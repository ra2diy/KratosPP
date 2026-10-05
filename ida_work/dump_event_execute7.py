import traceback
out = []
try:
    import idaapi, idautils, idc, ida_funcs, ida_bytes

    base = 0x7F5C70
    # vtable extent
    n = 0
    while n < 0x1000:
        p = ida_bytes.get_dword(base + n)
        f = ida_funcs.get_func(p) if p else None
        if not f or f.start_ea != p:
            # allow non-function entries (RTTI / 0) but stop after first big gap
            break
        n += 4
    out.append("vtable 0x%X extent 0x%X (%d slots)" % (base, n, n // 4))
    out.append("")

    for off in (0x13C, 0x140, 0x144, 0x1C4, 0x2C, 0x84, 0x280, 0x2B0, 0x370, 0x374, 0x378, 0x37C, 0x47C, 0x480, 0x3C8, 0x4A4):
        p = ida_bytes.get_dword(base + off)
        f = ida_funcs.get_func(p) if p else None
        out.append("  +0x%03X -> 0x%08X  %-40s func=%s" % (
            off, p, idc.get_name(p), hex(f.start_ea) if f and f.start_ea == p else "NO"))
    out.append("")

    out.append("=== entries +0x300 .. +0x3A0 ===")
    for off in range(0x300, 0x3A0, 4):
        p = ida_bytes.get_dword(base + off)
        f = ida_funcs.get_func(p) if p else None
        ok = "F" if (f and f.start_ea == p) else " "
        out.append("  +0x%03X [%s] 0x%08X  %s" % (off, ok, p, idc.get_name(p)))
    out.append("")

    # disassemble the functions at 0x374/0x378 slots
    for off in (0x374, 0x378):
        p = ida_bytes.get_dword(base + off)
        f = ida_funcs.get_func(p) if p else None
        if not f:
            continue
        out.append("=== disasm of slot +0x%03X = 0x%X (%s) ===" % (off, p, idc.get_name(p)))
        ea = f.start_ea
        cnt = 0
        while ea < f.end_ea and cnt < 60:
            out.append("  0x%08X  %s" % (ea, idc.generate_disasm_line(ea, 0)))
            nxt = idc.next_head(ea)
            if nxt <= ea:
                break
            ea = nxt
            cnt += 1
        out.append("")
except Exception:
    out.append(traceback.format_exc())

open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\event_execute_dump7.txt", "w", encoding="utf-8").write("\n".join(out))
print("SCRIPT DONE")
import idc
idc.qexit(0)
