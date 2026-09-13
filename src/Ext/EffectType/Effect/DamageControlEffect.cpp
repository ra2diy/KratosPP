#include "DamageControlEffect.h"

#include <algorithm>

#include <Ext/Helper/FLH.h>
#include <Ext/Helper/MathEx.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>
#include <Ext/Helper/StringEx.h>

#include <Extension/WarheadTypeExt.h>

#include <Ext/ObjectType/AttachEffect.h>

DamageControlEntity DamageControlEffect::GetDataEntity()
{
	DamageControlEntity data = Data->Data;
	if (_isElite)
	{
		data = Data->EliteData;
	}
	return data;
}

void DamageControlEffect::OnStart()
{
	_count = 0;
	_delay = 0;
	_delayTimer.Stop();

	_animDelay = 0;
	_animDelayTimer.Stop();

	_triggerFrame = -1;
	_attachFrame = -1;

	_isElite = pTechno && pTechno->Veterancy.IsElite();
}

void DamageControlEffect::OnUpdate()
{
	if (IsDone() || ForceDone)
	{
		// 次数用尽，或一次性效果已经触发过：结束自己所在的这条 AE
		Deactivate();
		AE->TimeToDie();
		return;
	}

	bool isElite = pTechno && pTechno->Veterancy.IsElite();
	if (isElite != _isElite)
	{
		// 精英状态发生变化时，按当前生效的配置决定是否清零计数
		DamageControlEntity data = Data->Data;
		if (isElite)
		{
			data = Data->EliteData;
		}
		if (data.ResetTimes)
		{
			_count = 0;
		}
	}
	_isElite = isElite;
}

DamageReactionMode DamageControlEffect::GetMode()
{
	return GetDataEntity().Mode;
}

int DamageControlEffect::GetPriority()
{
	return GetDataEntity().Priority;
}

double DamageControlEffect::GetDodgeRate()
{
	return GetDataEntity().DodgeRate;
}

DodgeStackMode DamageControlEffect::GetDodgeStack()
{
	return GetDataEntity().DodgeStack;
}

int DamageControlEffect::GetDamageValue()
{
	return GetDataEntity().DamageValue;
}

FortitudeAction DamageControlEffect::GetAction()
{
	return GetDataEntity().Action;
}

int DamageControlEffect::GetFortitudeTarget()
{
	return GetDataEntity().FortitudeTarget;
}

bool DamageControlEffect::GetIMustRegroupMyForces()
{
	return GetDataEntity().IMustRegroupMyForces;
}

bool DamageControlEffect::IsRunning()
{
	// 已启动、已唤醒且处于生效状态，且没有被 AE 暂停
	return _started && IsAwaked() && IsActive() && !_pause;
}

bool DamageControlEffect::IsDone()
{
	DamageControlEntity data = GetDataEntity();
	if (data.TriggeredTimes <= 0)
	{
		// 0 与负数都表示不限次数
		return false;
	}
	if (data.InfiniteTimesPerFrame && _triggerFrame == Unsorted::CurrentFrame)
	{
		// 本帧已经记过账：这一帧内继续有效，等帧变化之后再按次数判定，
		// 否则一次同帧连击就会把最后一点次数当场用光
		return false;
	}
	return _count >= data.TriggeredTimes;
}

bool DamageControlEffect::Timeup()
{
	DamageControlEntity data = GetDataEntity();
	if (data.InfiniteTimesPerFrame && _triggerFrame == Unsorted::CurrentFrame)
	{
		// 本帧已经记过账：视为同一次触发的延续，本帧内不受冷却限制
		return true;
	}
	return _delay <= 0 || _delayTimer.Expired();
}

bool DamageControlEffect::IsReady()
{
	return !IsDone() && Timeup();
}

bool DamageControlEffect::CanPlayAnim()
{
	return _animDelay <= 0 || _animDelayTimer.Expired();
}

