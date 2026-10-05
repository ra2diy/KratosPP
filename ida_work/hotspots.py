import idaapi, idc, idautils

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\hotspots.txt"
TSV = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\hooks.tsv"
f = open(OUT, "w", encoding="utf-8")


def w(s=""):
    f.write(s + "\n")


def funcinfo(ea):
    fn = idaapi.get_func(ea)
    if not fn:
        return ("<no func>", 0, 0)
    return (idc.get_func_name(fn.start_ea), fn.start_ea, fn.end_ea)


def callers_of(ea, depth=0, seen=None, maxdepth=2, out=None):
    """递归收集调用者（限制深度）"""
    if seen is None:
        seen = set()
    if out is None:
        out = []
    if depth > maxdepth or ea in seen:
        return out
    seen.add(ea)
    for x in idautils.XrefsTo(ea, 0):
        if x.type not in (16, 17, 18, 19, 20, 21):  # 只保留近调用/远调用
            continue
        cf = idaapi.get_func(x.frm)
        if cf:
            out.append((depth, x.frm, idc.get_func_name(cf.start_ea), cf.start_ea))
            callers_of(cf.start_ea, depth + 1, seen, maxdepth, out)
        else:
            out.append((depth, x.frm, "sub_%X" % x.frm, 0))
    return out


def dis(ea, n, title):
    w("=" * 120)
    w("### " + title)
    fn, fs, fe = funcinfo(ea)
    w("containing func: %s  [%08X-%08X]" % (fn, fs, fe))
    cs = callers_of(fs, maxdepth=2)
    if cs:
        w("--- callers (depth<=2) ---")
        for d, frm, nm, st in sorted(cs):
            w("   %s%s  (call site %08X)" % ("  " * d, nm, frm))
    w("-" * 120)
    cur = ea
    for i in range(n):
        tag = "  <<< HOOK" if i == 0 else ""
        w("   %08X  %-44s%s" % (cur, idc.generate_disasm_line(cur, 0), tag))
        nxt = idc.next_head(cur, cur + 16)
        if nxt <= cur:
            break
        cur = nxt
    w("")


# ---------- 1. Hook 按引擎函数分组 ----------
rows = []
for ln in open(TSV, "r", encoding="utf-8"):
    p = ln.rstrip("\n").split("\t")
    if len(p) < 4:
        continue
    try:
        rows.append((int(p[0], 16), p[1], p[2], p[3]))
    except ValueError:
        pass

w("#" * 120)
w("### A. 全部 %d 个 Hook 所在引擎函数分组（按 Hook 数降序）" % len(rows))
w("#" * 120)
funcs = {}
for addr, name, size, loc in rows:
    fn, fs, fe = funcinfo(addr)
    funcs.setdefault((fs, fn), []).append((addr, name, loc))
for (fs, fn), lst in sorted(funcs.items(), key=lambda kv: -len(kv[1])):
    w("%08X %-52s  %2d hooks | %s" % (fs, fn[:52], len(lst),
      ", ".join("%X" % a for a, _, _ in sorted(lst))))
    for a, nm, loc in sorted(lst):
        w("            %08X  %-54s %s" % (a, nm[:54], loc))
w("")

# ---------- 2. 热点反汇编 ----------
w("#" * 120)
w("### B. 索敌 / 移动 / 开火链路 热点 Hook 反汇编")
w("#" * 120)

dis(0x6F9039, 16, "0x6F9039 TechnoClass_Greatest_Threat_HealWeaponRange")
dis(0x4D9947, 14, "0x4D9947 FootClass_Greatest_Threat_GetTarget")
dis(0x4DA87A, 14, "0x4DA87A FootClass_Update_UpdateLayer")
dis(0x4D94B0, 14, "0x4D94B0 FootClass_SetDestination_Stand")
dis(0x75C7E0, 8, "0x75C7E0 WalkLocomotionClass_In_Which_Layer")
dis(0x5F6BF7, 12, "0x5F6BF7 ObjectClass_GetYSort")
dis(0x6FCDBE, 10, "0x6FCDBE TechnoClass_SetTarget_Stand")
dis(0x6FA45D, 10, "0x6FA45D TechnoClass_Update_NotHuman_ClearTarget_Stand")
dis(0x6FC749, 12, "0x6FC749 TechnoClass_CanFire_WhichLayer_Stand")
dis(0x6FA2A2, 10, "0x6FA2A2 TechnoClass_Update_DrawBehind")
dis(0x6F36DB, 10, "0x6F36DB TechnoClass_SelectWeapon")
dis(0x6FF66C, 10, "0x6FF66C TechnoClass_Fire_DecreaseAmmo")
dis(0x6FCDB0, 10, "0x6FCDB0 TechnoClass_AssignTarget_SyncLog")
dis(0x6F9256, 10, "0x6F9256 (Greatest_Threat 内 Stand 相关参考点)")
dis(0x6F903E, 12, "0x6F903E Greatest_Threat 续")
dis(0x6F8E50, 24, "0x6F8E50 Greatest_Threat 起点候选")

# ---------- 3. 关键函数是否可达 AttackMove ----------
w("#" * 120)
w("### C. Mission 相关函数与 Greatest_Threat 的调用关系")
w("#" * 120)
for addr, label in [(0x6F9039, "Greatest_Threat(HealWeaponRange hook)"),
                    (0x4D9947, "FootClass::Greatest_Threat(hook)"),
                    (0x4DA87A, "FootClass::Update(UpdateLayer hook)"),
                    (0x5F6BF7, "ObjectClass::GetYSort(hook)"),
                    (0x75C7E0, "WalkLocomotionClass::In_Which_Layer(hook)")]:
    fn, fs, fe = funcinfo(addr)
    w("--- %s -> %s [%08X]" % (label, fn, fs))
    for d, frm, nm, st in sorted(set(callers_of(fs, maxdepth=3))):
        w("   %s%s  (site %08X)" % ("  " * d, nm, frm))
    w("")

f.close()
print("DONE")
