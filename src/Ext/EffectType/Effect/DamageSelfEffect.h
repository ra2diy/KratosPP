#pragma once

#include <string>
#include <vector>

#include <GeneralDefinitions.h>
#include <AnimClass.h>

#include <Ext/BulletType/BulletStatus.h>

#include "../EffectScript.h"
#include "DamageSelfData.h"


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
class DamageSelfEffect : public EffectScript
{
public:
	EFFECT_SCRIPT(DamageSelf);

	virtual void Clean() override
	{
		EffectScript::Clean();

		_damage = 0; // 计算完fireMult后的伤害值
		_bulletDamage = { 1 };

		_pWH = nullptr;
		_isToyWarhead = false;

		_count = 0;
		_delayTimer = {};
	}

	virtual void OnStart() override;

	virtual void OnUpdate() override;

#pragma region Save/Load
	template <typename T>
	bool Serialize(T& stream) {
		return stream
			.Process(this->_damage)
			.Process(this->_bulletDamage)
			.Process(this->_pWH)
			.Process(this->_isToyWarhead)

			.Process(this->_count)
			.Process(this->_delayTimer)
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
		return const_cast<DamageSelfEffect*>(this)->Serialize(stream);
	}
#pragma endregion
private:
	int _damage = 0; // 计算完fireMult后的伤害值
	BulletDamage _bulletDamage{ 1 };

	WarheadTypeClass* _pWH = nullptr;
	bool _isToyWarhead = false;

	int _count = 0;
	CDTimerClass _delayTimer{};
};
