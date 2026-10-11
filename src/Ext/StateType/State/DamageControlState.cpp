#include "DamageControlState.h"

#include <algorithm>

#include <Ext/EffectType/Effect/DamageControlEffect.h>
#include <Ext/Helper/MathEx.h>
#include <Ext/Helper/Scripts.h>

#include <Extension/WarheadTypeExt.h>


namespace
{
	/// @brief 把一批段按 Priority 降序排列（同档内保持附着顺序），并做同档淘汰
	/// @param list 按附着顺序排好的段；调用处用 std::move 交出所有权，排序的就是它本身，不产生额外拷贝
	/// @param keepComparer true 时，只做比较不改数值的刚毅段不参与淘汰，全部保留
	/// @return 真正要执行的段，顺序即结算顺序
	std::vector<DamageControlEffect*> FilterByPriority(std::vector<DamageControlEffect*> list, bool keepComparer)
	{
		// 稳定排序保证同一档内仍然是附着顺序
		std::stable_sort(list.begin(), list.end(), [](DamageControlEffect* a, DamageControlEffect* b)
			{ return a->GetPriority() > b->GetPriority(); });

		auto isComparer = [keepComparer](DamageControlEffect* effect)
			{ return keepComparer
				&& effect->GetMode() == DamageReactionMode::FORTITUDE
				&& effect->GetAction() == FortitudeAction::NONE; };

		std::vector<DamageControlEffect*> result{};
		size_t i = 0;
		while (i < list.size())
		{
			int priority = list[i]->GetPriority();
			size_t j = i;
			while (j < list.size() && list[j]->GetPriority() == priority)
			{
				j++;
			}
			// 同一档里，涉及改数值的段只有最后附着的那一条能执行
			DamageControlEffect* winner = nullptr;
			for (size_t k = i; k < j; k++)
			{
				if (!isComparer(list[k]))
				{
					winner = list[k];
				}
			}
			// 按附着顺序收下胜出者与全部纯比较段
			for (size_t k = i; k < j; k++)
			{
				if (isComparer(list[k]) || list[k] == winner)
				{
					result.push_back(list[k]);
				}
			}
			i = j;
		}
		return result;
	}
}


void DamageControlState::Clean()
{
	StateScript<DamageControlData>::Clean();

	// 表是运行时结构，不跨复用残留
	_segments.clear();
	_evasions.clear();
	_modifiers.clear();
	_prevents.clear();
}

void DamageControlState::AddSegment(DamageControlEffect* segment)
{
	if (!segment)
	{
		return;
	}
	// 血量条件恢复时会把同一段再报一次，已在表里就不重复添加
	if (std::find(_segments.begin(), _segments.end(), segment) != _segments.end())
	{
		return;
	}
	_segments.push_back(segment);
	Rebuild();
}

void DamageControlState::RemoveSegment(DamageControlEffect* segment)
{
	if (!segment)
	{
		return;
	}
	auto it = std::find(_segments.begin(), _segments.end(), segment);
	if (it == _segments.end())
	{
		return;
	}
	_segments.erase(it);
	Rebuild();
}

void DamageControlState::Rebuild()
{
	_evasions.clear();
	_modifiers.clear();
	_prevents.clear();

	std::vector<DamageControlEffect*> evasions{};
	std::vector<DamageControlEffect*> modifiers{};
	std::vector<DamageControlEffect*> prevents{};
	for (DamageControlEffect* segment : _segments)
	{
		switch (segment->GetMode())
		{
		case DamageReactionMode::EVASION:
			evasions.push_back(segment);
			break;
		case DamageReactionMode::FORTITUDE:
		case DamageReactionMode::REDUCE:
			modifiers.push_back(segment);
			break;
		case DamageReactionMode::PREVENT:
			prevents.push_back(segment);
			break;
		default:
			break;
		}
	}

	// 闪避与刚毅/减免的顺序和同档淘汰在这里一次算好；免死不参与排序
	_evasions = FilterByPriority(std::move(evasions), false);
	_modifiers = FilterByPriority(std::move(modifiers), true);
	_prevents = std::move(prevents);

	// 段全没了就休眠：引擎遍历会直接跳过本组件，没配伤害控制的单位零成本
	if (_segments.empty())
	{
		Deactivate();
	}
	else
	{
		Activate();
	}
}

