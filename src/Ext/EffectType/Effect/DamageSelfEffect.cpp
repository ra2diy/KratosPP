#include "DamageSelfEffect.h"

#include <Ext/Helper/Finder.h>
#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>

#include <Extension/BulletExt.h>
#include <Extension/TechnoExt.h>

#include <Ext/BulletType/BulletStatus.h>
#include <Ext/TechnoType/TechnoStatus.h>

void DamageSelfEffect::OnStart()
{
	// 排除附着平民抛射体
	if (pBullet)
	{
		if (Data->DeactiveWhenCivilian && IsCivilian(AE->pSourceHouse))
		{
			Deactivate();
			AE->TimeToDie();
			return;
		}
	}
	// 计算伤害
	double fireMultip = 1.0;
	if (Data->FirepowerMultiplier)
	{
		fireMultip = GetDamageMulti(AE->pSource);
	}
	_damage = (int)(Data->Damage * fireMultip);
	_pWH = RulesClass::Instance()->C4Warhead;
	if (IsNotNone(Data->Warhead))
	{
		if (WarheadTypeClass* pWH = WarheadTypeClass::Find(Data->Warhead.c_str()))
		{
			_pWH = pWH;
		}
	}
	_bulletDamage.Damage = _damage;
	_bulletDamage.Eliminate = false;
	_bulletDamage.Harmless = false;
}

void DamageSelfEffect::OnUpdate()
{
	if (!AE->OwnerIsDead())
	{
		if (Data->Powered && AE->AEManager->PowerOff)
		{
			// 需要电力，但是没电
			return;
		}
		// 排除平民
		HouseClass* pHouse = nullptr;
		if (pTechno)
		{
			pHouse = pTechno->Owner;
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
		if (_delayTimer.Expired())
		{
			_delayTimer.Start(Data->ROF);

			CoordStruct location = pObject->GetCoords();
			// 开始制造伤害
			if (pTechno)
			{
				int realDamage = _damage;
				if (Data->Peaceful)
				{
					// 静默伤害，需要计算实际伤害数值
					realDamage = GetRealDamage(pTechno->GetTechnoType()->Armor, realDamage, _pWH, Data->IgnoreArmor);
					// 本次伤害足够击杀目标，直接将目标移除
					if (realDamage >= pTechno->Health)
					{
						TechnoStatus* status = nullptr;
						if (TryGetStatus<TechnoExt>(pTechno, status))
						{
							status->DestroySelf->DestroyNow(true);
							Deactivate();
							AE->TimeToDie();
							return;
						}
					}
				}
				// 正常伤害
				ObjectClass* pDamageMaker = nullptr;
				if (AE->pSource && AE->pSource != pTechno)
				{
					pDamageMaker = AE->pSource;
				}
				if (realDamage < 0 || pTechno->CloakState == CloakState::Uncloaked || Data->Decloak)
				{
					// 维修或者显形状态，直接炸
					pTechno->ReceiveDamage(&realDamage, 0, _pWH, pDamageMaker, Data->IgnoreArmor, pTechno->GetTechnoType()->Crewed, AE->pSourceHouse);
				}
				else
				{
					// 不显形不能使用ReceiveDamage，改用扣血
					if (!Data->Peaceful)
					{
						// 非静默伤害，表示之前没有计算过实际伤害，重新计算
						realDamage = GetRealDamage(pTechno->GetTechnoType()->Armor, realDamage, _pWH, Data->IgnoreArmor);
					}
					// 扣血
					if (realDamage >= pTechno->Health)
					{
						// 本次伤害足够打死目标
						pTechno->ReceiveDamage(&realDamage, 0, _pWH, pDamageMaker, true, pTechno->GetTechnoType()->Crewed, AE->pSourceHouse);
					}
					else
					{
						// 血量可以减到负数不死
						pTechno->Health -= realDamage;
					}
				}
				// 播放弹头动画
				if (Data->WarheadAnim)
				{
					if (AnimClass* pAnim = PlayWarheadAnim(_pWH, location, realDamage))
					{
						pAnim->Owner = AE->pSourceHouse;
					}
				}
			}
			else if (pBullet)
			{
				BulletStatus* status = nullptr;
				if (TryGetStatus<BulletExt>(pBullet, status))
				{
					status->TakeDamage(_bulletDamage);

					// 播放弹头动画
					if (Data->WarheadAnim)
					{
						if (AnimClass* pAnim = PlayWarheadAnim(_pWH, location, _bulletDamage.Damage))
						{
							pAnim->Owner = AE->pSourceHouse;
						}
					}
				}
			}
			else
			{
				Deactivate();
				AE->TimeToDie();
				return;
			}
			// 检查触发次数
			if (Data->TriggeredTimes > 0 && ++_count >= Data->TriggeredTimes)
			{
				Deactivate();
				AE->TimeToDie();
			}
		}
		else
		{
			Debug::Log("DamageSelfEffect: waiting for ROF, Owner = %d, time left = %d\n", AE->pTechno, _delayTimer.GetTimeLeft());
		}
	}
}
