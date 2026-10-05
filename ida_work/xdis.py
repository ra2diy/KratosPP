# -*- coding: utf-8 -*-
"""
p35: PE-direct disassembler for gamemd.exe (no IDA needed).
Usage:  python dis.py <start_va_hex> <end_va_hex> [<start> <end> ...]
"""
import sys, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_OP_IMM, CS_OP_MEM, CS_OP_REG

EXE = r"D:\Games\Yuri's Revenge\gamemd.exe"

NAMES = {
    0x69F670: "ShipLocomotionClass::Draw_Matrix",
    0x4AFF60: "DriveLocomotionClass::Draw_Matrix",
    0x55A730: "LocomotionClass::ILocomotion_Draw_Matrix",
    0x5AE890: "Matrix3D::Translate(x,y,z)",
    0x5AE8F0: "Matrix3D::Translate(Vec3)",
    0x5AE980: "Matrix3D::TranslateX",
    0x5AE9B0: "Matrix3D::TranslateY",
    0x5AE9E0: "Matrix3D::TranslateZ",
    0x5AEF60: "Matrix3D::RotateX",
    0x5AF080: "Matrix3D::RotateY",
    0x5AF1A0: "Matrix3D::RotateZ",
    0x5AF980: "Matrix3D::MatrixMultiply(M,M)",
    0x5AFB80: "Matrix3D::MatrixMultiply(M,v)",
    0x5AE610: "Matrix3D::Matrix3D(copy)",
    0x70B570: "TechnoClass::vt+41C (rocking/sinking integrator)",
    0x70A280: "FUN_70A280 (14x writer of +32C)",
    0x6FA1F0: "FUN_6FA1F0 (IsVoxel gate)",
    0x4C93D0: "FacingClass::Current / Desired_Facing256",
    0x6D8DB0: "Tactical_Draw_All",
    0x55D360: "W?Main_Loop",
    0x5F5850: "ObjectClass::Mark",
    0x4DB810: "FootClass::SetCoords",
}


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


def read(va, n):
    o = va2off(va)
    if o is None:
        return None
    return DATA[o:o + n]


def cstr(va, maxlen=64):
    b = read(va, maxlen)
    if b is None:
        return None
    z = b.find(b"\0")
    if z >= 0:
        b = b[:z]
    return b.decode("latin1", "replace")


def flt(va):
    b = read(va, 4)
    if b is None or len(b) < 4:
        return None
    return struct.unpack("<f", b)[0]


def dbl(va):
    b = read(va, 8)
    if b is None or len(b) < 8:
        return None
    return struct.unpack("<d", b)[0]


md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = True


def label(va):
    if va in NAMES:
        return "  ; " + NAMES[va]
    return ""


def disasm(start, end, title=""):
    print("=" * 78)
    print("### %s  0x%06X - 0x%06X%s" % (title, start, end,
          ("  [" + NAMES[start] + "]") if start in NAMES else ""))
    print("=" * 78)
    va = start
    while va < end:
        chunk = read(va, min(16, end - va))
        if not chunk:
            print("  --- unreadable ---")
            break
        insns = list(md.disasm(chunk, va))
        if not insns:
            print("  0x%06X: db 0x%02X" % (va, chunk[0]))
            va += 1
            continue
        ins = insns[0]
        note = ""
        # annotate operands
        try:
            for op in ins.operands:
                if op.type == CS_OP_IMM:
                    t = op.imm & 0xFFFFFFFF
                    if t in NAMES:
                        note += "  ; -> %s" % NAMES[t]
                    else:
                        s = cstr(t)
                        if s and len(s) > 1 and all(32 <= ord(c) < 127 for c in s):
                            note += '  ; "%s"' % s
                elif op.type == CS_OP_MEM:
                    base = op.mem.base
                    if base != 0:
                        reg = ins.reg_name(base)
                        d = op.mem.disp
                        # candidate data address
                        if reg in ("ds", "es") or reg == "":
                            pass
                        if d > 0x400000 and base == 0:
                            note += "  ; [0x%X]" % d
                    if op.mem.disp > 0x400000:
                        note += "  ; data 0x%X" % op.mem.disp
        except Exception:
            pass
        print("  0x%06X  %-26s %s %s%s" % (va, ins.bytes.hex(), ins.mnemonic, ins.op_str, note))
        va += ins.size
    print()


if __name__ == "__main__":
    args = sys.argv[1:]
    pairs = [(int(args[i], 16), int(args[i + 1], 16)) for i in range(0, len(args), 2)]
    for s, e in pairs:
        disasm(s, e, "range")
