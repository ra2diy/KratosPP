import re, sys
base = r"D:\Workspace\ra2mod\platform\KratosPP\YRpp"
files = [("AbstractClass.h",29,0),("ObjectClass.h",None,None),("MissionClass.h",None,None),("RadioClass.h",None,None),("TechnoClass.h",None,None),("FootClass.h",None,None)]
def virts(fn):
    out=[]
    for i,line in enumerate(open(base+"\\"+fn,encoding="utf-8",errors="replace"),1):
        s=line.strip()
        if s.startswith("virtual "):
            m=re.match(r"virtual\s+(?:[\w:<>,*&\s]+?)\s*([~\w]+)\s*\(", s)
            out.append((i, m.group(1) if m else s[:60]))
    return out
off=0
for fn,_,_ in files:
    v=virts(fn)
    print("=== %s : %d virtuals, start byte 0x%X (slot %d)" % (fn,len(v),off*4,off))
    for k,(ln,nm) in enumerate(v):
        slot=off+k; b=slot*4
        mark=""
        if b in (0x1C4,0x378): mark="   <<<<<< TARGET 0x%X"%b
        if fn in ("ObjectClass.h",) and b in (0x1C0,0x1C4,0x1C8): mark="   <<< near"
        if b in (0x370,0x374,0x378,0x37C): mark="   <<<<<< TARGET/near 0x%X"%b
        print("   %-5s [%3d] 0x%03X  %s%s" % (ln,slot,b,nm,mark))
    off+=len(v)
