# -*- coding: utf-8 -*-
# 探针 2：TechnoClass 字段偏移、vtable 槽位、rocking/GetZAdjustment/GetCoords 实现
import os

import ida_auto
import ida_bytes
import ida_funcs
import ida_lines
import ida_name
import ida_pro
import idc

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "stand_tilt_probe2.txt")
lines = []


def w(s=""):
    lines.append(s)


def dis(start, end, title, marks=(), maxn=900):
    w("== %s  0x%08X..0x%08X ==" % (title, start, end))
    ea = start
    n = 0
    while ea < end and n < maxn:
        line = ida_lines.generate_disasm_line(ea, 0)
        if not line:
            break
        b = ida_bytes.get_bytes(ea, 8)
        hexs = " ".join("%02X" % c for c in b) if b else "?"
        m = ">>" if ea in marks else "  "
        w("%s %08X: %-24s | %s" % (m, ea, hexs, ida_lines.tag_remove(line)))
        nxt = idc.next_head(ea)
        if nxt <= ea:
            break
        ea = nxt
        n += 1
    w("")


def find_bytes(pattern, limit=60):
    res = []
    pat = bytes(int(x, 16) for x in pattern.split())
    try:
        import ida_search
        cur = 0x401000
        end = 0x8A0000
        while len(res) < limit:
            hit = ida_search.find_binary(cur, end, " ".join("%02X" % c for c in pat), 16,
                                         ida_search.SEARCH_DOWN)
            if hit == idc.BADADDR or hit is None:
                break
            res.append(hit)
            cur = hit + 1
    except Exception as e:
        w("find err %s" % e)
    return res


def main():
    ida_auto.auto_wait()

    # ---------- A. 结构体成员偏移 ----------
    w("############ A. TechnoClass / FootClass 结构体成员（含 0x300..0x3A0 区） ############")
    try:
        import ida_typeinf
        import ida_struct

        def dump_struct(tname):
            tid = ida_typeinf.get_named_type_tid(tname)
            if tid is None or tid == idc.BADADDR:
                w("  (未找到 %s)" % tname)
                return
            members = []
            m = ida_struct.get_member_by_id(tid)
            # 遍历成员
            off = 0
            while True:
                mm = ida_struct.get_member(tid, off)
                if mm is None:
                    break
                nm = ida_struct.get_member_name(mm.id)
                sz = ida_struct.get_member_size(mm)
                members.append((mm.soff, sz, nm))
                off = mm.soff + max(sz, 1)
                if len(members) > 400:
                    break
            w("  === %s  成员数 %d ===" % (tname, len(members)))
            for so, sz, nm in members:
                if 0x2F0 <= so <= 0x3A0:
                    w("      +0x%03X  size=%-3d  %s" % (so, sz, nm))

        for t in ("_TechnoClass", "TechnoClass", "_FootClass", "FootClass"):
            dump_struct(t)
    except Exception as e:
        w("  结构体 API 失败: %s" % e)
    w("")

    # ---------- B. 反查 [esi+324h] 等浮点访问 ----------
    w("############ B. AngleRotated / Rocking 字段的机器码访问点 ############")
    pats = {
        "fld [esi+324h]": "D9 86 24 03 00 00",
        "fld [esi+328h]": "D9 86 28 03 00 00",
        "fstp [esi+324h]": "D9 9E 24 03 00 00",
        "fstp [esi+328h]": "D9 9E 28 03 00 00",
        "fst [esi+324h]": "D9 96 24 03 00 00",
        "fst [esi+328h]": "D9 96 28 03 00 00",
        "fld [esi+32Ch]": "D9 86 2C 03 00 00",
        "fld [esi+330h]": "D9 86 30 03 00 00",
        "fstp [esi+32Ch]": "D9 9E 2C 03 00 00",
        "fstp [esi+330h]": "D9 9E 30 03 00 00",
    }
    for nm, pat in pats.items():
        hits = find_bytes(pat, 40)
        w("  --- %s : %d 处 ---" % (nm, len(hits)))
        for h in hits[:20]:
            f = ida_funcs.get_func(h)
            w("      0x%08X  %s" % (h, ida_funcs.get_func_name(f.start_ea) if f else "(no func)"))
    w("")

    # ---------- C. 关键实现 ----------
    w("############ C. FootClass::GetZAdjustment 全体 ############")
    dis(0x4DAFC0, 0x4DB0A0, "FootClass_Get_ZAdjustment", marks=(0x4DB091,))

    w("############ D. TechnoClass::GetZAdjustment 全体 ############")
    dis(0x704350, 0x704400, "TechnoClass_Get_ZAdjustment")

    w("############ E. ObjectClass::GetCoords 实现 ############")
    dis(0x5F6C80, 0x5F6CC0, "ObjectClass_GetCoords_1")
    dis(0x4104C0, 0x410560, "AbstractClass_GetCoords家族")

    w("############ F. AnimClass::GetCoords ############")
    dis(0x422BE0, 0x422C20, "AnimClass_GetCoords")

    w("############ G. TechnoClass::GetHeight ############")
    dis(0x5F5F40, 0x5F5FB0, "TechnoClass_GetHeight")

    w("############ G2. TechnoClass_41C（含 PI/4 比较，疑似 rocking/下沉逻辑） ############")
    dis(0x70B570, 0x70BCAA, "TechnoClass_41C", marks=(0x70B791,), maxn=1400)

    # ---------- H. vtable 槽位 ----------
    w("############ H. TechnoClass / ObjectClass vtable ############")
    names = []
    n = ida_name.get_nlist_size()
    for i in range(n):
        nm = ida_name.get_nlist_name(i)
        if nm and nm.startswith("??_7") and ("TechnoClass" in nm or "ObjectClass" in nm or "FootClass" in nm):
            names.append((ida_name.get_nlist_ea(i), nm))
    names.sort()
    for ea, nm in names[:12]:
        w("  vtable %08X %s" % (ea, nm))
    w("")
    # 找 TechnoClass 主 vtable
    vt = None
    for ea, nm in names:
        if nm == "??_7TechnoClass@@6B@":
            vt = ea
            break
    if vt:
        w("  === ??_7TechnoClass@@6B@ @0x%08X 槽位表 ===" % vt)
        for i in range(0, 0x100, 4):
            tgt = ida_bytes.get_dword(vt + i)
            if tgt == 0:
                continue
            f = ida_funcs.get_func(tgt)
            fname = ida_funcs.get_func_name(f.start_ea) if f else ""
            mark = "   <<<" if i in (0xA0, 0xAC, 0xB0, 0xB4, 0xB8, 0xBC) else ""
            w("      vt+0x%03X -> 0x%08X  %s%s" % (i, tgt, fname, mark))
    w("")

    text = "\n".join(lines)
    with open(OUT, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT, len(text))


main()
ida_pro.qexit(0)
