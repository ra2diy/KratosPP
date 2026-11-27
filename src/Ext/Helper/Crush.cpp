#include "Crush.h"

#include <map>
#include <string>

#include <TechnoClass.h>
#include <InfantryClass.h>
#include <OverlayTypeClass.h>

#include <Common/INI/INI.h>

#include <Extension/TechnoExt.h>
#include <Extension/TechnoTypeExt.h>

#include <Ext/Common/CommonStatus.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/TechnoType/TechnoStatus.h>

namespace
{
	/// @brief 按军衔取三档值：精英 -> 老兵 -> 新兵，未设置(-1)则向下一档回退
	int PickLevel(int rookie, int veteran, int elite, Rank rank)
	{
		switch (rank)
		{
		case Rank::Elite:
			if (elite >= 0)
				return elite;
			[[fallthrough]];
		case Rank::Veteran:
			if (veteran >= 0)
				return veteran;
			[[fallthrough]];
		default:
			return rookie;
		}
	}
}

int Crush::CalcCrushLevel(TechnoClass* pThis)
{
	if (!pThis)
		return 0;

	TechnoTypeClass* pType = pThis->GetTechnoType();
	TechnoTypeExt::TypeData* pData = GetTypeData<TechnoTypeExt, TechnoTypeExt::TypeData>(pType);
	if (pData)
	{
		int level = PickLevel(pData->CrushLevel, pData->CrushLevel_Veteran, pData->CrushLevel_Elite,
			pThis->Veterancy.GetRemainingLevel());
		if (level >= 0)
			return level;
	}

	GeneralData* pGeneral = General::Data();

	if (pType->OmniCrusher)
		return pGeneral->OmniCrusherLevel;

	// 类型写了 Crusher，或运行时通过能力获得了 Crusher，都算普通碾压者
	if (pType->Crusher || pThis->HasAbility(Ability::Crusher))
		return pGeneral->CrusherLevel;

	return 0;
}

int Crush::CalcCrushableLevel(TechnoClass* pThis)
{
	if (!pThis)
		return 0;

	TechnoTypeClass* pType = pThis->GetTechnoType();
	TechnoTypeExt::TypeData* pData = GetTypeData<TechnoTypeExt, TechnoTypeExt::TypeData>(pType);
	GeneralData* pGeneral = General::Data();
	Rank rank = pThis->Veterancy.GetRemainingLevel();

	// 部署状态的步兵（Uncrushable）走 DeployedCrushableLevel 一套
	InfantryClass* pInfantry = abstract_cast<InfantryClass*>(pThis);
	if (pInfantry && pInfantry->Uncrushable)
	{
		if (pData)
		{
			int level = PickLevel(pData->DeployedCrushableLevel, pData->DeployedCrushableLevel_Veteran,
				pData->DeployedCrushableLevel_Elite, rank);
			if (level >= 0)
				return level;
		}

		if (pType->OmniCrushResistant)
			return pGeneral->OmniCrushResistantLevel;

		if (!pInfantry->Type->DeployedCrushable)
			return pGeneral->CrushableLevel;

		return 0;
	}

	if (pData)
	{
		int level = PickLevel(pData->CrushableLevel, pData->CrushableLevel_Veteran, pData->CrushableLevel_Elite, rank);
		if (level >= 0)
			return level;
	}

	// 建筑默认不可压（未配置时跟随 OmniCrushResistantLevel）
	if (pThis->WhatAmI() == AbstractType::Building)
		return pGeneral->GetBuildingCrushableLevel();

	if (pType->OmniCrushResistant)
		return pGeneral->OmniCrushResistantLevel;

	if (!pType->Crushable)
		return pGeneral->CrushableLevel;

	return 0;
}

int Crush::GetCrushLevel(TechnoClass* pThis)
{
	TechnoStatus* pStatus = nullptr;
	if (TryGetStatus<TechnoExt>(pThis, pStatus) && pStatus->CrushLevelReady)
		return pStatus->CrushLevelCache;

	return CalcCrushLevel(pThis);
}

int Crush::GetCrushableLevel(TechnoClass* pThis)
{
	TechnoStatus* pStatus = nullptr;
	if (TryGetStatus<TechnoExt>(pThis, pStatus) && pStatus->CrushLevelReady)
		return pStatus->CrushableLevelCache;

	return CalcCrushableLevel(pThis);
}

namespace
{
	// overlay 类型 -> 该 overlay 段落里配置的 CrushableLevel（-1 = 未配置）
	std::map<const OverlayTypeClass*, int> OverlayCrushableLevels;
}

int Crush::GetOverlayCrushableLevel(OverlayTypeClass* pOverlay)
{
	if (!pOverlay)
		return -1;

	auto it = OverlayCrushableLevels.find(pOverlay);
	if (it != OverlayCrushableLevels.end())
		return it->second;

	int level = -1;
	if (pOverlay->ID && pOverlay->ID[0])
	{
		INIBufferReader* reader = INI::GetSection(INI::Rules, pOverlay->ID);
		level = reader->Get("CrushableLevel", level);
	}

	OverlayCrushableLevels[pOverlay] = level;
	return level;
}

void Crush::ClearOverlayLevelCache()
{
	OverlayCrushableLevels.clear();
}
