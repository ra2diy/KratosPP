# -*- coding: utf-8 -*-
import os, idc, ida_pro
HERE = os.path.dirname(os.path.abspath(__file__))
f = open(os.path.join(HERE, "p17_objuupdate.txt"), "w", encoding="utf-8")
def w(s):
    f.write(s + "\n"); f.flush()
def disasm(ea, end, cap=200):
    n = 0
    while ea < end and n < cap:
        d = idc.GetDisasm(ea)
        if not d: break
        w("  %08X  %s" % (ea, d))
        sz = idc.get_item_size(ea)
        if sz <= 0: break
        ea += sz; n += 1
w("############ ObjectClass_Update 0x5F3F30..0x5F4030 (含 +0x18C 调用) ############")
disasm(0x5F3F30, 0x5F4030)
w("")
w("############ TechnoClass::Per_Cell_Process 0x6F5090 头部 ############")
disasm(0x6F5090, 0x6F5090 + 320)
w("")
w("############ FootClass_ExecutePlanningWaypoint 0x4DC8C0 头部 ############")
disasm(0x4DC8C0, 0x4DC8C0 + 260)
f.close(); ida_pro.qexit(0)
