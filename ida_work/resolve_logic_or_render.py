# -*- coding: utf-8 -*-
# 判定若干 Hook 地址所属函数，以及这些函数的调用者，用来区分"逻辑帧"还是"渲染帧"。
import idaapi, idc, idautils

OUT = r"D:/Workspace/ra2mod/platform/KratosPP/ida_work/logic_or_render.txt"

ADDRS = [
    (0x6FD38D, "WeaponExtHook VisualScatter Hook #1 (CreateLaser)"),
    (0x6FD514, "WeaponExtHook VisualScatter Hook #2 (CreateEBolt)"),
    (0x6FD70D, "WeaponExtHook VisualScatter Hook #3 (CreateRBeam)"),
    (0x6FF08B, "TechnoClass_Fire_RecordBullet"),
    (0x6FDD50, "TechnoClass::Fire"),
    (0x6F9B7E, "SyncLog SelectAutoTarget 点"),
    (0x6F9E50, "TechnoClass_Update"),
    (0x6FAF7A, "TechnoClass_UpdateEnd"),
    (0x702299, "Destroy_VxlDebris_Remap"),
]

def fname(ea):
    f = idaapi.get_func(ea)
    if not f:
        return None
    return (idc.get_func_name(f.start_ea), f.start_ea, f.end_ea)

lines = []
for ea, note in ADDRS:
    lines.append("=" * 90)
    lines.append("0x%08X  %s" % (ea, note))
    info = fname(ea)
    if not info:
        lines.append("  (不在任何已识别函数内)")
        continue
    name, s, e = info
    lines.append("  所属函数: %s  [0x%X - 0x%X]" % (name, s, e))
    lines.append("  调用者(XrefsTo 函数起点):")
    cnt = 0
    for x in idautils.XrefsTo(s, 0):
        lines.append("      0x%08X  %s" % (x.frm, fname(x.frm)[0] if fname(x.frm) else "?"))
        cnt += 1
    if cnt == 0:
        lines.append("      (无直接调用 -> 可能只通过虚表调用)")
    # 该函数内被引用的字符串/名字线索
    lines.append("")

with open(OUT, "w", encoding="utf-8") as fp:
    fp.write("\n".join(lines))

idc.qexit(0)
