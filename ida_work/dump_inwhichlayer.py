# -*- coding: utf-8 -*-
# 判定 In_Which_Layer 的真实语义：类间对比 + 调用者分析
import idaapi, idc, idautils, ida_bytes, ida_segment, ida_funcs, ida_name

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\inwhichlayer.txt"
f = open(OUT, "w", encoding="utf-8")
def w(s=""):
    f.write(str(s) + "\n"); f.flush()

LOCOS = [
    "Walk", "Ship", "Mech", "Hover", "Drive", "Jumpjet", "Fly", "Rocket",
    "Teleport", "Tunnel", "DropPod", "Parasite", "FlyBy", "Walker",
]

w("################ 1. 各 locomotion 的 ILocomotion vtable +0x70..+0x80 ################")
for nm in LOCOS:
    vtname = "??_7%sLocomotionClass@@6BILocomotion@@@" % nm
    ea = idc.get_name_ea_simple(vtname)
    if ea == idaapi.BADADDR:
        w("[%s] vtable name not found" % nm)
        continue
    w("--- %s : sub-vtable @ %08X ---" % (nm, ea))
    for off in (0x6C, 0x70, 0x74, 0x78, 0x7C, 0x80):
        v = ida_bytes.get_dword(ea + off)
        w("   +%02X %08X  %s" % (off, v, idc.get_name(v) or ""))

w()
w("################ 2. TechnoClass_InWhichLayer @0x41ADC0 ################")
p = 0x41ADC0
for _ in range(24):
    w("   %08X  %s" % (p, idc.generate_disasm_line(p, 0)))
    p = idc.next_head(p)

w()
w("################ 3. xrefs to 0x41ADC0 (谁调用 InWhichLayer) ################")
for x in idautils.XrefsTo(0x41ADC0):
    w("  from %08X type %d : %s" % (x.frm, x.type, idc.generate_disasm_line(x.frm, 0)))
    pp = x.frm
    ctx = []
    for _ in range(14):
        pp = idc.prev_head(pp)
        if pp == idaapi.BADADDR:
            break
        ctx.append("      %08X  %s" % (pp, idc.generate_disasm_line(pp, 0)))
    for line in reversed(ctx):
        w(line)

w()
w("################ 4. Jumpjet In_Which_Layer 候选 sub_54B8D0 ################")
p = 0x54B8D0
while p < 0x54B9B0:
    w("   %08X  %s" % (p, idc.generate_disasm_line(p, 0)))
    p = idc.next_head(p)

w()
w("################ 5. 反查：哪些函数读取 [reg+674h] 并 call [reg+74h] ################")
textseg = ida_segment.get_segm_by_name(".text")
start = textseg.start_ea
data = ida_bytes.get_bytes(start, textseg.end_ea - textseg.start_ea)
ln = len(data)
# 找 mov reg,[reg+674h]  (8B 8x 74 06 00 00 / 8B 9x ...) 后 30 字节内有 FF [5x/9x] 74
i = 0
found = []
while i < ln - 8:
    if data[i] == 0x8B and (0x80 <= data[i+1] <= 0xBF):
        disp = int.from_bytes(data[i+2:i+6], "little", signed=True)
        if disp == 0x674:
            ea = start + i
            # 向后 40 字节寻找 call [reg+74h]
            for j in range(0, 40):
                if i + j + 3 < ln and data[i+j] == 0xFF and (0x50 <= data[i+j+1] <= 0x57) and data[i+j+2] == 0x74:
                    found.append((ea, start + i + j))
                    break
                if i + j + 6 < ln and data[i+j] == 0xFF and (0x90 <= data[i+j+1] <= 0x97) \
                        and int.from_bytes(data[i+j+2:i+j+6], "little", signed=True) == 0x74:
                    found.append((ea, start + i + j))
                    break
    i += 1
for a, b in found:
    fn = ida_funcs.get_func(a)
    w("  read@%08X call@%08X  in %s" % (a, b, idc.get_func_name(fn.start_ea) if fn else "?"))
    w("     %08X  %s" % (b, idc.generate_disasm_line(b, 0)))

f.close()
print("DONE")
