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
#include <Ext/Common/PreOccupancyManager.h>

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

/**
 *@brief 查找 pTechno 与 targetPos 之间、距离 targetPos 最近的可落脚点
 * 从目标格开始检查是否可通行/可进入（IsCellOccupied + IsClearToMove），
 * 不可通过则用 Nearby_Location 逐格向外扩散（最多 9 格），返回第一个可用格。
 * 空中单位（含 Jumpjet）直接以目标格为落点。
 * 地面步兵额外调用引擎的子格选择（1/3格，避开已占用子格），Jumpjet 除外。
 *
 * @param pTechno 要落地的单位
 * @param targetPos 期望落点坐标
 * @param forceGround 为 true 时强制按地面单位搜索落点（忽略 IsInAir 早退，用于
 * 人间大炮等"需要真实落脚点"的场景；默认 false 时空中单位直接以目标格为落点）
 * @param pPreOccupancyManager 预占用管理器指针，用于预占用检查（可选）
 * @return 落点坐标（不含空中高度）；找不到可落脚点时返回 CoordStruct::Empty
 */
CoordStruct FindLandingPoint(TechnoClass* pTechno, const CoordStruct& targetPos, bool forceGround = false, PreOccupancyManager* pPreOccupancyManager = nullptr);

bool TryGetLandingPoint(TechnoClass* pTechno, const CoordStruct& targetPos, CoordStruct& landingPos, bool forceGround = false, PreOccupancyManager* pPreOccupancyManager = nullptr);

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
//   占格/释放的真正入口是本体（TechnoClass）虚表 +0xF0 / +0xF4，
//   即 YRpp 命名的 MarkAllOccupationBits / UnmarkAllOccupationBits：
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
//   用 MarkAllOccupationBits 写了子格位，Jumpjet 会认为该格落不了，飞回起飞点并把
//   目的地改回原格，导致状态机来回跳（日志表现为 inAir=0、dest 变回起点）。
//
// 【伞兵投放 AircraftClass::Paradrop (0x415C60) 的空位查找流程】
//   投放点 = 飞机位置沿当前朝向偏移，currentAmmo & 1 决定左右交替（±0x3FFF）；
//   拿到 Target_Cell 后：
//     1) passenger->IsCellOccupied(Target_Cell, -1, -1, 0, 1)
//        —— 粗粒度检查。对同阵营 1~2 个步兵只累计 v47、不累计 v19，
//           仍返回 Move::OK(0)；只有 3 个步兵位全占（field_124 & 0x1C == 28）
//           才返回 7，有载具/建筑时返回 2/5/6。
//     2) CellClass::PickInfantrySublocation(Target_Cell, ..., 0, 0, 0) (0x481180)
//        —— 真正的子格空位查找。读取 OccupationFlags 的子格位：
//           候选子格只取 2/3/4（byte_81CC84 / byte_81CC98 两张 4x4 表，
//           0 和 1 在循环里被跳过），检查 (1 << 子格) 是否为空；
//           0x20 位置位、或 0x40 载车位且无可穿越建筑 → 直接返回 -1 哨兵。
//     3) SpawnParachuting (0x521760) → ObjectClass::Paradrop (0x5F5940)
//        → InfantryClass::Put (0x51DFF0) → FootClass_Put (0x4D7170)。
//        Put 时 Z != 地面高度，不会调 F0 写子格位——所以空中伞兵只在
//        Content 列表里占格，OccupationFlags 全空；落地后才由 F0 写位。
//   子格几何（sub_4810A0 坐标→子格索引；InitList_0644 0x48E489 填充表 0x89E9F0，
//   格子内坐标 0~255，中心 128,128）：
//     子格 0 = 中心 (128,128)（含 X<=128&&Y<=128 的外圈区域，未用）
//     子格 2 = 东  (192,64)  → 位 0x04
//     子格 3 = 南  (64,192)  → 位 0x08
//     子格 4 = 东南 (192,192) → 位 0x10
//   三个步兵位掩码 0x1C = 1<<2|1<<3|1<<4，步兵一格最多 3 个就是这三位。
//   【FindLandingPoint 的对应处理】canLand 里步兵落点镜像上述子格位检查，
//   并额外扫 Content：格子里存在任何步兵（含正在落伞、未写子格位的）都视为被占，
//   这样"伞兵占的格子"能被正确过滤；PreOccupancyManager 的 3 个预占子格
//   也改用引擎的子格 2/3/4 及其偏移（+64,-64 / -64,+64 / +64,+64），
//   与引擎 F0 写入的位保持一致。
// ============================================================================
