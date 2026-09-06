#pragma once

#include <functional>
#include <string>
#include <vector>

#include <GeneralDefinitions.h>
#include <HouseClass.h>
#include <MapClass.h>
#include <TechnoClass.h>
#include <TechnoTypeClass.h>

#include <Helpers/Enumerators.h>

#include <Ext/EffectType/Effect/CrateBuffData.h>
#include <Ext/StateType/State/GiftBoxData.h>

class AttachEffect;

bool TryPutTechno(TechnoClass* pTechno, CoordStruct location, CellClass* pCell = nullptr, bool virtualUnit = false);

TechnoClass* CreateAndPutTechno(TechnoTypeClass* pType, HouseClass* pHouse, CoordStruct location, CellClass* pCell = nullptr);

TechnoClass* CreateAndPutTechno(std::string id, HouseClass* pHouse, CoordStruct location, CellClass* pCell = nullptr);

bool RealReleaseGift(std::string id, HouseClass* pHouse,
	CoordStruct location, // 预订投放位置
	CellClass* pCell, // 预订投放位置的格子
	std::vector<CellStruct> cellOffsets, // 随机格子的范围
	bool emptyCell, // 是否空格子再投放
	TechnoTypeClass*& pGiftType, // 礼物的类型
	TechnoClass*& pGift, // 礼物本体
	CoordStruct& putLocation, // 礼物的投放位置
	CellClass*& pPutCell // 礼物投放的格子
);

struct BoxStateCache
{
	CoordStruct Location = CoordStruct::Empty; // 当前位置
	int RandomRange = -1; // 随机分散距离
	bool EmptyCell = true; // 有限寻找空位

	Mission CurrentMission = Mission::None; // 当前任务
	Mission ForceMission = Mission::None; // 强制任务
	AbstractClass* pTarget = nullptr; // 目标
	bool InheritTarget = true; // 继承目标
	bool IsTransfrom = false; // 是否变形

	AbstractClass* pDest = nullptr; // 载具的移动目的地
	AbstractClass* pFocus = nullptr; // 步兵的移动目的地
	bool Scatter = false; // 散开

	bool SameDir = true; // 保持朝向
	DirStruct BodyDir{}; // 身体朝向
	DirStruct TurretDir{}; // 炮塔朝向

	int Group = -1; // 编队
	bool IsSelected = false; // 是否选中
	CrateBuffData CrateBuff{}; // 箱子属性
	bool BoxIsBullet = false; // 盒子是抛射体不复制箱子属性

	ObjectClass* pOwner = nullptr;
	HouseClass* pHouse = nullptr; // 所属

	std::vector<std::string> RemoveEffects{};
	std::vector<std::string> AttachEffects{};
	std::vector<double> AttachChances{};
};

BoxStateCache GiftBoxStateCache(GiftBoxData boxData);

void ReleaseGifts(std::vector<std::string> gifts, GiftBoxEntity data, BoxStateCache boxState, std::function<void(TechnoClass*, TechnoStatus*&, AttachEffect*&)> inheritFun);

std::vector<std::string> GetGiftList(GiftBoxEntity data);

AttachEffect* InheritAE(TechnoStatus* pBoxStatus, TechnoStatus*& pGiftStatus);
