# -*- coding: utf-8 -*-
# p29: 钉死 Matrix3D::Translate / TranslateX/Y/Z / RotateZ / MatrixMultiply 的语义
# 目的：判定 GetFLHOffset(mtx, flh) 的结果是否含旋转（尤其倾斜）。
import idaapi, idc, idautils, ida_funcs, ida_bytes, ida_name
import traceback

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p29_translate.txt"
f = open(OUT, "w", encoding="utf-8")

def w(s=""):
    f.write(str(s) + "\n")
    f.flush()

def func_range(ea):
    fa = ida_funcs.get_func(ea)
    if fa:
        return fa.start_ea, fa.end_ea
    return ea, ea + 0x200

def dis_func(ea, title, maxins=90):
    w("=" * 110)
    w("[%s] %#010x" % (title, ea))
    start, end = func_range(ea)
    w("range %#x .. %#x  size=%d" % (start, end, end - start))
    n = 0
    cur = start
    while cur < end and n < maxins:
        try:
            w("%#010x  %s" % (cur, idc.generate_disasm_line(cur, 0)))
        except Exception:
            w("%#010x  <err>" % cur)
        nxt = idc.next_head(cur, end)
        if nxt == idaapi.BADADDR or nxt <= cur:
            break
        cur = nxt
        n += 1
    w()

def xrefs(ea, title):
    w("=" * 110)
    w("[XrefsTo %s] %#010x" % (title, ea))
    try:
        for x in idautils.XrefsTo(ea, 0):
            fn = ida_funcs.get_func(x.frm)
            fnname = ida_name.get_name(fn.start_ea) if fn else "?"
            w("  %#010x (type=%d) in %s" % (x.frm, x.type, fnname))
    except Exception as e:
        w("  ERR " + repr(e))
    w()

w("### p29 Matrix3D translate semantics ###")

try:
    dis_func(0x5AE890, "Matrix3D::Translate(float,float,float)")
except Exception as e:
    w("ERR1 " + repr(e)); w(traceback.format_exc())

try:
    dis_func(0x5AE8F0, "Matrix3D::Translate(Vector3D)")
except Exception as e:
    w("ERR2 " + repr(e)); w(traceback.format_exc())

try:
    dis_func(0x5AE980, "Matrix3D::TranslateX")
    dis_func(0x5AE9B0, "Matrix3D::TranslateY")
    dis_func(0x5AE9E0, "Matrix3D::TranslateZ")
except Exception as e:
    w("ERR3 " + repr(e)); w(traceback.format_exc())

try:
    dis_func(0x5AFB80, "MatrixMultiply(Vector3D*, Matrix3D*, Vector3D*)")
except Exception as e:
    w("ERR4 " + repr(e)); w(traceback.format_exc())

try:
    dis_func(0x5AF1A0, "Matrix3D::RotateZ(float)")
    dis_func(0x5AEF60, "Matrix3D::RotateX(float)")
    dis_func(0x5AF080, "Matrix3D::RotateY(float)")
except Exception as e:
    w("ERR5 " + repr(e)); w(traceback.format_exc())

# 谁调用 ShipLocomotionClass::Draw_Matrix
try:
    xrefs(0x69F670, "ShipLocomotionClass::Draw_Matrix")
    xrefs(0x4AFF60, "DriveLocomotionClass::Draw_Matrix")
    xrefs(0x55A730, "LocomotionClass_ILocomotion_DrawMatrix")
except Exception as e:
    w("ERR6 " + repr(e)); w(traceback.format_exc())

# 倾斜积分器：确认 IsSinking 分支改的是哪个字段
try:
    dis_func(0x70B570, "TechnoClass vt+0x41C (tilt integrator)", 160)
except Exception as e:
    w("ERR7 " + repr(e)); w(traceback.format_exc())

# IsVoxel 门控上下文
try:
    w("=" * 110)
    w("[ctx 0x6FA1F0 .. 0x6FA260]")
    ea = 0x6FA1F0
    while ea < 0x6FA260:
        w("%#010x  %s" % (ea, idc.generate_disasm_line(ea, 0)))
        nxt = idc.next_head(ea, 0x6FA260)
        if nxt == idaapi.BADADDR or nxt <= ea:
            break
        ea = nxt
    w()
except Exception as e:
    w("ERR8 " + repr(e)); w(traceback.format_exc())

w("### DONE ###")
f.close()
