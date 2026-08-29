#pragma once

#include <string>
#include <vector>

#include <GeneralDefinitions.h>
#include <AnimClass.h>

#include "../EffectScript.h"
#include "BroadcastData.h"


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
class BroadcastEffect : public EffectScript
{
public:
	EFFECT_SCRIPT(Broadcast);

	virtual void Clean() override
	{
		EffectScript::Clean();

		_count = 0;
		_delayTimer = {};
	}

	virtual void OnUpdate() override;

#pragma region Save/Load
	template <typename T>
	bool Serialize(T& stream) {
		return stream
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
		return const_cast<BroadcastEffect*>(this)->Serialize(stream);
	}
#pragma endregion
private:
	void FindAndAttach(BroadcastEntity data, std::vector<std::string>& types, std::vector<double>& chances, HouseClass* pHouse, bool getMode = false);

	int _count = 0;
	CDTimerClass _delayTimer{};
};
