# -*- coding: utf-8 -*-
"""Find writers of AnimClass+0x104 in anim code range; disasm ObjectClass::GetYSort."""
import struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_OP_MEM

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

md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = True

def disasm_range(lo, hi):
    off = va2off(lo)
    return md.disasm(DATA[off:off + (hi - lo)], lo)

# 1) writers of +0x104 inside AnimClass code region (0x41E000..0x429000)
print("=== writers of [reg+0x104] in 0x41E000-0x429000 ===")
for ins in disasm_range(0x41E000, 0x429000):
    if ins.mnemonic == "mov" and len(ins.operands) == 2:
        dst, src = ins.operands
        if dst.type == CS_OP_MEM and dst.mem.disp == 0x104 and dst.mem.index == 0:
            print(f"  {ins.address:08X}  {ins.mnemonic} {ins.op_str}")

# 2) ObjectClass::GetYSort (0x5F6BD0) body
print("\n=== ObjectClass::GetYSort 0x5F6BD0 ===")
for ins in disasm_range(0x5F6BD0, 0x5F6C60):
    print(f"  {ins.address:08X}  {ins.mnemonic} {ins.op_str}")
