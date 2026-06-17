#include "AttachEffectScript.h"

#include <Ext/Helper/Status.h>
#include <Ext/Common/PrintTextManager.h>

#include "EffectScript.h"
#include "Effect/AnimationEffect.h"
#include "Effect/StandEffect.h"
#include "Effect/InfoEffect.h"

int AttachEffectScript::GetDuration()
{
	int duration = _immortal ? -1 : AEData.GetDuration();
	int timeLeft = -1;
	if (duration > -1 && TryGetDurationTimeLeft(timeLeft))
	{
		duration = timeLeft;
	}
	return duration;
}

bool AttachEffectScript::TryGetInitDelayTimeLeft(int& timeLeft)
{
	timeLeft = -1;
	if (_delayToEnable)
	{
		timeLeft = _initialDelayTimer.GetTimeLeft();
	}
	return timeLeft > -1;
}

bool AttachEffectScript::TryGetDurationTimeLeft(int& timeLeft)
{
	timeLeft = -1;
	if (!_immortal)
	{
		timeLeft = _lifeTimer.GetTimeLeft();
	}
	return timeLeft > -1;
}

void AttachEffectScript::MergeDuration(int otherDuration)
{
	if (!_started || otherDuration == 0)
	{
		// 延迟激活中，不接受时延修改
		return;
	}
	// 修改时间，正数加，负数减
	if (otherDuration < 0)
	{
		// 剩余时间
		int timeLeft = _immortal ? _duration : _lifeTimer.GetTimeLeft();
		// 削减生命总长
		_duration += otherDuration;
		if (_duration <= 0 || timeLeft <= 0)
		{
			// 削减的时间超过总长度，直接减没了
			Deactivate();
		}
		else
		{
			timeLeft += otherDuration;
			if (timeLeft <= 0)
			{
				// 削减完后彻底没了
				Deactivate();
			}
			else
			{
				// 还有剩
				// 重设时间
				ForceStartLifeTimer(timeLeft);
			}
		}
	}
	else
	{
		// 累加持续时间
		_duration += otherDuration;
		if (!_immortal)
		{
			int timeLeft = _lifeTimer.GetTimeLeft();
			timeLeft += otherDuration;
			ForceStartLifeTimer(timeLeft);
		}
	}
}

void AttachEffectScript::ForceStartLifeTimer(int timeLeft)
{
	_immortal = false;
	_lifeTimer.Start(timeLeft);
	ResetEffectsDuration();
}

void AttachEffectScript::ResetDuration()
{
	SetupLifeTimer();
	ResetEffectsDuration();
}

void AttachEffectScript::ResetEffectsDuration()
{
	ForeachChild([](Component* c) {
		if (auto cc = dynamic_cast<IAEScript*>(c)) { cc->ResetDuration(); }
		});
}

void AttachEffectScript::TimeToDie()
{
#ifdef DEBUG_AE
	Debug::Log("AE[%s] TimeToDie.\n", AEData.Name.c_str());
#endif
	_hold = false;
	_immortal = false;
	_lifeTimer.Stop();
}

void AttachEffectScript::InheritedTo(TechnoClass* pNewOwner, HouseClass* pNewOwnerHouse)
{
#ifdef DEBUG_AE
	Debug::Log("AE[%s] Inherited to Owner[%s]%d, House %d.\n", AEData.Name.c_str(), pNewOwner->GetType()->ID, pNewOwner, pNewOwnerHouse);
#endif
	pSource = pNewOwner;
	pSourceHouse = pNewOwnerHouse;
}

void AttachEffectScript::SetupInitTimer()
{
	if (_initDelay > 0)
	{
		_delayToEnable = true;
		_initialDelayTimer.Start(_initDelay); // 下一帧开始计时
	}
}

void AttachEffectScript::SetupLifeTimer()
{
	if (!_immortal)
	{
		_lifeTimer.Start(_duration);
	}
}

