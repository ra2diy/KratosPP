# -*- coding: utf-8 -*-
# p30: 验证 AbstractClass 布局（AbstractFlags 偏移）、FootClass.Locomotor 偏移、
#      以及 GetCoords(0x5F65A0) 的实体。
import idaapi, idc, idautils, ida_funcs, ida_bytes, ida_struct, ida_name
import traceback

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p30_struct.txt"
f = open(OUT, "w", encoding="utf-8")

def w(s=""):
    f.write(str(s) + "\n")
    f.flush()

def dump_struct(name):
    w("=" * 110)
    tid = ida_struct.get_struc_id(name)
    if tid == idaapi.BADADDR:
        w("[struct %s] NOT FOUND" % name)
        return
    s = ida_struct.get_struc(tid)
    w("[struct %s] id=%#x size=%d" % (name, tid, ida_struct.get_struc_size(s)))
    m = s.members
    cnt = 0
    while m:
        mname = ida_struct.get_member_name(m.id)
        moff = m.soff
        msize = ida_struct.get_member_size(m)
        mtype = idc.get_type(m.id) or ""
        w("  +%#06x  %-40s size=%-3d %s" % (moff, mname, msize, mtype))
        m = ida_struct.get_next_member(s, m.soff)
        cnt += 1
        if cnt > 200:
            w("  ...(truncated)")
            break
    w()

def dis(ea, title, n=12):
    w("=" * 110)
    w("[%s] %#010x" % (title, ea))
    cur = ea
    for _ in range(n):
        try:
            w("  %#010x  %s" % (cur, idc.generate_disasm_line(cur, 0)))
        except Exception:
            break
        nxt = idc.next_head(cur, cur + 0x100)
        if nxt == idaapi.BADADDR or nxt <= cur:
            break
        cur = nxt
    w()

w("### p30 struct layout ###")

for n in ["AbstractClass", "_AbstractClass", "AbstractClass_", "IAbstractClass"]:
    try:
        dump_struct(n)
    except Exception as e:
        w("ERR " + n + " " + repr(e))

for n in ["ObjectClass", "_ObjectClass"]:
    try:
        dump_struct(n)
    except Exception as e:
        w("ERR " + n + " " + repr(e))

for n in ["TechnoClass", "_TechnoClass"]:
    try:
        dump_struct(n)
    except Exception as e:
        w("ERR " + n + " " + repr(e))

for n in ["FootClass", "_FootClass"]:
    try:
        dump_struct(n)
    except Exception as e:
        w("ERR " + n + " " + repr(e))

try:
    dis(0x5F65A0, "GetCoords?", 10)
    dis(0x5F5850, "ObjectClass::Mark head", 12)
except Exception as e:
    w("ERR dis " + repr(e)); w(traceback.format_exc())

# 列出所有含 AbstractFlags / Locomotor 字样的结构体成员，便于交叉核对
try:
    w("=" * 110)
    w("[grep members containing 'Flags' or 'Locomotor']")
    qty = ida_struct.get_struc_qty()
    found = 0
    for i in range(qty):
        tid = ida_struct.get_struc_by_idx(i)
        s = ida_struct.get_struc(tid)
        if not s:
            continue
        sname = ida_struct.get_struc_name(tid)
        m = s.members
        while m:
            mname = ida_struct.get_member_name(m.id) or ""
            if ("Flags" in mname) or ("Locomotor" in mname):
                w("  %s . %s @ +%#x (size=%d)" % (sname, mname, m.soff, ida_struct.get_member_size(m)))
                found += 1
            m = ida_struct.get_next_member(s, m.soff)
            if found > 60:
                break
        if found > 60:
            break
    w("total=%d" % found)
except Exception as e:
    w("ERR grep " + repr(e)); w(traceback.format_exc())

w("### DONE ###")
f.close()
