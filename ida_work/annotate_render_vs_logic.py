# -*- coding: utf-8 -*-
"""把"渲染 vs 逻辑帧"的结论以注释 + 重命名写进规范 idb。

依据前面的探针（probe_render_vs_logic*.txt / *_report.txt）。
只写注释与名字，不改字节。
"""
import idaapi, idc, idautils, ida_funcs, ida_auto, ida_pro, ida_name, ida_bytes
import traceback

report = []
def log(s=""):
    report.append(str(s))

CMTS = [
 (0x4F4480, "GScreenClass render body (mod calls it GScreenClass::Render / Render_Late; idb name DrawOnTop). "
            "Draws the tactical map + sidebar + messages. NOT tied 1:1 to logic frames - see 0x4F4497 / 0x55D8F2 / 0x55E271."),
 (0x4F4497, "mod hook GScreenClass_Render (Kratos/Phobos GScreenHook.cpp:21) -> BeginRender event. "
            "Runs once per *render*, not per logic frame: normal frame-start call site 0x55D8F2 "
            "(requires GameInFocus==1 && (arg_ATTRACT_Flags&2)==0 && dword_A8EDA0==0); "
            "extra calls from the MP frame limiter sub_55E160 @0x55E271 (may fire several times inside one logic frame); "
            "replay playback call site 0x55DBBE. NEVER runs while GameInFocus(0xA8ED80)==0 (Alt-Tab) - "
            "in MP modes LogicClass::Update + sync hash still run for those frames."),
 (0x4F4583, "mod hook GScreenClass_Render_Late (Kratos/Phobos GScreenHook.cpp:27) -> same render pass, "
            "executed near the end of the render body (after map/sidebar/MessageList/tooltip). Same call conditions as 0x4F4497."),
 (0x55D360, "Main_Loop: one call = one logic frame (Frame++ at 0x55DE73, exactly once per call). "
            "Order inside one iteration: render(0x55D8F2, conditional) -> LayerClass::Sort(0x55DBC8) -> "
            "LogicClass::Update(0x55DC9E) -> sync hash(0x55DE40, via sub_647260) -> frame limiter sub_55E160(0x55DE9A, may re-render)."),
 (0x55D377, "if (!GameInFocus): GameMode 0/5 (SP) spins here with Sleep(500) until focus returns (whole frame stalls); "
            "other modes (MP: 1..4) only Sleep(10) and continue -> logic+hash keep running while rendering is skipped."),
 (0x55D84F, "SP-only pause path (GameMode 0/5 && Scenario->field_62C != 0, test at 0x55D826): render once, then RETURN. "
            "No LayerClass::Sort, no LogicClass_Update, no sync hash, Frame not incremented."),
 (0x55D878, "render gate for the normal frame path: (arg_ATTRACT_Flags & 2)==0 && dword_A8EDA0==0 && GameInFocus!=0, "
            "otherwise jump to 0x55D901 and skip the frame-start render."),
 (0x55D8F2, "frame-start render (normal path) -> Map.DrawOnTop -> mod hooks 0x4F4497/0x4F4583. "
            "Sits BEFORE Sort(0x55DBC8), LogicClass::Update(0x55DC9E) and the sync hash(0x55DE40) of the same logic frame."),
 (0x55DBBE, "replay-playback render (only when (arg_ATTRACT_Flags & 2)!=0, same iteration); "
            "in that mode the 0x55D8F2 render is skipped (see 0x55D881)."),
 (0x55DBC8, "LayerClass::Sort(vec_ObjectsInLayers[2] = Ground layer @0x8A0390): one adjacent-swap pass keyed on obj vt+0xB8 "
            "(ObjectClass::ReturnRealYSort 0x5F6BD0, coordinate-derived). Runs unconditionally, even if this frame's render was skipped."),
 (0x55DC9E, "LogicClass::Update - the actual per-frame simulation. Runs even when render was skipped (unfocused MP window)."),
 (0x55DE40, "sub_647260 -> sync-check driver. For Session.idxGameMode 1..4 it calls sub_6475F0 -> sub_64DAB0 (frame hash) every logic frame; "
            "GameMode 0/5 take the SP branch and never hash."),
 (0x55DE9A, "sub_55E160 = frame-time limiter (called after every logic frame). In MP modes (GameMode 1..4) it loops Call_Back/Sleep(0) "
            "until the frame budget is used and RE-RENDERS at 0x55E271 whenever >10ms remain -> render count per logic frame is >=1 and variable."),
 (0x55E204, "limiter loop break: !dword_A8EDA0 && GameInFocus==1. While unfocused the loop never breaks -> no re-render at all."),
 (0x55E271, "re-render inside the frame-time limiter (second/third/... render of the same logic frame); "
            "same mod hooks 0x4F4497/0x4F4583 fire again."),
 (0x55DE73, "Frame++ : exactly one increment per Main_Loop call => one logic frame per iteration (reference for 'per frame' claims)."),
 (0xA8ED80, "GameInFocus. Written ONLY in Windows_Procedure @0x7778CE from the window activation/focus message "
            "(prints \"Focus_Loss()\" when it becomes 0). GameInFocus==0 => every render call in Main_Loop/sub_55E160 is skipped."),
 (0x64DAB0, "Sync-check frame hash (dword_AC51FC): for every object of vec_ObjectsInLayers[0..4] and vec_Logics: "
            "hash = 3*hash + (WhatAmI(obj) + ((Y/10)<<16) + X/10), where X/Y/Z are read from the CoordStruct at obj+0x9C / +0xA0 / +0xA4 "
            "(= the Location written by ObjectClass::SetPosition/SetLocation, vt+0x1B4). Z is copied to var_4 and NEVER used. "
            "Called once per logic frame for GameMode 1..4 => a Location write that happens earlier in the same frame (e.g. from the render hook) is hashed."),
 (0x64DCE4, "hash layer loop: 5 layers, base 0x8A0360, stride 0x18, terminates at 0x8A03E8. Order-sensitive (see 0x551A30)."),
 (0x64DD1B, "per-object read: [obj+0x9C]=X, [obj+0xA0]=Y, [obj+0xA4]=Z of the object's CoordStruct (Location)."),
 (0x64DD5F, "hash = 3*dword_AC51FC + term (per object)."),
 (0x6474C7, "switch(Session.idxGameMode) via jpt_6474D5: 0 and 5 -> SP branch (no frame hash); 1..4 -> 0x6475C8 -> sub_6475F0 (frame hash every frame)."),
 (0x647684, "sub_64DAB0 call: per-logic-frame sync hash for GameMode 1..4."),
 (0x5F6940, "ObjectClass::SetPosition = the virtual SetLocation (vt_ObjectClass slot +0x1B4, see vtable 0x7E4070): "
            "writes CoordStruct X@+0x9C, Y@+0xA0, Z@+0xA4. This is exactly what the sync hash reads - "
            "a Location write from a render callback enters the same frame's hash."),
 (0x5F6060, "ObjectClass::SetZ: writes obj+0xA4 = Z of the CoordStruct at 0x9C (Z is NOT part of the sync hash)."),
 (0x4A88C0, "DisplayClass_InitClear: the ONLY place that clears all 5 layer arrays (vt->Clear loop over vec_ObjectsInLayers). "
            "Callers: RadarClass_InitClear <- PowerClass::Init_Clear <- SidebarClass::Init_Clear => game init/restart only, NOT per frame. "
            "Per-frame layer membership is maintained incrementally by logic-time Submit/Remove (ObjectClass::Update 0x5F400E/0x5F414C, TechnoClass_EC ...)."),
 (0x6D8DB0, "Tactical_Draw_All: READS vec_ObjectsInLayers[0..4] and draws each object via vt+0x104 / vt+0x110. "
            "Reached from TacticalClass_Draw(0x6D3D10 @0x6D465F) <- GScreenClass render (0x4F44DF/0x4F44F4/0x4F4515). "
            "The layer arrays are NOT rebuilt here - only read."),
 (0x551A30, "LayerClass::Sort: single adjacent-swap pass, key = obj vt+0xB8 (ObjectClass::ReturnRealYSort 0x5F6BD0, coordinate-derived). "
            "=> object coordinates influence the Ground-layer order, and the sync hash is order-sensitive."),
]