bool DamageControlEffect::CheckUsable(WarheadTypeClass* pWH)
{
	if (!IsRunning() || !IsReady())
	{
		return false;
	}

	DamageControlEntity data = GetDataEntity();
	if (!data.Enable || !data.WarheadOnMark(pWH->ID))
	{
		return false;
	}

	// 弹头穿透：写了 Modes 就以清单为准，只对列出的类别不响应；没写清单才看总开关
	WarheadTypeExt::TypeData* whData = GetTypeData<WarheadTypeExt, WarheadTypeExt::TypeData>(pWH);
	std::vector<DamageReactionMode> ignoreModes = whData->IgnoreDamageReactionModes;
	if (!ignoreModes.empty())
	{
		// 清单非空时弹头侧会把总开关一并置真，这里必须让清单优先，否则"只穿透指定类别"会退化成全部穿透
		if (std::find(ignoreModes.begin(), ignoreModes.end(), data.Mode) != ignoreModes.end())
		{
			return false;
		}
	}
	else if (whData->IgnoreDamageReaction)
	{
		// 没写清单：总开关为真则本机制全部类别都不响应
		return false;
	}

	// 概率判定放在最后：前面的硬条件都成立之后才摇骰
	return Bingo(data.Chance);
}

bool DamageControlEffect::CheckCondition(int damage)
{
	DamageControlEntity data = GetDataEntity();
	switch (data.Compare)
	{
	case FortitudeCompare::EQ:
		return damage == data.DamageValue;
	case FortitudeCompare::NE:
		return damage != data.DamageValue;
	case FortitudeCompare::LT:
		return damage < data.DamageValue;
	case FortitudeCompare::GE:
		return damage >= data.DamageValue;
	case FortitudeCompare::LE:
		return damage <= data.DamageValue;
	default:
		// GT：伤害大于判定值
		return damage > data.DamageValue;
	}
}

bool DamageControlEffect::IsLethal(int damage, args_ReceiveDamage* args)
{
	if (!pTechno)
	{
		return false;
	}
	// 按护甲与弹头估算这一发实际会打掉多少血，再和当前血量比较
	int realDamage = GetRealDamage(pTechno->GetTechnoType()->Armor, damage, args->WH, args->IgnoreDefenses, args->DistanceToEpicenter);
	return realDamage >= pTechno->Health;
}

void DamageControlEffect::OnTriggered(args_ReceiveDamage* args)
{
	DamageControlEntity data = GetDataEntity();
	int currentFrame = Unsorted::CurrentFrame;

	// 记账：默认每帧只记一次账，同一帧内的后续命中视为同一次触发
	if (!data.InfiniteTimesPerFrame || _triggerFrame != currentFrame)
	{
		_count++;
		_triggerFrame = currentFrame;
		ForceDone = data.ActiveOnce;
		_delay = data.Delay;
		if (_delay > 0)
		{
			_delayTimer.Start(_delay);
		}
	}

	// 附属 AE：默认本帧内可以贴无数次，关掉该开关则本帧只贴一次
	if (data.InfiniteAttachPerFrame || _attachFrame != currentFrame)
	{
		_attachFrame = currentFrame;
		AttachTriggeredEffects(args);
	}

	// 动画仍按 AnimDelay 节流，不受上面两个开关影响
	if (IsNotNone(data.Anim) && CanPlayAnim())
	{
		PlayAnim();
	}
}

void DamageControlEffect::PlayAnim()
{
	DamageControlEntity data = GetDataEntity();
	if (AnimTypeClass* pAnimType = AnimTypeClass::Find(data.Anim.c_str()))
	{
		CoordStruct location = pTechno->GetCoords();
		if (!data.AnimFLH.IsEmpty())
		{
			location = GetFLHAbsoluteCoords(pTechno, data.AnimFLH, false);
		}
		AnimClass* pAnim = GameCreate<AnimClass>(pAnimType, location);
		SetAnimOwner(pAnim, pTechno);
	}
	_animDelay = data.AnimDelay;
	if (_animDelay > 0)
	{
		_animDelayTimer.Start(_animDelay);
	}
}

void DamageControlEffect::AttachTriggeredEffects(args_ReceiveDamage* args)
{
	DamageControlEntity data = GetDataEntity();
	if (data.TriggeredAttachEffects.empty())
	{
		return;
	}
	if (AttachEffect* aem = _gameObject->GetComponent<AttachEffect>())
	{
		if (data.TriggeredAttachEffectsFromAttacker)
		{
			aem->Attach(data.TriggeredAttachEffects, data.TriggeredAttachEffectChances, false, args->Attacker, args->SourceHouse);
		}
		else
		{
			aem->Attach(data.TriggeredAttachEffects, data.TriggeredAttachEffectChances, false);
		}
	}
}
