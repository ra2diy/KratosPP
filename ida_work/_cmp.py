import re
def virts(path):
    out=[]
    for i,line in enumerate(open(path,encoding="utf-8",errors="replace"),1):
        s=line.strip()
        if s.startswith("virtual "):
            m=re.match(r"virtual\s+(?:[\w:<>,*&\s]+?)\s*(~?\w+)\s*\(", s)
            out.append((i, m.group(1) if m else s[:50]))
    return out
res={}
for tag,base in (("OURS",r"D:\Workspace\ra2mod\platform\KratosPP\YRpp"),("PHOBOS",r"D:\Workspace\ra2mod\platform\Phobos\YRpp")):
    off=0; rows=[]
    for fn in ["AbstractClass.h","ObjectClass.h","MissionClass.h","RadioClass.h","TechnoClass.h"]:
        v=virts(base+"\\"+fn)
        for k,(ln,nm) in enumerate(v):
            b=(off+k)*4
            if "vt_entry" in nm or nm in ("IsCellOccupied","GetCell","GetMapCoords","EnterGrinder","ScanForTiberium"):
                rows.append("%-7s %-19s L%-5s slot=%3d 0x%03X  %-24s  label_delta=%s" % (tag,fn,ln,off+k,b,nm, ("0x%X"%(b-int(nm.split("_")[-1],16))) if nm.startswith("vt_entry") else "-"))
        off+=len(v)
    res[tag]=rows
    print("### %s total slots through TechnoClass = %d" % (tag,off))
for r in res["OURS"]: print(r)
print()
for r in res["PHOBOS"]: print(r)
