# -*- coding: utf-8 -*-
"""
p34: 找 0x70A280 / 0x70B570 / 0x69F670 的调用者；并 dump 0x70A280 头部字节。
"""
import struct

EXE = r"D:\Games\Yuri's Revenge\gamemd.exe"
OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p34_callers.txt"

data = open(EXE, "rb").read()
out = open(OUT, "w", encoding="utf-8")
def w(s=""):
    out.write(str(s) + "\n"); out.flush()

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

def find_callers(target_va):
    w("=" * 100)
    w("### callers of %#010x ###" % target_va)
    res = []
    i = 0
    while True:
        i = tbuf.find(b"\xE8", i)
        if i < 0:
            break
        if i + 5 <= len(tbuf):
            rel = struct.unpack_from("<i", tbuf, i+1)[0]
            site_va = tbase_va + i
            tgt = site_va + 5 + rel
            if tgt == target_va:
                owner = None
                for back in range(0, 0x4000):
                    jj = i - back
                    if jj <= 0:
                        break
                    if tbuf[jj] == 0x55 and tbuf[jj+1] == 0x8B and tbuf[jj+2] == 0xEC:
                        owner = off2va(jj)
                        break
                res.append((site_va, owner))
        i += 1
    for site, own in res:
        w("  call at %#010x   owner~%#010x" % (site, own or 0))
    w("  total=%d" % len(res))
    w()

for t in (0x70A280, 0x70B570, 0x69F670, 0x4AFF60, 0x69EC50):
    find_callers(t)

def hexdump(va, n=64):
    w("=" * 100)
    o = va2off(va)
    w("[hexdump %#010x] off=%#x" % (va, o or 0))
    if o is None:
        return
    b = data[o:o+n]
    w("  " + " ".join("%02X" % x for x in b))
    w()

hexdump(0x70A280, 64)
hexdump(0x70B570, 48)
hexdump(0x69EC50, 48)

# 列出 0x70A280 与 0x70B570 之间所有 retn 位置，估计函数边界
w("=" * 100)
w("[retn sites between 0x70A280 and 0x70B640]")
o1 = va2off(0x70A280); o2 = va2off(0x70B640)
prev_ret = None
cnt = 0
for o in range(o1, o2):
    if tbuf[o - trptr] == 0xC3:
        va = off2va(o)
        w("  retn @ %#010x" % va)
        cnt += 1
    elif tbuf[o - trptr] == 0xC2:
        imm = struct.unpack_from("<H", tbuf, o - trptr + 1)[0]
        va = off2va(o)
        w("  retn %#x @ %#010x" % (imm, va))
        cnt += 1
    if cnt > 60:
        break
w()

out.close()
print("ok")