NAMES = [
 (0x55E160, "MainLoop_FrameWait_ReRender"),
 (0x647260, "MainLoop_SyncHash_Dispatch"),
 (0x6475F0, "Multiplay_SyncCheck_PerFrame"),
 (0x64DAB0, "SyncCheck_ComputeFrameHash"),
 (0x551A30, "LayerClass_Sort"),
]

try:
    ida_auto.auto_wait()
    log("idb = %s" % idc.get_idb_path())
    ok = 0
    for ea, txt in CMTS:
        try:
            old = idc.get_cmt(ea, 0)
            idc.set_cmt(ea, txt, 0)
            log("  cmt %08X  (old=%r)" % (ea, old))
            ok += 1
        except Exception as e:
            log("  cmt %08X FAILED %r" % (ea, e))
    log("  注释写入 %d/%d" % (ok, len(CMTS)))
    for ea, nm in NAMES:
        try:
            old = idc.get_name(ea)
            r = ida_name.set_name(ea, nm, ida_name.SN_NOCHECK | ida_name.SN_FORCE)
            log("  name %08X %s -> %s (idc ret=%s)" % (ea, old, nm, r))
        except Exception as e:
            log("  name %08X FAILED %r" % (ea, e))
    # 回读验证
    log("")
    log("回读验证：")
    for ea, _t in CMTS:
        c = idc.get_cmt(ea, 0)
        log("  %08X name=%-32s cmt=%s" % (ea, idc.get_name(ea), (c[:60] + "...") if c and len(c) > 60 else c))
except Exception:
    log("!!!! EXCEPTION !!!!")
    log(traceback.format_exc())
finally:
    open(r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\annotate_render_vs_logic.txt", "w",
         encoding="utf-8", errors="replace").write("\n".join(report))
    ida_pro.qexit(0)
