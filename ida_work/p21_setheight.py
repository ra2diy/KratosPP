# -*- coding: utf-8 -*-
# p21: 定位 TechnoClass/FootClass 虚表的 +0x1C8(GetHeight) / +0x1CC(SetHeight) / +0x1D0(GetZ)
#      并反汇编 SetHeight，确认它到底写哪个字段。
import idaapi, idc, idautils, ida_bytes, ida_funcs, ida_hexrays

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p21_setheight.txt"
f = open(OUT, "w", encoding="utf-8")


def w(s=""):
    f.write(str(s) + "\n")
    f.flush()


def disasm(ea, n=40):
    lines = []
    cur = ea
    for _ in range(n):
        lines.append("%08X  %s" % (cur, idc.generate_disasm_line(cur, 0)))
        cur = idc.next_head(cur)
        if cur == idaapi.BADADDR:
            break
    return lines


try:
    # 逐字节扫描 .rdata / 全部段，找 dword == TechnoClass::Mark(0x6F4A70)
    targets = {
        0x6F4A70: "TechnoClass::Mark",
        0x5F5850: "ObjectClass::Mark",
        0x5F3F30: "ObjectClass::Update",
        0x5F6940: "ObjectClass::SetLocation",
    }
    vt_hits = []
    for seg in idautils.Segments():
        s = idc.get_segm_start(seg)
        e = idc.get_segm_end(seg)
        data = ida_bytes.get_bytes(s, e - s)
        if not data:
            continue
        for ptr, name in targets.items():
            needle = ptr.to_bytes(4, "little")
            off = data.find(needle)
            while off != -1:
                ea = s + off
                vt_hits.append((ea, ptr, name))
                off = data.find(needle, off + 1)
    w("== dword 命中（可能是虚表槽）==")
    for ea, ptr, name in vt_hits:
        w("%08X  -> %08X %s" % (ea, ptr, name))

    # 由 TechnoClass::Mark 的命中点反推虚表基址（槽 +0x124）
    w()
    w("== 反推虚表基址（slot+0x124 == 0x6F4A70）==")
    bases = set()
    for ea, ptr, name in vt_hits:
        if ptr == 0x6F4A70:
            bases.add(ea - 0x124)
    for b in sorted(bases):
        w("vtable base = %08X" % b)
        for slot, label in ((0x120, "See"), (0x124, "Mark"), (0x18C, "UpdatePosition"),
                            (0x1B4, "SetCoords"), (0x1C8, "GetHeight"), (0x1CC, "SetHeight"),
                            (0x1D0, "GetZ")):
            tgt = ida_bytes.get_dword(b + slot)
            nm = idc.get_func_name(tgt) or ""
            w("   +0x%03X %-16s = %08X  %s" % (slot, label, tgt, nm))
except Exception as ex:
    w("EXC1: %r" % (ex,))

# 反汇编 SetHeight / GetHeight / GetZ
try:
    w()
    w("== SetHeight 目标反汇编 ==")
    bases = set()
    for ea, ptr, name in vt_hits:
        if ptr == 0x6F4A70:
            bases.add(ea - 0x124)
    seen = set()
    for b in sorted(bases):
        for slot, label in ((0x1C8, "GetHeight"), (0x1CC, "SetHeight"), (0x1D0, "GetZ")):
            tgt = ida_bytes.get_dword(b + slot)
            if tgt in seen:
                continue
            seen.add(tgt)
            w()
            w("--- vt %08X +0x%03X %s -> %08X (%s) ---" % (b, slot, label, tgt, idc.get_func_name(tgt)))
            for ln in disasm(tgt, 26):
                w("   " + ln)
except Exception as ex:
    w("EXC2: %r" % (ex,))

# 尝试反编译 SetHeight
try:
    w()
    w("== SetHeight 反编译 ==")
    seen2 = set()
    for b in sorted(bases):
        tgt = ida_bytes.get_dword(b + 0x1CC)
        if tgt in seen2:
            continue
        seen2.add(tgt)
        try:
            cf = ida_hexrays.decompile(tgt)
            w("--- %08X ---" % tgt)
            w(cf)
        except Exception as ex:
            w("decompile fail %08X: %r" % (tgt, ex))
except Exception as ex:
    w("EXC3: %r" % (ex,))

# 顺带：ObjectClass::Update 里 SetHeight/GetHeight 调用上下文（0x5F3F30 起）
try:
    w()
    w("== ObjectClass::Update 0x5F3F30 头部反汇编 ==")
    for ln in disasm(0x5F3F30, 40):
        w("   " + ln)
except Exception as ex:
    w("EXC4: %r" % (ex,))

f.close()
print("DONE p21")
