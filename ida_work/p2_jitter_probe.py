# -*- coding: utf-8 -*-
# 替身「移动抖动」位置通道取证探针 v3（只读 IDB 副本 gamemd_p1.idb）
import os
import traceback
import ida_bytes
import idc
import ida_pro
import idautils

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p2_jitter_probe.txt")

f = open(OUT, "w", encoding="utf-8")


def w(s):
    f.write(s + "\n")
    f.flush()


def disasm(ea, limit=60, indent="  "):
    n = 0
    while n < limit:
        dis = idc.GetDisasm(ea)
        if not dis:
            w(indent + "%08X  <cannot disasm>" % ea)
            break
        w(indent + "%08X  %s" % (ea, dis))
        m = idc.print_insn_mnem(ea)
        if m in ("retn", "ret", "retf"):
            break
        sz = idc.get_item_size(ea)
        if sz <= 0:
            w(indent + "  <size 0, stop>")
            break
        ea += sz
        n += 1


def fname_or_zero(ea):
    if ea == 0:
        return "(0)"
    n = idc.get_func_name(ea)
    if not n:
        n = idc.get_name(ea, idc.NAME_NO_STRING) or "(noname)"
    return "%s" % n


segs = []
for se in idautils.Segments():
    segs.append((idc.get_segm_name(se), se, idc.get_segm_end(se)))

w("############ 0. 段表 ############")
for nm, st, en in segs:
    w("  %-12s %08X..%08X size=%d" % (nm, st, en, en - st))
w("")

ANCHOR = 0x5F6BD0
start = end = 0
data = b""
best = None
for nm, st, en in segs:
    if st <= 0x7E22A4 < en:
        best = (nm, st, en)
        break
if best is None:
    for nm, st, en in segs:
        if nm == ".rdata":
            best = (nm, st, en)
            break
w("扫描段: %r" % (best,))
start, end = best[1], best[2]
data = ida_bytes.get_bytes(start, end - start)
w("读取 %d 字节" % len(data))
w("")

hits = []
anchor_le = ANCHOR.to_bytes(4, "little")
pos = 0
while True:
    i = data.find(anchor_le, pos)
    if i < 0:
        break
    pos = i + 1
    if i % 4 != 0:
        continue
    hits.append(start + i - 0xB8)
w("############ A. 命中虚表 base %d 个 ############" % len(hits))
w(" ".join("%08X" % h for h in hits))
w("")

SLOTS = [
    (0x00, "QInterface"),
    (0x48, "GetCoords"),
    (0x4C, "GetDestination"),
    (0x54, "IsInAir"),
    (0x58, "GetCenterCoords"),
    (0x78, "InWhichLayer"),
    (0x84, "GetTechnoType"),
    (0xA4, "GetTargetCoords"),
    (0xA8, "GetDockCoords"),
    (0xAC, "GetRenderCoords"),
    (0xB0, "GetFLH"),
    (0xB8, "GetYSort"),
    (0x1B4, "SetLocation"),
    (0x1C8, "GetHeight"),
    (0x1CC, "SetHeight"),
    (0x1D0, "GetZ"),
]
for vt in hits:
    w("---- vtable base %08X ----" % vt)
    for off, label in SLOTS:
        raw = ida_bytes.get_bytes(vt + off, 4)
        if not raw or len(raw) < 4:
            w("   +%03X %-16s <unreadable>" % (off, label))
            continue
        val = int.from_bytes(raw, "little")
        w("   +%03X %-16s %08X  %s" % (off, label, val, fname_or_zero(val)))
    w("")

try:
    w("############ F. SetLocation / GetHeight / SetHeight / GetYSort impl ############")
    seen = set()
    for vt in hits:
        for off, label in ((0x1B4, "SetLocation"), (0x1C8, "GetHeight"),
                           (0x1CC, "SetHeight"), (0xB8, "GetYSort"), (0x1D0, "GetZ")):
            raw = ida_bytes.get_bytes(vt + off, 4)
            if not raw or len(raw) < 4:
                continue
            val = int.from_bytes(raw, "little")
            if val in seen or val == 0:
                continue
            seen.add(val)
            w("---- %s impl @ %08X (%s) ----" % (label, val, fname_or_zero(val)))
            disasm(val, 90)
            w("")
except Exception as e:
    w("!! F fail: %r" % e)
    w(traceback.format_exc())

for title, ea, lim in (
    ("H1. DriveLocomotionClass_Draw_Matrix 0x4AFF60", 0x4AFF60, 90),
    ("H2. ShipLocomotionClass_Draw_Matrix 0x69F670", 0x69F670, 90),
    ("H3. LocomotionClass_Draw_Matrix 0x55A730", 0x55A730, 60),
    ("H4. ObjectClass_ReturnRealYSort 0x5F6BD0", 0x5F6BD0, 60),
):
    try:
        w("############ %s ############" % title)
        disasm(ea, lim)
        w("")
    except Exception as e:
        w("!! %s fail: %r" % (title, e))

# I. call [reg+slot] 分布
for slot in (0xAC, 0x48, 0x1B4, 0x1CC):
    try:
        w("############ I. call [reg+%03X] 分布 ############" % slot)
        tot = []
        for op in (0x90, 0x91, 0x92, 0x93, 0x95, 0x96, 0x97, 0x94):
            p = bytes([0xFF, op]) + slot.to_bytes(4, "little")
            ppos = 0
            while True:
                j = data.find(p, ppos)
                if j < 0:
                    break
                ppos = j + 1
                tot.append(start + j)
        tot.sort()
        w("共 %d 处" % len(tot))
        for a in tot[:50]:
            w("   %08X  in %s" % (a, idc.get_func_name(a)))
        w("")
    except Exception as e:
        w("!! I(%03X) fail: %r" % (slot, e))

f.close()
ida_pro.qexit(0)
