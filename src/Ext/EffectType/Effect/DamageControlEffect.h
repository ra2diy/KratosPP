#pragma once

#include <string>
#include <vector>

#include <GeneralDefinitions.h>
#include <AnimClass.h>

#include "../EffectScript.h"
#include "DamageControlData.h"


/// @brief 一条 DamageControl AE 的效果器。
/// 每段只负责自己那一类的一次处理：判断自己能不能用、算自己那一份、记账、放表现。
/// 多个类别之间的执行顺序（闪避 → 刚毅 / 减免 → 免死）由 AE 管理器统一编排，
/// 见 AttachEffect::ApplyDamageControl。
class DamageControlEffect : public EffectScript
{
public:
	EFFECT_SCRIPT(DamageControl);

	virtual void Clean() override
	{
		EffectScript::Clean();

		ForceDone = false;
		_count = 0;
		_delay = 0;
		_delayTimer = {};

		_animDelay = 0;
		_animDelayTimer = {};

		_isElite = false;

		_triggerFrame = -1;
		_attachFrame = -1;
	}

	virtual void OnStart() override;

	virtual void OnUpdate() override;

	/// @brief 本段的类别
	DamageReactionMode GetMode();

	/// @brief 本段的执行优先级，数值越大越先执行
	int GetPriority();

	/// @brief 本段的闪避率（EVASION 用）
	double GetDodgeRate();

	/// @brief 本段闪避率并入总率的方式（EVASION 用）
	DodgeStackMode GetDodgeStack();

	/// @brief 本段的固定增减量（REDUCE 用，正=减少、负=增加）
	int GetDamageValue();

	/// @brief 本段条件成立后的动作（FORTITUDE 用）
	FortitudeAction GetAction();

	/// @brief 本段条件成立后要把伤害强制改成的值（FORTITUDE 用）
	int GetFortitudeTarget();

	/// @brief 本段免死触发后要不要把单位血量直接设成 1（PREVENT 用）
	bool GetIMustRegroupMyForces();

	/// @brief 判断本段在这一次伤害里是否可用
	/// @param pWH 本次伤害的弹头
	/// @return true = 本次可用；否则本段对这一次伤害不产生任何影响
	/// @note 内部含概率摇骰，每一条段在每一次伤害事件里只允许调用一次
	bool CheckUsable(WarheadTypeClass* pWH);

	/// @brief 刚毅的条件判定
	/// @param damage 当前伤害值
	/// @return true = 条件成立（严格大于或严格小于，相等不算成立）
	bool CheckCondition(int damage);

	/// @brief 免死判定：这一发会不会把单位打死
	/// @param damage 当前伤害值
	/// @param args 本次伤害的参数（取弹头、穿甲标记与距离用于估算）
	/// @return true = 估算出的实际掉血不低于当前血量，判为致死
	bool IsLethal(int damage, args_ReceiveDamage* args);

	/// @brief 触发一次：记账（次数 / 冷却 / 一次性）+ 贴附属 AE + 播放动画
	/// @param args 本次伤害的参数（附属 AE 可能要记攻击者为来源）
	void OnTriggered(args_ReceiveDamage* args);

#pragma region Save/Load
	template <typename T>
	bool Serialize(T& stream) {
		return stream
			.Process(this->ForceDone)

			.Process(this->_count)
			.Process(this->_delay)
			.Process(this->_delayTimer)

			.Process(this->_animDelay)
			.Process(this->_animDelayTimer)

			.Process(this->_isElite)

			.Process(this->_triggerFrame)
			.Process(this->_attachFrame)
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
		return const_cast<DamageControlEffect*>(this)->Serialize(stream);
	}
#pragma endregion
private:
	/// @brief 取本条段当前生效的配置（精英单位取精英配置）
	DamageControlEntity GetDataEntity();

	/// @brief 本段是否处于运行状态（已启动且未暂停）
	bool IsRunning();

	/// @brief 是否已经触发到位（次数用尽即结束；每帧只记一次账时，本帧内不算用尽）
	bool IsDone();

	/// @brief 冷却是否已过（每帧只记一次账时，本帧内视为同一次触发延续，不受冷却限制）
	bool Timeup();

	/// @brief 本段现在能否触发：既没触发到位，也不在冷却中
	bool IsReady();

	/// @brief 动画是否已经过了重播间隔
	bool CanPlayAnim();

	/// @brief 按配置播一次动画，并刷新动画重播间隔
	void PlayAnim();

	/// @brief 按配置把清单里的 AE 贴给自己
	void AttachTriggeredEffects(args_ReceiveDamage* args);

	bool ForceDone = false;

	int _count = 0;
	int _delay = 0;
	CDTimerClass _delayTimer{};

	int _animDelay = 0;
	CDTimerClass _animDelayTimer{};

	bool _isElite = false;

	int _triggerFrame = -1; // 本段最近一次记账的帧号，用于"每帧只记一次账"
	int _attachFrame = -1; // 本段最近一次贴附属 AE 的帧号，用于"每帧只贴一次"
};
