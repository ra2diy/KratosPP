# -*- coding: utf-8 -*-
"""PE-direct:FootClass::GetZAdjustment(0x4DB091 附近)的函数体、虚表槽、以及全部调用点。

回答"ZAdjust hook / 缓存是否必要"的第一步（静态）：
  · 若所有调用点都只对**不同对象**各调一次 ⇒ 缓存命中分支永不进入 ⇒ 缓存是死代码（同 StandYSort 那组的判据）；
  · 若存在"同一对象连续调用"的调用点 ⇒ 缓存会命中，需要动态计数确认其值是否与现算一致。
"""
import struct

EXE = r"D:\Games\Yuri's Revenge\gamemd.exe"


def load():
    d = open(EXE, "rb").read()
    e = struct.unpack_from("<I", d, 0x3C)[0]
    n = struct.unpack_from("<H", d, e + 6)[0]
    base = struct.unpack_from("<I", d, e + 0x34)[0]
    opt = e + 24
    so = struct.unpack_from("<H", d, e + 20)[0]
    s0 = opt + so
    secs = []
    for i in range(n):
        o = s0 + 40 * i
        nm = d[o:o + 8].rstrip(b"\0").decode()
        vs, va, rs, ra = struct.unpack_from("<IIII", d, o + 8)
        secs.append((nm, va, vs, ra, rs))
    return d, base, secs


D, BASE, SECS = load()
SEC = {x[0]: x[1:] for x in SECS}


def off(va):
    r = va - BASE
    for nm, (v, vs, ra, rs) in SEC.items():
        if v <= r < v + max(vs, rs):
            return ra + (r - v)
    return None


def rd(va, n):
    o = off(va)
    return None if o is None else D[o:o + n]


def dd(va):
    b = rd(va, 4)
    return struct.unpack("<I", b)[0] if b and len(b) == 4 else None


print("=== 0x4DB060..0x4DB110（hook 落点 0x4DB091，size 0x6）===")
o = off(0x4DB060)
for i in range(0, 0xB0, 16):
    print("  0x%06X  %s" % (0x4DB060 + i, D[o + i:o + i + 16].hex(" ")))

# 函数入口：向前找最近的 0xCC/0x90 填充或已知规律 —— 这里直接看 0x4DB091 前面有没有 retn
print("\n=== 向上回溯找函数入口（最近一条 retn 之后的指令头）===")
for va in range(0x4DB060, 0x4DB091):
    b = rd(va, 1)
    if b and b[0] in (0xC3, 0xC2):
        print("  retn @ 0x%06X ⇒ 函数入口应在其后" % va)

# 虚表槽：在 .rdata 找指向该函数入口的 dword
FN = 0x4DB091  # 先按 hook 落点当作入口试；下面再用 vtable 反查确认
print("\n=== .rdata 中指向 0x4DB091 / 0x4DB080 / 0x4DB070 的 dword（定位虚表槽）===")
rv, rvs, rr, rrs = SEC['.rdata']
lo, hi = rr, rr + min(rvs, rrs)
for target in (0x4DB091, 0x4DB090, 0x4DB080, 0x4DB070, 0x4DB060):
    pat = struct.pack("<I", target)
    p = lo
    hits = []
    while True:
        i = D.find(pat, p, hi)
        if i < 0:
            break
        hits.append(BASE + rv + (i - rr))
        p = i + 1
    if hits:
        print("  0x%06X: %s" % (target, ", ".join("槽 0x%06X" % h for h in hits)))
