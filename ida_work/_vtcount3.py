import re
base = r"D:\Workspace\ra2mod\platform\KratosPP\YRpp"
def virts(fn):
    out=[]
    for i,line in enumerate(open(base+"\\"+fn,encoding="utf-8",errors="replace"),1):
        s=line.strip()
        if s.startswith("virtual "):
            m=re.match(r"virtual\s+(?:[\w:<>,*&\s]+?)\s*([~\w]+)\s*\(", s)
            out.append((i, m.group(1) if m else s[:60]))
    return out
out=[]
for fn,start in (("ObjectClass.h",29),):
    v=virts(fn)
    for k,(ln,nm) in enumerate(v):
        slot=start+k
        if "vt_entry" in nm or k<12:
            out.append("%-5s [%3d] 0x%03X  %-28s  (self 0x%03X)" % (ln,slot,slot*4,nm,(slot-29)*4))
open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\_obj.txt","w",encoding="utf-8").write("\n".join(out))
print("\n".join(out))
