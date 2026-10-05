# -*- coding: utf-8 -*-
"""Identify helper functions around the Flat drawing path."""
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

# AnimClass vtable base: slot 0x78 holds 0x424CB0, find the vtable by scanning rdata
# (we know from prior work: vtable RVA 0x3E33CC holds 0x424CB0)
vt_slot_addr = 0x3E33CC
base_off = va2off(IMGBASE + vt_slot_addr)
slot74 = struct.unpack_from("<I", DATA, base_off - 4)[0]   # slot 0x74
slot7c = struct.unpack_from("<I", DATA, base_off + 4)[0]   # slot 0x7c
print(f"AnimClass vt slot 0x74 = {slot74:08X}")
print(f"AnimClass vt slot 0x7c = {slot7c:08X}")

md = Cs(CS_ARCH_X86, CS_MODE_32)

def head(va, n=14, label=""):
    off = va2off(va)
    print(f"\n--- {label} {va:08X} ---")
    for ins in md.disasm(DATA[off:off + 80], va):
        print(f"  {ins.address:08X}  {ins.mnemonic} {ins.op_str}")
        n -= 1
        if n <= 0: break

head(0x6D20E0, 12, "call-after-vt74")
head(0x4A1CA0, 8, "flag-setter?")
head(0x4A1D50, 8, "layer-setter?")
head(slot74, 12, "AnimClass vt slot 0x74")
