import re
CHAIN=["AbstractClass.h","ObjectClass.h","MissionClass.h","RadioClass.h","TechnoClass.h","FootClass.h"]
# secondary-interface methods of AbstractClass that live in SECONDARY vtables (not the primary one)
SECONDARY={"What_Am_I","Fetch_ID","Create_ID","INoticeSink_Unknown","INoticeSource_Unknown"}
def parse(path):
    out=[]
    for i,line in enumerate(open(path,encoding="utf-8",errors="replace"),1):
        s=line.strip()
        if s.startswith("virtual "):
            m=re.match(r"virtual\s+(?:[\w:<>,*&\s]+?)\s*(~?\w+)\s*\(", s)
            out.append((i, m.group(1) if m else s[:50], s))
    return out
slot=0; known={}; rows=[]
for fn in CHAIN:
    rows.append("### %s starts slot %d = 0x%X"%(fn,slot,slot*4))
    for ln,nm,full in parse(r"D:\Workspace\ra2mod\platform\KratosPP\YRpp\\"+fn):
        if nm.startswith("~"):
            continue
        if nm in known:
            continue
        if nm in SECONDARY:
            rows.append("  %-6s SECONDARY-vtable (no primary slot)  %s"%(ln,nm)); continue
        known[nm]=slot; b=slot*4
        lab=int(nm.split("_")[-1],16) if nm.startswith("vt_entry_") else None
        mark = "  label=0x%03X %s"%(lab, "OK" if lab==b else "*** MISMATCH by %+d slots"%((b-lab)//4)) if lab else ""
        flag = "   <<<<< GAME SLOT 0x%X"%b if b in (0x1C4,0x378) else ""
        # also flag slots where the IDA db has a real name we know
        rows.append("  %-6s %4d 0x%03X  %-34s%s%s"%(ln,slot,b,nm,mark,flag))
        slot+=1
open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\_layout24.txt","w",encoding="utf-8").write("\n".join(rows))
print("total slots=%d (0x%X)"%(slot,slot*4))
for r in rows:
    if "MISMATCH" in r or "GAME SLOT" in r or r.startswith("###") or "SECONDARY" in r or "label=" in r:
        print(r)
