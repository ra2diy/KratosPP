#pragma once

#include <string>
#include <vector>
#include <map>
#include <type_traits>

#include <GeneralDefinitions.h>
#include <TechnoClass.h>
#include <HouseClass.h>

#include <Utilities/Debug.h>

#include <Common/Components/ScriptComponent.h>
#include <Common/EventSystems/EventSystem.h>

#include <Ext/Helper/MathEx.h>

#include <Ext/ObjectType/AttachEffect.h>
#include "AttachEffectData.h"

class IAEScript
{
public:
	virtual void Start() {};
	virtual void OnStart() {};
	virtual void End(CoordStruct location) {};
	virtual void ResetDuration() {};

	virtual void Pause() {};
	virtual void OnPause() {};
	virtual void Recover() {};
	virtual void OnRecover() {};

	virtual bool IsAlive() { return true; };

	/**
	 *@brief 渲染事件开始
	 *
	 * @param location 定位
	 */
	virtual void OnGScreenRender(CoordStruct location) {};
	/**
	 *@brief 渲染事件结束
	 *
	 * @param location 定位
	 */
	virtual void OnGScreenRenderEnd(CoordStruct location) {};
};

/// @brief AE组件
/// GameObject
///		|__ AttachEffect
///				|__ AttachEffectScript#0
///						|__ EffectScript#0
///						|__ EffectScript#1
///				|__ AttachEffectScript#1
///						|__ EffectScript#0
///						|__ EffectScript#1
///						|__ EffectScript#2
class AttachEffectScript : public ObjectScript, public IAEScript
{
public:
	OBJECT_SCRIPT(AttachEffectScript);

	virtual void ResetDuration() override;
	int GetDuration();
	bool TryGetInitDelayTimeLeft(int& timeLeft);
	bool TryGetDurationTimeLeft(int& timeLeft);
	void MergeDuration(int otherDuration);
	void ForceStartLifeTimer(int timeLeft);
	void ResetEffectsDuration();

	void TimeToDie();
	void InheritedTo(TechnoClass* pNewOwner, HouseClass* pNewOwnerHouse);

	bool IsSameGroup(const AttachEffectData& otherType);
	void GetMarks(std::vector<std::string>& marks);

	bool OwnerIsDead();
	void OnTechnoDelete(EventSystem* sender, Event e, void* args);

	void UpdateStandLocation(LocationMark locationMark);
	void UpdateAnimOffset(CoordStruct offset);
	void UpdateInfoOffset(CoordStruct offset);

	TechnoClass* GetStand();
	bool TryGetStand(TechnoClass*& pStand);

	/**
	 *@brief 检查AE是否仍然可用，只要有任意一个Effect失效，则判定为失活，失活的AE会从AE管理器中移除
	 *
	 * @return true
	 * @return false
	 */
	virtual bool IsAlive() override;

	bool IsPaused();

	virtual void Clean() override
	{
		ObjectScript::Clean();

		Token = GetUUID();
		AEData = {};

		_aeManager = nullptr;

		pSource = nullptr; // AE来源单位
		pSourceHouse = nullptr; // AE来源所属

		FromWarhead = false;
		WarheadLocation = CoordStruct::Empty;

		AEMode = -1;
		FromPassenger = false;

		_duration = -1; // 寿命
		_immortal = true; // 永生
		_lifeTimer = {};

		_initDelay = -1; // 启动初始延迟
		_delayToEnable = false; // 延迟开启
		_initialDelayTimer = {}; // 初始延迟计时器

		_inBuilding = false; // 是否在建造中
		_started = false; // 已开启
		_paused = false; // 暂停
		_hold = false; // 跳过效果器死亡检查，用于暂停效果器，AE不会当成死亡结束

		_diffSource = false; // pSource是外人，需要监听死亡
	}

	/**
	 *@brief 根据AEData初始化AE配置，如果延迟生效则令Component失活等待延迟激活，同时调用InitEffects初始化子Effects
	 *
	 */
	virtual void Awake() override;

	virtual void Destroy() override;

