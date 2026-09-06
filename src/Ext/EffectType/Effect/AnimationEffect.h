#pragma once

#include <string>
#include <vector>

#include <GeneralDefinitions.h>
#include <AnimClass.h>

#include "../EffectScript.h"
#include "AnimationData.h"


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
class AnimationEffect : public EffectScript
{
public:
	EFFECT_SCRIPT(Animation);

	void UpdateLocationOffset(CoordStruct offset);
	void OnAnimUnInit(EventSystem* sender, Event e, void* args);

	virtual void Awake() override;
	virtual void Destroy() override;

	virtual void Clean() override
	{
		EffectScript::Clean();

		pIdleAnim = nullptr;
		animFlags = BlitterFlags::None;
		ownerIsCloak = false;
	}

	virtual void OnStart() override;
	virtual void End(CoordStruct location) override;

	virtual void OnRecover() override;
	virtual void OnPause() override;

	virtual void ExtChanged() override;

	virtual void OnPut(CoordStruct* pCoord, DirType faceDir) override;
	virtual void OnUpdate() override;
	virtual void OnRemove() override;
	virtual void OnReceiveDamage(args_ReceiveDamage* args) override;

#pragma region Save/Load
	template <typename T>
	bool Serialize(T& stream) {
		return stream
			.Process(this->pIdleAnim)
			.Process(this->animFlags)
			.Process(this->ownerIsCloak)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		EffectScript::Load(stream, registerForChange);
		EventSystems::General.AddHandler(Events::ObjectUnInitEvent, this, &AnimationEffect::OnAnimUnInit);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		EffectScript::Save(stream);
		return const_cast<AnimationEffect*>(this)->Serialize(stream);
	}
#pragma endregion
private:
	void CreateIdleAnim(bool force = false, CoordStruct location = CoordStruct::Empty);

	void KillIdleAnim();

	AnimClass* pIdleAnim = nullptr;
	BlitterFlags animFlags = BlitterFlags::None;
	bool ownerIsCloak = false;
};
