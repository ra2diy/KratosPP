import idaapi, idc, idautils

def show(start, count, title):
    print("="*100)
    print("### " + title)
    print("="*100)
    ea = start
    for i in range(count):
        f = idaapi.get_func(ea)
        print("  0x%08X  %-42s  %s" % (ea, idc.generate_disasm_line(ea, 0), ("func:"+idc.get_func_name(f.start_ea)) if f else ""))
        nxt = idc.next_head(ea, ea+16)
        if nxt <= ea: break
        ea = nxt

show(0x6FDD50-0x30, 40, "TechnoClass::Fire prologue around 0x6FDD50")
show(0x6F36DB-0x20, 30, "TechnoClass::SelectWeapon hook 0x6F36DB")
