#include "AmmoEffect.h"

#include <Ext/Helper/Finder.h>
#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>

#include <Extension/WarheadTypeExt.h>

int AmmoEffect::GetMaxAmmo()
{
	TechnoClass* pTechno = abstract_cast<TechnoClass*, true>(pObject);
	if (pTechno)
	{
		return pTechno->GetTechnoType()->Ammo;
	}
	return 0;
}

void AmmoEffect::ClampAmmo()
{
	TechnoClass* pTechno = abstract_cast<TechnoClass*, true>(pObject);
	if (pTechno)
	{
		int maxAmmo = pTechno->GetTechnoType()->Ammo;
		if (maxAmmo > 0)
		{
			if (pTechno->Ammo < 0) pTechno->Ammo = 0;
			if (pTechno->Ammo > maxAmmo) pTechno->Ammo = maxAmmo;
		}
	}
}

void AmmoEffect::Watch()
{
	TechnoClass* pTechno = abstract_cast<TechnoClass*, true>(pObject);
	if (!pTechno) return;

	for (auto& [idx, entity] : Data->RemoveWhenNums)
	{
		if (entity.RemoveWhenNum.Y >= entity.RemoveWhenNum.X)
		{
			if (pTechno->Ammo >= entity.RemoveWhenNum.X && (entity.RemoveWhenNum.Y < 0 || pTechno->Ammo <= entity.RemoveWhenNum.Y))
			{
				Deactivate();
				AE->TimeToDie();
				return;
			}
		}
	}
}

int AmmoEffect::CalculateRemainingDamage(int Damage)
{
	if (Damage <= 0) return 0;

	if (Data->Reaction.Protect <= 0.0 || Data->Reaction.Percent <= 0.0)
		return Damage;

	TechnoClass* pTechno = abstract_cast<TechnoClass*, true>(pObject);
	if (!pTechno) return Damage;

	int damageToProtect = Damage;
	if (Data->Reaction.Limit > 0 && damageToProtect > Data->Reaction.Limit)
	{
		damageToProtect = Data->Reaction.Limit;
	}

	double protectedPart = damageToProtect * Data->Reaction.Protect;
	double requiredCount = protectedPart * Data->Reaction.Percent;

	// 向上取整，至少消耗 1
	int actualUsed = std::max(1, static_cast<int>(std::ceil(requiredCount)));
	// 不超过当前弹药
	actualUsed = std::min(actualUsed, pTechno->Ammo);

	// 直接扣除弹药
	pTechno->Ammo -= actualUsed;
	ClampAmmo();

	// 实际抵扣的伤害
	double actualProtectedDamage = actualUsed / Data->Reaction.Percent;
	int intProtectedDamage = static_cast<int>(std::round(actualProtectedDamage));

	return std::max(0, Damage - intProtectedDamage);
}

void AmmoEffect::OnStart()
{
	// 无弹药单位不允许附着
	int maxAmmo = GetMaxAmmo();
	if (maxAmmo <= 0)
	{
		Deactivate();
		return;
	}
	// 附着时按 Action 直接操作当前弹药，再钳位到 [0, 单位最大弹药]
	ModifyAmmo(Data->Action, GetModifyNum());
}

void AmmoEffect::OnPause()
{
}

void AmmoEffect::OnRecover()
{
}

void AmmoEffect::OnUpdate()
{
	if (!AE->OwnerIsDead())
	{
		Watch();
	}
}

void AmmoEffect::OnWarpUpdate()
{
	if (!AE->OwnerIsDead())
	{
		Watch();
	}
}

void AmmoEffect::OnReceiveDamageReal(int* pRealDamage, WarheadTypeClass* pWH, TechnoClass* pAttacker, HouseClass* pAttackingHouse)
{
	if (AE->OwnerIsDead())
	{
		return;
	}

	if (Data->ReactionMode != AmmoReaction::NORMAL)
	{
		WarheadTypeExt::TypeData* whData = GetTypeData<WarheadTypeExt, WarheadTypeExt::TypeData>(pWH);
		if (!whData->IgnoreCounterReaction && Data->Reaction.WarheadOnMark(pWH->ID))
		{
			int damage = CalculateRemainingDamage(*pRealDamage);

			if (Data->ReactionMode == AmmoReaction::SHIELD)
			{
				*pRealDamage = damage;
			}
			// HITPOINT: 弹药已扣除，伤害照常传递
		}
	}
}

void AmmoEffect::ModifyAmmo(AmmoAction action, double num)
{
	TechnoClass* pTechno = abstract_cast<TechnoClass*, true>(pObject);
	if (!pTechno) return;

	int maxAmmo = pTechno->GetTechnoType()->Ammo;
	if (maxAmmo <= 0) return;

	int intNum = static_cast<int>(num);

	switch (action)
	{
	case AmmoAction::INIT:
		pTechno->Ammo = intNum;
		break;
	case AmmoAction::ADD:
		pTechno->Ammo += intNum;
		break;
	case AmmoAction::SUB:
		pTechno->Ammo -= intNum;
		break;
	case AmmoAction::MUL:
		pTechno->Ammo = static_cast<int>(pTechno->Ammo * num);
		break;
	}

	ClampAmmo();
}

double AmmoEffect::GetModifyNum()
{
	// 解析本次操作的弹药量：普通数字直接使用，特殊值从指定来源（默认自身）读取
	double num = Data->Num;
	if (Data->NumType != AmmoType::Number)
	{
		ObjectClass* pFrom = Data->NumFromSource ? AE->pSource : pObject;
		if (pFrom && !IsDeadOrInvisible(pFrom))
		{
			switch (Data->NumType)
			{
			case AmmoType::HP:
				num = pFrom->Health;
				break;
			case AmmoType::MaxHP:
				if (Data->NumFromSource)
				{
					num = pFrom->GetType()->Strength;
				}
				else
				{
					num = IsBullet() ? pFrom->Health : pFrom->GetType()->Strength;
				}
				break;
			case AmmoType::Ammo:
			{
				TechnoClass* pFromTech = abstract_cast<TechnoClass*, true>(pFrom);
				if (pFromTech) num = pFromTech->Ammo;
				break;
			}
			case AmmoType::MaxAmmo:
			{
				TechnoClass* pFromTech = abstract_cast<TechnoClass*, true>(pFrom);
				if (pFromTech) num = pFromTech->GetTechnoType()->Ammo;
				break;
			}
			}
		}
	}
	return num;
}
