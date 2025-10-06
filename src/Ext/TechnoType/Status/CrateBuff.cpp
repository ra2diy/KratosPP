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
		bool shouldUpdateCloakable = CrateBuff.Cloakable;
		bool cloakable = CanICloakByDefault() || CrateBuff.Cloakable;
		// 算上AE加成
		AttachEffect* ae = AEManager();
		if (ae)
		{
			CrateBuffData aeMultiplier = ae->CountAttachStatusMultiplier();
			firepowerMult *= aeMultiplier.FirepowerMultiplier;
			armorMult *= aeMultiplier.ArmorMultiplier;

			// 检查Cloakable
			if (aeMultiplier.Cloakable)
			{
				shouldUpdateCloakable = true;
				cloakable = true;
			}

			speedMult *= aeMultiplier.SpeedMultiplier;
		}

		// 赋予单位
		pTechno->FirepowerMultiplier = firepowerMult;
		pTechno->ArmorMultiplier = armorMult;

		// 更新Cloakable
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
