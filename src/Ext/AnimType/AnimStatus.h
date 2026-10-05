#pragma once

#include <string>

#include <Extension.h>
#include <TechnoClass.h>

#include <Utilities/Macro.h>
#include <Utilities/Debug.h>

#include <Common/Components/ScriptComponent.h>
#include <Extension/AnimExt.h>

#include <Ext/EffectType/Effect/FireSuperData.h>
#include <Ext/EffectType/Effect/OffsetData.h>
#include <Ext/StateType/State/PaintballData.h>
#include "AnimDamageData.h"
#include "RelationData.h"
#include "SpawnAnimsData.h"

class AnimStatus : public AnimScript
{
public:
	ANIM_SCRIPT(AnimStatus);

	bool TryGetCreater(TechnoClass*& pTechno);

	void AttachToObject(ObjectClass* pObject, OffsetData data = {});
	void SetOffset(OffsetData data);

	void UpdateVisibility(Relation visibility);

	/**
	 *@brief 接管伤害制造
	 * @param isBounce 是否流星，碎片
	 * @param bright 弹头闪光
	*/
	void Explosion_Damage(bool isBounce = false, bool bright = false);

	/**
	 *@brief 染色
	 *
	 * @param R
	 */
	void DrawSHP_Paintball(REGISTERS* R);

	void OnTechnoDelete(EventSystem* sender, Event e, void* args)
	{
		if (args == pCreater)
		{
			_createrIsDeadth = true;
			pCreater = nullptr;
		}
		if (args == pAttachOwner)
		{
			pAttachOwner = nullptr;
		}
	}

	virtual void Clean() override
	{
		AnimScript::Clean();

		pCreater = nullptr;
		pAttachOwner = nullptr; // 动画附着的对象
		Offset = CoordStruct::Empty; // 附着的偏移位置
		_attachLayerCached = -2;

		_offsetData = {};

		_initDamageDelayFlag = false;
		_damageDelayTimer = {};

		_createrIsDeadth = false;

		_initInvisibleFlag = false;
		_playSuperFlag = false;

		_initSpawnFlag = false;
		_spawnInitDelayTimer = {};
		_spawnDelayTimer = {};
		_spawnCount = 0;

		_animDamageData = nullptr;
		_paintballData = nullptr;
		_playSuperData = nullptr;
		_spawnAnimsData = nullptr;
	}

	virtual void Awake() override
	{
		EventSystems::General.AddHandler(Events::ObjectUnInitEvent, this, &AnimStatus::OnTechnoDelete);

		InitExt();
	}

	virtual void Destroy() override
	{
		EventSystems::General.RemoveHandler(Events::ObjectUnInitEvent, this, &AnimStatus::OnTechnoDelete);
	}

	virtual void OnUpdate() override;

	virtual void OnLoop() override;

	virtual void OnDone() override;

	virtual void OnNext(AnimTypeClass* pNext) override;

	TechnoClass* pCreater = nullptr;
	ObjectClass* pAttachOwner = nullptr; // 动画附着的对象
	CoordStruct Offset = CoordStruct::Empty; // 附着的偏移位置

	// 附着对象层索引（ObjectClass::LastLayer，偏移 +0x94）的缓存，用于检测主身换层（-2 = 未初始化）。
	// 仅渲染排序用途，不参与存档；换存档后首帧多 Submit 一次，无副作用。
	int _attachLayerCached = -2;

#pragma region Save/Load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->pCreater)
			.Process(this->pAttachOwner)
			.Process(this->Offset)

			.Process(this->_initDamageDelayFlag)
			.Process(this->_damageDelayTimer)
			.Process(this->_createrIsDeadth)

			.Process(this->_offsetData)
			.Process(this->_initInvisibleFlag)
			.Process(this->_playSuperFlag)

			.Process(this->_initSpawnFlag)
			.Process(this->_spawnInitDelayTimer)
			.Process(this->_spawnDelayTimer)
			.Process(this->_spawnCount)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		Component::Load(stream, registerForChange);
		EventSystems::General.AddHandler(Events::ObjectUnInitEvent, this, &AnimStatus::OnTechnoDelete);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		Component::Save(stream);
		return const_cast<AnimStatus*>(this)->Serialize(stream);
	}
#pragma endregion

private:
	bool GetInvisible(Relation visibility);

	void ResetLoopSpawn();

	void InitExt();

	void OnUpdate_Visibility();
	void OnUpdate_Damage();
	void OnUpdate_SpawnAnims();
	void OnUpdate_PlaySuper();

	void OnLoop_SpawnAnims();
	void OnLoop_PlaySuper();

	void OnDone_SpawnAnims();
	void OnDone_PlaySuper();

	void OnNext_SpawnAnims(AnimTypeClass* pNext);

	OffsetData _offsetData{};

	bool _initDamageDelayFlag = false;
	CDTimerClass _damageDelayTimer{};

	bool _createrIsDeadth = false;

	bool _initInvisibleFlag = false;
	bool _playSuperFlag = false;

	bool _initSpawnFlag = false;
	CDTimerClass _spawnInitDelayTimer{};
	CDTimerClass _spawnDelayTimer{};
	int _spawnCount = 0;

	AnimDamageData* _animDamageData = nullptr;
	AnimDamageData* GetAnimDamageData();

	PaintballData* _paintballData = nullptr;
	PaintballData* GetPaintballData();

	PlaySuperData* _playSuperData = nullptr;
	PlaySuperData* GetPlaySuperData();

	SpawnAnimsData* _spawnAnimsData = nullptr;
	SpawnAnimsData* GetSpawnAnimsData();
};
