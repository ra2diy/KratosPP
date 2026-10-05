# -*- coding: utf-8 -*-
"""PE-direct: dump the ILocomotion::In_Which_Layer implementations of each locomotor."""
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


def va2off(va):
    r = va - BASE
    for nm, va0, vs, ra, rs in SECS:
        if va0 <= r < va0 + max(vs, rs):
            return ra + (r - va0)
    return None


def dump(va, n, title):
    o = va2off(va)
    b = DATA[o:o + n]
    print("--- 0x%06X %s" % (va, title))
    for i in range(0, len(b), 16):
        print("   0x%06X  %s" % (va + i, b[i:i + 16].hex(" ")))
    print()


dump(0x4CFCF0, 0x50, "FlyLocomotionClass::In_Which_Layer")
dump(0x663460, 0x50, "RocketLocomotionClass::In_Which_Layer")
dump(0x719E20, 0x50, "TeleportLocomotionClass_719E20")
dump(0x72A1A0, 0x50, "TunnelLocomotionClass::In_Which_Layer")
dump(0x4B64D0, 0x50, "DropPodLocomotionClass::In_Which_Layer")
dump(0x54B8D0, 0x30, "JumpjetLocomotionClass::In_Which_Layer (vanilla head)")
dump(0x5F6B90, 0x40, "TechnoClass::IsInAir")
dump(0x5F3E70, 0x60, "ObjectClass::Update (head)")

# IsInAir byte-pattern sanity: [obj+0x74]!=0 && GetHeight()>=208
