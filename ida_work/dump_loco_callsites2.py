# -*- coding: utf-8 -*-
# 修正版：字节级定位 ILocomotion::In_Which_Layer 真实调用点
import idaapi, idc, idautils, ida_bytes, ida_segment, ida_funcs

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\loco_callsites2.txt"
f = open(OUT, "w", encoding="utf-8")
def w(s=""):
    f.write(str(s) + "\n"); f.flush()

TARGETS = {
    0x75C7E0: ("WalkLoco",  0x7F69F8),
    0x6A3E50: ("ShipLoco",  0x7F2D8C),
    0x5B19D0: ("MechLoco",  0x7EDB6C),
    0x517100: ("HoverLoco", 0x7EACFC),
    0x4B4820: ("DriveLoco", 0x7E7EB0),
}

def find_dword_holders(val):
    """在整镜像里找所有等于 val 的 dword 地址"""
    res = []
    for segname in (".rdata", ".data", ".text"):
        seg = ida_segment.get_segm_by_name(segname)
        if not seg:
            continue
        data = ida_bytes.get_bytes(seg.start_ea, seg.end_ea - seg.start_ea)
        if not data:
            continue
        tgt = val.to_bytes(4, "little")
        i = data.find(tgt)
        while i != -1:
            res.append(seg.start_ea + i)
            i = data.find(tgt, i + 1)
    return res

def scan_calls(slot_off):
    hits = []
    seg = ida_segment.get_segm_by_name(".text")
    if not seg:
        return hits
    start = seg.start_ea
    data = ida_bytes.get_bytes(start, seg.end_ea - seg.start_ea)
    if not data:
        return hits
    ln = len(data)
    i = 0
    while i < ln - 6:
        if data[i] == 0xFF and 0x90 <= data[i+1] <= 0x97:
            disp = int.from_bytes(data[i+2:i+6], "little", signed=True)
            if disp == slot_off:
                hits.append(start + i)
        i += 1
    return hits

# 先确定真实的 vtable 槽地址（字节级）
SLOTS = {}
for addr, (nm, vtguess) in TARGETS.items():
    holders = find_dword_holders(addr)
    w("=" * 96)
    w("TARGET %08X %s : dword holders = %s" % (addr, nm, ", ".join("%08X" % h for h in holders)))
    for h in holders:
        off = h - vtguess
        w("   holder %08X  offset vs guess(%08X) = %X" % (h, vtguess, off))
        SLOTS.setdefault(off, []).append((nm, h))

w("=" * 96)
w("槽偏移汇总: %s" % ", ".join("%X x%d" % (o, len(v)) for o, v in SLOTS.items()))

# 用最常见的偏移做调用点扫描
for off, lst in SLOTS.items():
    w("=" * 96)
    w("### 扫描 call dword ptr [reg+%X]  (来自 %s)" % (off, ", ".join("%s@%X" % (n, h) for n, h in lst)))
    hits = scan_calls(off)
    w("  hits = %d" % len(hits))
    for h in hits:
        fn = ida_funcs.get_func(h)
        w("  --- @%08X  in %s ---" % (h, idc.get_func_name(fn.start_ea) if fn else "?"))
        p = h
        ctx = []
        for _ in range(14):
            p = idc.prev_head(p)
            if p == idaapi.BADADDR:
                break
            ctx.append("      %08X  %s" % (p, idc.generate_disasm_line(p, 0)))
        for line in reversed(ctx):
            w(line)

f.close()
print("DONE")
