# -*- coding: utf-8 -*-
# p26: 钉死 AngleRotatedForwards / AngleRotatedSideways / Rocking* 的真实偏移
#  1) TechnoClass 结构体在 IDB 里的成员（0x300..0x360）
#  2) 0x70B570 (TechnoClass 倾斜更新 vt+0x41C) 里对这四个字段的读写偏移
#  3) 0x4AFF60 (DriveLocomotionClass::Draw_Matrix) 里读取 AngleRotated* 的偏移
#  4) 0x69F670 (ShipLocomotionClass::Draw_Matrix) 同上
import idaapi, idc, idautils, ida_bytes, ida_funcs, ida_struct, ida_frame

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p26_tilt.txt"
f = open(OUT, "w", encoding="utf-8")

def w(s):
    try:
        f.write(s + "\n")
        f.flush()
    except Exception:
        pass

def disasm_range(start, count, tag):
    w("=" * 70)
    w("== %s @ 0x%X (%d insns)" % (tag, start, count))
    w("=" * 70)
    ea = start
    for i in range(count):
        try:
            line = idc.generate_disasm_line(ea, 0)
            w("  %08X  %s" % (ea, line))
            ea = idc.next_head(ea)
            if ea == idaapi.BADADDR:
                break
        except Exception as e:
            w("  ERR %s" % e)
            break

# ---------- 1) 结构体成员 ----------
try:
    w("#" * 70)
    w("# 1) TechnoClass 结构体成员 0x2F0..0x370")
    w("#" * 70)
    sid = ida_struct.get_struc_id("TechnoClass")
    if sid == idaapi.BADADDR:
        sid = ida_struct.get_struc_id("_TechnoClass")
    w("struct id = 0x%X" % sid)
    if sid != idaapi.BADADDR:
        s = ida_struct.get_struc(sid)
        w("size = 0x%X" % ida_struct.get_struc_size(s))
        mt = -1
        off = 0
        while off < ida_struct.get_struc_size(s):
            m = ida_struct.get_member(s, off)
            if m is None:
                off += 1
                continue
            mname = ida_struct.get_member_name(m.id)
            msize = ida_struct.get_member_size(m)
            if 0x2F0 <= off <= 0x370:
                w("  +0x%03X  %-32s size=%d" % (off, mname, msize))
            off += max(msize, 1)
except Exception as e:
    w("ERR struct: %s" % e)

# ---------- 2/3/4) 反汇编 ----------
try:
    disasm_range(0x70B570, 90, "TechnoClass tilt update (vt+0x41C)")
except Exception as e:
    w("ERR 70B570: %s" % e)

try:
    # Drive_Matrix 开头（含倾斜分支）
    disasm_range(0x4AFF60, 60, "DriveLocomotionClass::Draw_Matrix head")
except Exception as e:
    w("ERR 4AFF60: %s" % e)

try:
    disasm_range(0x4AFFC0, 60, "DriveLocomotionClass::Draw_Matrix tilt part")
except Exception as e:
    w("ERR 4AFFC0: %s" % e)

try:
    disasm_range(0x69F670, 60, "ShipLocomotionClass::Draw_Matrix head")
except Exception as e:
    w("ERR 69F670: %s" % e)

# ---------- 5) 全体 [reg+328h]/[reg+32Ch] 引用 ----------
try:
    w("#" * 70)
    w("# 5) 全 .text 内 [reg+328h] / [reg+32Ch] 引用")
    w("#" * 70)
    found = 0
    for seg in idautils.Segments():
        if idc.get_segm_name(seg) != ".text":
            continue
        s = idc.get_segm_start(seg)
        e = idc.get_segm_end(seg)
        ea = s
        while ea < e:
            try:
                ins = idc.generate_disasm_line(ea, 0)
                if ("+328h]" in ins) or ("+32Ch]" in ins) or ("+32Ch]" in ins):
                    w("  %08X  %s" % (ea, ins))
                    found += 1
            except Exception:
                pass
            ea = idc.next_head(ea)
            if ea == idaapi.BADADDR:
                break
    w("total = %d" % found)
except Exception as e:
    w("ERR scan: %s" % e)

f.close()
print("DONE p26")
