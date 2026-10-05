import idaapi, idc, idautils, ida_hexrays

TSV = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\hooks.tsv"

rows = []
with open(TSV, "r", encoding="utf-8") as f:
    for ln in f:
        ln = ln.rstrip("\n")
        if not ln.strip():
            continue
        p = ln.split("\t")
        if len(p) < 4:
            continue
        try:
            addr = int(p[0], 16)
        except ValueError:
            continue
        rows.append((addr, p[1], p[2], p[3]))

print("=" * 130)
print("### 全部 %d 个 Hook 按所在引擎函数归类" % len(rows))
print("=" * 130)
print("%-10s %-46s %-46s %-10s  %s" % ("HOOKADDR", "HOOKNAME", "CONTAINING_FUNC", "FUNCSTART", "FILE:LINE"))

funcs = {}
out = []
for addr, name, size, loc in rows:
    f = idaapi.get_func(addr)
    if f:
        fname = idc.get_func_name(f.start_ea)
        fstart = f.start_ea
        fend = f.end_ea
    else:
        fname = "<no func>"
        fstart = 0
        fend = 0
    out.append((fstart, addr, name, size, loc, fname, fend))
    funcs.setdefault((fstart, fname), []).append((addr, name, loc))

out.sort(key=lambda x: (x[0], x[1]))
for fstart, addr, name, size, loc, fname, fend in out:
    print("%08X   %-46s %-46s %08X  %s" % (addr, name[:46], fname[:46], fstart, loc))

print()
print("=" * 130)
print("### 按引擎函数聚合（Hook 数降序）")
print("=" * 130)
print("%8s %-58s %6s  %s" % ("START", "FUNC", "NHOOKS", "HOOKS"))
for (fstart, fname), lst in sorted(funcs.items(), key=lambda kv: -len(kv[1])):
    print("%08X %-58s %6d  %s" % (fstart, fname[:58], len(lst),
          ", ".join("%X" % a for a, _, _ in sorted(lst))))

# 顶部 3 个函数的调用者
print()
print("=" * 130)
print("### 每个含 Hook 的引擎函数的调用者（XrefsTo -> 调用者函数）")
print("=" * 130)
for (fstart, fname), lst in sorted(funcs.items(), key=lambda kv: -len(kv[1]))[:60]:
    callers = set()
    for x in idautils.XrefsTo(fstart, 0):
        cf = idaapi.get_func(x.frm)
        callers.add(idc.get_func_name(cf.start_ea) if cf else ("sub_%X" % x.frm))
    print("%08X %-52s  <- %s" % (fstart, fname[:52], ", ".join(sorted(callers)[:12]) or "(no xref)"))
