import traceback

out = []

def w():
    open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\event_execute_dump3.txt", "w", encoding="utf-8").write("\n".join(out))

try:
    import idaapi, idautils, idc, ida_funcs, ida_bytes, ida_typeinf

    # --- jump table of Networking_RespondToEvent (0x4C6CD7) ---
    jt = idc.get_name_ea_simple("jpt_4C6CD7")
    out.append("jump table jpt_4C6CD7 @ 0x%X" % jt)
    if jt != idaapi.BADADDR:
        for i in range(46):
            tgt = ida_bytes.get_dword(jt + i * 4)
            out.append("  EventType 0x%02X (idx %2d) -> 0x%08X" % (i + 1, i, tgt))
    out.append("")

    til = ida_typeinf.get_idati()
    for nm in ("vt_TechnoClass", "vt_ObjectClass", "vt_AbstractClass", "vt_UnitClass",
               "vt_FootClass", "vt_BuildingClass", "vt_AircraftClass", "TechnoClass_vtbl"):
        t = ida_typeinf.tinfo_t()
        try:
            ok = t.get_named_type(til, nm)
        except Exception as e:
            out.append("type %s raised %s" % (nm, e))
            continue
        if not ok:
            out.append("type %s not found" % nm)
            continue
        out.append("=== %s size=0x%X ===" % (nm, t.get_size()))
        udt = ida_typeinf.udt_type_data_t()
        try:
            if t.get_udt(udt):
                for m in udt:
                    if 0x340 <= m.offset <= 0x3A0 or m.offset in (0x2C, 0x84, 0x1C4, 0x1E8, 0x1EC, 0x274, 0x280, 0x2B0, 0x47C, 0x480, 0x3C8, 0x4A4):
                        out.append("  +0x%03X  %s" % (m.offset, m.name))
                out.append("  (members %d)" % len(udt))
        except Exception as e:
            out.append("  udt failed: %s" % e)
        out.append("")
except Exception:
    out.append(traceback.format_exc())

w()
print("SCRIPT DONE")
try:
    import idc
    idc.qexit(0)
except Exception:
    pass
