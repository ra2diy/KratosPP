# -*- coding: utf-8 -*-
"""
p33: 在整个 .text 里搜索所有「写入 +0x32C / +0x328 (AngleRotated*)」的指令，
     回答「沉船倾斜有几条写入路径」。
思路：直接搜 disp32 小端的字节 2C 03 00 00 (0x32C) / 28 03 00 00 (0x328)，
      再回看前 1..6 字节判断是否是 store 指令。
"""
import struct

EXE = r"D:\Games\Yuri's Revenge\gamemd.exe"
OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p33_writers.txt"

data = open(EXE, "rb").read()
out = open(OUT, "w", encoding="utf-8")
def w(s=""):
    out.write(str(s) + "\n")
    out.flush()

e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
coff = e_lfanew + 4
nsec = struct.unpack_from("<H", data, coff+2)[0]
opt_size = struct.unpack_from("<H", data, coff+16)[0]
opt = coff + 20
image_base = struct.unpack_from("<I", data, opt+28)[0]

secs = []
sh = opt + opt_size
for i in range(nsec):
    off = sh + i*40
    name = data[off:off+8].rstrip(b"\0").decode("latin1")
    vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", data, off+8)
    secs.append((name, vaddr, vsize, rptr, rsize))

def off2va(o):
    for name, vaddr, vsize, rptr, rsize in secs:
        if rptr <= o < rptr + rsize:
            return image_base + vaddr + (o - rptr)
    return None

def va2off(va):
    rva = va - image_base
    for name, vaddr, vsize, rptr, rsize in secs:
        if vaddr <= rva < vaddr + max(vsize, rsize):
            return rptr + (rva - vaddr)
    return None

TEXT = next(s for s in secs if s[0] == ".text")
tname, tvaddr, tvsize, trptr, trsize = TEXT
tbuf = data[trptr:trptr+trsize]
tbase_va = image_base + tvaddr

# modrm reg 位 → 是否有 "store" 语义
FP_STORE = {  # D9 /3 = fstp m32fp
    0x9E: "fstp [esi+d]", 0x99: "fstp [ecx+d]", 0x9A: "fstp [edx+d]",
    0x9B: "fstp [ebx+d]", 0x9F: "fstp [edi+d]",
}
MOV_STORE = {  # 89 /r  reg=src
    0x9E: "mov [esi+d], ebx", 0x99: "mov [ecx+d], ebx", 0x9A: "mov [edx+d], ebx",
    0xBE: "mov [esi+d], edi", 0xB9: "mov [ecx+d], edi", 0xBA: "mov [edx+d], edi",
    0x8E: "mov [esi+d], ecx", 0x89: "mov [ecx+d], ecx", 0x8A: "mov [edx+d], ecx",
    0x96: "mov [esi+d], edx", 0x91: "mov [ecx+d], edx", 0x92: "mov [edx+d], edx",
    0x86: "mov [esi+d], eax", 0x81: "mov [ecx+d], eax", 0x82: "mov [edx+d], eax",
}
FADD_M = {  # D8 /0-7
    0x86: "fadd [esi+d]", 0x81: "fadd [ecx+d]", 0x82: "fadd [edx+d]",
    0x8E: "fsub [esi+d]", 0x89: "fsub [ecx+d]", 0x8A: "fsub [edx+d]",
}

def report(disp, label):
    w("=" * 110)
    w("### writers of %s (disp=%#x) ###" % (label, disp))
    pat = struct.pack("<i", disp)
    hits = []
    start = 0
    while True:
        i = tbuf.find(pat, start)
        if i < 0:
            break
        start = i + 1
        # 回看前 1..8 字节
        desc = None
        for back in range(1, 9):
            j = i - back
            if j < 0:
                break
            b = tbuf[j]
            # FSTP  D9 9x
            if b == 0xD9 and j+1 == i-0:
                pass
            if back == 2:
                b0, b1 = tbuf[j], tbuf[j+1]
                if b0 == 0xD9 and b1 in FP_STORE:
                    desc = "%-22s (D9 %02X)" % (FP_STORE[b1], b1)
                elif b0 == 0x89 and b1 in MOV_STORE:
                    desc = "%-22s (89 %02X)" % (MOV_STORE[b1], b1)
                elif b0 == 0xD8 and b1 in FADD_M:
                    desc = "%-22s (D8 %02X)" % (FADD_M[b1], b1)
                elif b0 == 0xC7 and b1 in (0x86, 0x81, 0x82, 0x9E):
                    desc = "mov dword [r+d], imm32 (C7 %02X)" % b1
                elif b0 == 0x0F and b1 in (0x11, 0x29, 0x7F):
                    desc = "movaps/movss store (0F %02X)" % b1
            if back == 3:
                b0, b1, b2 = tbuf[j], tbuf[j+1], tbuf[j+2]
                if b0 == 0xF3 and b1 == 0x0F and b2 == 0x11:
                    desc = "movss store (F3 0F 11)"
                if b0 == 0xF3 and b1 == 0x0F and b2 == 0x10:
                    desc = "movss load  (F3 0F 10)"
            if back == 4:
                b0, b1, b2, b3 = tbuf[j], tbuf[j+1], tbuf[j+2], tbuf[j+3]
                if b0 == 0xF3 and b1 == 0x0F and b2 == 0x10 and b3 in (0x86,):
                    desc = "movss load"
            if desc:
                break
        if desc is None:
            continue
        va = off2va(i - 2) if i >= 2 else None
        if va is None:
            continue
        # 找所属函数（粗略：往前找最近的 0xCCCC 对齐或 0x55 push ebp）
        owner_va = None
        k = i - 2
        for back in range(0, 0x1200):
            jj = k - back
            if jj <= 0:
                break
            if tbuf[jj] == 0x55 and tbuf[jj+1] == 0x8B and tbuf[jj+2] == 0xEC:
                owner_va = off2va(jj)
                break
        hits.append((va, desc, owner_va))
    for va, desc, own in hits:
        w("  %#010x  %-36s  owner~%#010x" % (va, desc, own or 0))
    w("  total=%d" % len(hits))
    w()

report(0x32C, "AngleRotatedForwards")
report(0x328, "AngleRotatedSideways")
report(0x330, "RockingSidewaysPerFrame")
report(0x334, "RockingForwardsPerFrame")

out.close()
print("ok")
