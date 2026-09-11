#pragma once

#include <string>
#include <vector>

#include <GeneralDefinitions.h>
#include <AnimClass.h>

#include "../EffectScript.h"
#include "AmmoData.h"

class AmmoEffect : public EffectScript
{
public:
	EFFECT_SCRIPT(Ammo);

	virtual void Clean() override
	{
		EffectScript::Clean();
	}

	virtual void OnStart() override;
	virtual void OnPause() override;
	virtual void OnRecover() override;

	virtual void OnUpdate() override;
	virtual void OnWarpUpdate() override;

	virtual void OnReceiveDamageReal(int* pRealDamage, WarheadTypeClass* pWH, TechnoClass* pAttacker, HouseClass* pAttackingHouse) override;

	void ModifyAmmo(AmmoAction action, double num);
	double GetModifyNum();
	void ClampAmmo();

#pragma region Save/Load
	template <typename T>
	bool Serialize(T& stream) {
		return stream.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		EffectScript::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		EffectScript::Save(stream);
		return const_cast<AmmoEffect*>(this)->Serialize(stream);
	}
#pragma endregion
private:
	void Watch();

	int CalculateRemainingDamage(int Damage);

	int GetMaxAmmo();
};
