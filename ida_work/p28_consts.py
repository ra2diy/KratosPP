# -*- coding: utf-8 -*-
# p28: 读关键浮点常量 + TechnoTypeClass 0x350..0x370 成员名
import idc, ida_bytes, ida_struct, idaapi

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p28_consts.txt"
f = open(OUT, "w", encoding="utf-8")

def w(s):
    try:
        f.write(s + "\n"); f.flush()
    except Exception:
        pass

def rd_double(ea):
    try:
        import struct
        b = ida_bytes.get_bytes(ea, 8)
        return struct.unpack("<d", b)[0]
    except Exception as e:
        return "ERR %s" % e

def rd_float(ea):
    try:
        import struct
        b = ida_bytes.get_bytes(ea, 4)
        return struct.unpack("<f", b)[0]
    except Exception as e:
        return "ERR %s" % e

w("### 关键常量")
for ea, name in [
    (0x7E44E8, "驱动/船 -- 倾斜分支阈值 (dbl)"),
    (0x7EF8F8, "IsSinking 角度上限 (flt)"),
    (0x7F4E74, "角度下限 (flt)"),
    (0x7EC0B0, "2.0e-5 (dbl)"),
    (0x7F4E78, "-2.0e-5 (dbl)"),
    (0x7E4408, "facing->voxel 缩放 (dbl)"),
]:
    w("  0x%X  %-38s dbl=%r  flt=%r" % (ea, name, rd_double(ea), rd_float(ea)))

w("")
w("### TechnoTypeClass 成员 0x350..0x3A0")
try:
    sid = ida_struct.get_struc_id("TechnoTypeClass")
    if sid == idaapi.BADADDR:
        sid = ida_struct.get_struc_id("_TechnoTypeClass")
    w("struct id = 0x%X" % sid)
    if sid != idaapi.BADADDR:
        s = ida_struct.get_struc(sid)
        w("size = 0x%X" % ida_struct.get_struc_size(s))
        off = 0
        while off < ida_struct.get_struc_size(s):
            m = ida_struct.get_member(s, off)
            if m is None:
                off += 1; continue
            mname = ida_struct.get_member_name(m.id)
            msize = ida_struct.get_member_size(m)
            if 0x350 <= off <= 0x3A0:
                w("  +0x%03X  %-34s size=%d" % (off, mname, msize))
            off += max(msize, 1)
except Exception as e:
    w("ERR: %s" % e)

# _TechnoClass 的同名区段（用于确认 IDB 4 字节偏移差）
w("")
w("### _TechnoClass 成员 0x320..0x340（确认偏移差）")
try:
    sid2 = ida_struct.get_struc_id("_TechnoClass")
    w("struct id = 0x%X" % sid2)
    if sid2 != idaapi.BADADDR:
        s2 = ida_struct.get_struc(sid2)
        w("size = 0x%X" % ida_struct.get_struc_size(s2))
        off = 0
        while off < ida_struct.get_struc_size(s2):
            m = ida_struct.get_member(s2, off)
            if m is None:
                off += 1; continue
            mname = ida_struct.get_member_name(m.id)
            msize = ida_struct.get_member_size(m)
            if 0x318 <= off <= 0x340:
                w("  +0x%03X  %-34s size=%d" % (off, mname, msize))
            off += max(msize, 1)
except Exception as e:
    w("ERR: %s" % e)

f.close()
print("DONE p28")
