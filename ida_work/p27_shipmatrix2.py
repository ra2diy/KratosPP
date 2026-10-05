# -*- coding: utf-8 -*-
# p27: 判定 Ship/Drive Draw_Matrix 的"带倾斜路径"是否把倾斜写进输出矩阵
#  反汇编完整函数，并把 关键的矩阵操作调用（RotateX/RotateY/RotateZ/Translate*）标注出来
import idc, idautils, idaapi

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p27_shipmatrix2.txt"
f = open(OUT, "w", encoding="utf-8")

def w(s):
    try:
        f.write(s + "\n"); f.flush()
    except Exception:
        pass

# 关心的矩阵操作（来自之前取证）
KNOWN = {
    0x5AE980: "Matrix3D::TranslateX",
    0x5AE9B0: "Matrix3D::TranslateY",
    0x5AE9E0: "Matrix3D::TranslateZ",
    0x5AEF60: "Matrix3D::RotateX",
    0x5AF080: "Matrix3D::RotateY",
    0x5AF1A0: "Matrix3D::RotateZ",
    0x5AF980: "Matrix3D::Multiply",
    0x5AFB80: "Matrix3D::Multiply2",
    0x7559B0: "Matrix_from_X(?)",
    0x755A40: "Matrix_lerp(?)",
    0x55A730: "ILocomotion_DrawMatrix(base)",
    0x55ABD0: "ILocomotion_DrawPoint",
}

def dump(start, tag, limit=400):
    w("=" * 78)
    w("== %s @ 0x%X" % (tag, start))
    w("=" * 78)
    ea = start
    n = 0
    while n < limit:
        try:
            ins = idc.generate_disasm_line(ea, 0)
        except Exception as e:
            w("  ERR %s" % e); break
        note = ""
        # 找 call target
        try:
            mnem = idc.print_insn_mnem(ea)
            if mnem and mnem.lower() == "call":
                tgt = idc.get_operand_value(ea, 0)
                if tgt in KNOWN:
                    note = "   ; <<<< " + KNOWN[tgt]
        except Exception:
            pass
        w("  %08X  %-46s%s" % (ea, ins, note))
        # 遇到 retn 结束
        try:
            if idc.print_insn_mnem(ea).lower().startswith("ret"):
                break
        except Exception:
            pass
        ea = idc.next_head(ea)
        if ea == idaapi.BADADDR:
            break
        n += 1
    w("")

# Ship / Drive 带倾斜路径入口 + 主函数体
for a, t in [
    (0x69F670, "ShipLocomotionClass::Draw_Matrix  FULL"),
    (0x69F9C3, "Ship  TILT PATH  0x69F9C3"),
    (0x4B02B3, "Drive TILT PATH  0x4B02B3"),
]:
    try:
        dump(a, t)
    except Exception as e:
        w("ERR %s: %s" % (t, e))

# 统计两个函数体内对矩阵操作的调用
w("#" * 78)
w("# 调用点统计（Ship 0x69F670 起 与 Drive 0x4AFF60 起，各扫 0x900 字节）")
w("#" * 78)
for base, name in [(0x69F670, "Ship"), (0x4AFF60, "Drive")]:
    w("--- %s ---" % name)
    ea = base
    end = base + 0x900
    while ea < end:
        try:
            if idc.print_insn_mnem(ea).lower() == "call":
                tgt = idc.get_operand_value(ea, 0)
                if tgt in KNOWN:
                    w("   %08X  call %s (0x%X)" % (ea, KNOWN[tgt], tgt))
        except Exception:
            pass
        ea = idc.next_head(ea)
        if ea == idaapi.BADADDR:
            break

f.close()
print("DONE p27")