bool AttachEffectScript::IsSameGroup(AttachEffectData otherType)
{
	const std::string& group = AEData.Group;
	const std::string& otherGroup = otherType.Group;
	return IsNotNone(group) && IsNotNone(otherGroup) && group == otherGroup;
}

void AttachEffectScript::GetMarks(std::vector<std::string>& marks)
{
	if (AEData.Mark.Enable)
	{
		if (_started)
		{
			for (std::string mark : AEData.Mark.Names)
			{
				marks.push_back(mark);
			}
		}
	}
}

bool AttachEffectScript::OwnerIsDead()
{
	return AEManager->OwnerIsDead();
}

void AttachEffectScript::OnTechnoDelete(EventSystem* sender, Event e, void* args)
{
	if (args == pSource)
	{
		pSource = nullptr;
	}
}

void AttachEffectScript::UpdateStandLocation(LocationMark locationMark)
{
	if (StandEffect* c = GetComponent<StandEffect>())
	{
		c->UpdateLocation(locationMark);
	}
}

void AttachEffectScript::UpdateAnimOffset(CoordStruct offset)
{
	if (AnimationEffect* c = GetComponent<AnimationEffect>())
	{
		c->UpdateLocationOffset(offset);
	}
}

void AttachEffectScript::UpdateInfoOffset(CoordStruct offset)
{
	if (InfoEffect* c = GetComponent<InfoEffect>())
	{
		c->UpdateLocationOffset(offset);
	}
}

TechnoClass* AttachEffectScript::GetStand()
{
	TechnoClass* pTechno = nullptr;
	if (StandEffect* c = GetComponent<StandEffect>())
	{
		pTechno = c->pStand;
	}
	return pTechno;
}

bool AttachEffectScript::TryGetStand(TechnoClass*& pStand)
{
	pStand = GetStand();
	return pStand != nullptr;
}

bool AttachEffectScript::IsAlive()
{
#ifdef DEBUG
	int timeLeft = GetDuration();
	Tag = std::to_string(timeLeft) + "#" + AEData.Name + (_paused ? "#Paused" : "");
#endif
	// 建筑变卖处理
	if (AEManager->InSelling())
	{
		if (AEData.RemoveOnSold)
		{
			Deactivate();
			return false;
		}
		if (AEData.DeactiveOnSold)
			return false;
	}
	if (!_started)
	{
		if (!InDelayToEnable())
		{
			// 激活效果器
			EnableEffects();
		}
		else
		{
			return true;
		}
	}
	if (IsActive())
	{
		// 检查血量暂停或恢复效果器
		if (_hold)
		{
			if (CheckHealthPercent())
			{
				RecordEffects();
			}
			else
			{
				PauseEffects();
			}
		}
		// AE来源于乘客，检查乘客是否已经下车
		if (FromPassenger)
		{
			if (IsDead(pSource)
				|| !(pSource->InLimbo
					|| pSource->BunkerLinkedItem == pObject
					|| pSource->Transporter
					|| pSource->Absorbed))
			{
				// 乘客已经下车
				Deactivate();
			}
		}
		// 检查AEMode
		if (IsActive() && AEMode >= 0)
		{
			std::vector<int> ids = AEManager->PassengerIds;
			auto it = std::find(ids.begin(), ids.end(), AEMode);
			if (ids.empty() || it == ids.end())
			{
				// 乘客已经下车
				Deactivate();
			}
		}
		// 检查是否有Effect失活
		if (IsActive() && !_hold)
		{
			bool hasDead = false;
			ForeachChild([&hasDead](Component* c) {
				if (auto e = dynamic_cast<EffectScript*>(c))
				{
				int ea = (int)e->IsActive();
				int ea2 = (int)e->IsAlive();
				hasDead = !ea || !ea2;
				if (hasDead) {
					c->Break();
				}
				}
				});
			if (hasDead)
			{
				Deactivate();
			}
		}
		// 检查计时器
		if (IsActive() && !_immortal && _lifeTimer.Expired())
		{
			Deactivate();
		}
	}
	return IsActive();
}

bool AttachEffectScript::IsPaused()
{
	return _paused;
}

