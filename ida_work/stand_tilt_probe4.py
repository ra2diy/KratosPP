# -*- coding: utf-8 -*-
# 只读探针 4：锁定 "倾斜积分器" 的调用者 / 写点 / 渲染坐标实现 / loco Process 是否写 Location
import idaapi, idc, ida_bytes, ida_funcs, ida_name, ida_ua, ida_nalt, idautils

OUT = open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\stand_tilt_probe4.txt", "w", encoding="utf-8")

def w(s=""):
    OUT.write(str(s) + "\n")

def hdr(t):
    w("")
    w("############ " + t + " ############")

def ea2name(ea):
    n = ida_name.get_ea_name(ea)
    f = ida_funcs.get_func(ea)
    fn = ida_name.get_ea_name(f.start_ea) if f else ""
    return "{} ({:#010x}) [{}]".format(n or "-", ea, fn or "-")

# ---------- 字节级线性解码成指令 ----------
def linear_insns(start, end, limit=4000):
    insns = []
    ea = start
    while ea < end and len(insns) < limit:
        sz = idc.get_item_size(ea)
        if sz <= 0:
            sz = 1
        mnem = idc.generate_disasm_line(ea, 0) or ""
        insns.append((ea, sz, mnem))
        ea += sz
    return insns

def dump(ea, end, title=None):
    if title:
        w("== {}  {:#010x}..{:#010x} ==".format(title, ea, end))
    else:
        w("== {:#010x}..{:#010x} ==".format(ea, end))
    for (a, sz, m) in linear_insns(ea, end):
        w("   {:#010x}: {:28s}".format(a, m))

hdr("A. 关键函数名字库检索")
KEYS = ["GetRenderCoords", "GetCoords", "41C", "Rocking", "Sink", "Ship", "Drive", "ILocomotion",
        "Desired_Facing256", "GetCellFloorHeight", "Sinking"]
names = {}
for ea, nm in idautils.Names():
    for k in KEYS:
        if k.lower() in nm.lower():
            names.setdefault(k, []).append((ea, nm))
for k in KEYS:
    lst = names.get(k, [])
    w("  --- 关键字 [{}] : {} 条 ---".format(k, len(lst)))
    for ea, nm in lst[:40]:
        w("      {:#010x}  {}".format(ea, nm))

hdr("B. TechnoClass_41C (0x70B570) 的调用者")
target = 0x70B570
xrefs = []
try:
    for x in idautils.XrefsTo(target, 0):
        xrefs.append((x.frm, x.type))
except Exception as e:
    w("  XrefsTo 失败: " + str(e))
for frm, t in xrefs:
    w("  被引用: {}".format(ea2name(frm)))
    f = ida_funcs.get_func(frm)
    if f:
        # 打印调用点上下文
        lo = max(f.start_ea, frm - 0x40)
        dump(lo, min(frm + 0x10, f.end_ea))

hdr("C. Rocking 步进字段 (+32Ch / +330h) 的全库写点")
# fstp / fst  [reg+32Ch] ; mov [reg+32Ch], ...
patterns = [
    (b"\xD9\x9E\x2C\x03\x00\x00", "fstp [esi+32Ch]"),
    (b"\xD9\x9E\x30\x03\x00\x00", "fstp [esi+330h]"),
    (b"\xD9\x9F\x2C\x03\x00\x00", "fstp [edi+32Ch]"),
    (b"\xD9\x9F\x30\x03\x00\x00", "fstp [edi+330h]"),
    (b"\xD9\x96\x2C\x03\x00\x00", "fst  [esi+32Ch]"),
    (b"\xD9\x96\x30\x03\x00\x00", "fst  [esi+330h]"),
    (b"\xD9\x9D\x2C\x03\x00\x00", "fstp [ebp+32Ch]"),
    (b"\xD9\x9D\x30\x03\x00\x00", "fstp [ebp+330h]"),
    (b"\x89\x9E\x2C\x03\x00\x00", "mov  [esi+32Ch], ebx"),
    (b"\x89\x9E\x30\x03\x00\x00", "mov  [esi+330h], ebx"),
    (b"\x89\x9E\x28\x03\x00\x00", "mov  [esi+328h], ebx"),
    (b"\x89\x9E\x24\x03\x00\x00", "mov  [esi+324h], ebx"),
    (b"\xF3\x0F\x11", "movss  (需人工看操作数)"),
]
start_ea = ida_nalt.get_imagebase()
# 搜代码段
for name in [".text"]:
    seg = idaapi.get_segm_by_name(name)
    if not seg:
        continue
    s, e = seg.start_ea, seg.end_ea
    w("  .text {:#x}..{:#x}".format(s, e))
    for pat, desc in patterns:
        if len(pat) < 6:
            continue
        ea = ida_bytes.find_bytes(pat, s, e)
        cnt = 0
        while ea != idaapi.BADADDR and cnt < 60:
            f = ida_funcs.get_func(ea)
            fn = ida_name.get_ea_name(f.start_ea) if f else "-"
            w("     [{}] {:#010x}  {}".format(desc, ea, fn))
            cnt += 1
            ea = ida_bytes.find_bytes(pat, ea + 1, e)
        if cnt == 0:
            w("     [{}] 无".format(desc))

hdr("D. ObjectClass::GetCoords / GetRenderCoords")
dump(0x5F6C80, 0x5F6D00, "ObjectClass_GetCoords")
for a in (0x5F6BD0,):
    f = ida_funcs.get_func(a)
    if f:
        dump(f.start_ea, min(f.start_ea + 0x80, f.end_ea), "GetYSort")

hdr("E. GetCellFloorHeight 的调用者（找 GetRenderCoords）")
gcfh = None
for ea, nm in idautils.Names():
    if "GetCellFloorHeight" in nm:
        gcfh = ea
        w("  找到: {} {}".format(nm, hex(ea)))
if gcfh:
    callers = set()
    for x in idautils.XrefsTo(gcfh, 0):
        f = ida_funcs.get_func(x.frm)
        if f:
            callers.add((f.start_ea, ida_name.get_ea_name(f.start_ea)))
    for s, n in sorted(callers):
        w("      caller: {:#010x} {}".format(s, n))

hdr("F. ILocomotion / DriveLocomotionClass::Process 是否写 [obj+9Ch]")
for nm_key in ["Process", "Draw_Matrix", "ZAdjust", "Is_Moving"]:
    pass
# 直接 dump 已知 loco 函数
for a, title in [(0x55A730, "ILocomotion_Draw_Matrix"), (0x55AB90, "ILocomotion_ZAdjust"),
                 (0x55ABA0, "ILocomotion_TiltPitchAI")]:
    f = ida_funcs.get_func(a)
    if f:
        dump(f.start_ea, min(f.start_ea + 0x90, f.end_ea), title)

OUT.close()
print("DONE")
