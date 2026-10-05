# -*- coding: utf-8 -*-
# 全量 DEFINE_HOOK 尺寸合规审计 v2（Syringe 落点契约）
#
# 规则来源：SyringeEx/SyringeDebugger.cpp:529-640
#   R1  原地址被覆盖长度 = max(声明的 size, 5)（jmp 固定 5 字节 E9 rel32，尾部 nop 补齐）
#   R2  return 0 时只重放"声明的 size"字节，然后无条件跳到 addr+5
#   R3  被覆盖区间若含相对跳转(Jcc/JMP/CALL)，搬迁到 trampoline 后偏移失效
#       （仅 SyringeEx 的 ReladdrInstructionFixup 会修，老版 Syringe 不修）
#
# 判定：
#   · size >= 5 且线性解码恰好 = size            → 基线合格
#   · size <  5                                  → 仅当重放块以 retn/ret/jmp 收尾时安全；
#                                                  否则 handler 绝不能 return 0，必须 return 显式地址
#   · 覆盖区间含相对跳转 且 handler 会 return 0   → 真实风险（需复核）
#   · 覆盖区间含相对跳转 但 handler 总返回显式地址 → 无风险（重放块永不执行）
import os
import re
import ida_auto
import ida_bytes
import idc
import ida_pro

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, "..", "src"))
OUT = os.path.join(HERE, "hooksize_audit.txt")

RE_HOOK = re.compile(
    r"DEFINE_HOOK(?:_AGAIN)?\s*\(\s*(0x[0-9A-Fa-f]+)\s*,\s*([A-Za-z_]\w*)\s*,\s*(0x[0-9A-Fa-f]+)\s*\)")
RE_RET0 = re.compile(r"\breturn\s+0\s*;")


def extract_body(text, start):
    """从 start 起找第一个 '{'，做括号配对返回函数体。"""
    i = text.find("{", start)
    if i < 0:
        return ""
    depth = 0
    j = i
    n = len(text)
    while j < n:
        c = text[j]
        if c == '"':
            j += 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
        elif c == "'":
            j += 1
            while j < n and text[j] != "'":
                j += 2 if text[j] == "\\" else 1
        elif c == "/" and j + 1 < n and text[j + 1] == "/":
            while j < n and text[j] != "\n":
                j += 1
        elif c == "/" and j + 1 < n and text[j + 1] == "*":
            j = text.find("*/", j + 2)
            if j < 0:
                break
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return text[i:j + 1]
        j += 1
    return text[i:]


def collect():
    out = []
    for root, _d, files in os.walk(SRC):
        for fn in files:
            if not fn.lower().endswith((".cpp", ".h", ".hpp")):
                continue
            p = os.path.join(root, fn)
            try:
                txt = open(p, "r", encoding="utf-8", errors="replace").read()
            except Exception:
                continue
            for m in RE_HOOK.finditer(txt):
                body = extract_body(txt, m.end())
                out.append((int(m.group(1), 16), m.group(2), int(m.group(3), 16),
                            os.path.relpath(p, os.path.dirname(SRC)),
                            bool(RE_RET0.search(body))))
    return out


def is_leave(mn):
    mn = (mn or "").lower()
    return mn in ("retn", "ret", "retf", "jmp")


def linear_decode(ea, need):
    """线性解码（不依赖 is_defined/next_head），累加到 >= need。"""
    seq = []
    cur = ea
    total = 0
    for _ in range(32):
        if total >= need:
            break
        if not idc.is_code(ida_bytes.get_flags(cur)):
            break
        ln = idc.get_item_size(cur)
        if ln <= 0:
            break
        seq.append((cur, ln, idc.print_insn_mnem(cur)))
        total += ln
        cur += ln
    return seq, total


