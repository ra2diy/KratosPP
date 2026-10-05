# -*- coding: utf-8 -*-
"""
PE-direct probe (no IDA, no capstone):
  1) bytes at AnimClass::InWhichLayer (0x424CB0)
  2) AnimTypeClass ctor range -> find `mov dword ptr [esi+364h], imm` (Layer default)
  3) whole .text scan for writers referencing disp 0x364 (Layer field)
  4) whole .text scan for writers referencing disp 0x369 (Flat) for cross-check
"""
import struct

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
TEXT = [s for s in SECS if s[0] == ".text"][0]
print("ImageBase = 0x%08X" % IMGBASE)
print("sections  = %s" % [(n, hex(v), hex(vs), hex(r), hex(rs)) for n, v, vs, r, rs in SECS])


def va2off(va):
    rva = va - IMGBASE
    for name, vaddr, vsize, raddr, rsize in SECS:
        if vaddr <= rva < vaddr + max(vsize, rsize):
            return raddr + (rva - vaddr)
    return None


def read(va, n):
    o = va2off(va)
    return None if o is None else DATA[o:o + n]


def hexdump(va, n):
    b = read(va, n)
    print("0x%06X: %s" % (va, b.hex(" ")))


print("\n" + "=" * 78)
print("### 1) AnimClass::InWhichLayer  0x424CB0 .. 0x424CE0")
print("=" * 78)
hexdump(0x424CB0, 0x30)

print("\n" + "=" * 78)
print("### 2) AnimTypeClass ctor 0x427530 .. 0x427730 :  [reg+disp] writes")
print("=" * 78)
# generic: C7 8x/86 disp32 imm32  (mov dword ptr [reg+disp32], imm32)
for va in range(0x427530, 0x427730):
    o = va2off(va)
    if o is None:
        continue
    b = DATA[o:o + 10]
    if len(b) < 6:
        continue
    if b[0] == 0xC7 and b[1] in (0x86, 0x87):
        disp = struct.unpack_from("<I", b, 2)[0]
        if disp < 0x1000:
            imm = struct.unpack_from("<I", b, 6)[0]
            print("  0x%06X  mov dword ptr [%s+0x%03X], 0x%X   (%s)"
                  % (va, "esi" if b[1] == 0x86 else "edi", disp, imm,
                     struct.unpack_from("<i", b, 6)[0]))

print("\n" + "=" * 78)
print("### 3) .text scan: instructions whose disp32 == 0x364 (AnimTypeClass::Layer)")
print("=" * 78)
tname, tvaddr, tvsize, traddr, trsize = TEXT
lo, hi = traddr, traddr + min(tvsize, trsize)
pat = struct.pack("<I", 0x364)
found = []
p = lo
while True:
    i = DATA.find(pat, p, hi)
    if i < 0:
        break
    found.append(i)
    p = i + 1
print("  hits: %d" % len(found))
for i in found:
    va = IMGBASE + tvaddr + (i - traddr)
    ctx = DATA[max(0, i - 6):i + 8]
    print("  0x%06X  ...%s..." % (va, ctx.hex(" ")))

print("\n" + "=" * 78)
print("### 4) .text scan: disp32 == 0x369 (AnimTypeClass::Flat)  [cross-check]")
print("=" * 78)
pat = struct.pack("<I", 0x369)
found = []
p = lo
while True:
    i = DATA.find(pat, p, hi)
    if i < 0:
        break
    found.append(i)
    p = i + 1
print("  hits: %d" % len(found))
for i in found[:20]:
    va = IMGBASE + tvaddr + (i - traddr)
    ctx = DATA[max(0, i - 6):i + 8]
    print("  0x%06X  ...%s..." % (va, ctx.hex(" ")))
