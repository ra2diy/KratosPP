# -*- coding: utf-8 -*-
# 定位 ILocomotion::In_Which_Layer 的真实调用点，判定 hook 处 ESI / 栈参数是什么
import idaapi, idc, idautils, ida_bytes, ida_segment, ida_funcs

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\loco_callsites.txt"
f = open(OUT, "w", encoding="utf-8")
def w(s=""):
    f.write(str(s) + "\n")

TARGETS = {
    0x75C7E0: ("WalkLoco",  "??_7WalkLocomotionClass@@6BILocomotion@@@"),
    0x6A3E50: ("ShipLoco",  "??_7ShipLocomotionClass@@6BILocomotion@@@"),
    0x5B19D0: ("MechLoco",  "??_7MechLocomotionClass@@6BILocomotion@@@"),
    0x517100: ("HoverLoco", "??_7HoverLocomotionClass@@6BILocomotion@@@"),
    0x4B4820: ("DriveLoco", "??_7DriveLocomotionClass@@6BILocomotion@@@"),
}

def scan_calls(slot_off):
    hits = []
    seg = ida_segment.get_segm_by_name(".text")
    if not seg:
        return hits
    start, end = seg.start_ea, seg.end_ea
    data = ida_bytes.get_bytes(start, end - start)
    if not data:
        return hits
    pat = b"\xFF"
    ln = len(data)
    i = 0
    while i < ln - 7:
        if data[i] == 0xFF and 0x90 <= data[i+1] <= 0x97:
            disp = int.from_bytes(data[i+2:i+6], "little", signed=True)
            if disp == slot_off:
                hits.append(start + i)
        i += 1
    return hits

allhits = {}
for addr, (nm, vtname) in TARGETS.items():
    w("=" * 96)
    w("TARGET %08X  %s" % (addr, nm))
    vtstart = idc.get_name_ea_simple(vtname)
    w("  vtable name %s -> %s" % (vtname, ("%08X" % vtstart) if vtstart != idaapi.BADADDR else "NOT FOUND"))
    holders = [x.frm for x in idautils.XrefsTo(addr)]
    w("  slot holders: %s" % ", ".join("%08X" % h for h in holders))
    if holders and vtstart != idaapi.BADADDR:
        slot = holders[0]
        off = slot - vtstart
        w("  slot index offset = %X (%d)" % (off, off))
        # 打印 vtable 前 0x50 项
        w("  --- vtable dump ---")
        for i in range(0, 0x50, 4):
            ea = vtstart + i
            v = ida_bytes.get_dword(ea)
            nmv = idc.get_name(v) or ""
            w("     +%02X  %08X: %08X  %s" % (i, ea, v, nmv))
        hits = scan_calls(off)
        w("  --- call sites (call dword ptr [reg+%X]) count=%d ---" % (off, len(hits)))
        for h in hits[:60]:
            fn = ida_funcs.get_func(h)
            w("    @%08X  in %s" % (h, idc.get_func_name(fn.start_ea) if fn else "?"))
            p = h
            ctx = []
            for _ in range(20):
                p = idc.prev_head(p)
                if p == idaapi.BADADDR:
                    break
                ctx.append("        %08X  %s" % (p, idc.generate_disasm_line(p, 0)))
            for line in reversed(ctx):
                w(line)
        allhits.setdefault(off, []).extend(hits)

w("=" * 96)
w("SUMMARY by slot offset:")
for off, hs in allhits.items():
    w("  offset %X : %d call sites -> %s" % (off, len(hs), ", ".join("%08X" % x for x in sorted(set(hs)))))

f.close()
print("DONE")
