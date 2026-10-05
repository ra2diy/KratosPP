# -*- coding: utf-8 -*-
"""Scan gamemd .text for instructions referencing disp32 == AnimTypeClass::Flat (0x369)
and neighbors (0x368 DoubleThick, 0x36A Translucent) for context."""
import struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

EXE = r"D:\Games\Yuri's Revenge\gamemd.exe"

def load():
    data = open(EXE, "rb").read()
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, e_lfanew + 6)[0]
    imgbase = struct.unpack_from("<I", data, e_lfanew + 0x34)[0]
    opt = e_lfanew + 24
    sizeof_opt = struct.unpack_from("<H", data, e_lfanew + 20)[0]
    sec0 = opt + sizeof_opt
    secs = []
    for i in range(nsec):
        off = sec0 + 40 * i
        name = data[off:off + 8].rstrip(b"\0").decode("latin1")
        vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", data, off + 8)
        secs.append((name, vaddr, vsize, raddr, rsize))
    return data, imgbase, secs

DATA, IMGBASE, SECS = load()

def va2off(va):
    rva = va - IMGBASE
    for name, vaddr, vsize, raddr, rsize in SECS:
        if vaddr <= rva < vaddr + max(vsize, rsize):
            return raddr + (rva - vaddr)
    return None

def off2va(off):
    for name, vaddr, vsize, raddr, rsize in SECS:
        if raddr <= off < raddr + rsize:
            return IMGBASE + vaddr + (off - raddr)
    return None

text = [s for s in SECS if s[0] == ".text"][0]
tname, tvaddr, tvsize, traddr, trsize = text
md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = True

TARGETS = {0x368: "DoubleThick", 0x369: "Flat", 0x36A: "Translucent"}

hits = []
code = DATA[traddr:traddr + trsize]
for ins in md.disasm(code, IMGBASE + tvaddr):
    for op in ins.operands:
        if op.type == 3:  # MEM
            m = op.mem
            if m.base != 0 and m.index == 0 and m.disp in TARGETS:
                hits.append((ins.address, ins.mnemonic, ins.op_str, TARGETS[m.disp]))

for addr, mn, ops, tag in hits:
    print(f"{addr:08X}  {mn} {ops}   ; {tag}")
print(f"\ntotal: {len(hits)}")
