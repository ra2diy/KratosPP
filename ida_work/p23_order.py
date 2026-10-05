# -*- coding: utf-8 -*-
# p23: 钉死一帧内的顺序：逻辑更新 / 地面层 Sort / DrawOnTop(AE渲染事件) / 绘制的先后
import idaapi, idc, idautils, ida_bytes, ida_hexrays

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p23_order.txt"
f = open(OUT, "w", encoding="utf-8")


def w(s=""):
    f.write(str(s) + "\n")
    f.flush()


def dis(ea, n=200, stop=None):
    cur = ea
    for _ in range(n):
        if stop is not None and cur >= stop:
            break
        w("   %08X  %s" % (cur, idc.generate_disasm_line(cur, 0)))
        nxt = idc.next_head(cur)
        if nxt == idaapi.BADADDR or nxt == cur:
            break
        cur = nxt


try:
    w("== Main_Loop 0x55D360 size=%d ==" % (idc.get_func_attr(0x55D360, idc.FUNCATTR_END) - 0x55D360))
    dis(0x55D360, 400, 0x55DF00)
except Exception as ex:
    w("EXC main: %r" % (ex,))

try:
    w()
    w("== GScreenClass_DrawOnTop 0x4F4480 size=%d ==" % (idc.get_func_attr(0x4F4480, idc.FUNCATTR_END) - 0x4F4480))
    dis(0x4F4480, 80)
except Exception as ex:
    w("EXC dot: %r" % (ex,))

try:
    w()
    w("== DrawOnTop 内 call 目标 ==")
    ea = 0x4F4480
    end = idc.get_func_attr(0x4F4480, idc.FUNCATTR_END)
    while ea < end:
        if idc.print_insn_mnem(ea) == "call":
            tgt = idc.get_operand_value(ea, 0)
            w("   %08X -> %08X %s" % (ea, tgt, idc.get_func_name(tgt)))
        ea = idc.next_head(ea)
except Exception as ex:
    w("EXC dotcalls: %r" % (ex,))

for ea, nm in ((0x6D8DB0, "Tactical_Draw_All"), (0x551A30, "LayerClass::Sort"), (0x55D100, "Main_Loop_helpers?")):
    try:
        w()
        w("== %s @ %08X ==" % (nm, ea))
        dis(ea, 90)
    except Exception as ex:
        w("EXC %s: %r" % (nm, ex))

# Main_Loop 里所有 call
try:
    w()
    w("== Main_Loop 内所有 call ==")
    ea = 0x55D360
    end = idc.get_func_attr(0x55D360, idc.FUNCATTR_END)
    while ea < end:
        if idc.print_insn_mnem(ea) == "call":
            tgt = idc.get_operand_value(ea, 0)
            w("   %08X -> %08X %s" % (ea, tgt, idc.get_func_name(tgt)))
        ea = idc.next_head(ea)
    w("   Main_Loop end = %08X" % end)
except Exception as ex:
    w("EXC maincalls: %r" % (ex,))

f.close()
print("DONE p23")
