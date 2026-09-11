import os

import ida_auto
import ida_funcs
import ida_hexrays
import ida_name
import ida_pro
import idc


REPORT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "annotate_report.txt")

FN_TRY_DEPLOY = 0x7393C0
HOOK_DEPLOY = 0x739971
FN_MISSION_DECONSTRUCTION = 0x449C30
CALL_REMOVE = 0x449FE7
CALL_PUT = 0x44A002
HOOK_UNDEPLOY = 0x44A04C


def log(msg):
    print(msg)
    with open(REPORT, "a", encoding="utf-8", errors="replace") as fp:
        fp.write(msg + "\n")


def rename_func(ea, new_name):
    old = ida_funcs.get_func_name(ea) or ""
    ok = ida_name.set_name(ea, new_name, ida_name.SN_NOCHECK | ida_name.SN_FORCE)
    log("RENAME 0x%08X: %r -> %r ok=%r" % (ea, old, new_name, bool(ok)))


def add_cmt(ea, text):
    ok = idc.set_cmt(ea, text, 1)  # repeatable comment
    log("CMT    0x%08X ok=%r len=%d" % (ea, bool(ok), len(text)))


def add_func_cmt(ea, text):
    f = ida_funcs.get_func(ea)
    if not f:
        log("NO FUNC at 0x%08X" % ea)
        return
    ok = idc.set_func_cmt(f.start_ea, text, 1)  # repeatable function comment
    log("FUNCCMT 0x%08X ok=%r len=%d" % (f.start_ea, bool(ok), len(text)))


def main():
    if os.path.exists(REPORT):
        os.remove(REPORT)
    ida_auto.auto_wait()
    if not ida_hexrays.init_hexrays_plugin():
        log("hexrays init failed")
        ida_pro.qexit(1)
        return

    # 1) 修正函数名
    rename_func(FN_TRY_DEPLOY, "UnitClass_Try_To_Deploy")
    rename_func(FN_MISSION_DECONSTRUCTION, "BuildingClass_Mission_Deconstruction")

    # 2) 函数级说明
    add_func_cmt(
        FN_TRY_DEPLOY,
        "UnitClass::Try_To_Deploy（IDA 原名 W?Try_To_Deploy$... 是错的）：\n"
        "载具 DeploysInto 部署成建筑（如 MCV -> ConYard）。\n"
        "顺序：可部署检查 -> 创建并 Put 目标建筑 -> 复制 Group/Veterancy/血量/Target\n"
        "      -> SlaveManager 接管 -> [Hook 0x739971: DeploysInto/AE 继承]\n"
        "      -> 拷 StrX_6/Tag -> 末尾 this->UnInit() 拆除源载具。\n"
        "注意：新建筑先 Put，源载具到函数末尾才拆除，Hook 时两者都存活。")
    add_func_cmt(
        FN_MISSION_DECONSTRUCTION,
        "BuildingClass::Mission_Deconstruction（出售/拆除任务）：\n"
        "其中 MissionStatus==1 && field_6DD && PoweredUnit 的分支是 UndeploysInto：\n"
        "建筑收起变回载具（如卖 ConYard -> MCV）。\n"
        "顺序：this->Remove()(0x449FE7, 派发 OnRemove) -> pUnit->Put()(0x44A002, 派发 OnPut)\n"
        "      -> 复制血量/Group/Veterancy -> SlaveManager 接管\n"
        "      -> [Hook 0x44A04C: DeploysInto/AE 继承] -> 拷 Tag/StrX\n"
        "      -> Object_DC/StrX_DTOR/UnInit 拆除建筑。\n"
        "注意：源建筑在 Hook 前已被 Remove，DiscardOnEntry=true 的 AE 此时已 End。")

    # 3) Hook 点注释
    add_cmt(
        HOOK_DEPLOY,
        "Kratos Hook 0x739971 (UnitClass_TryToDeploy_TransferAE): "
        "EBP=源载具(this), EBX=新建筑。建筑已 Put、载具尚未 UnInit。\n"
        "此处调用 TechnoStatus::DeploysInto(pBuilding, isDeploying=true)。\n"
        "若采用“整包交换 AttachEffect 管理器”，交换后不要再对管理器调用 "
        "ExtChanged()/DetachWhenTransform()（默认 DiscardOnTransform=true 的 AE 会被清掉）。")
    add_cmt(
        HOOK_UNDEPLOY,
        "Kratos Hook 0x44A04C: EBP=源建筑(this), EBX=新载具。"
        "建筑已 Remove、载具已 Put、建筑尚未 UnInit。\n"
        "此处调用 TechnoStatus::DeploysInto(pUnit, isDeploying=false)。\n"
        "注意普通“卖成钱”路径不经过本分支；这是 UndeploysInto 的原版入口。")
    add_cmt(
        CALL_REMOVE,
        "this->Remove(): 源建筑移出地图 -> Kratos 0x6F6AC4 Remove hook 派发 OnRemove；\n"
        "DiscardOnEntry=true 的 AE 在本行之后已 End，AE 继承时拿不到。")
    add_cmt(
        CALL_PUT,
        "pUnit->Put(): 新载具放上地图 -> Kratos TechnoClass_Put hook 派发 OnPut；\n"
        "新载具自己的 AttachEffect 已经做过一次 OnPut/AttachStateEffect。")

    log("DONE")


main()
ida_pro.qexit(0)
