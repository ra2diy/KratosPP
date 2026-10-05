# -*- coding: utf-8 -*-
# Who has 0x5F6A10 at vtable+0x1C4 and 0x6FFBE0 at vtable+0x378? Any subclass override?
import idaapi, idc, idautils, ida_bytes, ida_name, ida_segment, ida_funcs

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\vt_slots.txt"
f = open(OUT, "w", encoding="utf-8")


def w(s=""):
    f.write(str(s) + "\n")
    f.flush()


A = 0x5F6A10   # slot 0x1C4 candidate (YRpp: ObjectClass::GetCellAgain)
B = 0x6FFBE0   # slot 0x378 candidate (YRpp: TechnoClass::ClickedMission)

w("[symbols]")
for ea in (A, B, 0x6FFE00, 0x5F6B20, 0x6F6AC0, 0x708D90, 0x6FBDC0):
    raw = idc.get_name(ea)
    w("  0x%08X raw=%r" % (ea, raw))
    for flag, lbl in ((idc.INF_SHORT_DN, "short"), (idc.INF_LONG_DN, "long")):
        try:
            w("            %-5s=%r" % (lbl, idc.demangle_name(raw, idc.get_inf_attr(flag))))
        except Exception as e:
            w("            %s err %s" % (lbl, e))
w()

vtabs = [(ea, n) for ea, n in idautils.Names() if n.startswith("??_7") and "@@6B@" in n]
w("[vtable count] %d" % len(vtabs))

# reference prefix from ??_7ObjectClass@@6B@ (QueryInterface/AddRef/Release = slots 0..2)
ref = None
for ea, n in vtabs:
    if n == "??_7ObjectClass@@6B@":
        ref = (ea, [ida_bytes.get_dword(ea + 4 * i) for i in range(3)])
w("[??_7ObjectClass@@6B@] %s" % (("%08X prefix=%s" % (ref[0], [hex(x) for x in ref[1]])) if ref else "NOT FOUND"))
w()


def is_obj_derived(ea):
    if ref is None:
        return False
    return all(ida_bytes.get_dword(ea + 4 * i) == ref[1][i] for i in range(3))


def fname(ea):
    n = idc.get_name(ea)
    return n or ""


groups = {"1C4": {}, "378": {}}
rows = []
for ea, n in sorted(vtabs):
    if not is_obj_derived(ea):
        continue
    d1 = ida_bytes.get_dword(ea + 0x1C4)
    d2 = ida_bytes.get_dword(ea + 0x378)
    rows.append("%-56s vt=0x%08X  +1C4=0x%08X %-34s  +378=0x%08X %s" % (
        n, ea, d1, fname(d1), d2, fname(d2)))
    groups["1C4"].setdefault(d1, []).append(n)
    groups["378"].setdefault(d2, []).append(n)

w("[ObjectClass-derived vtables and their +0x1C4 / +0x378] count=%d" % len(rows))
for r in rows:
    w("  " + r)
w()

for key, tgt, yname in (("1C4", A, "ObjectClass::GetCellAgain"), ("378", B, "TechnoClass::ClickedMission")):
    w("### slot +0x%s  (expect 0x%08X = YRpp %s)" % (key, tgt, yname))
    for val, names in sorted(groups[key].items(), key=lambda kv: -len(kv[1])):
        tag = "  <<<< EXPECTED" if val == tgt else "  <<<< DIFFERENT (override?)"
        w("   0x%08X  %-4d vtables%s  e.g. %s" % (val, len(names), tag, ", ".join(sorted(names)[:6])))
    w()

w("[xrefs to A]")
for x in idautils.XrefsTo(A):
    seg = ida_segment.getseg(x.frm)
    w("   from 0x%08X seg=%s" % (x.frm, ida_segment.get_segm_name(seg) if seg else "?"))
w("[xrefs to B]")
for x in idautils.XrefsTo(B):
    seg = ida_segment.getseg(x.frm)
    fn = ida_funcs.get_func(x.frm)
    w("   from 0x%08X seg=%s in %s" % (x.frm, ida_segment.get_segm_name(seg) if seg else "?",
                                       fname(fn.start_ea) if fn else "?"))
w("[done]")
f.close()
import ida_pro
ida_pro.qexit(0)
