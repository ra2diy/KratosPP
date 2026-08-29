#include "BroadcastEffect.h"

#include <Ext/Helper/Finder.h>
#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>

void BroadcastEffect::FindAndAttach(BroadcastEntity data, std::vector<std::string>& types, std::vector<double>& chances, HouseClass* pHouse, bool getMode)
{
	if (types.empty())
	{
		return;
	}
	CoordStruct location = pObject->GetCoords();
	ObjectClass* pSource = AE->pSource;
	HouseClass* pSourceHouse = AE->pSourceHouse;
	CoordStruct projectileLocation{};
	if (AEData.ReceiverOwn || !pSource || !pSourceHouse)
	{
		pSource = pObject;
		pSourceHouse = pHouse;
	}
	if (pBullet)
	{
		pSource = pBullet->Owner;
		pSourceHouse = pHouse;
		projectileLocation = pObject->GetCoords();
	}
	// Get模式：范围内单位给广播塔（self）贴AE
	AttachEffect* selfAEM = nullptr;
	if (getMode)
	{
		selfAEM = AE->AEManager;
	}
	// 搜索单位
	if (Data->AffectTechno)
	{
		int attachCount = 0;
		FindTechnoOnMark([&](TechnoClass* pTarget, AttachEffect* aeManager)
			{
				if (getMode)
				{
					selfAEM->Attach(types, chances, false, pTarget, pTarget->Owner, projectileLocation);
				}
				else
				{
					aeManager->Attach(types, chances, false, pSource, pSourceHouse, projectileLocation);
				}
				attachCount++;
				if (Data->MaxAttachTechno > 0 && attachCount >= Data->MaxAttachTechno)
				{
					return true;
				}
				return false;
			}, location, data.RangeMax, data.RangeMin, data.FullAirspace, pSourceHouse, *Data, pObject);
	}
	// 搜索抛射体
	if (Data->AffectBullet)
	{
		int attachCount = 0;
		FindBulletOnMark([&](BulletClass* pTarget, AttachEffect* aeManager)
			{
				if (getMode)
				{
					HouseClass* pTargetHouse = GetSourceHouse(pTarget);
					selfAEM->Attach(types, chances, false, pTarget, pTargetHouse, projectileLocation);
				}
				else
				{
					aeManager->Attach(types, chances, false, pSource, pSourceHouse, projectileLocation);
				}
				attachCount++;
				if (Data->MaxAttachBullet > 0 && attachCount >= Data->MaxAttachBullet)
				{
					return true;
				}
				return false;
			}, location, data.RangeMax, data.RangeMin, data.FullAirspace, pSourceHouse, *Data, pObject);
	}
}

void BroadcastEffect::OnUpdate()
{
	if (!AE->OwnerIsDead())
	{
		if (Data->Powered && AE->AEManager->PowerOff)
		{
			// 需要电力，但是没电
			return;
		}
		BroadcastEntity data = Data->Data;
		HouseClass* pHouse = nullptr;
		if (pTechno)
		{
			pHouse = pTechno->Owner;
			if (pTechno->Veterancy.IsElite())
			{
				data = Data->EliteData;
			}
		}
		else if (pBullet)
		{
			pHouse = GetSourceHouse(pBullet);
		}
		else
		{
			return;
		}

		// 检查平民
		if (Data->DeactiveWhenCivilian && IsCivilian(pHouse))
		{
			return;
		}
		if (data.Enable)
		{
			if (_delayTimer.Expired())
			{
				// 检查次数
				if (Data->TriggeredTimes > 0 && ++_count >= Data->TriggeredTimes)
				{
					// 结束效果器
					Deactivate();
					AE->TimeToDie();
				}
				_delayTimer.Start(data.Rate);
				FindAndAttach(data, data.Types, data.AttachChances, pHouse);
				FindAndAttach(data, data.GetTypes, data.GetChances, pHouse, true);
			}
		}
	}
}
