# -*- coding: utf-8 -*-
"""Kratos Hook 指令边界全量审计（直接从 src/ 解析，含最新改动）。

判据（对每个 DEFINE_HOOK/DEFINE_PATCH 的 (addr,size)）：
  1. addr 必须是 IDA 认定的指令头（is_head）且已定义；
  2. 从 addr 起逐条指令累加，必须能**恰好**凑出 size 字节
     （即最后一条指令的结束地址 == addr+size）；
     否则 Syringe 重放会把半条指令交给 CPU —— 垃圾跳转/野写的高危来源。
  3. 统计「跨边界 / 非指令头 / 未定义」三类问题。

输出：boundary_audit.txt
"""

import os
import re

import ida_auto
import ida_bytes
import ida_funcs
import ida_lines
import ida_pro
import ida_ua
import idc

HERE = os.path.dirname(os.path.abspath(__file__))
SRC_ROOT = os.path.normpath(os.path.join(HERE, "..", "src"))
OUT_FILE = os.path.join(HERE, "boundary_audit.txt")

# DEFINE_HOOK(0xADDR, name, 0xSIZE)      DEFINE_PATCH(0xADDR, bytes...)   DEFINE_JUMP(...)
RE_HOOK = re.compile(r"DEFINE_HOOK\s*\(\s*(0x[0-9A-Fa-f]+)\s*,\s*([A-Za-z0-9_]+)\s*,\s*(0x[0-9A-Fa-f]+)\s*\)")
RE_PATCH = re.compile(r"DEFINE_PATCH\s*\(\s*(0x[0-9A-Fa-f]+)\s*,\s*([A-Za-z0-9_\s]+)\)")
RE_JUMP = re.compile(r"DEFINE_JUMP\s*\(\s*([A-Za-z0-9_]+)\s*,\s*(0x[0-9A-Fa-f]+)")


def collect():
    hooks = []   # (addr, size, name, file:line)
    for dirpath, _dirnames, filenames in os.walk(SRC_ROOT):
        for fn in filenames:
            if not fn.lower().endswith((".cpp", ".h", ".c")):
                continue
            path = os.path.join(dirpath, fn)
            try:
                with open(path, "r", encoding="utf-8", errors="replace") as fp:
                    for ln, line in enumerate(fp, 1):
                        m = RE_HOOK.search(line)
                        if m:
                            hooks.append((int(m.group(1), 16), int(m.group(3), 16), m.group(2),
                                          "%s:%d" % (os.path.basename(path), ln)))
                            continue
                        m = RE_PATCH.search(line)
                        if m:
                            toks = [t for t in m.group(2).split() if t.strip()]
                            hooks.append((int(m.group(1), 16), len(toks), m.group(2).strip()[:28],
                                          "%s:%d" % (os.path.basename(path), ln)))
            except OSError:
                pass
    hooks.sort()
    return hooks


def walk_instructions(ea, limit=64):
    """从 ea 起**线性解码**指令序列 [(start, size)]。

    注意：不要用 idc.next_head() —— 它在函数尾部的 padding/数据洞处会跳到
    远处下一个指令头，把数据长度当成指令长度，从而产生大量 CROSS 误报。
    """
    out = []
    cur = ea
    for _ in range(limit):
        insn = ida_ua.insn_t()
        if ida_ua.decode_insn(insn, cur) <= 0:
            break
        out.append((cur, insn.size))
        cur += insn.size
    return out


def check(addr, size):
    if not ida_bytes.is_loaded(addr):
        return "NOT_LOADED", []
    flags = ida_bytes.get_flags(addr)
    if idc.is_unknown(flags):
        return "NOT_DEFINED", []
    if not idc.is_head(flags):
        return "NOT_HEAD", []
    seq = walk_instructions(addr)
    if not seq:
        return "NO_INSN", []
    total = 0
    covered = []
    for start, sz in seq:
        if total == size:
            break
        if total + sz > size:
            covered.append((start, sz))
            return "CROSS_%d_of_%d" % (total + sz, size), covered
        total += sz
        covered.append((start, sz))
    if total == size:
        return "OK", covered
    return "SHORT_%d_of_%d" % (total, size), covered


def main():
    ida_auto.auto_wait()
    hooks = collect()
    lines = []
    problems = []
    for addr, size, name, src in hooks:
        verdict, covered = check(addr, size)
        if verdict != "OK":
            problems.append((addr, size, name, src, verdict, covered))
    lines.append("Kratos Hook 指令边界审计 —— 共 %d 条 (DEFINE_HOOK + DEFINE_PATCH)" % len(hooks))
    lines.append("=" * 110)
    lines.append("")
    lines.append("### 有问题的条目（%d）" % len(problems))
    lines.append("")
    for addr, size, name, src, verdict, covered in problems:
        lines.append("[%s]  0x%08X  size=0x%X  %s" % (verdict, addr, size, name))
        lines.append("        %s" % src)
        lo = covered[0][0] if covered else addr
        hi = (covered[-1][0] + covered[-1][1]) if covered else addr + size
        cur = lo
        while cur < hi:
            line = ida_lines.generate_disasm_line(cur, 0)
            if not line:
                break
            b = ida_bytes.get_bytes(cur, 8)
            hexs = " ".join("%02X" % c for c in b) if b else "?"
            lines.append("        %s %08X: %-30s | %s" % (">>" if cur == addr else "  ", cur, hexs, line))
            insn = ida_ua.insn_t()
            n = ida_ua.decode_insn(insn, cur)
            if n <= 0:
                break
            cur += insn.size
        lines.append("")
    lines.append("=" * 110)
    lines.append("### 全部 %d 条清单" % len(hooks))
    lines.append("")
    for addr, size, name, src in hooks:
        verdict, _ = check(addr, size)
        lines.append("%-6s  0x%08X  0x%-3X  %-58s %s" % (verdict, addr, size, name[:58], src))

    text = "\n".join(lines)
    with open(OUT_FILE, "w", encoding="utf-8", errors="replace") as fp:
        fp.write(text)
    print("written", OUT_FILE, len(text), "problems:", len(problems))


main()
ida_pro.qexit(0)
