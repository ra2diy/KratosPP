#pragma once

#include <string>
#include <vector>
#include <map>

#include <GeneralDefinitions.h>
#include <WeaponTypeClass.h>

#include <Extension/WeaponTypeExt.h>

#include <Ext/BulletType/BulletStatus.h>

#include "../EffectScript.h"
#include "AutoWeaponData.h"


/// @brief EffectScript
/// GameObject
///		|__ AttachEffect
///				|__ AttachEffectScript#0
///						|__ EffectScript#0
///						|__ EffectScript#1
///				|__ AttachEffectScript#1
///						|__ EffectScript#0
///						|__ EffectScript#1
///						|__ EffectScript#2
class AutoWeaponEffect : public EffectScript
{
public:
	EFFECT_SCRIPT(AutoWeapon);

	static void SetupFakeTargetToBullet(int index, int burst, BulletClass*& pBullet, AbstractClass*& pTarget);
	static void SetupBulletAtProjectile(int index, int burst, BulletClass*& pBullet, AbstractClass*& pTarget);

	virtual void Clean() override
	{
		EffectScript::Clean();

		_rof.clear();
	}

	virtual void End(CoordStruct location) override;

	virtual void OnUpdate() override;

	virtual void OnRemove() override;

	virtual void OnReceiveDamageDestroy() override;

	virtual void OnDetonate(CoordStruct* pCoords, bool& skip) override;

#pragma region Save/Load
	template <typename T>
	bool Serialize(T& stream) {
		return stream
			.Process(this->_rof)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		EffectScript::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		EffectScript::Save(stream);
		return const_cast<AutoWeaponEffect*>(this)->Serialize(stream);
	}
#pragma endregion
private:
	bool TryGetShooterAndTarget(TechnoClass* pReceiverOwner, HouseClass* pReceiverHouse, AbstractClass* pReceiverTarget,
		ObjectClass*& pShooter, TechnoClass*& pAttacker, HouseClass*& pAttackingHouse, AbstractClass*& pTarget,
		bool& dontMakeFakeTarget);

	ObjectClass* MakeFakeTarget(HouseClass* pHouse, ObjectClass* pShooter, CoordStruct fireFLH, CoordStruct targetFLH);

	bool CheckROF(WeaponTypeClass* pWeapon, WeaponTypeExt::TypeData* weaponData);

	void ResetROF(WeaponTypeClass* pWeapon, WeaponTypeExt::TypeData* weaponData,double rofMultip);

	std::map<std::string, CDTimerClass> _rof{};
};
