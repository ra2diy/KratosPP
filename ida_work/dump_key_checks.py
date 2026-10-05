# -*- coding: utf-8 -*-
# 关键判定：GetWeapon 语义 / LayerClass::Sort / DisplayClass::Submit / Greatest_Threat 门控与随机数
import idaapi, idc, idautils, ida_bytes, ida_segment, ida_funcs, ida_name

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\key_checks.txt"
f = open(OUT, "w", encoding="utf-8")
def w(s=""):
    f.write(str(s) + "\n"); f.flush()

def dump(ea, n=40, title=""):
    w("---- %s  @%08X ----" % (title, ea))
    p = ea
    for _ in range(n):
        w("   %08X  %s" % (p, idc.generate_disasm_line(p, 0)))
        p = idc.next_head(p)
        if p == idaapi.BADADDR:
            break
    w()

w("############ 0. 搜索含 GetWeapon 的名字 ############")
for i in range(1, idc.get_name_ea_simple.__self__.__sizeof__() if False else 0):
    pass
ea = idc.get_name_ea_simple("TechnoClass_GetWeapon")
w("TechnoClass_GetWeapon direct -> %s" % ("%08X" % ea if ea != idaapi.BADADDR else "N/A"))
names = []
for addr, name in idautils.Names():
    if "GetWeapon" in name or "WeaponStruct" in name:
        names.append((addr, name))
for a, n in sorted(names)[:40]:
    w("   %08X %s" % (a, n))
w()

w("############ 1. LayerClass::Sort  (vanilla 0x55DBC3 调用的 sub_551A30) ############")
dump(0x551A30, 60, "sub_551A30 (Sort)")
w("  xrefs to sub_551A30:")
for x in idautils.XrefsTo(0x551A30):
    w("    from %08X : %s" % (x.frm, idc.generate_disasm_line(x.frm, 0)))
w()

w("############ 2. Main_Loop 0x55DBC3 附近 ############")
dump(0x55DB80, 40, "Main_Loop tail")
w()

w("############ 3. DisplayClass::Submit ############")
for cand in ("DisplayClass_Submit", "DisplayClass::Submit"):
    e = idc.get_name_ea_simple(cand)
    if e != idaapi.BADADDR:
        w("found %s @%08X" % (cand, e))
        dump(e, 50, cand)
w()

w("############ 4. Greatest_Threat 0x6F8FE0..0x6F9060 (0x6F9039 的门控) ############")
dump(0x6F8FE0, 30, "Greatest_Threat region")
w()

w("############ 5. Greatest_Threat 内的 Random 调用 ############")
fn = ida_funcs.get_func(0x6F8DF0)
w("func %s %08X-%08X" % (idc.get_func_name(fn.start_ea), fn.start_ea, fn.end_ea))
p = fn.start_ea
cnt = 0
while p < fn.end_ea and p != idaapi.BADADDR:
    line = idc.generate_disasm_line(p, 0)
    if line and ("Random" in line or "rand" in line):
        w("   RNG? %08X  %s" % (p, line))
        cnt += 1
    p = idc.next_head(p, fn.end_ea)
w("  hits=%d" % cnt)
w()

w("############ 6. 0x8A0370 附近全局（层数组） ############")
for a in range(0x8A0370, 0x8A03F0, 4):
    w("   %08X : %08X  %s" % (a, ida_bytes.get_dword(a), idc.get_name(a) or ""))
w("  xrefs to 0x8A0390:")
for x in idautils.XrefsTo(0x8A0390):
    w("    from %08X : %s" % (x.frm, idc.generate_disasm_line(x.frm, 0)))

f.close()
print("DONE")
