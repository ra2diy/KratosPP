# -*- coding: utf-8 -*-
# 交叉验证 Phobos 结论所需的反汇编证据
import idc, idautils, idaapi

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\cv_verify.txt"
f = open(OUT, "w", encoding="utf-8")

def w(s=""):
    f.write(str(s) + "\n")

def dis(ea, n=1, tag=""):
    for i in range(n):
        line = idc.generate_disasm_line(ea, 0)
        w("   %08X  %s" % (ea, line if line else "<bad>"))
        ea = idc.next_head(ea)

def func_of(ea):
    s = idc.get_func_attr(ea, idc.FUNCATTR_START)
    e = idc.get_func_attr(ea, idc.FUNCATTR_END)
    nm = idc.get_func_name(ea)
    return s, e, nm

def dump_func(ea, label):
    s, e, nm = func_of(ea)
    w("==== %s : %s  [%08X - %08X] ====" % (label, nm, s, e))
    if s == idc.BADADDR:
        w("   (no function)")
        return
    cur = s
    cnt = 0
    while cur < e and cnt < 400:
        w("   %08X  %s" % (cur, idc.generate_disasm_line(cur, 0) or ""))
        cur = idc.next_head(cur)
        cnt += 1

def name_at(ea):
    n = idc.get_name(ea)
    return n if n else ""

# ---------------------------------------------------------------
w("############ A. 层数组区域 0x8A0300 - 0x8A0410 的命名符号 ############")
for ea, nm in idautils.Names():
    if 0x8A0300 <= ea <= 0x8A0410:
        w("   %08X  %s" % (ea, nm))

w()
w("############ B. 关键地址处的名字 ############")
for a in [0x8A0358, 0x8A0360, 0x8A0364, 0x8A036C, 0x8A0370, 0x8A037C,
          0x8A0390, 0x8A0394, 0x8A03A0, 0x8A03A8, 0x8A03AC, 0x8A03C4,
          0x8A03DC, 0x8A03E8, 0x8A0394]:
    w("   %08X  name=%s  seg=%s" % (a, name_at(a), idc.get_segm_name(a)))

w()
w("############ C. sub_64DAB0 中读取层数组的片段 (0x64DCB0-0x64DD70) ############")
cur = 0x64DCB0
while cur < 0x64DD70:
    w("   %08X  %s" % (cur, idc.generate_disasm_line(cur, 0) or ""))
    cur = idc.next_head(cur)

w()
w("############ D. unk_8A0390 的 xref ############")
for x in idautils.XrefsTo(0x8A0390):
    s, e, nm = func_of(x.frm)
    w("   from %08X (type=%d) in %s" % (x.frm, x.type, nm))
    dis(x.frm - 0x10, 12)

w()
w("############ E. DisplayClass::GetLayer ############")
gl = idc.get_name_ea_simple("DisplayClass_GetLayer")
if gl == idc.BADADDR:
    for nm in ["DisplayClass_GetLayer", "GetLayer"]:
        gl = idc.get_name_ea_simple(nm)
        if gl != idc.BADADDR:
            break
w("   name ea = %08X" % gl)
if gl != idc.BADADDR:
    dump_func(gl, "DisplayClass::GetLayer")

w()
w("############ F. ObjectClass::ReturnRealYSort (0x5F6BD0) ############")
dump_func(0x5F6BD0, "ObjectClass_ReturnRealYSort")
w("   --- 0x5F6BF7 前后 ±6 条 ---")
dis(0x5F6BF7 - 0x18, 10)
w("   --- 0x5F6BF7 原始字节(8) ---")
b = idc.get_bytes(0x5F6BF7, 8)
w("   bytes = %s" % (b.hex() if b else "None"))
w("   --- 该地址处的函数归属 ---")
s, e, nm = func_of(0x5F6BF7)
w("   %s [%08X-%08X]" % (nm, s, e))

w()
w("############ G. Kratos Hook 0x4DA87A (FootClass_Update_UpdateLayer) ############")
s, e, nm = func_of(0x4DA87A)
w("   containing = %s [%08X-%08X]" % (nm, s, e))
w("   --- 原始字节(6) = %s" % (idc.get_bytes(0x4DA87A, 6).hex()))
dis(0x4DA87A - 0x14, 14)

w()
w("############ H. Phobos 已删除的 Hook 0x4A9750 (DisplayClass_Submit_LayerSort) ############")
s, e, nm = func_of(0x4A9750)
w("   containing = %s [%08X-%08X]" % (nm, s, e))
w("   --- 原始字节(9) = %s" % (idc.get_bytes(0x4A9750, 9).hex()))
dis(0x4A9750 - 0x18, 18)

w()
w("############ I. DisplayClass::Submit 的 LayerSort 逻辑（找 sorted add 分支）############")
for a in [0x4A9700, 0x4A9720, 0x4A9740, 0x4A9760, 0x4A9780, 0x4A97A0]:
    w("   %08X  %s" % (a, idc.generate_disasm_line(a, 0) or ""))

w()
w("############ J. 0x55DBC3 附近（Kratos 强制排序 Air 层）############")
dis(0x55DBB0, 14)
w("   --- 0x8A0390 与 0x55DBC8 的参数关系 ---")
w("   call sub_551A30 的参数 ecx=0x8A0390")

f.close()
idc.qexit(0)
