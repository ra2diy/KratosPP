#include "AttackBeaconEffect.h"

#include <Ext/Helper/Finder.h>
#include <Ext/Helper/MathEx.h>

#include <Ext/TechnoType/TechnoStatus.h>

bool AttackBeaconEffect::RecruitMission(Mission mission)
{
	switch (mission)
	{
	case Mission::None:
	case Mission::Sleep:
	case Mission::Guard:
	case Mission::Area_Guard:
	case Mission::Stop:
		return true;
	}
	return false;
}

int AttackBeaconEffect::RecruitNumber(std::string typeId, std::map<std::string, int> numbers)
{
	int num = -1;
	auto it = numbers.find(typeId);
	if (it != numbers.end())
	{
		num = it->second;
	}
	return num >= 0 ? num : INT16_MAX;
}

void AttackBeaconEffect::OnUpdate()
{
	if (AE->OwnerIsDead())
	{
		return;
	}
	// 检查电力
	if (Data->Powered && AE->AEManager->PowerOff)
	{
		// 需要电力，但是没电
		return;
	}
	// 检查附着对象的所属是不是平民
	HouseClass* pHouse = nullptr;
	if (pTechno)
	{
		pHouse = pTechno->Owner;
		_isElite = pTechno->Veterancy.IsElite();
	}
	else if (pBullet)
	{
		pHouse = GetSourceHouse(pBullet);
	}
	else
	{
		return;
	}
	if (Data->DeactiveWhenCivilian && IsCivilian(pHouse))
	{
		return;
	}
	AttackBeaconEntity data = GetABData();
	if (data.Enable && Timeup())
	{
		// 可以开刷
		_delay = data.Rate;
		if (_delay > 0)
		{
			_delayTimer.Start(_delay);
		}

		bool noTypeLimit = data.Types.empty();
		CoordStruct location = pObject->GetCoords();

		std::map<std::string, std::map<std::pair<double, int>, TechnoClass*>> candidates;
		FindObject<TechnoClass>(TechnoClass::Array.get(), [&](TechnoClass* pTarget) {
			std::string type = pTarget->GetTechnoType()->ID;
			if ((noTypeLimit || data.Contains(type))
				&& (data.Force ? true : RecruitMission(pTarget->GetCurrentMission())))
			{
				if ((data.Force || !pTarget->Target || pTarget->Target != pObject)
					&& CanAttack(pTarget, pObject))
				{
					double dist = pTarget->GetCoords().DistanceFrom(location);
					candidates[type][{dist, pTarget->UniqueID}] = pTarget;
				}
			}
			return false;
			}, location, data.RangeMax, data.RangeMin, false, pHouse, Data->AffectsOwner, Data->AffectsAllies, Data->AffectsEnemies, Data->AffectsCivilian);

		AbstractClass* pBeacon = pObject;
		if (data.TargetToCell)
		{
			if (CellClass* pCell = MapClass::Instance->TryGetCellAt(location))
			{
				pBeacon = pCell;
			}
		}

		int recruitMax = data.Count > 1 ? data.Count : 99999;
		int recruitCount = 0;
		bool isDone = false;
		std::map<std::string, int> numbers = data.GetNumbers();
		for (auto candidate : candidates)
		{
			std::string type = candidate.first;
			auto technos = candidate.second;
			// check this type is full.
			int typeCount = 0;
			bool isFull = false;
			for (auto targetPair : technos)
			{
				if (isFull)
				{
					break;
				}
				if (!noTypeLimit && ++typeCount > RecruitNumber(type, numbers))
				{
					isFull = true;
					break;
				}
				// recruit limit
				if (++recruitCount > recruitMax)
				{
					isDone = true;
					break;;
				}
				// recruit one
				TechnoClass* pTarget = targetPair.second;
				pTarget->SetTarget(pBeacon);
				if (TechnoStatus* status = GetStatus<TechnoExt, TechnoStatus>(pTarget))
				{
					status->AttackBeaconRecruited = true;
				}
			}
			if (isDone)
			{
				break;
			}
		}
		// 检查触发次数
		if (Data->TriggeredTimes > 0 && ++_count >= Data->TriggeredTimes)
		{
			Deactivate();
			AE->TimeToDie();
		}
	}
}
