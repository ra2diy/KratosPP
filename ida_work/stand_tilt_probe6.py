# -*- coding: utf-8 -*-
# 只读探针 6：倾斜角(0x324/0x328)的全部读取者；Ship/Drive 的 Draw_Matrix；YSort；TechnoClass_Update 调用点上下文
import idaapi, idc, ida_bytes, ida_funcs, ida_name, idautils, struct

OUT = open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\stand_tilt_probe6.txt", "w", encoding="utf-8")
def w(s=""): OUT.write(str(s) + "\n")
def hdr(t):
    w(""); w("############ " + t + " ############")

seg = idaapi.get_segm_by_name(".text")
TEXT_S, TEXT_E = seg.start_ea, seg.end_ea
BLOB = ida_bytes.get_bytes(TEXT_S, TEXT_E - TEXT_S)

def nm(ea):
    f = ida_funcs.get_func(ea)
    fn = ida_name.get_ea_name(f.start_ea) if f else ""
    return "{} | {}".format(fn or "-", hex(ea))

REG = {"\x80":"eax","\x81":"ecx","\x82":"edx","\x83":"ebx","\x84":"esp",
       "\x85":"ebp","\x86":"esi","\x87":"edi"}

def scan_float_reads(off, dn=""):
    """fld/fcomp/fadd/fsub/fmul/fst(p) [reg+off]  —— 抓全部读取者"""
    res = {}
    for i in range(len(BLOB) - 6):
        if BLOB[i] != 0xD8 and BLOB[i] != 0xD9:
            continue
        # D9 84/85/86/87 xx  ; modrm = 10 reg rm -> [reg+disp32]
        modrm = BLOB[i+1]
        if (modrm & 0xC0) != 0x80:
            continue
        rm = modrm & 0x07
        if rm == 5:
            continue
        if rm in (4,):
            continue
        if BLOB[i+2]*1 + BLOB[i+3]*256 + BLOB[i+4]*65536 + BLOB[i+5]*16777216 != off:
            continue
        # 需 0xD9 0x84..0x87 (fld) / 0xD8 0x84..0x87 (fadd/fsub/fmul/fcom)
        op = idc.generate_disasm_line(TEXT_S + i, 0) or ""
        res.setdefault(op.split()[0], []).append(TEXT_S + i)
    w("  --- [reg+{:#05x}] {} 读取者 ---".format(off, dn))
    for k, v in sorted(res.items()):
        w("     {} : {} 处".format(k, len(v)))
        for a in v:
            w("        {}".format(nm(a)))
    if not res:
        w("     （无）")

hdr("A. 倾斜角字段的全部浮点读取者")
for off, dn in [(0x324, "AngleRotatedSideways"), (0x328, "AngleRotatedForwards"),
                (0x32C, "RockingSidewaysPerFrame"), (0x330, "RockingForwardsPerFrame")]:
    scan_float_reads(off, dn)

hdr("B. 用 mov/整数方式读 [reg+324h] 等（结构体拷贝除外）")
for off in (0x324, 0x328, 0x32C, 0x330):
    hits = []
    b = struct.pack("<I", off)
    for pref in [b"\x8B\x80", b"\x8B\x81", b"\x8B\x82", b"\x8B\x83", b"\x8B\x85", b"\x8B\x86", b"\x8B\x87",
                 b"\x8B\x48", b"\x8B\x50", b"\x8B\x58"]:
        pat = pref + b
        i = BLOB.find(pat)
        while i != -1:
            hits.append(TEXT_S + i); i = BLOB.find(pat, i + 1)
    w("  [reg+{:#05x}] mov 读取 : {} 处".format(off, len(hits)))
    for h in hits[:30]:
        w("      {}".format(nm(h)))

def dump(ea, end, title=""):
    w("== {} {:#010x}..{:#010x} ==".format(title, ea, end))
    a = ea
    while a < end:
        sz = idc.get_item_size(a)
        if sz <= 0: sz = 1
        w("   {:#010x}: {}".format(a, idc.generate_disasm_line(a, 0) or ""))
        a += sz

hdr("C. LocomotionClass_ILocomotion_DrawMatrix (0x55a730)")
f = ida_funcs.get_func(0x55a730)
if f: dump(f.start_ea, f.end_ea, "ILocomotion_DrawMatrix")

hdr("D. ShipLocomotionClass / DriveLocomotionClass 的 ILocomotion vtable")
def dump_vtable(start, n=90):
    for i in range(n):
        ea = start + i * 4
        v = ida_bytes.get_dword(ea)
        if not (TEXT_S <= v < TEXT_E):
            w("     [{:#05x}] {:#010x} (非代码/越界)".format(i*4, v))
            break
        ff = ida_funcs.get_func(v)
        fn = ida_name.get_ea_name(ff.start_ea) if ff else "-"
        w("     [{:#05x}] {:#010x} {}".format(i*4, v, fn))
dump_vtable(0x007f2d8c, 70)
w("")
dump_vtable(0x007e7eb0, 70)

hdr("E. ShipLocomotionClass_blah (0x6a1c80)")
f = ida_funcs.get_func(0x6a1c80)
if f: dump(f.start_ea, min(f.start_ea + 0x200, f.end_ea), "ShipLocomotionClass_blah")

hdr("F. TechnoClass_Update 中 call [reg+41Ch] 的上下文 (0x6fa236)")
f = ida_funcs.get_func(0x6fa236)
if f:
    w("  所属函数: {}".format(nm(f.start_ea)))
    dump(max(f.start_ea, 0x6fa1d0), 0x6fa270)

hdr("G. ObjectClass_ReturnRealYSort (0x5f6bd0)")
f = ida_funcs.get_func(0x5f6bd0)
if f: dump(f.start_ea, f.end_ea, "ReturnRealYSort")

hdr("H. TechnoClass_Update 里的 vt+0x41C 槽位确认 (读 0x7f4d7c)")
for base in (0x7f4960,):
    v = ida_bytes.get_dword(base + 0x41C)
    w("  dword @ {:#x} = {:#x}  {}".format(base + 0x41C, v, nm(v)))

OUT.close()
print("DONE")
