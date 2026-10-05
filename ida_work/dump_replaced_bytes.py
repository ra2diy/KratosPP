import idaapi, idc

OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\replaced_bytes.tsv"
TSV = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\hooks.tsv"

rows = []
for ln in open(TSV, "r", encoding="utf-8"):
    p = ln.rstrip("\n").split("\t")
    if len(p) < 4:
        continue
    try:
        rows.append((int(p[0], 16), p[1], int(p[2], 16), p[3]))
    except ValueError:
        pass

f = open(OUT, "w", encoding="utf-8")
f.write("ADDR\tSIZE\tNAME\tLOC\tREPLACED_BYTES\tREPLACED_DISASM\tWRITES_REG\tNEXT_INSTR\n")

REGNAMES = {0xA: "EDX", 0xB: "EBX", 0x8: "EAX", 0xC: "ESP", 0xD: "EBP", 0xE: "ESI", 0xF: "EDI"}


def writes_reg(disasm):
    """粗略判定被替换指令写了哪个 32 位寄存器（mov r32, ... / lea r32,... / pop r32 等）"""
    d = disasm.strip()
    toks = d.split()
    if not toks:
        return ""
    op = toks[0]
    if op in ("mov", "lea", "pop", "xor", "or", "and", "add", "sub", "imul", "sar", "shr", "shl", "not", "neg", "inc", "dec", "movzx", "movsx"):
        if len(toks) > 1:
            dst = toks[1].rstrip(",").upper()
            return dst
    return ""


for addr, name, size, loc in rows:
    ea = addr
    end = addr + size
    bs = []
    dis_parts = []
    wr = ""
    while ea < end:
        b = idc.get_bytes(ea, 1)
        bs.append("%02X" % (b[0] if b else 0))
        d = idc.generate_disasm_line(ea, 0)
        dis_parts.append("0x%X: %s" % (ea, d))
        if not wr:
            wr = writes_reg(d)
        nxt = idc.next_head(ea, end + 16)
        if nxt <= ea:
            break
        ea = nxt
    # 被替换区域结束后的第一条指令
    nxt_ea = addr + size
    nxt_d = idc.generate_disasm_line(idc.get_item_head(nxt_ea) if idc.get_item_head(nxt_ea) else nxt_ea, 0)
    f.write("%08X\t%d\t%s\t%s\t%s\t%s\t%s\t%s\n" % (
        addr, size, name, loc, " ".join(bs), " || ".join(dis_parts), wr, nxt_d))

f.close()
print("DONE")
