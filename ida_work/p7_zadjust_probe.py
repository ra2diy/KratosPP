# -*- coding: utf-8 -*-
import os
import ida_bytes
import idc
import ida_pro
import idautils

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "p7_zadjust_probe.txt")
f = open(OUT, "w", encoding="utf-8")


def w(s):
    f.write(s + "\n")
    f.flush()


def disasm(ea, n=40, indent="  "):
    for _ in range(n):
        dis = idc.GetDisasm(ea)
        if not dis:
            w(indent + "%08X <fail>" % ea)
            break
        w(indent + "%08X  %s" % (ea, dis))
        m = idc.print_insn_mnem(ea)
        if m.startswith("ret"):
            break
        sz = idc.get_item_size(ea)
        if sz <= 0:
            break
        ea += sz


def raw(ea, n, label):
    b = ida_bytes.get_bytes(ea, n)
    w("%s @ %08X : %s" % (label, ea, " ".join("%02X" % c for c in b)))


def nm(ea):
    return idc.get_func_name(ea) or (idc.get_name(ea, idc.NAME_NO_STRING) or "(noname)")


TGT = 0x4DB091
fs = idc.get_func_attr(TGT, idc.FUNCATTR_START)
fe = idc.get_func_attr(TGT, idc.FUNCATTR_END)
w("############ 0x4DB091 所在函数 %s [%08X..%08X] ############" % (idc.get_func_name(fs), fs, fe))
raw(fs, 8, "func head")
w("")
disasm(fs, 45)
w("")

w("############ 逐条指令边界（从头线性推进，标出 0x4DB091）############")
ea = fs
while ea < fe and ea < fs + 400:
    sz = idc.get_item_size(ea)
    if sz <= 0:
        break
    mark = ""
    if ea == TGT:
        mark = "  <<<<<< HOOK 落点（指令起点）"
    elif ea < TGT < ea + sz:
        mark = "  <<<<<< !!! HOOK 落在指令中间 !!!"
    w("  %08X  size=%d  %-30s%s" % (ea, sz, idc.GetDisasm(ea), mark))
    if ea >= TGT:
        # 落点之后再走 6 条就停
        if ea > TGT:
            break
    ea += sz
w("")
raw(TGT, 8, "落点 8 字节")
w("")

w("############ GetZAdjustment 的引用者（谁消费这个值）############")
for a in (fs,):
    for x in idautils.XrefsTo(a, 0):
        w("   func %08X <- from %08X type=%d in %s" % (a, x.frm, x.type, idc.get_func_name(x.frm)))
# 虚表中的槽位
w("-- 该函数在虚表里的槽位 --")
segs = [(idc.get_segm_name(se), se, idc.get_segm_end(se)) for se in idautils.Segments()]
best = None
for n_, st, en in segs:
    if st <= 0x7E22A4 < en:
        best = (n_, st, en)
        break
start, end = best[1], best[2]
data = ida_bytes.get_bytes(start, end - start)
le = fs.to_bytes(4, "little")
pos = 0
while True:
    i = data.find(le, pos)
    if i < 0:
        break
    pos = i + 1
    if i % 4:
        continue
    w("   vt+0x%03X slot %08X （vtable base ≈ %08X）" % (0x0, start + i, 0))
w("")

w("############ GetZAdjustment 调用点（call [reg+?] 里的 AdjustForZ）############")
for ea, name in idautils.Names():
    if "AdjustForZ" in name or "GetZAdjustment" in name or "ZAdjust" in name:
        w("   %08X  %s" % (ea, name))

f.close()
ida_pro.qexit(0)
