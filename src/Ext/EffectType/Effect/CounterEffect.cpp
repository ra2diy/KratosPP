#include "CounterEffect.h"

#include <Ext/Helper/Finder.h>
#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>

#include <Extension/WarheadTypeExt.h>

void CounterEffect::Watch()
{
	// 计数器归零时移除
	for (auto& [idx, entity] : Data->RemoveWhenNums)
	{
		if (entity.RemoveWhenNum.Y >= entity.RemoveWhenNum.X)
		{
			if (CountNum >= entity.RemoveWhenNum.X && (entity.RemoveWhenNum.Y < 0 || CountNum <= entity.RemoveWhenNum.Y))
			{
				Deactivate();
				AE->TimeToDie();
				return;
			}
		}
	}
}

int CounterEffect::CalculateRemainingDamage(int Damage)
{
	if (Damage <= 0) return 0;

	// 如果保护比例或百分比无效，返回原伤害
	if (Data->Reaction.Protect <= 0.0 || Data->Reaction.Percent <= 0.0)
		return Damage;

	// 抵扣伤害上限
	int damageToProtect = Damage;
	if (Data->Reaction.Limit > 0 && damageToProtect > Data->Reaction.Limit)
	{
		damageToProtect = Data->Reaction.Limit;
	}

	// 需要CountNum抵扣的伤害部分
	double protectedPart = damageToProtect * Data->Reaction.Protect;
	// 这部分伤害需要的CountNum
	double requiredCount = protectedPart * Data->Reaction.Percent;

	// 实际可用的CountNum
	double actualUsed = std::min(CountNum, requiredCount);

	// Debug::Log("CounterEffect::CalculateRemainingDamage(), Mark: %s, AE: %s, damage = %d, protectedPart = %.2f(%.2f), requiredCount = %.2f(%.2f), actualUsed = %.2f, CountNum = %.2f\n",
	// 	AEData.Counter.Mark.c_str(), AEData.Name.c_str()
	// 	, damage, protectedPart, Data->Reaction.Protect, requiredCount, Data->Reaction.Percent
	// 	, actualUsed, CountNum);

	// 更新CountNum
	ModifyCount(CounterAction::SUB, actualUsed);

	// 实际抵扣的伤害
	double actualProtectedDamage = actualUsed / Data->Reaction.Percent;
	int intProtectedDamage = static_cast<int>(std::round(actualProtectedDamage));

	// 最终伤害 = 原伤害 - 实际抵扣的伤害
	return std::max(0, Damage - intProtectedDamage);
}

void CounterEffect::AddSelfToManager()
{
	if (_started && !_pause && AE->AEManager)
	{
		AE->AEManager->AddCounter(AEData, this);
		// Debug::Log("CounterEffect::AddSelfToManager(), Mark: %s, AE: %s\n", AEData.Counter.Mark.c_str(), AEData.Name.c_str());
	}
}

void CounterEffect::RemoveSelfFromManager()
{
	if (AE->AEManager)
	{
		AE->AEManager->RemoveCounter(AEData);
	}
}

void CounterEffect::OnStart()
{
	ResetNum();
	// 向AE管理器注册自己
	AddSelfToManager();
}

void CounterEffect::OnPause()
{
	// 向AE管理器注销自己
	RemoveSelfFromManager();
}

void CounterEffect::OnRecover()
{
	// 向AE管理器注册自己
	AddSelfToManager();
}

void CounterEffect::OnUpdate()
{
	if (!AE->OwnerIsDead())
	{
		Watch();
	}
}

void CounterEffect::OnWarpUpdate()
{
	if (!AE->OwnerIsDead())
	{
		Watch();
	}
}

void CounterEffect::OnReceiveDamageReal(int* pRealDamage, WarheadTypeClass* pWH, TechnoClass* pAttacker, HouseClass* pAttackingHouse)
{
	if (AE->OwnerIsDead())
	{
		return;
	}

	// 无视防御的真实伤害不做任何响应
	if (Data->ReactionMode != CounterReaction::NORMAL)
	{
		WarheadTypeExt::TypeData* whData = GetTypeData<WarheadTypeExt, WarheadTypeExt::TypeData>(pWH);
		if (!whData->IgnoreCounterReaction && Data->Reaction.WarheadOnMark(pWH->ID))
		{
			// 扣除计数并返回抵扣后的伤害值
			int damage = CalculateRemainingDamage(*pRealDamage);
			// 抵扣伤害
			if (Data->ReactionMode == CounterReaction::SHIELD)
			{
				*pRealDamage = damage;
			}
		}
	}
}

void CounterEffect::ModifyCount(CounterAction action, double num)
{
	switch (action)
	{
	case CounterAction::INIT:
		CountNum = num;
		break;
	case CounterAction::ADD:
		CountNum += num;
		break;
	case CounterAction::SUB:
		CountNum -= num;
		break;
	case CounterAction::MUL:
		CountNum *= num;
		break;
	}

	if (CountNum < Data->Min)
	{
		CountNum = Data->Min;
	}
	if (Data->Max >= 0 && CountNum > Data->Max)
	{
		CountNum = Data->Max;
	}
}

void CounterEffect::ResetNum()
{
	double num = Data->Num;
	if (Data->NumType != CounterType::Number)
	{
		ObjectClass* pFrom = Data->NumFromSource ? AE->pSource : pObject;
		if (pFrom && !IsDeadOrInvisible(pFrom))
		{
			switch (Data->NumType)
			{
			case CounterType::HP:
				num = pFrom->Health;
				break;
			case CounterType::MaxHP:
				if (Data->NumFromSource)
				{
					num = pFrom->GetType()->Strength;
				}
				else
				{
					num = IsBullet() ? pFrom->Health : pFrom->GetType()->Strength;
				}
				// 强制修改取值范围
				Data->Min = 0;
				Data->Max = num;
				break;
			}
		}
	}
	CountNum = num;
}

void CounterEffect::RemoveCounter()
{
	RemoveSelfFromManager();
	Deactivate();
	AE->TimeToDie();
}