	/**
	 *@brief 启用AE，根据初始延迟设置延迟启动效果
	 *
	 * @param pSource 来源
	 * @param pSourceHouse 来源所属
	 * @param warheadLocation 弹头赋予时爆炸的位置
	 * @param aeMode AE模式
	 * @param fromPassenger 由乘客赋予
	 */
	void Start(TechnoClass* pSource, HouseClass* pSourceHouse, CoordStruct warheadLocation, int aeMode, bool fromPassenger);

	/**
	 *@brief 强制关闭AE
	 *
	 * @param location 结束时的位置
	 */
	virtual void End(CoordStruct location) override;

	virtual void OnReceiveDamageEnd(int* pRealDamage, WarheadTypeClass* pWH, DamageState damageState, TechnoClass* pAttacker, HouseClass* pAttackingHouse);

	virtual void OnGScreenRender(CoordStruct location) override;
	virtual void OnGScreenRenderEnd(CoordStruct location) override;

	std::string Token = GetUUID();
	AttachEffectData AEData{};

	AttachEffect* _aeManager = nullptr;
	AttachEffect* GetAEManager();
	__declspec(property(get = GetAEManager)) AttachEffect* AEManager;

	TechnoClass* pSource = nullptr; // AE来源单位
	HouseClass* pSourceHouse = nullptr; // AE来源所属

	bool FromWarhead = false;
	CoordStruct WarheadLocation = CoordStruct::Empty;

	int AEMode = -1;
	bool FromPassenger = false;

	bool SkipNext = false; // 跳过下一个

#pragma region Save/Load
	template <typename T>
	bool Serialize(T& stream) {
		return stream
			.Process(this->Token)
			.Process(this->AEData)
			.Process(this->pSource)
			.Process(this->pSourceHouse)
			.Process(this->FromWarhead)
			.Process(this->WarheadLocation)
			.Process(this->AEMode)
			.Process(this->FromPassenger)
			.Process(this->SkipNext)

			.Process(this->_duration)
			.Process(this->_immortal)
			.Process(this->_lifeTimer)
			.Process(this->_initDelay)
			.Process(this->_delayToEnable)
			.Process(this->_initialDelayTimer)
			.Process(this->_inBuilding)
			.Process(this->_started)
			.Process(this->_paused)
			.Process(this->_hold)

			.Process(this->_diffSource)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		Component::Load(stream, registerForChange);
		bool res = this->Serialize(stream);
		if (_diffSource)
		{
			EventSystems::General.AddHandler(Events::ObjectUnInitEvent, this, &AttachEffectScript::OnTechnoDelete);
		}
		return res;
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		Component::Save(stream);
		return const_cast<AttachEffectScript*>(this)->Serialize(stream);
	}
#pragma endregion
private:

	/**
	 *@brief 初始化所有的Effects并加入Sub-Component
	 *
	 */
	void InitEffects();

	/**
	 * @brief 启动初始延迟计时器
	 *
	 */
	void SetupInitTimer();

	/**
	 *@brief 启动生命计时器开始计时
	 *
	 */
	void SetupLifeTimer();

	/**
	 *@brief 是否仍在延迟启动中
	 *
	 * @return true 延迟中
	 * @return false 延迟结束
	 */
	bool InDelayToEnable();

	/**
	 *@brief 激活所有的Effects，开始执行Foreach逻辑
	 *
	 */
	void EnableEffects();

	/**
	 *@brief 暂停所有的Effects
	 *
	 */
	void PauseEffects();

	/**
	 *@brief 恢复所有的Effects
	 *
	 */
	void RecordEffects();

	/**
	 * @brief 检查血量比例是否满足开启效果器
	 *
	 * @return true 满足
	 * @return false 不满足
	 */
	bool CheckHealthPercent();

	int _duration = -1; // 寿命
	bool _immortal = true; // 永生
	CDTimerClass _lifeTimer{};

	int _initDelay = -1; // 启动初始延迟
	bool _delayToEnable = false; // 延迟开启
	CDTimerClass _initialDelayTimer{}; // 初始延迟计时器

	bool _inBuilding = false; // 是否在建造中
	bool _started = false; // 已开启
	bool _paused = false; // 暂停
	bool _hold = false; // 跳过效果器死亡检查，用于暂停效果器，AE不会当成死亡结束

	bool _diffSource = false; // pSource是外人，需要监听死亡

};