void DamageControlState::OnReceiveDamage(args_ReceiveDamage* args)
{
	// 无伤害不进管线；负数（治疗）也要进管线，交给刚毅处理
	if (!pTechno || !args || !args->Damage || *args->Damage == 0)
	{
		return;
	}

	// 无视防御的真实伤害不做任何响应（与旧 DamageReaction 同口径）
	if (args->IgnoreDefenses)
	{
		return;
	}

	if (_evasions.empty() && _modifiers.empty() && _prevents.empty())
	{
		return;
	}

	// 弹头扩展数据在这里取一次，供所有段复用
	// 用 :: 限定：避免被本类继承链上的同名成员遮蔽
	WarheadTypeClass* pWH = args->WH;
	WarheadTypeExt::TypeData* whData = ::GetTypeData<WarheadTypeExt, WarheadTypeExt::TypeData>(pWH);

	int incoming = *args->Damage;
	int damage = incoming;

	// 治疗不受闪避影响：入口值为负数时整个相位不参与
	if (incoming > 0)
	{
		// 相位 1（闪避）：按 Priority 降序并出总闪避率，摇中则伤害归零并直接出口
		double dodgeRate = 0;
		std::vector<DamageControlEffect*> dodgeHits{};
		for (DamageControlEffect* effect : _evasions)
		{
			if (!effect->CheckUsable(pWH, whData))
			{
				continue;
			}
			dodgeHits.push_back(effect);
			if (effect->GetDodgeStack() == DodgeStackMode::MUL)
			{
				dodgeRate = 1 - (1 - dodgeRate) * (1 - effect->GetDodgeRate());
			}
			else
			{
				dodgeRate += effect->GetDodgeRate();
			}
		}
		if (!dodgeHits.empty() && Bingo(dodgeRate))
		{
			for (DamageControlEffect* effect : dodgeHits)
			{
				effect->OnTriggered(args);
			}
#ifdef DEBUG_AE
			Debug::Log("[DamageControl]%d 闪避摇中，伤害归零\n", pTechno);
#endif // DEBUG_AE
			*args->Damage = 0;
			return;
		}
	}

	// 相位 2（刚毅 / 减免）：按 Priority 降序逐段结算，前一段改完的数值就是后一段看到的
	bool exit = false;
	for (DamageControlEffect* effect : _modifiers)
	{
		if (!effect->CheckUsable(pWH, whData))
		{
			continue;
		}
		switch (effect->GetMode())
		{
		case DamageReactionMode::REDUCE:
			// 减免只作用于正伤害，结果不会低于 0；治疗（负值）不受影响
			if (damage > 0)
			{
				damage = std::max(0, damage - effect->GetDamageValue());
				effect->OnTriggered(args);
			}
			break;
		case DamageReactionMode::FORTITUDE:
			if (effect->CheckCondition(damage))
			{
				switch (effect->GetAction())
				{
				case FortitudeAction::SET:
					// 刚毅是强制修改，允许改成负数
					damage = effect->GetFortitudeTarget();
					break;
				case FortitudeAction::DISCARD:
					// 丢弃：伤害归零，本次结算到此结束，免死相位也不跑
					damage = 0;
					exit = true;
					break;
				default:
					// 只比较，不改数值
					break;
				}
				effect->OnTriggered(args);
			}
			break;
		default:
			break;
		}
		if (exit)
		{
			break;
		}
	}

	// 相位 3（免死）：不参与 Priority，按附着顺序逐条判定致死
	if (!exit)
	{
		for (DamageControlEffect* effect : _prevents)
		{
			if (!effect->CheckUsable(pWH, whData))
			{
				continue;
			}
			if (!effect->IsLethal(damage, args))
			{
				continue;
			}
			effect->OnTriggered(args);
			// 丢弃这次伤害：后面的免死段拿到的判定值已经是 0，不会再判为致死
			damage = 0;
			if (effect->GetIMustRegroupMyForces())
			{
				// 管线只能改原始伤害，而它之后还要被护甲与 verses 缩小，
				// 所以这里直接把血量设成 1，保证单位精确剩 1 血
				pTechno->Health = 1;
			}
		}
	}

	// 出口：与入口相同说明没有任何段改过它，原样放过（原本就是治疗的那一发交给别处处理）
	if (damage != incoming)
	{
		if (damage > 0)
		{
			*args->Damage = damage;
		}
		else
		{
			// 伤害变治疗。引擎在 0x7019D8 会把小于 1 的伤害提升成 1 点，
			// 负数写回伤害通道到不了治疗，所以把治疗量直接加到血量，伤害写 0
			if (damage < 0)
			{
				pTechno->Health += -damage;
			}
			*args->Damage = 0;
		}
	}
#ifdef DEBUG_AE
	Debug::Log("[DamageControl]%d 管线结算完成，伤害 %d -> %d\n", pTechno, incoming, *args->Damage);
#endif // DEBUG_AE
}
