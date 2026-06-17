#include "StackEffect.h"

#include <Ext/Helper/Finder.h>
#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>

bool StackEffect::CanActive(int stacks, int level, Condition condition)
{
	if (level >= 0)
	{
		switch (condition)
		{
		case Condition::EQ:
			return stacks == level;
		case Condition::NE:
			return stacks != level;
		case Condition::GT:
			return stacks > level;
		case Condition::LT:
			return stacks < level;
		case Condition::GE:
			return stacks >= level;
		case Condition::LE:
			return stacks <= level;
		}
	}
	return true;
}

void StackEffect::Watch()
{
	bool action = false;

	// WatchMission 独立条件：当前 Mission 不匹配则直接返回
	if (Data->WatchMission != Mission::None)
	{
		if (!pTechno || pTechno->CurrentMission != Data->WatchMission)
			return;
		// 仅 WatchMission 无 Watch 列表时，直接触发
		if (Data->Watch.empty())
		{
			action = true;
		}
	}

	if (!action)
	{
		int index = 0;
		bool modeIsAnd = Data->ActionMode == StackActionMode::AND;
		for (std::string watch : Data->Watch)
		{
			if (index > 0)
			{
				if (modeIsAnd)
				{
					if (!action)
						break;
				}
				else
				{
					if (action)
						break;
				}
			}

			int level = 0;
			if (!Data->Level.empty() && index < (int)Data->Level.size())
				level = Data->Level[index];
			Condition con = Condition::EQ;
			if (!Data->Condition.empty() && index < (int)Data->Condition.size())
				con = Data->Condition[index];
			int stacks = -1;
			std::map<std::string, int> aeStacks = AE->AEManager->AEStacks;
			auto it = aeStacks.find(watch);
			if (it != aeStacks.end())
			{
				stacks = it->second;
				action = stacks > 0 && CanActive(stacks, level, con);
			}
			else
			{
				action = false;
			}
			index++;
		}
	}

	if (action)
	{
		_count++;
		// 添加AE
		if (Data->Attach)
		{
			AttachEffect* aeManager = AE->AEManager;
			TechnoClass* pSource = AE->pSource;
			HouseClass* pSourceHouse = AE->pSourceHouse;
			if (Data->AttachToSource)
			{
				aeManager = GetAEManager<TechnoExt>(AE->pSource);
				if (pTechno)
				{
					pSource = pTechno;
					pSourceHouse = pTechno->Owner;
				}
				else if (pBullet)
				{
					pSource = pBullet->Owner;
					pSourceHouse = GetHouse(pBullet);
				}
				else
				{
					aeManager = nullptr;
				}
			}
			if (aeManager)
			{
				aeManager->Attach(Data->AttachEffects, Data->AttachChances, false, pSource, pSourceHouse);
			}
		}
		// 移除AE
		if (Data->Remove)
		{
			AttachEffect* aeManager = AE->AEManager;
			if (Data->RemoveToSource)
			{
				aeManager = GetAEManager<TechnoExt>(AE->pSource);
			}
			if (aeManager)
			{
				if (!Data->RemoveEffects.empty())
				{
					if (!Data->RemoveEffectsLevel.empty())
					{
						// 移除指定的层数
						std::map<std::string, int> aeTypes;
						int idx = 0;
						int count = Data->RemoveEffects.size();
						for (std::string removeAE : Data->RemoveEffects)
						{
							int level = -1;
							if (idx < count)
							{
								level = Data->RemoveEffectsLevel[idx];
							}
							if (level > 0)
							{
								aeTypes[removeAE] = level;
							}
						}
						if (!aeTypes.empty())
						{
							aeManager->DetachByName(aeTypes, Data->RemoveEffectsSkipNext);
						}
					}
					else
					{
						aeManager->DetachByName(Data->RemoveEffects, Data->RemoveEffectsSkipNext);
					}
				}
				if (!Data->RemoveEffectsWithMarks.empty())
				{
					aeManager->DetachByMarks(Data->RemoveEffectsWithMarks, Data->RemoveEffectsSkipNext);
				}
			}
		}
		// 移除被监视者
		bool removeLevel = false;
		if (!Data->RemoveLevel.empty())
		{
			// 移除指定的层数
			std::map<std::string, int> aeTypes;
			int idx = 0;
			int count = Data->RemoveLevel.size();
			for (std::string watch : Data->Watch)
			{
				int level = 0;
				if (idx < count)
				{
					level = Data->RemoveLevel[idx];
				}
				if (level > 0)
				{
					aeTypes[watch] = level;
				}
			}
			removeLevel = !aeTypes.empty();
			if (removeLevel)
			{
				AE->AEManager->DetachByName(aeTypes, Data->RemoveSkipNext);
			}
		}
		if (!removeLevel && Data->RemoveAll)
		{
			AE->AEManager->DetachByName(Data->Watch, Data->RemoveSkipNext);
		}
		// 检查触发次数
		if (Data->TriggeredTimes > 0 && _count >= Data->TriggeredTimes)
		{
			Deactivate();
			AE->TimeToDie();
		}
	}
}

void StackEffect::OnUpdate()
{
	if (!AE->OwnerIsDead())
	{
		Watch();
	}
}

void StackEffect::OnWarpUpdate()
{
	if (!AE->OwnerIsDead())
	{
		Watch();
	}
}
