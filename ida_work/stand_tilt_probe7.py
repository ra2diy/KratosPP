# -*- coding: utf-8 -*-
# 探针7：TechnoClass::GetCRC 是否消费 rocking/倾斜字段；FootClass_Crash 全景
import idaapi, idc, ida_bytes, ida_funcs, ida_name

OUT = open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\stand_tilt_probe7.txt", "w", encoding="utf-8")
def w(s=""): OUT.write(str(s) + "\n")
def hdr(t):
    w(""); w("############ " + t + " ############")

def dump(ea, end, title=""):
    w("== {} {:#010x}..{:#010x} ==".format(title, ea, end))
    a = ea
    while a < end:
        sz = idc.get_item_size(a)
        if sz <= 0: sz = 1
        raw = " ".join("{:02X}".format(b) for b in (ida_bytes.get_bytes(a, min(sz, 6)) or b""))
        w("   {:#010x}: {:20s} {}".format(a, raw, idc.generate_disasm_line(a, 0) or ""))
        a += sz

hdr("A. TechnoClass::GetCRC (0x70c270)")
f = ida_funcs.get_func(0x70c270)
if f: dump(f.start_ea, f.end_ea, "TechnoClass_GetCRC")

hdr("B. FootClass::GetCRC (0x4dbad0)")
f = ida_funcs.get_func(0x4dbad0)
if f: dump(f.start_ea, f.end_ea, "FootClass_GetCRC")

hdr("C. FootClass_Crash 全景")
f = ida_funcs.get_func(0x4debb0)
if f: dump(f.start_ea, f.end_ea, "FootClass_Crash")

hdr("D. TechnoClass_3D8 尾部 (0x70b520..0x70b570)")
dump(0x70b520, 0x70b570, "TechnoClass_3D8_tail")

hdr("E. AnimClass_AnimExtras 附近 0x423a40 (确认是否为误匹配)")
dump(0x423a30, 0x423a70, "AnimClass_AnimExtras_ctx")

OUT.close()
print("DONE")
