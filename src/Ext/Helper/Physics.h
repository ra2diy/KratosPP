#pragma once
#include <string>
#include <map>
#include <vector>

#include <GeneralDefinitions.h>
#include <CellClass.h>
#include <MapClass.h>
#include <BuildingClass.h>
#include <InfantryClass.h>

#include <Common/INI/INIConfig.h>

#include "Status.h"

enum class PassError : int
{
	NONE = 0,
	PASS = 1, // 可通行
	UNDERGROUND = 2, // 潜地
	HITWALL = 3, // 不可通行
	HITBUILDING = 4, // 撞建筑
	DOWNBRIDGE = 5, // 从上方撞桥
	UPBRIDEG = 6 // 从下方撞桥
};

/**
 *@brief 是否撞击建筑物
 *
 * @param pBuilding 待检查的建筑物
 * @param targetZ 撞击位置的Z轴高度
 * @param blade 贯穿天地的剑气，必中
 * @param zOffset 高度偏移值
 * @return true 撞到了
 * @return false 没撞到
 */
bool CanHit(BuildingClass* pBuilding, int targetZ, bool blade = false, int zOffset = 0);

/**
 *@brief 前方格子是否可以通行，有无障碍物
 *
 * @param sourcePos 当前位置
 * @param nextPos 下一帧的位置
 * @param passBuilding 能穿透建筑
 * @param nextCellPos 目标位置的格子
 * @param onBridge 目标位置是桥
 * @return PassError
 */
PassError CanMoveTo(CoordStruct sourcePos, CoordStruct nextPos, bool passBuilding, CoordStruct& nextCellPos, bool& onBridge);

/**
 * @brief 检查脚下是否可以通行
 *
 * @param pTechno 检查的单位
 * @param targetPos 摔的位置
 * @param pCell 脚下的格子
 * @param isWater 脚下的格子是否是水面
 * @return true 可以通行
 * @return false 不可，应该摔死
 */
bool CanPassUnder(TechnoClass* pTechno, CoordStruct& targetPos, CellClass*& pCell, bool& isWater);

enum FallingError : int
{
	FLY = -1, // 没想到吧，哥会飞
	UNCHANGED = 0, // 停在地上
	FALLING = 1, // 掉落在地上
	SINKING = 2, // 沉入水中
	BOMB = 3 // 摔死
};

/**
 *@brief 强制摔在地上，如果已经在地上了则尝试散开，如果在空中则摔落在地
 *
 * @param pTechno 要摔的单位
 * @param targetPos 摔的位置
 * @param fallingDestroyHeight 强制摔死的高度
 * @param hasParachute 有降落伞
 * @param isWater 下方是否是水
 * @param canPass 可以安全进入
 * @return FallingError
 */
FallingError Falling(TechnoClass* pTechno, CoordStruct targetPos, int fallingDestroyHeight, bool hasParachute, bool isWater, bool& canPass);

/**
 *@brief 判断脚下能否着陆，然后往下摔
 *
 * @param pTechno 
 * @param fallingDestroyHeight 
 * @param hasParachute 
 * @return true 
 * @return false 
 */
FallingError FallingDown(TechnoClass* pTechno, int fallingDestroyHeight, bool hasParachute);

/**
 *@brief 除了会飞的单位之外，其他的单位往下坠落
 *
 * @param pTechno 要摔的单位
 * @param fallingDestroyHeight 强制摔死的高度
 * @param hasParachute 有降落伞
 * @return true 安全下落
 * @return false BOOM!
 */
FallingError FallingExceptAircraft(TechnoClass* pTechno, int fallingDestroyHeight, bool hasParachute);

// ============================================================================
// 格子占用位备忘（2026-08-31 通过 gamemd.idb 反编译确认）
//
// 【Force_Track 的真相】
//   LocomotionClass::ILocomotion::ForceTrack（0x55AC10）是空函数，
//   YRpp 注释："Force drive track -- special case only"。
//   只有 DriveLocomotionClass 覆写了它（0x4B0C40）：设置 HeadToCoord →
//   调本体 +0xF0 占格 → 设置 Destination。对步兵 / Jumpjet / TeleportLoco
//   是空操作。
//
// 【loco 移动与 Cell 的交互（游戏虚表实测）】
//   ILocomotion 虚表布局：
//     +0x10 Is_Moving      +0x14 Destination      +0x18 HeadToCoord
//     +0x40 Process        +0x44 Move_To          +0x48 Stop_Moving
//     +0x70 Force_Track    +0x9C Mark_All_Occupation_Bits
//   占格/释放的真正入口是本体（TechnoClass）虚表 +0xF0 / +0xF4：
//     InfantryClass_F0（0x5217C0）：cell->OccupationFlags |= 1 << subcellIndex（子格位）
//     ObjectClass_F0  （0x5F60A0）：cell->OccupationFlags |= 0x40（载具位）
//     BuildingClass_F0（0x453D60）：cell->OccupationFlags |= 0x80
//     F4 是对应的清位实现（InfantryClass_F4 0x521850 / ObjectClass_F4 0x5F6120）
//   loco 移动时通过 Mark_All_Occupation_Bits / Process 调这两个槽位：
//     WalkLoco（0x75CA30，只处理 Remove 调 F4）、DriveLoco（0x4B48D0 → 0x4B0AD0）。
//   CellClass::OccupationFlags（0x124 地面 / 0x128 桥上）由 IsClearToMove
//   （0x4834A0）和 PickInfantrySublocation（0x481180，检查 1<<子格）读取——
//   这就是"下一个单位踩不过去"和"步兵按 1/3 格子分散"的机制。
//
// 【为什么手动位移必须补调 F0/F4】
//   UpdatePlacement(PlacementType) 实际是 FootClass::SetCoords（0x4DB810），
//   只设置坐标、做隧道检查，不写 CellClass::OccupationFlags。
//   所以手动 SetLocation + UpdatePlacement 之后，落点格子的占用位是空的：
//     - PickInfantrySublocation 永远认为子格空闲 → 步兵全选同一个子格 → 堆叠
//     - IsClearToMove 也认为格子空着
//   TeleportLoco 不堆叠，正是因为它的 MoveTo/Process 会正确调用本体的占格方法。
//
// 【Jumpjet / 飞行器】
//   不调用 F0/F4。Jumpjet 的占用是 CellClass::Jumpjet 指针，
//   由 FootClass::Jumpjet_OccupyCell 维护；而且 Jumpjet 自己的降落逻辑
//   （JumpjetLocomotionClass::Process 的 Descending 分支）会用 sub_481130
//   检查目标子格的占用位，落地成功后才自己调 F0 写位。如果传送落地时提前
//   用 OccupyCell 写了子格位，Jumpjet 会认为该格落不了，飞回起飞点并把
//   目的地改回原格，导致状态机来回跳（日志表现为 inAir=0、dest 变回起点）。
// ============================================================================

/**
 *@brief 占用格子：调用本体虚表 +0xF0，写 CellClass::OccupationFlags
 * 步兵写子格位（1<<子格），载具写 0x40 位，使 PickInfantrySublocation /
 * IsClearToMove 能感知到该格被占。
 *
 * @param pTechno 要占格的单位
 * @param coords 占格位置（步兵需为子格位置）
 */
void OccupyCell(TechnoClass* pTechno, const CoordStruct& coords);

/**
 *@brief 释放格子：调用本体虚表 +0xF4，清掉 CellClass::OccupationFlags 中本单位占的位
 *
 * @param pTechno 要释放的单位
 * @param coords 释放位置（步兵需为子格位置）
 */
void ReleaseCell(TechnoClass* pTechno, const CoordStruct& coords);
