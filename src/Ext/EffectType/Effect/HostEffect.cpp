#include "HostEffect.h"

#include <Ext/Helper/MathEx.h>
#include <Ext/Helper/Gift.h>

void HostEffect::OnStart()
{
	// Dynamic 模式：首次执行时自动填入被附加对象的类型名
	if ((Data->Data.Dynamic || Data->Data.DynamicFromSource) && !_dynamicFilled)
	{
		_dynamicFilled = true;
		std::vector<std::string> types;
		if (Data->Data.DynamicFromSource)
		{
			if (TechnoClass* pSourceTechno = abstract_cast<TechnoClass*>(AE->pSource))
			{
				types.push_back(pSourceTechno->GetTechnoType()->ID);
			}
		}
		if (Data->Data.Dynamic)
		{
			if (pTechno)
			{
				types.push_back(pTechno->GetTechnoType()->ID);
			}
			else if (pBullet && pBullet->Type)
			{
				types.push_back(pBullet->Type->ID);
			}
		}
		if (!types.empty())
		{
			std::sort(types.begin(), types.end());
			types.erase(std::unique(types.begin(), types.end()), types.end());
			Data->Data.Gifts = types;
			Data->EliteData.Gifts = Data->Data.Gifts;
		}
	}
}

void HostEffect::OnUpdate()
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
	GiftBoxEntity data = GetGiftData();
	if (data.Enable && Timeup())
	{
		// 可以开刷
		_delay = GetRandomValue(data.RandomDelay, data.Delay);
		if (_delay > 0)
		{
			_delayTimer.Start(_delay);
		}

		std::vector<std::string> gifts = GetGiftList(data);
		if (!gifts.empty())
		{
			pHouse = AE->pSourceHouse;

			// 记录盒子的状态
			bool isSelected = false;
			int group = -1;
			DirStruct bodyDir{};
			DirStruct turretDir{};

			AbstractClass* pTarget = nullptr;
			AbstractClass* pDest = nullptr;
			AbstractClass* pFocus = nullptr;

			Mission currentMission = pHouse->IsControlledByHuman() ? Mission::Guard : Mission::Hunt;

			// 继承
			bool changeHealth = false;
			int healthNumber = 0;
			double healthPercent = 0;
			bool inheritExperience = false;
			VeterancyStruct veterancy;

			if (pTechno)
			{
				if (AE->AEData.ReceiverOwn)
				{
					pHouse = pTechno->Owner;
				}
				isSelected = pTechno->IsSelected;
				group = pTechno->Group;
				bodyDir = pTechno->PrimaryFacing.Current();
				turretDir = pTechno->SecondaryFacing.Current();

				currentMission = pTechno->GetCurrentMission();
				pTarget = pTechno->Target;
				if (!IsBuilding())
				{
					pDest = abstract_cast<FootClass*, true>(pTechno)->Destination;
					pFocus = pTechno->Focus;
				}
				// 继承血量
				healthNumber = pTechno->Health;
				healthPercent = pTechno->GetHealthPercentage();
				healthPercent = healthPercent <= 0 ? 1 : healthPercent; // 盒子死了，继承的血量就是满的
				changeHealth = Data->IsTransform || Data->InheritHealth; // Transform强制继承
				if (!changeHealth && Data->HealthPercent > 0)
				{
					// 强设血量比例
					healthPercent = Data->HealthPercent;
					changeHealth = true;
				}
				// 继承等级
				if (Data->InheritExperience && pTechno->GetTechnoType()->Trainable)
				{
					inheritExperience = true;
					veterancy = pTechno->Veterancy;
				}
			}
			if (pBullet)
			{
				if (AE->AEData.ReceiverOwn)
				{
					pHouse = GetSourceHouse(pBullet);
				}
				if (TechnoClass* pBulletOwner = pBullet->Owner)
				{
					isSelected = pBulletOwner->IsSelected;
					group = pBulletOwner->Group;
				}
				bodyDir = Facing(pBullet);
				turretDir = bodyDir;
				pTarget = pBullet->Target;
				if (pTarget)
				{
					currentMission = Mission::Attack;
					CoordStruct targetLocation = pBullet->TargetCoords;
					if (CellClass* pCell = MapClass::Instance->TryGetCellAt(targetLocation))
					{
						pFocus = pCell;
					}
				}
			}

			BoxStateCache boxState = GiftBoxStateCache(*Data);
			boxState.BodyDir = bodyDir;
			boxState.TurretDir = turretDir;
			boxState.Group = group;
			boxState.IsSelected = isSelected;

			boxState.Location = pObject->GetCoords();
			boxState.pTarget = pTarget;
			boxState.pDest = pDest;
			boxState.pFocus = pFocus;
			boxState.Scatter = !Data->Remove || Data->ForceMission == Mission::Move; // 分散行动

			boxState.pOwner = pObject;
			boxState.pHouse = pHouse;

			// 开刷
			ReleaseGifts(gifts, data, boxState,
				[&](TechnoClass* pGift, TechnoStatus* pGiftStatus, AttachEffect* pGiftAEM)
				{
					TechnoTypeClass* pGiftType = pGift->GetTechnoType();
					// 修改血量
					if (changeHealth)
					{
						int strength = pGiftType->Strength;
						int health = 0;
						if (Data->InheritHealthNumber)
						{
							// 直接继承血量数字
							health = healthNumber;
						}
						else if (Data->HealthNumber > 0)
						{
							// 直接赋予指定血量
							health = Data->HealthNumber;
						}
						else
						{
							// 按比例计算血量
							health = (int)(strength * healthPercent);
						}
						if (health <= 0)
						{
							health = 1;
						}
						if (health < strength)
						{
							pGift->Health = health;
						}
					}
					// 继承等级
					if (inheritExperience && pGiftType->Trainable)
					{
						pGift->Veterancy = veterancy;
					}
				});

			// 检查触发次数
			if (Data->TriggeredTimes > 0 && ++_count >= Data->TriggeredTimes)
			{
				Deactivate();
				AE->TimeToDie();
			}
		}
	}
}