def main():
    ida_auto.auto_wait()
    hooks = collect()
    bad, warn, small_ok, allrows = [], [], [], []

    for addr, name, size, srcfile, ret0 in hooks:
        if idc.is_unknown(ida_bytes.get_flags(addr)):
            bad.append((addr, name, size, srcfile, ret0, "地址在 IDB 中未定义"))
            allrows.append(("BAD", addr, name, size, srcfile, "UNDEFINED"))
            continue

        seq, total = linear_decode(addr, max(size, 1))
        mnems = [m for (_a, _s, m) in seq]
        has_rel = any((m or "").lower().startswith("j") or (m or "").lower() == "call"
                      for m in mnems)
        leaves = bool(seq) and is_leave(seq[-1][2])
        first_leave = bool(seq) and is_leave(seq[0][2])

        # 关键区分（v3）：
        #   · 只有 handler 会 return 0 时，R1/R2 的"覆盖 max(size,5) / 跳 addr+5"才会真正生效；
        #     若 handler 总返回显式地址，重放块永不执行 ⇒ size 多小都无所谓（Ares/LaserDraw 就是这种）
        #   · size<5 时若重放块自身以 retn/ret/jmp 收尾 ⇒ 控制流直接离开 ⇒ 安全
        #   · 首条指令跨过 size 边界 ⇒ 声明 size 切断了指令 ⇒ 只有 return 0 时才致命
        crosses = bool(seq) and seq[0][1] > size

        if ret0 and crosses:
            v = "BAD_CROSS(首条指令 %d 字节 > 声明 %d，且 handler 会 return 0)" % (seq[0][1], size)
            bad.append((addr, name, size, srcfile, ret0, v))
        elif not ret0:
            v = "OK   handler 只 return 显式地址，重放块永不执行"
            small_ok.append((addr, name, size, srcfile, v)) if size < 5 else None
            allrows.append(("ok", addr, name, size, srcfile, v))
        elif size < 5:
            if leaves:
                v = "OK   size<5 但重放块自身以 %s 收尾（控制流离开函数）" % seq[-1][2]
                small_ok.append((addr, name, size, srcfile, v))
                allrows.append(("ok", addr, name, size, srcfile, v))
            else:
                v = "BAD  size<5 且 handler 会 return 0 ⇒ 必落 addr+5 指令中间"
                bad.append((addr, name, size, srcfile, ret0, v))
                allrows.append(("BAD", addr, name, size, srcfile, v))
        else:
            if has_rel:
                v = "WARN 覆盖区间含相对跳转(%s) 且 handler 会 return 0" % ",".join(
                    sorted(set(m for m in mnems if (m or "").lower().startswith("j")
                               or (m or "").lower() == "call")))
                warn.append((addr, name, size, srcfile, ret0, v))
                allrows.append(("WRN", addr, name, size, srcfile, v))
            else:
                note = []
                if total < size:
                    note.append("尾部 %d 字节为填充" % (size - total))
                v = "OK" + (("  (" + "; ".join(note) + ")") if note else "")
                allrows.append(("ok", addr, name, size, srcfile, v))

    L = []
    L.append("=" * 112)
    L.append("DEFINE_HOOK 尺寸合规审计 v2   共 %d 条   BAD=%d   WARN=%d   size<5 但合法=%d"
             % (len(hooks), len(bad), len(warn), len(small_ok)))
    L.append("=" * 112)
    L.append("")
    L.append("### BAD —— 必须修（%d 条）" % len(bad))
    for addr, name, size, srcfile, ret0, v in bad:
        L.append("  !!! 0x%08X  %-52s size=0x%-3X %-34s\n        %s" % (addr, name, size, srcfile, v))
    L.append("")
    L.append("### WARN —— 需复核（%d 条）" % len(warn))
    for addr, name, size, srcfile, ret0, v in warn:
        L.append("  ~~~ 0x%08X  %-52s size=0x%-3X %-34s\n        %s" % (addr, name, size, srcfile, v))
    L.append("")
    L.append("### size<5 但合法（%d 条）" % len(small_ok))
    for addr, name, size, srcfile, v in small_ok:
        L.append("  ok  0x%08X  %-52s size=0x%-3X %-34s %s" % (addr, name, size, srcfile, v))
    L.append("")
    L.append("### 全量清单（%d）" % len(allrows))
    for tag, addr, name, size, srcfile, v in allrows:
        L.append("  [%s] 0x%08X  %-52s size=0x%-3X %-34s %s" % (tag, addr, name, size, srcfile, v))

    text = "\n".join(L)
    open(OUT, "w", encoding="utf-8", errors="replace").write(text)
    print("written", OUT, len(text))
    print("BAD=%d WARN=%d SMALL_OK=%d TOTAL=%d" % (len(bad), len(warn), len(small_ok), len(hooks)))


main()
ida_pro.qexit(0)
