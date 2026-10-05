# -*- coding: utf-8 -*-
# p25: 反汇编 ShipLocomotionClass 自己的 Draw_Matrix，检查它是否处理 AngleRotated*(倾斜)
import idaapi, idc, idautils, ida_bytes, ida_hexrays

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p25_shipmatrix.txt"
f = open(OUT, "w", encoding="utf-8")


def w(s=""):
    f.write(str(s) + "\n")
    f.flush()


def dis(ea, n=140, stop=None):
    cur = ea
    for _ in range(n):
        if stop is not None and cur >= stop:
            break
        w("   %08X  %s" % (cur, idc.generate_disasm_line(cur, 0)))
        nxt = idc.next_head(cur)
        if nxt == idaapi.BADADDR or nxt == cur:
            break
        cur = nxt


VTS = {
    "ShipLocomotionClass::ILocomotion": 0x7F2D8C,
    "ShipLocomotionClass::IPiggyback": 0x7F2D68,
    "ShipLocomotionClass": 0x7F2E58,
    "DriveLocomotionClass::ILocomotion": 0x7E7EB0,
    "WalkLocomotionClass::ILocomotion": 0x7F69F8,
    "TeleportLocomotionClass::ILocomotion": 0x7F5000,
}
for nm, vt in VTS.items():
    try:
        w("== %s  vt=%08X ==" % (nm, vt))
        for slot in (0x00, 0x04, 0x24, 0x28, 0x2C, 0x30):
            t = ida_bytes.get_dword(vt + slot)
            w("   +0x%02X = %08X  %s" % (slot, t, idc.get_func_name(t) or ""))
        w()
    except Exception as ex:
        w("EXC %s: %r" % (nm, ex))

# 反汇编 Ship 的 Draw_Matrix
try:
    tgt = ida_bytes.get_dword(0x7F2D8C + 0x24)
    w("=" * 70)
    w("== ShipLocomotionClass Draw_Matrix @ %08X  size=%d =="
      % (tgt, idc.get_func_attr(tgt, idc.FUNCATTR_END) - tgt))
    w("=" * 70)
    dis(tgt, 170)
    w()
    w("--- decompile ---")
    try:
        w(ida_hexrays.decompile(tgt))
    except Exception as ex:
        w("decompile fail: %r" % (ex,))
    w()
    w("--- 该函数内所有 mov/lea 引用 [+328h]/[+32Ch] 的位置 ---")
    ea = tgt
    end = idc.get_func_attr(tgt, idc.FUNCATTR_END)
    while ea < end:
        line = idc.generate_disasm_line(ea, 0)
        if "328h" in line or "32Ch" in line or "Angle" in line:
            w("   %08X  %s" % (ea, line))
        ea = idc.next_head(ea)
except Exception as ex:
    w("EXC ship: %r" % (ex,))

# 对比基准：base 0x55A730
try:
    w()
    w("=" * 70)
    w("== base LocomotionClass Draw_Matrix @ 0x55A730 ==")
    w("=" * 70)
    dis(0x55A730, 60)
except Exception as ex:
    w("EXC base: %r" % (ex,))

f.close()
print("DONE p25")
