# -*- coding: utf-8 -*-
# Dump vtables and locate slots for 0x5F6A10 / 0x6FFBE0, plus disasm.
import idaapi, idc, idautils, ida_bytes, ida_funcs, ida_name, ida_segment

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\vt_probe.txt"
f = open(OUT, "w", encoding="utf-8")


def w(s=""):
    f.write(str(s) + "\n")
    f.flush()


CODEPTR_LO = 0x401000
CODEPTR_HI = 0x7E0000

TARGETS = {0x5F6A10: "TARGET_A(GetCell1?)", 0x6FFBE0: "TARGET_B(Player_Assign_Mission?)"}
ANCHOR_LIMBO = 0x6F6AC0  # TechnoClass::Limbo per YRpp header JMP_THIS(0x6F6AC0)

vtabs = []
for ea, name in idautils.Names():
    if name.startswith("??_7") and "@@6B@" in name:
        vtabs.append((ea, name))
w("[vtable symbols] count=%d" % len(vtabs))
for ea, name in sorted(vtabs):
    w("   0x%08X  %s" % (ea, name))
w()


def seg_of(ea):
    s = ida_segment.getseg(ea)
    if not s:
        return "?"
    return ida_segment.get_segm_name(s)


def scan_vtable(ea, maxn=1200):
    """read dwords while they look like code pointers"""
    out = []
    for i in range(maxn):
        v = ida_bytes.get_byte(ea)  # noqa
        d = ida_bytes.get_dword(ea + 4 * i)
        if d < CODEPTR_LO or d >= CODEPTR_HI:
            break
        if seg_of(d) not in (".text", ".rdata"):
            break
        if seg_of(d) == ".rdata" and seg_of(ea) == ".rdata":
            # allow RTTI pointers only for the first entry (COL); require text
            break
        out.append((i, d))
    return out


w("=" * 110)
w("[vtable contents]")
info = {}
for ea, name in sorted(vtabs):
    ent = scan_vtable(ea)
    info[ea] = (name, ent)
    w("--- 0x%08X %-60s slots=%d (bytes=0x%X)" % (ea, name, len(ent), 4 * len(ent)))
w()

w("=" * 110)
w("[who owns target functions]")
for t, label in sorted(TARGETS.items()):
    w("### 0x%08X %s" % (t, label))
    hit = False
    for ea, (name, ent) in sorted(info.items()):
        for i, d in ent:
            if d == t:
                w("    slot %4d  byte +0x%03X   vtable 0x%08X %s" % (i, 4 * i, ea, name))
                hit = True
    if not hit:
        w("    (not found in any scanned vtable)")
    w()

w("=" * 110)
w("[Limbo anchor 0x%08X]" % ANCHOR_LIMBO)
for ea, (name, ent) in sorted(info.items()):
    for i, d in ent:
        if d == ANCHOR_LIMBO:
            w("    slot %4d  byte +0x%03X   vtable 0x%08X %s" % (i, 4 * i, ea, name))
w()

CHAIN = ["AbstractClass", "ObjectClass", "MissionClass", "RadioClass", "TechnoClass",
         "FootClass", "UnitClass", "InfantryClass", "BuildingClass", "AircraftClass"]
w("=" * 110)
w("[chain vtables: entries at interesting offsets]")
OFFS = [0x2C, 0x1C0, 0x1C4, 0x1C8, 0x1CC, 0x1D0, 0x1D4, 0x1D8, 0x1DC, 0x1E0,
        0x2A8, 0x2AC, 0x2B0, 0x2DC, 0x2E0, 0x2E4, 0x2FC, 0x338, 0x340,
        0x370, 0x374, 0x378, 0x37C, 0x380, 0x384]


def fname(ea):
    n = ida_name.get_name(ea)
    return n if n else ""


for cname in CHAIN:
    for ea, name in sorted(vtabs):
        if name == "??_7%s@@6B@" % cname:
            ent = info[ea][1]
            w("### %s  vtable 0x%08X  slots=%d (0x%X bytes)" % (cname, ea, len(ent), 4 * len(ent)))
            for off in OFFS:
                i = off // 4
                v = ent[i][1] if i < len(ent) else None
                if v is None:
                    w("     +0x%03X  <beyond vtable end>" % off)
                else:
                    w("     +0x%03X [%3d] = 0x%08X  %s" % (off, i, v, fname(v)))
            w()


def disasm(ea, title, n=400):
    w("=" * 110)
    w("[disasm %s] 0x%08X" % (title, ea))
    cur = ea
    for _ in range(n):
        if cur == idaapi.BADADDR:
            break
        try:
            line = idc.generate_disasm_line(cur, 0)
        except Exception:
            break
        cmt = ida_bytes.get_cmt(cur, 0)
        w("  0x%08X  %s%s" % (cur, line, ("   ; " + cmt) if cmt else ""))
        nxt = idc.next_head(cur, ea + 0x2000)
        if nxt == idaapi.BADADDR or nxt == cur:
            break
        if idc.get_func_attr(cur, idc.FUNCATTR_END) and nxt >= idc.get_func_attr(cur, idc.FUNCATTR_END):
            break
        cur = nxt
    w()


try:
    import ida_hexrays
    HAS_HX = ida_hexrays.init_hexrays_plugin()
except Exception:
    HAS_HX = False
w("[hexrays] %s" % HAS_HX)


def decompile(ea, title):
    w("=" * 110)
    w("[decompile %s] 0x%08X" % (title, ea))
    if not HAS_HX:
        w("   (unavailable)")
        return
    try:
        cf = ida_hexrays.decompile(ea)
        w(str(cf))
    except Exception as e:
        w("   ERROR %s" % e)
    w()


for t, label in sorted(TARGETS.items()):
    disasm(t, label)
    decompile(t, label)

w("=" * 110)
w("[xrefs to targets]")
for t, label in sorted(TARGETS.items()):
    w("### 0x%08X %s" % (t, label))
    for x in idautils.XrefsTo(t):
        ff = ida_funcs.get_func(x.frm)
        w("    from 0x%08X type=%d  in_func=%s (%s)" % (
            x.frm, x.type, fname(ff.start_ea) if ff else "?", "0x%08X" % ff.start_ea if ff else "?"))
    w()

# guard hotkey callers
for a in (0x730D60, 0x730DEB, 0x730E56, 0x730EEB):
    w("[func at 0x%08X] %s" % (a, fname(a)))
    ff = ida_funcs.get_func(a)
    if ff:
        w("   func start 0x%08X %s  end 0x%08X" % (ff.start_ea, fname(ff.start_ea), ff.end_ea))
w()
disasm(0x730D60, "GuardCommandClass_Execute?", 400)

w("[done]")
f.close()
