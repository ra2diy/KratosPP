import re
CHAIN=["AbstractClass.h","ObjectClass.h","MissionClass.h","RadioClass.h","TechnoClass.h","FootClass.h"]
def parse(path):
    out=[]
    for i,line in enumerate(open(path,encoding="utf-8",errors="replace"),1):
        s=line.strip()
        if s.startswith("virtual "):
            m=re.match(r"virtual\s+(?:[\w:<>,*&\s]+?)\s*(~?\w+)\s*\(", s)
            out.append((i, m.group(1) if m else s[:50], s))
    return out
def analyze(base,tag):
    slot=0; known={}; rows=[]
    for fn in CHAIN:
        p=base+"\\"+fn
        rows.append("### %s (starts slot %d = 0x%X)"%(fn,slot,slot*4))
        for ln,nm,full in parse(p):
            if nm.startswith("~"):
                rows.append("  %-6s  (dtor override -> slot %d)"%(ln,known.get("~base",13)))
                continue
            if nm in known:
                rows.append("  %-6s  override   -> slot %3d 0x%03X  %s"%(ln,known[nm],known[nm]*4,nm))
                continue
            known[nm]=slot
            b=slot*4
            lab=None
            if nm.startswith("vt_entry_"):
                lab=int(nm.split("_")[-1],16)
            mark=""
            if lab is not None:
                mark="  label=0x%03X delta=%s"%(lab,("%+d"%((b-lab)//4)))
            tgt=""
            if b in (0x1C4,0x378): tgt="   <<<<<<<< TARGET SLOT 0x%X"%b
            rows.append("  %-6s  NEW %3d 0x%03X  %-32s%s%s"%(ln,slot,b,nm,mark,tgt))
            slot+=1
    open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\_layout_%s.txt"%tag,"w",encoding="utf-8").write("\n".join(rows))
    print("=== %s : total slots=%d (0x%X)"%(tag,slot,slot*4))
    # print label deltas summary
    for r in rows:
        if "label=" in r or "TARGET SLOT" in r or r.startswith("###"):
            print(r)
analyze(r"D:\Workspace\ra2mod\platform\KratosPP\YRpp","ours")
print()
print("~~~~~~~~ PHOBOS ~~~~~~~~")
analyze(r"D:\Workspace\ra2mod\platform\Phobos\YRpp","phobos")
