# -*- coding: utf-8 -*-
# 验证：DEFINE_HOOK 的 size 参数 == 该处原始指令的字节长度（与"跳转长度"无关）
import os
import ida_auto
import ida_bytes
import ida_lines
import ida_pro
import idc

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "hooksize_probe.txt")
lines = []

# (地址, 声明 size, 名称, 来源)
TARGETS = [
    (0x004A975E, 0x2, "DisplayClass_Submit_KeepStandAboveMaster", "Kratos 本次新增"),
    (0x0065C7D0, 0x1, "Random2Class_Random_SyncLog", "Kratos/Phobos 均有"),
    (0x0065C88A, 0x3, "Random2Class_RandomRanged_SyncLog", "Kratos/Phobos 均有"),
    (0x0046C722, 0x4, "BulletTypeClass_Load_Suffix", "Kratos"),
    (0x0046C74A, 0x3, "BulletTypeClass_Save_Suffix", "Kratos"),
    (0x0042898A, 0x3, "AnimTypeClass_Save_Suffix", "Kratos"),
    (0x006CE8EA, 0x3, "SuperWeaponTypeClass_Save_Suffix", "Kratos"),
    (0x006F42F7, 0x2, "TechnoClass_Init", "Phobos"),
    (0x0050B669, 0x3, "HouseClass_ShouldDisableCameo_GreyCameo", "Phobos"),
    (0x00518505, 0x4, "InfantryClass_ReceiveDamage_NotHuman", "Phobos"),
    (0x006CDE40, 0x3, "SuperClass_Place_FireExt", "Phobos"),
]


def decode_len(ea, need):
    """从 ea 起线性解码，累加到 >= need，返回 (指令列表, 总长)。"""
    out = []
    cur = ea
    total = 0
    for _ in range(16):
        if total >= need:
            break
        nxt = idc.next_head(cur)
        if nxt <= cur:
            out.append((cur, 0, "<无法解码>"))
            break
        line = ida_lines.generate_disasm_line(cur, 0)
        out.append((cur, nxt - cur, line))
        total += nxt - cur
        cur = nxt
    return out, total


def main():
    ida_auto.auto_wait()
    for addr, size, name, src in TARGETS:
        f = ida_bytes.get_flags(addr)
        if idc.is_unknown(f):
            lines.append("== 0x%08X [%s] size=0x%X  源=%s" % (addr, name, size, src))
            lines.append("    !! 该地址在 IDB 中未定义为指令")
            lines.append("")
            continue

        b16 = ida_bytes.get_bytes(addr, 16)
        hexs = " ".join("%02X" % c for c in b16) if b16 else "?"
        seq, total = decode_len(addr, size)
        verdict = "OK" if total == size else "!!MISMATCH"
        lines.append("== 0x%08X [%s]  size=0x%X  源=%s" % (addr, name, size, src))
        lines.append("   16 字节: %s" % hexs)
        for s, sz, txt in seq:
            lines.append("     +%-2d 0x%08X (%d 字节) | %s" % (s - addr, s, sz, txt))
        lines.append("   累加长度 = %d (0x%X)  vs  声明 size = %d (0x%X)   => %s"
                     % (total, total, size, size, verdict))
        lines.append("")

    text = "\n".join(lines)
    with open(OUT, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT, len(text))


main()
ida_pro.qexit(0)
