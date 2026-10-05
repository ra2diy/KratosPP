# -*- coding: utf-8 -*-
"""Find EVERY ILocomotion sub-vtable (slot +0x6C == 0x55AB80 Shove, or 0x516FC0 for Hover)
and dump its +0x74 = In_Which_Layer, decoding the trivial 'mov eax,imm; retn 4' shapes."""
import struct

EXE = r"D:\Games\Yuri's Revenge\gamemd.exe"


def load():
    data = open(EXE, "rb").read()
    e = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, e + 6)[0]
    base = struct.unpack_from("<I", data, e + 0x34)[0]
    opt = e + 24
    so = struct.unpack_from("<H", data, e + 20)[0]
    s0 = opt + so
    secs = []
    for i in range(nsec):
        o = s0 + 40 * i
        nm = data[o:o + 8].rstrip(b"\0").decode("latin1")
        vs, va, rs, ra = struct.unpack_from("<IIII", data, o + 8)
        secs.append((nm, va, vs, ra, rs))
    return data, base, secs


DATA, BASE, SECS = load()
SEC = {n: (v, vs, r, rs) for n, v, vs, r, rs in SECS}


def va2off(va):
    r = va - BASE
    for n, (v, vs, ra, rs) in SEC.items():
        if v <= r < v + max(vs, rs):
            return ra + (r - v)
    return None


def rd(va, n):
    o = va2off(va)
    return None if o is None else DATA[o:o + n]


def dd(va):
    b = rd(va, 4)
    return None if not b or len(b) < 4 else struct.unpack("<I", b)[0]


# scan .rdata for the Shove / Hover slots
names = {0x55AB80: "Shove", 0x516FC0: "Hover-Shove"}
found = []
for probe, tag in names.items():
    pat = struct.pack("<I", probe)
    rv, rvs, rr, rrs = SEC[".rdata"]
    lo, hi = rr, rr + min(rvs, rrs)
    p = lo
    while True:
        i = DATA.find(pat, p, hi)
        if i < 0:
            break
        p = i + 1
        va = BASE + rv + (i - rr)
        vtable = va - 0x6C
        # vtable must live in .rdata
        if va2off(vtable) is None:
            continue
        found.append((vtable, tag))

seen = set()
rows = []
for vtable, tag in found:
    if vtable in seen:
        continue
    seen.add(vtable)
    iwl = dd(vtable + 0x74)
    if iwl is None or va2off(iwl) is None:
        rows.append((vtable, tag, iwl, "?"))
        continue
    b = rd(iwl, 8)
    shape = "raw"
    if len(b) >= 7 and b[0] == 0xB8 and b[5] == 0xC2 and b[6] == 0x04:
        shape = "const %d" % struct.unpack_from("<I", b, 1)[0]
    elif len(b) >= 6 and b[0] == 0xB8 and b[5] == 0xC3:
        shape = "const %d" % struct.unpack_from("<I", b, 1)[0]
    rows.append((vtable, tag, iwl, shape))

rows.sort()
for vtable, tag, iwl, shape in rows:
    print("vtable 0x%06X (+0x6C=%s)  In_Which_Layer=0x%06X   %s" % (vtable, tag, iwl or 0, shape))
print("\ntotal vtables: %d" % len(rows))
