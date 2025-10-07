#include "../TechnoStatus.h"

#include <Ext/Common/CommonStatus.h>

#include <Ext/Helper/Scripts.h>

#include <Ext/ObjectType/AttachEffect.h>

void TechnoStatus::RecalculateStatus()
{
	if (!IsDead(pTechno))
	{
		// 获取箱子加成
		double firepowerMult = CrateBuff.FirepowerMultiplier;
		double armorMult = CrateBuff.ArmorMultiplier;
		double speedMult = CrateBuff.SpeedMultiplier;

		// 隐身状态获取
		bool cloakable = InnateCloakable;
		bool shouldUpdateCloakable = false;
		bool forceDecloak = CrateBuff.ForceDecloak;
		bool cloakableBuff = CrateBuff.Cloakable;

		// 算上AE加成
		AttachEffect* ae = AEManager();
		if (ae)
		{
			CrateBuffData aeMultiplier = ae->CountAttachStatusMultiplier();
			firepowerMult *= aeMultiplier.FirepowerMultiplier;
			armorMult *= aeMultiplier.ArmorMultiplier;

			cloakableBuff |= aeMultiplier.Cloakable;
			forceDecloak |= aeMultiplier.ForceDecloak;

			speedMult *= aeMultiplier.SpeedMultiplier;
		}

		// 隐身和破隐效果及更新隐身属性
		if (ForceDecloakActive != forceDecloak)
		{
			ForceDecloakActive = forceDecloak;

			if (ForceDecloakActive && pTechno->CloakState == CloakState::Cloaked)
			{
				pTechno->Uncloak(true);
			}
			shouldUpdateCloakable = true;
		}

		if (ExternalCloakable != cloakableBuff)
		{
			ExternalCloakable = cloakableBuff;
			shouldUpdateCloakable = true;
		}

		// 计算是否隐身，先判破隐，无破隐才看AE隐身，也没有再看单位原本属性
		if (ForceDecloakActive)
		{
			cloakable = false;
		}
		else if (ExternalCloakable)
		{
			cloakable = true;
		}
		else
		{
			cloakable = InnateCloakable;
		}

		// 赋予单位
		pTechno->FirepowerMultiplier = firepowerMult;
		pTechno->ArmorMultiplier = armorMult;

		// 需要时更新隐身属性
		if (shouldUpdateCloakable)
		{
			pTechno->Cloakable = cloakable;
		}

		FootClass* pFoot = nullptr;
		if (CastToFoot(pTechno, pFoot))
		{
			pFoot->SpeedMultiplier = speedMult;
		}
	}
}

bool TechnoStatus::CanICloakByDefault()
{
	return pTechno && pTechno->GetTechnoType() && (pTechno->GetTechnoType()->Cloakable || pTechno->HasAbility(Ability::Cloak));
}
