# -*- coding: utf-8 -*-
# p24: 判定 Ship / Drive / Walk / Hover 等 locomotor 的 Draw_Matrix(+0x24) 分别指向谁，
#      以及各自是否处理 AngleRotatedForwards/Sideways（倾斜）。
import idaapi, idc, idautils, ida_bytes

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p24_ship.txt"
f = open(OUT, "w", encoding="utf-8")


def w(s=""):
    f.write(str(s) + "\n")
    f.flush()


def dis(ea, n=90, stop=None):
    cur = ea
    for _ in range(n):
        if stop is not None and cur >= stop:
            break
        w("   %08X  %s" % (cur, idc.generate_disasm_line(cur, 0)))
        nxt = idc.next_head(cur)
        if nxt == idaapi.BADADDR or nxt == cur:
            break
        cur = nxt


# 找出所有把某函数放在 +0x24 的虚表
def vtables_pointing_to(func_ea):
    res = []
    for seg in idautils.Segments():
        s = idc.get_segm_start(seg)
        e = idc.get_segm_end(seg)
        data = ida_bytes.get_bytes(s, e - s)
        if not data:
            continue
        needle = func_ea.to_bytes(4, "little")
        off = data.find(needle)
        while off != -1:
            res.append(s + off)
            off = data.find(needle, off + 1)
    return res


TARGETS = {
    0x4AFF60: "DriveLocomotionClass_Draw_Matrix",
    0x55A730: "LocomotionClass_ILocomotion_DrawMatrix (base)",
}
for ea, nm in TARGETS.items():
    w("== dword hits for %08X (%s) ==" % (ea, nm))
    for hit in vtables_pointing_to(ea):
        w("   %08X   (若为 +0x24 槽, 则 vtable base = %08X)" % (hit, hit - 0x24))
    w()

# 尝试用 IDA 名字库找 Ship
try:
    w("== IDA 名字含 Ship 的函数/符号 ==")
    for ea, nm in idautils.Names():
        if "ShipLocomotion" in nm or "Ship_Loco" in nm:
            w("   %08X  %s" % (ea, nm))
except Exception as ex:
    w("EXC names: %r" % (ex,))

# 逐个检查候选虚表的 +0x24 / +0x28
try:
    w()
    w("== 候选虚表 quick scan (0x7E7E00..0x7F8000, 每 4 字节中含 .text 指针的起点) ==")
    bases = set()
    for ea in TARGETS:
        for hit in vtables_pointing_to(ea):
            bases.add(hit - 0x24)
    for b in sorted(bases):
        t24 = ida_bytes.get_dword(b + 0x24)
        t28 = ida_bytes.get_dword(b + 0x28)
        t0 = ida_bytes.get_dword(b + 0x00)
        w("   vt %08X : +0x00=%08X(%s)  +0x24=%08X(%s)  +0x28=%08X(%s)"
          % (b, t0, idc.get_func_name(t0), t24, idc.get_func_name(t24),
             t28, idc.get_func_name(t28)))
except Exception as ex:
    w("EXC scan: %r" % (ex,))

# 反汇编 Drive_Matrix 的 0x4B0100..0x4B0220（倾斜 + 斜坡组合段）
try:
    w()
    w("== Drive_Matrix 0x4B0100..0x4B0230 ==")
    dis(0x4B0100, 90, 0x4B0230)
except Exception as ex:
    w("EXC d1: %r" % (ex,))

try:
    w()
    w("== Drive_Matrix 0x4B02B3..0x4B0402 (无斜坡/无倾斜路径) ==")
    dis(0x4B02B3, 90, 0x4B0408)
except Exception as ex:
    w("EXC d2: %r" % (ex,))

# ShipLocomotionClass 构造/CLSID 附近
try:
    w()
    w("== 搜索 CLSID {2BEA74E1-7CCA-11d3-BE14-00104B62A16C} 数据 ==")
    pat = bytes.fromhex("E174EA2B CA7C D311 BE14 00104B62A16C".replace(" ", ""))
    for seg in idautils.Segments():
        s = idc.get_segm_start(seg)
        e = idc.get_segm_end(seg)
        data = ida_bytes.get_bytes(s, e - s)
        if not data:
            continue
        off = data.find(pat)
        while off != -1:
            w("   %08X  (nearby refs:)" % (s + off))
            for x in idautils.XrefsTo(s + off):
                w("      xref %08X from %08X %s" % (x.frm, x.frm, idc.get_func_name(x.frm)))
            off = data.find(pat, off + 1)
except Exception as ex:
    w("EXC clsid: %r" % (ex,))

f.close()
print("DONE p24")