void AttachEffectScript::Awake()
{
	Tag = AEData.Name;

	_duration = AEData.Duration;
	_immortal = AEData.HoldDuration;

	// 初始化效果
	InitEffects();
}

void AttachEffectScript::Destroy()
{
	EventSystems::General.RemoveHandler(Events::ObjectUnInitEvent, this, &AttachEffectScript::OnTechnoDelete);
}

void AttachEffectScript::Start(TechnoClass * pSource, HouseClass * pSourceHouse, CoordStruct warheadLocation, int aeMode, bool fromPassenger)
{
	Activate();
	this->pSource = pSource;
	this->pSourceHouse = pSourceHouse;
	if (pSource != pObject)
	{
		this->_diffSource = true;
		EventSystems::General.AddHandler(Events::ObjectUnInitEvent, this, &AttachEffectScript::OnTechnoDelete);
	}
	this->FromWarhead = !warheadLocation.IsEmpty();
	if (FromWarhead)
	{
		this->WarheadLocation = warheadLocation;
	}
	this->AEMode = aeMode;
	this->FromPassenger = fromPassenger;

	this->_inBuilding = AEManager->InBuilding();

	this->_initDelay = GetRandomValue(AEData.InitialRandomDelay, AEData.InitialDelay);
	if (_initDelay > 0)
	{
		_initDelay += 1;
		SetupInitTimer();
	}

	// Start函数由AttachEffect的Attach函数调用，新添加的AE都应该延迟一帧激活
	// AE管理器在每一帧的开始时检查AE的有效，AE的真正激活，将有IsAlive函数进行
	this->_delayToEnable = true;

	// 添加后立刻激活，会导致AE无法被检测到
	//
	// [Kratos PP](315) AE [STACK2] OnUpdate 监视 ReplicantBusterAE
	// [Kratos PP](315) AE [STACK] OnUpdate 监视 ZZZZZ
	// [Kratos PP](315) 添加AE[ReplicantBusterAE] <--- STACK 成功触发
	// [Kratos PP](315) AE [ZZZZZ] OnUpdate
	// [Kratos PP](315) AE [ReplicantBusterAE] OnUpdate
	//
	// [Kratos PP](316) 移除AE[ZZZZZ]
	// [Kratos PP](316) 移除AE[ReplicantBusterAE]
	// [Kratos PP](316) AE [STACK2] OnUpdate  监视 ReplicantBusterAE (不会触发)
	// [Kratos PP](316) AE [STACK] OnUpdate
	// this->_isDelayToEnable = InDelayToEnable();
	// if (_started)
	// {
	// 	EnableEffects();
	// }
}

void AttachEffectScript::End(CoordStruct location)
{
	Deactivate();
	if (!_started)
	{
		return;
	}
	ForeachChild([&location](Component* c) {
		if (auto cc = dynamic_cast<IAEScript*>(c)) { cc->End(location); }
		});
}

void AttachEffectScript::OnReceiveDamageEnd(int* pRealDamage, WarheadTypeClass * pWH, DamageState damageState, TechnoClass * pAttacker, HouseClass * pAttackingHouse)
{
	if (!_started)
	{
		return;
	}
	if (damageState != DamageState::NowDead && !OwnerIsDead())
	{
		if (_hold)
		{
			if (CheckHealthPercent())
			{
				RecordEffects();
			}
			else
			{
				PauseEffects();
			}
		}
	}
};


void AttachEffectScript::OnGScreenRender(CoordStruct location)
{
	if (!_started)
	{
		return;
	}
	ForeachChild([&location](Component* c) {
		if (auto cc = dynamic_cast<IAEScript*>(c)) { cc->OnGScreenRender(location); }
		});
}

void AttachEffectScript::OnGScreenRenderEnd(CoordStruct location)
{
	if (!_started)
	{
		return;
	}
	ForeachChild([&location](Component* c) {
		if (auto cc = dynamic_cast<IAEScript*>(c)) { cc->OnGScreenRenderEnd(location); }
		});
}

