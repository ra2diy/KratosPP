# -*- coding: utf-8 -*-
# 探针8：GetCRC (vt+0x34) 的调用点；Multiplay_LogToSync 的调用者
import idaapi, idc, ida_bytes, ida_funcs, ida_name, idautils, struct

OUT = open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\stand_tilt_probe8.txt", "w", encoding="utf-8")
def w(s=""): OUT.write(str(s) + "\n")
def hdr(t):
    w(""); w("############ " + t + " ############")

seg = idaapi.get_segm_by_name(".text")
TEXT_S, TEXT_E = seg.start_ea, seg.end_ea
BLOB = ida_bytes.get_bytes(TEXT_S, TEXT_E - TEXT_S)

def nm(ea):
    f = ida_funcs.get_func(ea)
    fn = ida_name.get_ea_name(f.start_ea) if f else ""
    return "{} | {:#010x}".format(fn or "-", ea)

hdr("A. call [reg+0x34]  (GetCRC) 的全部调用点")
for pref in [b"\xFF\x50", b"\xFF\x51", b"\xFF\x52", b"\xFF\x53", b"\xFF\x55", b"\xFF\x56", b"\xFF\x57"]:
    pat = pref + b"\x34"
    i = BLOB.find(pat)
    while i != -1:
        w("   {}".format(nm(TEXT_S + i)))
        i = BLOB.find(pat, i + 1)

hdr("B. 可能的 CRC/同步采样函数名（含 CRC / Sync / LogToSync）")
for ea, n in idautils.Names():
    ln = n.lower()
    if ("crc" in ln) or ("sync" in ln):
        w("   {:#010x}  {}".format(ea, n))

hdr("C. Multiplay_LogToSync 相关函数的反汇编头部")
for a, t in [(0x651f00, "LogToSync?"), (0x651c00, "LogToSync?"), (0x651e00, "LogToSync?")]:
    f = ida_funcs.get_func(a)
    if f:
        w("  == {} {:#010x}..{:#010x} ==".format(t, f.start_ea, f.end_ea))
        ea = f.start_ea
        for _ in range(40):
            if ea >= f.end_ea:
                break
            w("     {:#010x}: {}".format(ea, idc.generate_disasm_line(ea, 0) or ""))
            sz = idc.get_item_size(ea) or 1
            ea += sz

hdr("D. 写 [reg+0x425] 的点（坠机/沉没标志）")
b = struct.pack("<I", 0x425)
for pref in [b"\xC6\x86", b"\xC6\x87", b"\xC6\x85", b"\xC6\x80", b"\xC6\x81", b"\xC6\x82", b"\xC6\x83"]:
    pat = pref + b
    i = BLOB.find(pat)
    while i != -1:
        w("   {}".format(nm(TEXT_S + i)))
        i = BLOB.find(pat, i + 1)

OUT.close()
print("DONE")
