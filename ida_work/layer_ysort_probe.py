# -*- coding: utf-8 -*-
# 目的是把"层级 / 排序 / 遮挡"三个旋钮的引擎侧事实钉死
import os
import ida_auto
import ida_bytes
import ida_lines
import ida_pro
import idc

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "layer_ysort_probe.txt")
lines = []


def dis(start, end, title, marks=()):
    lines.append("== %s  0x%08X..0x%08X ==" % (title, start, end))
    ea = start
    n = 0
    while ea < end and n < 120:
        line = ida_lines.generate_disasm_line(ea, 0)
        if not line:
            break
        b = ida_bytes.get_bytes(ea, 8)
        hexs = " ".join("%02X" % c for c in b) if b else "?"
        m = ">>" if ea in marks else "  "
        lines.append("%s %08X: %-30s | %s" % (m, ea, hexs, line))
        nxt = idc.next_head(ea)
        if nxt <= ea:
            break
        ea = nxt
        n += 1
    lines.append("")


def callers(target, limit=40):
    lines.append("---- 调用/引用 0x%08X 的位置 ----" % target)
    cnt = 0
    ea = 0
    while True:
        ea = idc.next_head(ea)
        if ea == idc.BADADDR or ea == 0:
            break
        cnt += 1
        if cnt > 4000000:
            break
        for x in ida_bytes.get_dword(ea), :
            pass
        # 简化：只查 call rel32 与 立即数引用
        if ida_bytes.get_byte(ea) == 0xE8:
            rel = ida_bytes.get_dword(ea + 1)
            if rel >= 0x80000000:
                rel -= 0x100000000
            if ea + 5 + rel == target:
                lines.append("   call  @%08X -> 0x%08X" % (ea, target))
        if ida_bytes.get_word(ea) == 0xFF15 or ida_bytes.get_byte(ea) == 0x68:
            pass
    lines.append("")


def main():
    ida_auto.auto_wait()

    # 1. vanilla AnimClass::InWhichLayer（Phobos 用 0x424CB0 size 6 覆盖它）
    dis(0x424C90, 0x424CE0, "AnimClass::InWhichLayer 区域（Phobos kernel 落点 0x424CB0/6字节）",
        marks=(0x424CB0,))

    # 2. DisplayClass::Remove（确认保序前移）
    dis(0x4A9770, 0x4A97D0, "DisplayClass::Remove", marks=(0x4A9770,))

    # 3. FootClass::GetZAdjustment（Kratos 0x4DB091 size 6）
    dis(0x4DB080, 0x4DB0C0, "FootClass::GetZAdjustment 区域", marks=(0x4DB091,))

    # 4. AnimClass vtable 里 0x78 槽（InWhichLayer）指向
    #    AnimClass vtable 基址先找一下：搜 data refs to 0x424CB0
    lines.append("---- 谁的数据表里出现 0x424CB0（含 vtable 槽） ----")
    ea = 0
    hits = 0
    while hits < 40:
        ea = idc.next_head(ea)
        if ea == idc.BADADDR or ea == 0:
            break
        if ida_bytes.get_dword(ea) == 0x424CB0:
            lines.append("   ref @%08X" % ea)
            hits += 1
    lines.append("")

    # 5. 直接 call 到 DisplayClass::Submit(0x4A9720) 的位置
    lines.append("---- 直接 call DisplayClass::Submit(0x4A9720) 的位置 ----")
    ea = 0
    while True:
        ea = idc.next_head(ea)
        if ea == idc.BADADDR or ea == 0:
            break
        if ida_bytes.get_byte(ea) == 0xE8:
            rel = ida_bytes.get_dword(ea + 1)
            if rel >= 0x80000000:
                rel -= 0x100000000
            if ea + 5 + rel == 0x4A9720:
                f = idc.get_func_name(ea)
                lines.append("   call @%08X   in %s" % (ea, f))
    lines.append("")

    # 6. 直接 call 到 ObjectClass::GetYSort(0x5F6BD0) 的位置（除 vt 槽外）
    lines.append("---- 直接 call ObjectClass::GetYSort(0x5F6BD0) 的位置 ----")
    ea = 0
    while True:
        ea = idc.next_head(ea)
        if ea == idc.BADADDR or ea == 0:
            break
        if ida_bytes.get_byte(ea) == 0xE8:
            rel = ida_bytes.get_dword(ea + 1)
            if rel >= 0x80000000:
                rel -= 0x100000000
            if ea + 5 + rel == 0x5F6BD0:
                lines.append("   call @%08X   in %s" % (ea, idc.get_func_name(ea)))
    lines.append("")

    text = "\n".join(lines)
    open(OUT, "w", encoding="utf-8", errors="replace").write(text)
    print("written", OUT, len(text))


main()
ida_pro.qexit(0)
