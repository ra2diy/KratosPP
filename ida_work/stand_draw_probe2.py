# -*- coding: utf-8 -*-
# 确认两件事：
#  1) ObjectClass::CompareYSortValues (0x5F9220) 的极性 —— 返回值 true 表示什么
#  2) vt[0xB8] 是否指向 0x5F6BD0（即 hook 0x5F6BF7 真的影响排序键）
import idc
import idautils
import idaapi

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\stand_draw_probe2.txt"
fp = open(OUT, "w", encoding="utf-8", errors="replace")


def w(s=""):
    fp.write(str(s) + "\n")


def disasm_line(ea):
    try:
        return idc.generate_disasm_line(ea, 0)
    except Exception:
        return idc.GetDisasm(ea)


def dump_func(ea, label="", limit=120):
    w("")
    w("################ %s  @%08X ################" % (label, ea))
    f = idaapi.get_func(ea)
    if not f:
        w("  <no function>")
        return
    w("---- func %08X - %08X ----" % (f.start_ea, f.end_ea))
    cur, n = f.start_ea, 0
    while cur < f.end_ea and n < limit:
        raw = ""
        try:
            b = idc.get_bytes(cur, 6)
            if b:
                raw = " ".join("%02X" % c for c in bytearray(b))
        except Exception:
            pass
        w("  %08X  %-46s  %s" % (cur, disasm_line(cur), raw))
        cur = idc.next_head(cur, f.end_ea)
        n += 1


# 1) 比较函数
fam = idaapi.get_func(0x5F9220)
w("=== 0x551ADB 的 call 目标推算: 0x551AE0 + 0x0A4740 = 0x%08X ===" % (0x551AE0 + 0xA4740))
w("=== IDA 对该地址的命名: %s ===" % idc.get_name(0x5F9220))
dump_func(0x5F9220, "ObjectClass_CompareYSortValues", 80)

# 2) vt[0xB8] 归属
w("")
w("################ 谁引用了 0x5F6BD0 / 0x5F6BF7 ################")
for target in (0x5F6BD0, 0x5F6BF7):
    w("")
    w("---- DataRefsTo(0x%08X) ----" % target)
    got = False
    for x in idautils.DataRefsTo(target):
        got = True
        # 判断它落在哪个 vtable：向上找连续的 code-pointer 串
        base = x
        while True:
            prev = base - 4
            v = idc.get_wide_dword(prev)
            f = idaapi.get_func(v)
            if f and f.start_ea == v:
                base = prev
            else:
                break
        w("   ref @%08X   slot_off_in_guess_vtbl(0x%08X) = 0x%X   name_of_containing=%s"
          % (x, base, x - base, idc.get_name(base, 0)))
    if not got:
        w("   (none)")

# 3) ObjectClass 的 vtable 中找到 0xAB(0xAC) 与 0xB8 槽
w("")
w("################ 搜索 vtbl 中同时含 0x5F6BD0 的槽位 ################")
for x in idautils.DataRefsTo(0x5F6BD0):
    w("   0x%08X -> 0x5F6BD0 ; 该地址 - 0xB8 = 0x%08X" % (x, x - 0xB8))
    w("        [base-0x10 .. base+0x8] = %s" % " ".join(
        "%08X" % idc.get_wide_dword(x - 0xB8 + i * 4) for i in range(-1, 6)))

fp.close()
print("DONE2")
