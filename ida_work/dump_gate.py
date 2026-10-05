# -*- coding: utf-8 -*-
import idaapi, idc, idautils, ida_bytes, ida_funcs

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\gate_check.txt"
f = open(OUT, "w", encoding="utf-8")
def w(s=""):
    f.write(str(s) + "\n"); f.flush()

def dump(ea, n, title):
    w("---- %s @%08X ----" % (title, ea))
    p = ea
    for _ in range(n):
        w("   %08X  %s" % (p, idc.generate_disasm_line(p, 0)))
        nxt = idc.next_head(p)
        if nxt == idaapi.BADADDR:
            break
        p = nxt
    w()

w("########## 0x6F9039 精确门控：0x6F9020 - 0x6F9080 ##########")
dump(0x6F9020, 40, "Greatest_Threat heal branch")

w("########## TechnoClass_GetWeapon 0x70E140 ##########")
dump(0x70E140, 25, "TechnoClass_GetWeapon")

w("########## TechnoType_GetWeapon 0x7177C0 ##########")
dump(0x7177C0, 20, "TechnoType_GetWeapon")

w("########## ObjectClass_GetWeaponRange 0x5F4390 ##########")
dump(0x5F4390, 20, "ObjectClass_GetWeaponRange")

w("########## 找 DisplayClass::GetLayer / LayerClass::Sort 名字 ##########")
for addr, name in idautils.Names():
    low = name.lower()
    if "getlayer" in low or "layerclass" in low or ("sort" in low and "layer" in low) or "shortlayer" in low:
        w("   %08X %s" % (addr, name))

w("########## 0x55DBC3 的 unk_8A0390 是否是 Layer（看 GetLayer 返回值写入点） ##########")
for x in idautils.XrefsTo(0x8A0390):
    w("   xref %08X : %s" % (x.frm, idc.generate_disasm_line(x.frm, 0)))
w("  对 0x4A8D10(候选 GetLayer) 的 xref 计数: %d" % len(list(idautils.XrefsTo(0x4A8D10))))

f.close()
print("DONE")
