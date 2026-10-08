#include "../TechnoStatus.h"

#include <Ext/Helper/Scripts.h>

#include <Ext/EffectType/AttachEffectScript.h>
#include <Ext/EffectType/Effect/VampireEffect.h>

void TechnoStatus::OnReceiveDamageEnd_Vampire(int* pRealDamage, WarheadTypeClass* pWH, DamageState damageState, TechnoClass* pAttacker, HouseClass* pAttackingHouse)
{
	AttachEffect* aem = nullptr;
	bool inAir = pTechno->IsInAir();
	HouseClass* pHouse = pTechno->Owner;
	// 自己伤害自己不能吸血
	if (pAttacker != pTechno && !IsDead(pAttacker) && TryGetAEManager<TechnoExt>(pAttacker, aem))
	{
		aem->ForeachChild([&](Component* c) {
			if (auto ae = dynamic_cast<AttachEffectScript*>(c))
			{
				if (ae->AEData.Vampire.Enable && Bingo(ae->AEData.Vampire.Chance))
				{
					// 检查可否影响
					if ((!ae->AEData.Vampire.DeactiveWhenCivilian || !IsCivilian(pAttackingHouse))
						&& (ae->AEData.Vampire.AffectInAir || !inAir)
						&& ae->AEData.Vampire.CanAffectHouse(pAttackingHouse, pHouse)
						&& ae->AEData.Vampire.CanAffectType(pTechno)
						&& ae->AEData.OnMark(this->AEManager()->GetMarks())
						)
					{
						// 吸血
						if (VampireEffect* vampireEffect = ae->GetComponent<VampireEffect>())
						{
							vampireEffect->Trigger();
							int damage = -(int)(*pRealDamage * ae->AEData.Vampire.Percent);
							if (damage != 0)
							{
								pAttacker->TakeDamage(damage, pAttacker->GetTechnoType()->Crewed, true, pAttacker, pAttackingHouse);
							}
						}
					}
				}
			}
			});
	}
}
