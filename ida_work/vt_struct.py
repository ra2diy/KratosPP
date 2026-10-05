# -*- coding: utf-8 -*-
# Dump the IDA db's vt_* structs (nested base-class layout) + names of targets.
import idaapi, idc, idautils, ida_struct, ida_bytes, ida_name, ida_typeinf

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\vt_struct.txt"
f = open(OUT, "w", encoding="utf-8")


def w(s=""):
    f.write(str(s) + "\n")
    f.flush()


w("[names of targets]")
for ea in (0x5F6A10, 0x6FFBE0, 0x6FFE00, 0x708D90, 0x6F6AC0, 0x5F6B00, 0x5F6B20):
    n = idc.get_name(ea)
    w("  0x%08X raw_name=%r" % (ea, n))
    try:
        w("            demangled=%r" % (idc.demangle_name(n, idc.get_inf_attr(idc.INF_SHORT_DN)),))
        w("            long_dm  =%r" % (idc.demangle_name(n, idc.get_inf_attr(idc.INF_LONG_DN)),))
    except Exception as e:
        w("            demangle err %s" % e)
    t = idc.get_type(ea)
    w("            typeinfo =%r" % (t,))
w()

w("[all structs matching vt_*]")
names = []
for si in range(ida_struct.get_struc_qty()):
    tid = ida_struct.get_struc_by_idx(si)
    nm = ida_struct.get_struc_name(tid)
    if nm and (nm.startswith("vt_") or "vftable" in nm.lower()):
        names.append((nm, tid))
w("   found %d" % len(names))
for nm, tid in sorted(names):
    w("      0x%08X %s" % (tid, nm))
w()


def dump_struct(tid, indent, limit=None, minoff=0, maxoff=None):
    s = ida_struct.get_struc(tid)
    if not s:
        return
    size = ida_struct.get_struc_size(s)
    off = 0
    while off < size:
        m = ida_struct.get_member(s, off)
        if m is None:
            off += 1
            continue
        moff = m.get_soff()
        mname = ida_struct.get_member_name(m.id) or "?"
        try:
            msize = ida_struct.get_member_size(m)
        except Exception:
            msize = 1
        mtid = ida_struct.get_member_struc_id(m.id) if hasattr(ida_struct, "get_member_struc_id") else idaapi.BADADDR
        is_nested = mtid not in (None, idaapi.BADADDR)
        if maxoff is None or moff <= maxoff:
            if moff >= minoff:
                w("%s+0x%03X  %-34s size=%-3d %s" % (
                    indent, moff, mname, msize,
                    ("-> NESTED struct" if is_nested else (idc.get_type(m.id) or ""))))
        if is_nested:
            dump_struct(mtid, indent + "      ", minoff=max(0, minoff - moff), maxoff=None if maxoff is None else maxoff - moff)
        off = moff + max(msize, 1)


for nm, tid in sorted(names):
    if nm in ("vt_AbstractClass", "vt_ObjectClass", "vt_MissionClass", "vt_RadioClass",
              "vt_TechnoClass", "vt_FootClass", "vt_UnitClass", "vt_InfantryClass",
              "vt_BuildingClass", "vt_AircraftClass", "vt_OverlayClass", "vt_CellClass"):
        w("=" * 110)
        w("[struct %s] tid=0x%X" % (nm, tid))
        dump_struct(tid, "  ")
        w()

w("[done]")
f.close()
