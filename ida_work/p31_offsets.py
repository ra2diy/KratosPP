# -*- coding: utf-8 -*-
# p31: 用「真实 disp 数值」核实 Ship_Matrix(0x69F670) / 0x70B570 消费的字段偏移，
#      并 dump TechnoClass / LocomotionClass / ShipLocomotionClass 的成员表。
import idaapi, idc, idautils, ida_funcs, ida_bytes, ida_struct, ida_name
import traceback

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p31_offsets.txt"
f = open(OUT, "w", encoding="utf-8")

def w(s=""):
    f.write(str(s) + "\n")
    f.flush()

def dump_struct(name, maxm=300):
    w("=" * 110)
    tid = ida_struct.get_struc_id(name)
    if tid == idaapi.BADADDR:
        w("[struct %s] NOT FOUND" % name)
        return
    s = ida_struct.get_struc(tid)
    w("[struct %s] id=%#x size=%d" % (name, tid, ida_struct.get_struc_size(s)))
    m = s.members
    cnt = 0
    while m and cnt < maxm:
        try:
            mname = ida_struct.get_member_name(m.id)
        except Exception:
            mname = "?"
        try:
            msize = ida_struct.get_member_size(m)
        except Exception:
            msize = -1
        try:
            mt = idc.get_type(m.id) or ""
        except Exception:
            mt = ""
        w("  +%#06x  %-42s size=%-4d %s" % (m.soff, mname, msize, mt))
        m = ida_struct.get_next_member(s, m.soff)
        cnt += 1
    w()

def dis_num(ea, title, n=200, stop=None):
    """反汇编并额外打印操作数 disp 数值"""
    w("=" * 110)
    w("[%s] %#010x" % (title, ea))
    cur = ea
    for _ in range(n):
        if stop is not None and cur >= stop:
            break
        try:
            line = idc.generate_disasm_line(cur, 0)
        except Exception:
            break
        extra = ""
        try:
            t1 = idc.get_operand_type(cur, 1)
            # o_displ == 4
            if t1 == 4:
                v = idc.get_operand_value(cur, 1)
                extra = "   ; disp=%#x (%d)" % (v, v)
            elif t1 == 5:  # o_imm
                v = idc.get_operand_value(cur, 1)
                extra = "   ; imm=%#x" % v
        except Exception:
            pass
        w("  %#010x  %s%s" % (cur, line, extra))
        nxt = idc.next_head(cur, ea + 0x2000)
        if nxt == idaapi.BADADDR or nxt <= cur:
            break
        cur = nxt
    w()

w("### p31 real displacement verification ###")

for n in ["TechnoClass", "_TechnoClass", "TechnoClass_", "FootClass", "_FootClass"]:
    try:
        dump_struct(n)
    except Exception as e:
        w("ERR dump " + n + " " + repr(e))

for n in ["LocomotionClass", "ILocomotion", "ShipLocomotionClass", "DriveLocomotionClass"]:
    try:
        dump_struct(n)
    except Exception as e:
        w("ERR dump " + n + " " + repr(e))

# Ship_Matrix 的 A 路径判定与倾斜补偿段（真实 disp）
try:
    dis_num(0x69F6C0, "Ship_Matrix branch decision", 30, 0x69F6FE)
    dis_num(0x69F7F0, "Ship_Matrix tilt compensation reads", 40, 0x69F8A0)
except Exception as e:
    w("ERR dis ship " + repr(e)); w(traceback.format_exc())

# tilt integrator 的字段访问
try:
    dis_num(0x70B570, "Techno_41C head", 60, 0x70B6C0)
except Exception as e:
    w("ERR dis 70B570 " + repr(e)); w(traceback.format_exc())

# [loco+18h]/[loco+1Ch] 的使用者
for fn, nm in [(0x7559B0, "sub_7559B0 (matrix from X)"), (0x755A40, "sub_755A40 (lerp)")]:
    try:
        dis_num(fn, nm, 60)
    except Exception as e:
        w("ERR dis %#x %s" % (fn, repr(e)))

# IsVoxel 实现
try:
    for vtbase, tag in [(0x7E8C94, "vt_TechnoClass(Unit)")]:
        w("=" * 110)
        w("[IsVoxel slot scan in %s]" % tag)
        # 搜索 vt 表附近指向的 IsVoxel
        pass
except Exception as e:
    w("ERR isvoxel " + repr(e))

# 直接找 TechnoTypeClass 里 IsVoxel 相关
try:
    dump_struct("TechnoTypeClass", 400)
except Exception as e:
    w("ERR dump ttype " + repr(e)); w(traceback.format_exc())

w("### DONE ###")
f.close()
