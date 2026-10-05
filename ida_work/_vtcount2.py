import re
base = r"D:\Workspace\ra2mod\platform\KratosPP\YRpp"
files = ["AbstractClass.h","ObjectClass.h","MissionClass.h","RadioClass.h","TechnoClass.h","FootClass.h"]
def virts(fn):
    out=[]
    for i,line in enumerate(open(base+"\\"+fn,encoding="utf-8",errors="replace"),1):
        s=line.strip()
        if s.startswith("virtual "):
            m=re.match(r"virtual\s+(?:[\w:<>,*&\s]+?)\s*([~\w]+)\s*\(", s)
            out.append((i, m.group(1) if m else s[:60]))
    return out
off=0; bound={}
for fn in files:
    v=virts(fn); bound[fn]=(off,off+len(v)); off+=len(v)
lines=[]
for fn in files:
    v=virts(fn); s0,_=bound[fn]
    for k,(ln,nm) in enumerate(v):
        b=(s0+k)*4
        if 0x1B0<=b<=0x1E0 or 0x360<=b<=0x3A0:
            lines.append("%-18s %-5s [%3d] 0x%03X  %s" % (fn,ln,s0+k,b,nm))
open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\_vt_targets.txt","w",encoding="utf-8").write("\n".join(lines))
print("\n".join(lines))
print("\nBOUNDS:", {k:v for k,v in bound.items()})
