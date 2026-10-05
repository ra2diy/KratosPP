# -*- coding: utf-8 -*-
# 修正版2：disp8/disp32 两种编码都扫 ILocomotion::In_Which_Layer (vtable +0x74) 的调用点
import idaapi, idc, idautils, ida_bytes, ida_segment, ida_funcs

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\loco_callsites3.txt"
f = open(OUT, "w", encoding="utf-8")
def w(s=""):
    f.write(str(s) + "\n"); f.flush()

SLOT_OFF = 0x74
SLOT_ADDRS = [0x7F6A6C, 0x7F2E00, 0x7EDBE0, 0x7EAD70, 0x7E7F24]

hits = set()
textseg = ida_segment.get_segm_by_name(".text")
start = textseg.start_ea
data = ida_bytes.get_bytes(start, textseg.end_ea - textseg.start_ea)
ln = len(data)
i = 0
while i < ln - 3:
    if data[i] == 0xFF and (0x50 <= data[i+1] <= 0x57):
        # call dword ptr [reg+disp8]
        if data[i+2] == SLOT_OFF:
            hits.add(start + i)
    i += 1
i = 0
while i < ln - 6:
    if data[i] == 0xFF and (0x90 <= data[i+1] <= 0x97):
        if int.from_bytes(data[i+2:i+6], "little", signed=True) == SLOT_OFF:
            hits.add(start + i)
    i += 1

w("call [reg+74h] hits = %d" % len(hits))
w("")
for h in sorted(hits):
    fn = ida_funcs.get_func(h)
    w("--- @%08X in %s ---" % (h, idc.get_func_name(fn.start_ea) if fn else "?"))
    p = h
    ctx = []
    for _ in range(16):
        p = idc.prev_head(p)
        if p == idaapi.BADADDR:
            break
        ctx.append("      %08X  %s" % (p, idc.generate_disasm_line(p, 0)))
    for line in reversed(ctx):
        w(line)
    w("   >>> %08X  %s" % (h, idc.generate_disasm_line(h, 0)))
    w("")

# 另外：谁调用 0075C7E0 本体（非虚调用）
w("==== direct xrefs to 0x75C7E0 ====")
for x in idautils.XrefsTo(0x75C7E0):
    w("  from %08X type %d : %s" % (x.frm, x.type, idc.generate_disasm_line(x.frm, 0)))
w("==== xrefs to vtable slot 0x7F6A6C ====")
for x in idautils.XrefsTo(0x7F6A6C):
    w("  from %08X type %d : %s" % (x.frm, x.type, idc.generate_disasm_line(x.frm, 0)))

# 参考：vanilla 里 InWhichLayer 的调用惯例 —— 看 FootClass::AI 全反汇编关键段
w("==== FootClass::AI 0x4DA87A..0x4DA9C0 ====")
p = 0x4DA87A
while p < 0x4DA9C0:
    w("  %08X  %s" % (p, idc.generate_disasm_line(p, 0)))
    p = idc.next_head(p)

f.close()
print("DONE")