void AttachEffectScript::InitEffects()
{
	std::vector<std::string> scriptNames = AEData.GetScriptNames();
	for (std::string scriptName : scriptNames)
	{
#ifdef DEBUG_AE
		Debug::Log(" - AE[%s]初始化添加效果器组件[%s]\n", AEData.Name.c_str(), scriptName.c_str());
#endif
		Component* c = AddComponent(scriptName);
		if (c)
		{
			EffectScript* e = dynamic_cast<EffectScript*>(c);
			// 初始化Effect
			e->AEData = this->AEData;
			e->EnsureAwaked();
			e->Deactivate(); // 令其失活等待唤醒
		}
	}
}

bool AttachEffectScript::InDelayToEnable()
{
	if (_delayToEnable)
	{
		_delayToEnable = _initialDelayTimer.InProgress();
	}
	if (_inBuilding)
	{
		_inBuilding = AEManager->InBuilding();
		if (!_inBuilding && _initDelay > 0)
		{
			SetupInitTimer();
		}
	}
	return _delayToEnable || _inBuilding;
}

void AttachEffectScript::EnableEffects()
{
	_started = true;
#ifdef DEBUG_AE
	Debug::Log("  - [%s]%d 上的 AE[%s]%d 激活持有的%d个效果器\n", pObject->GetType()->ID, pObject, AEData.Name.c_str(), this, _children.size());
#endif // DEBUG_AE
	SetupLifeTimer();
	_paused = false;
	if (pTechno)
	{
		// 需要满足血量触发
		if (AEData.CheckHealthPrecent)
		{
			_hold = true;
			_paused = !CheckHealthPercent();
		}
	}
	else
	{
		_hold = false;
	}
	if (!_paused)
	{
		// Effect is disable stats, so there cannot use ForeachChild function
		for (Component* c : _children)
		{
			if (EffectScript* effect = dynamic_cast<EffectScript*>(c))
			{
#ifdef DEBUG_AE
				Debug::Log("  - 效果器[%s]%d 激活\n", effect->Name.c_str(), effect);
#endif // DEBUG_AE
				effect->Activate();
				effect->Start();
			}
		}
	}
}

void AttachEffectScript::PauseEffects()
{
	_paused = true;
	// 暂停Effects
	for (Component* c : _children)
	{
		if (auto cc = dynamic_cast<IAEScript*>(c))
		{
			if (c->IsActive())
			{
				c->Deactivate();
				cc->Pause();
			}
		}
	}
}

void AttachEffectScript::RecordEffects()
{
	_paused = false;
	// 恢复Effects
	for (Component* c : _children)
	{
		if (auto cc = dynamic_cast<IAEScript*>(c))
		{
			if (!c->IsActive())
			{
				c->Activate();
				cc->Recover();
			}
		}
	}
}

bool AttachEffectScript::CheckHealthPercent()
{
	if (pTechno && AEData.CheckHealthPrecent)
	{
		double healthPrecent = pTechno->GetHealthPercentage();
		double min = AEData.DeactiveWhenHealthPercent;
		double max = AEData.ActiveWhenHealthPercent;
		if (max <= min)
		{
			Debug::Log("Warning: AE [%s] Active when health precent setup error.\n", AEData.Name.c_str());
			_hold = false;
			return true;
		}
#ifdef DEBUG_AE
		Debug::Log("  - [%s]%d 上的 AE[%s]%d 需要满足血量触发[%s, %s]%s\n", pObject->GetType()->ID, pObject, AEData.Name.c_str(), this, _children.size(), std::to_string(min).c_str(), std::to_string(max).c_str(), std::to_string(healthPrecent).c_str());
#endif // DEBUG_AE
		return healthPrecent <= max && healthPrecent >= min;
	}
	return true;
}

AttachEffect* AttachEffectScript::GetAEManager()
{
	if (!_aeManager)
	{
		_aeManager = dynamic_cast<AttachEffect*>(_parent);
	}
	return _aeManager;
}


