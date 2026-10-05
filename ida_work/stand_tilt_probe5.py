# -*- coding: utf-8 -*-
# 只读探针 5：定位 0x70B570 的 vtable 槽位与调用点；定位 GetRenderCoords；找 Rocking 写点
import idaapi, idc, ida_bytes, ida_funcs, ida_name, idautils, ida_nalt, struct

OUT = open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\stand_tilt_probe5.txt", "w", encoding="utf-8")
def w(s=""): OUT.write(str(s) + "\n")
def hdr(t):
    w(""); w("############ " + t + " ############")

def nm(ea):
    f = ida_funcs.get_func(ea)
    fn = ida_name.get_ea_name(f.start_ea) if f else ""
    n = ida_name.get_ea_name(ea)
    return "{} | {} | {:#010x}".format(n or "-", fn or "-", ea)

# 读整个 .text 字节
seg = idaapi.get_segm_by_name(".text")
TEXT_S, TEXT_E = seg.start_ea, seg.end_ea
BLOB = ida_bytes.get_bytes(TEXT_S, TEXT_E - TEXT_S)
w(".text {:#x}..{:#x} len={}".format(TEXT_S, TEXT_E, len(BLOB)))

hdr("A. vtable 定位（含 0x70B570 的 vtable 起始与槽位）")
def dump_vtable(addr, back=0x120, title=""):
    # 向前找起始：vtable[-1] 是 RTTI 指针，前面不是代码指针
    s = addr
    while s - 4 > addr - back:
        d = ida_bytes.get_dword(s - 4)
        if TEXT_S <= d < TEXT_E:
            s -= 4
        else:
            break
    w("  == {} 基准 {:#010x} 起始 {:#010x} ==".format(title, addr, s))
    for i in range(0, 0x40):
        ea = s + i * 4
        v = ida_bytes.get_dword(ea)
        eff = ea - s
        if not (TEXT_S <= v < TEXT_E):
            if eff >= (addr - s):
                break
            w("     [{:#05x}] {:#010x} (非代码)".format(eff, v))
            continue
        f = ida_funcs.get_func(v)
        fn = ida_name.get_ea_name(f.start_ea) if f else "-"
        mark = "   <<< 0x70B570" if v == 0x70B570 else ""
        w("     [{:#05x}] {:#010x} {}{}".format(eff, v, fn, mark))
    return s

for base, title in [(0x007f4960, "??_7TechnoClass@@6BTechnoClass@@@"),
                    (0x007e8c94, "??_7FootClass@@6BFootClass@@@"),
                    (0x007eb058, "??_7InfantryClass@@6BInfantryClass@@@"),
                    (0x007e26c0, "vtable@0x7e26c0"),
                    (0x007e42d8, "vtable@0x7e42d8"),
                    (0x007f608c, "vtable@0x7f608c"),
                    (0x007ef03c, "??_7ObjectClass@@6BObjectClass@@@")]:
    dump_vtable(base, 0x200, title)

hdr("B. 直接搜：call [reg+0x41C] / [reg+0x420] / [reg+0x418] 等候选槽位")
def scan_calloffsets(offsets):
    for off in offsets:
        b = struct.pack("<I", off)
        hits = []
        pat_prefixes = [b"\xFF\x90", b"\xFF\x91", b"\xFF\x92", b"\xFF\x93",
                        b"\xFF\x95", b"\xFF\x96", b"\xFF\x97"]
        for pref in pat_prefixes:
            pat = pref + b
            i = BLOB.find(pat)
            while i != -1:
                hits.append(TEXT_S + i)
                i = BLOB.find(pat, i + 1)
        w("  call [reg+{:#05x}] : {} 处".format(off, len(hits)))
        for h in hits[:20]:
            w("      {}".format(nm(h)))

scan_calloffsets([0x41C, 0x420, 0x418, 0x424])

hdr("C. GetRenderCoords（vt+0x48）的实现与调用点")
hits = []
for pref in [b"\xFF\x50", b"\xFF\x51", b"\xFF\x52", b"\xFF\x53", b"\xFF\x55", b"\xFF\x56", b"\xFF\x57"]:
    pat = pref + b"\x48"
    i = BLOB.find(pat)
    while i != -1:
        hits.append(TEXT_S + i)
        i = BLOB.find(pat, i + 1)
w("  call [reg+0x48] : {} 处（前 40）".format(len(hits)))
for h in hits[:40]:
    w("      {}".format(nm(h)))

hdr("D. ObjectClass_GetCoords_1 (0x5f6c80) 全体")
f = ida_funcs.get_func(0x5f6c80)
def dump(ea, end, title=""):
    w("== {} {:#010x}..{:#010x} ==".format(title, ea, end))
    a = ea
    while a < end:
        sz = idc.get_item_size(a)
        if sz <= 0: sz = 1
        w("   {:#010x}: {}".format(a, idc.generate_disasm_line(a, 0) or ""))
        a += sz
if f:
    dump(f.start_ea, f.end_ea, "ObjectClass_GetCoords_1")

hdr("E. MapClass_GetCellFloorHeight (0x578080) 的调用者")
callers = set()
for x in idautils.XrefsTo(0x578080, 0):
    ff = ida_funcs.get_func(x.frm)
    if ff: callers.add((ff.start_ea, ida_name.get_ea_name(ff.start_ea)))
for s0, n0 in sorted(callers):
    w("      caller {:#010x} {}".format(s0, n0))
w("  --- 调用点 ---")
for x in idautils.XrefsTo(0x578080, 0):
    w("      {:#010x}".format(x.frm))

hdr("F. Rocking 步进字段写点（手工扫描）")
pats = [
    ("fstp [esi+32Ch]", b"\xD9\x9E\x2C\x03\x00\x00"),
    ("fstp [esi+330h]", b"\xD9\x9E\x30\x03\x00\x00"),
    ("fstp [edi+32Ch]", b"\xD9\x9F\x2C\x03\x00\x00"),
    ("fstp [edi+330h]", b"\xD9\x9F\x30\x03\x00\x00"),
    ("fst  [esi+32Ch]", b"\xD9\x96\x2C\x03\x00\x00"),
    ("fst  [esi+330h]", b"\xD9\x96\x30\x03\x00\x00"),
    ("fstp [ebp+32Ch]", b"\xD9\x9D\x2C\x03\x00\x00"),
    ("fstp [ebp+330h]", b"\xD9\x9D\x30\x03\x00\x00"),
    ("mov  [esi+32Ch],ebx", b"\x89\x9E\x2C\x03\x00\x00"),
    ("mov  [esi+330h],ebx", b"\x89\x9E\x30\x03\x00\x00"),
    ("mov  [esi+328h],ebx", b"\x89\x9E\x28\x03\x00\x00"),
    ("mov  [esi+324h],ebx", b"\x89\x9E\x24\x03\x00\x00"),
]
for desc, pat in pats:
    h2 = []
    i = BLOB.find(pat)
    while i != -1:
        h2.append(TEXT_S + i); i = BLOB.find(pat, i + 1)
    w("  [{}] {} 处".format(desc, len(h2)))
    for h in h2[:30]:
        w("      {}".format(nm(h)))

hdr("G. 谁写 [obj+330h]/[32Ch] 之后立刻 call 随机数？(FootClass_Crash 复核)")
f = ida_funcs.get_func(0x4DEC60)
if f:
    dump(f.start_ea, min(f.start_ea + 0x130, f.end_ea), "FootClass_Crash")

OUT.close()
print("DONE")
