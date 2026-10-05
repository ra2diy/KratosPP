# -*- coding: utf-8 -*-
"""
p32: 不依赖 IDA —— 直接从原版 gamemd.exe 抠出指定 VA 区间内
     浮点/整型指令的「真实 displacement」，用于核实
       0x70B570 (TechnoClass tilt integrator)
       0x69F670 (ShipLocomotionClass::Draw_Matrix)
     到底访问了哪些字段偏移。

模式表（x86-32, ModRM mod=10 => disp32）:
  D9 86 disp32  fld   dword [esi+disp32]
  D9 9E disp32  fstp  dword [esi+disp32]
  D8 86 disp32  fadd  dword [esi+disp32]
  D8 A6 disp32  fsub  dword [esi+disp32]
  D8 9E disp32  fcomp dword [esi+disp32]
  38 9E disp32  cmp   byte  [esi+disp32], bl
  8B 8E disp32  mov   ecx,  [esi+disp32]
  8B 96 disp32  mov   edx,  [esi+disp32]
  8B 86 disp32  mov   eax,  [esi+disp32]
  89 9E disp32  mov   [esi+disp32], ebx
  F3 0F 10 ...  (SSE, skip)
  并支持 [ecx+disp32]: ModRM 0x81/0x89/0x91/0x99/0xA1/0xA9/0xB1/0xB9
"""
import struct, sys

EXE = r"D:\Games\Yuri's Revenge\gamemd.exe"
OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p32_pe_disp.txt"

data = open(EXE, "rb").read()
out = open(OUT, "w", encoding="utf-8")
def w(s=""):
    out.write(str(s) + "\n")
    out.flush()

# ---- PE parse ----
e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
assert data[e_lfanew:e_lfanew+4] == b"PE\0\0", "not PE"
coff = e_lfanew + 4
nsec = struct.unpack_from("<H", data, coff+2)[0]
opt_size = struct.unpack_from("<H", data, coff+16)[0]
opt = coff + 20
magic = struct.unpack_from("<H", data, opt)[0]
image_base = struct.unpack_from("<I", data, opt+28)[0]
w("PE magic=%#x image_base=%#x nsec=%d" % (magic, image_base, nsec))

secs = []
sh = opt + opt_size
for i in range(nsec):
    off = sh + i*40
    name = data[off:off+8].rstrip(b"\0").decode("latin1")
    vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", data, off+8)
    secs.append((name, vaddr, vsize, rptr, rsize))
    w("  sec %-8s VA=%#010x VS=%#x RAW=%#010x RS=%#x" % (name, vaddr, vsize, rptr, rsize))

def va2off(va):
    rva = va - image_base
    for name, vaddr, vsize, rptr, rsize in secs:
        if vaddr <= rva < vaddr + max(vsize, rsize):
            return rptr + (rva - vaddr)
    return None

# ---- scan ----
# (opcode bytes, length of opcode, mnemonic)
PATS = [
    (b"\xD9\x86", 2, "fld  dword [esi+d]"),
    (b"\xD9\x9E", 2, "fstp dword [esi+d]"),
    (b"\xD8\x86", 2, "fadd dword [esi+d]"),
    (b"\xD8\xA6", 2, "fsub dword [esi+d]"),
    (b"\xD8\x9E", 2, "fcomp dword [esi+d]"),
    (b"\x38\x9E", 2, "cmp  byte [esi+d], bl"),
    (b"\x8B\x8E", 2, "mov  ecx, [esi+d]"),
    (b"\x8B\x96", 2, "mov  edx, [esi+d]"),
    (b"\x8B\x86", 2, "mov  eax, [esi+d]"),
    (b"\x89\x9E", 2, "mov  [esi+d], ebx"),
    (b"\x80\xBE", 2, "cmp  byte [esi+d], imm8"),
    # [ecx+disp32]
    (b"\xD9\x81", 2, "fld  dword [ecx+d]"),
    (b"\xD9\x99", 2, "fstp dword [ecx+d]"),
    (b"\xD8\x81", 2, "fadd dword [ecx+d]"),
    (b"\xD8\x99", 2, "fcomp dword [ecx+d]"),
    (b"\x8B\x81", 2, "mov  eax, [ecx+d]"),
    (b"\x8B\x91", 2, "mov  edx, [ecx+d]"),
]

def scan(va_from, va_to, title):
    w("=" * 100)
    w("[%s] %#010x .. %#010x" % (title, va_from, va_to))
    o_from = va2off(va_from)
    o_to = va2off(va_to)
    if o_from is None or o_to is None:
        w("  !! va2off failed")
        return
    buf = data[o_from:o_to]
    hits = []
    # 顺序扫描，记录所有匹配（可能穿过指令边界，用 disp 合理性过滤）
    for pat, plen, mn in PATS:
        start = 0
        while True:
            i = buf.find(pat, start)
            if i < 0:
                break
            start = i + 1
            if i + plen + 4 > len(buf):
                continue
            disp = struct.unpack_from("<i", buf, i+plen)[0]
            if not (0x100 <= disp <= 0x800):
                continue
            hits.append((i, va_from + i, mn, disp))
    hits.sort()
    for i, va, mn, disp in hits:
        w("  %#010x  %-26s disp=%#06x (%d)" % (va, mn, disp, disp))
    w("  total=%d" % len(hits))
    w()

scan(0x70B570, 0x70B6D0, "TechnoClass tilt integrator 0x70B570")
scan(0x70B700, 0x70BCB0, "TechnoClass tilt integrator tail 0x70B700")
scan(0x69F6C0, 0x69F8B0, "Ship_Matrix branch + tilt reads")
scan(0x69F8B0, 0x69F9C5, "Ship_Matrix lerp part")

out.close()
print("ok ->", OUT)
