# -*- coding: utf-8 -*-
"""
p36: 用真反汇编器（线性扫描 .text）统计对 TechnoClass 倾斜字段的真实引用，
     取代之前基于原始字节匹配（2C 03 00 00 之类）的假阳性扫描。
"""
import sys, struct
sys.path.insert(0, r"D:\Workspace\ra2mod\platform\KratosPP\ida_work")
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_OP_MEM
from xdis import DATA, IMGBASE, SECS, NAMES

TARGETS = {0x328: "AngleRotatedSideways", 0x32C: "AngleRotatedForwards",
           0x330: "RockingSidewaysPerFrame", 0x334: "RockingForwardsPerFrame",
           0x320: "FiringWave"}

md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = True


def text_range():
    for name, vaddr, vsize, raddr, rsize in SECS:
        if name == ".text":
            return IMGBASE + vaddr, rsize
    return None, None


def main():
    start, size = text_range()
    print("### .text  VA 0x%X  size 0x%X" % (start, size))
    buf = DATA[0:]  # keep full image; we decode via va->off
    from xdis import va2off
    off0 = va2off(start)
    code = DATA[off0:off0 + size]

    hits = {}
    counts = {}
    va = start
    i = 0
    n = len(code)
    while i < n:
        chunk = code[i:i + 15]
        insns = list(md.disasm(chunk, va))
        if not insns:
            i += 1
            va += 1
            continue
        ins = insns[0]
        try:
            for op in ins.operands:
                if op.type == CS_OP_MEM and op.mem.disp in TARGETS:
                    base = ins.reg_name(op.mem.base) if op.mem.base else "?"
                    if base in ("esp", "ebp", "eip", ""):
                        continue
                    key = op.mem.disp
                    counts[key] = counts.get(key, 0) + 1
                    hits.setdefault(key, []).append((va, base, ins.mnemonic + " " + ins.op_str))
        except Exception:
            pass
        i += ins.size
        va += ins.size

    for k in sorted(counts, reverse=True):
        print("\n=== 0x%03X  %s   (真引用 %d 处) ===" % (k, TARGETS[k], counts[k]))
        for va, base, txt in hits[k]:
            print("   0x%06X  [%s+0x%X]  %s" % (va, base, k, txt))


if __name__ == "__main__":
    main()
