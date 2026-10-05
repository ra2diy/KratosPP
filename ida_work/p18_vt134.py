# -*- coding: utf-8 -*-
import os, idc, ida_pro, ida_struct, ida_bytes, idautils, idaapi
HERE = os.path.dirname(os.path.abspath(__file__))
f = open(os.path.join(HERE, "p18_vt134.txt"), "w", encoding="utf-8")
def w(s):
    f.write(s + "\n"); f.flush()
def fn_end(ea):
    e = idc.get_func_attr(ea, idc.FUNCATTR_END)
    return e if e else ea + 200
def disasm(ea, end, cap=200):
    n = 0
    while ea < end and n < cap:
        d = idc.GetDisasm(ea)
        if not d: break
        w("  %08X  %s" % (ea, d))
        sz = idc.get_item_size(ea)
        if sz <= 0: break
        ea += sz; n += 1
for sname in ("vt_ObjectClass", "vt_FootClass", "vt_TechnoClass"):
    sid = ida_struct.get_struc_id(sname)
    if sid == idaapi.BADADDR: 
        continue
    s = ida_struct.get_struc(sid)
    w("== %s ==" % sname)
    for i in range(ida_struct.get_struc_size(s)):
        m = ida_struct.get_member(s, i)
        if m is None: continue
        off = m.get_soff()
        if 0x120 <= off <= 0x1E0:
            w("   +0x%03X  %s" % (off, ida_struct.get_member_name(m.id) or "?"))
    w("")
w("############ vt+0x134 指向的函数（各虚表） ############")
for base in (0x7E3354, 0x7E22A4, 0x7E8C94, 0x7E3AD0):
    v = ida_bytes.get_dword(base + 0x134)
    w("   vtable 0x%08X  +0x134 = 0x%08X %s" % (base, v, idc.get_func_name(v) or ""))
w("")
base = 0x7E8C94   # FootClass
v = ida_bytes.get_dword(base + 0x134)
w("############ FootClass +0x134 = 0x%08X %s ############" % (v, idc.get_func_name(v) or ""))
e = fn_end(v)
w("   size=%d" % (e - v))
disasm(v, min(e, v + 300))
w("   -- 调用 --")
cur = v
while cur < e:
    m = idc.print_insn_mnem(cur)
    if m in ("call", "jmp"):
        t = idc.get_operand_value(cur, 0)
        tag = ""
        if t == 0x4A9770: tag = "  <<<< DisplayClass::Remove"
        if t == 0x4A9720: tag = "  <<<< DisplayClass::Submit"
        if t == 0x5683C0: tag = "  <<<< Place_Down"
        if t == 0x5687F0: tag = "  <<<< Pick_Up"
        if t == 0x5F5850: tag = "  <<<< ObjectClass::Mark"
        w("      %08X %s -> %08X  %s%s" % (cur, m, t, idc.get_func_name(t) or "", tag))
    sz = idc.get_item_size(cur)
    if sz <= 0: break
    cur += sz
w("")
f.close(); ida_pro.qexit(0)
