#include "DamageControlEffect.h"

#include <algorithm>

#include <Ext/Helper/FLH.h>
#include <Ext/Helper/MathEx.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>
#include <Ext/Helper/StringEx.h>

#include <Extension/WarheadTypeExt.h>

#include <Ext/StateType/State/DamageControlState.h>

const DamageControlEntity& DamageControlEffect::GetDataEntity()
{
	return _isElite ? Data->EliteData : Data->Data;
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

	// 登记进所在单位的结算管线：这条段从此参与伤害结算
	RegisterToState();
}

void DamageControlEffect::RegisterToState()
{
	// 段在新建时走 OnStart、读档时走 Load、被 InheritAE 换宿主时走 ExtChanged，三条路径都要登记
	if (_gameObject)
	{
		if (DamageControlState* state = _gameObject->GetComponent<DamageControlState>())
		{
			_state = state;
			state->AddSegment(this);
		}
	}
}

void DamageControlEffect::UnregisterFromState()
{
	// 用登记时记下的指针把自己划掉：只做一次表内查找与删除，不碰宿主组件树，
	// 所以即使宿主正在销毁过程中也安全
	if (_state)
	{
		_state->RemoveSegment(this);
		_state = nullptr;
	}
}

void DamageControlEffect::OnPause()
{
	// AE 因血量等条件暂停效果器：本段暂时不参与结算，从结算管线里退出
	UnregisterFromState();
}

void DamageControlEffect::OnRecover()
{
	// 效果器恢复：重新登记回结算管线
	RegisterToState();
}

void DamageControlEffect::ExtChanged()
{
	// InheritAE 会把整棵 AE 树搬到新单位（只换宿主、不走 Clean）：
	// 先让基类重新识别宿主，再把自己登记到新单位的结算管线上
	EffectScript::ExtChanged();
	RegisterToState();
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
		const DamageControlEntity& data = isElite ? Data->EliteData : Data->Data;
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
	// 只兜一道底：段被 AE 关掉（Deactivate）时不该继续参与结算。
	// 增删由登记 / 注销负责，这里不再逐项追问 started / awaked / pause
	return IsActive();
}

bool DamageControlEffect::IsDone()
{
	const DamageControlEntity& data = GetDataEntity();
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
	const DamageControlEntity& data = GetDataEntity();
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

bool DamageControlEffect::CheckUsable(WarheadTypeClass* pWH, WarheadTypeExt::TypeData* whData)
{
	if (!IsRunning() || !IsReady())
	{
		return false;
	}

	const DamageControlEntity& data = GetDataEntity();
	if (!data.Enable || !data.WarheadOnMark(pWH->ID))
	{
		return false;
	}

	// 弹头穿透：写了 Modes 就以清单为准，只对列出的类别不响应；没写清单才看总开关
	const std::vector<DamageReactionMode>& ignoreModes = whData->IgnoreDamageReactionModes;
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
	const DamageControlEntity& data = GetDataEntity();
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
	const DamageControlEntity& data = GetDataEntity();
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
	const DamageControlEntity& data = GetDataEntity();
	if (AnimTypeClass* pAnimType = AnimTypeClass::Find(data.Anim.c_str()))
	{
		CoordStruct location = pTechno->GetCoords();
		// CoordStruct::IsEmpty() 不是 const 成员函数，这里用与空坐标比较代替（data 是 const 引用）
		if (!(data.AnimFLH == CoordStruct::Empty))
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
	const DamageControlEntity& data = GetDataEntity();
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
